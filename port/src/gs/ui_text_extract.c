#include "../port_pack.h"
/* Extract the game's UTF-16 text tables from menu archives at startup. */
#include "ui_text_extract.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#define UT_MAX_FILE (16u * 1024 * 1024)
#define UT_MAX_UNPACKED (64u * 1024 * 1024)
#define UT_MAX_LINES 16384u

#include "menu_translation.h"

typedef struct UtBuf { const uint8_t *p; size_t n; } UtBuf;
typedef struct UtJob { char data[1200], output[1200]; } UtJob;
/* A line as the walk found it (collected, not written): its ID, its UTF-16 bytes with the byte-order mark and the
   closing zero, and how many lines its table has (for matching the two discs' tables, UiText_EuropeanOnStartup). */
typedef struct UtLine { char *id; uint8_t *text; size_t n; unsigned tableLines; } UtLine;
/* Where the walk's lines go: written as the catalog's JSON (fp), or collected (fp NULL). */
typedef struct UtWriter { FILE *fp; unsigned count; int file; UtLine *lines; unsigned nLines, capLines; int failed; } UtWriter;
static pthread_mutex_t sUtLock = PTHREAD_MUTEX_INITIALIZER;
static int sUtStarted;
/* All 31 menu packs, including the separately loaded tournament, saga and Ultimate Battle screens.
   Story command 701 loads file ID 0x231 + story: archive entry 560 + story (50 episodes). */
static const int kUtTextFiles[] = {
    5,
    448, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 460, 461, 462, 463,
    464, 465, 466, 467, 468, 469, 470, 471, 472, 473, 474, 475, 476, 477, 478,
    560, 561, 562, 563, 564, 565, 566, 567, 568, 569, 570, 571, 572, 573, 574, 575,
    576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591,
    592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 605, 606, 607,
    608, 609
};

static uint32_t ut_u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t ut_u16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static int ut_offset(UtBuf b, uint32_t i, uint32_t *out) {
    if ((uint64_t)i * 4 + 4 > b.n) return 0;
    *out = ut_u32(b.p + (size_t)i * 4) & ~3u;
    return *out <= b.n;
}
static int ut_archive(UtBuf b, uint32_t *count) {
    uint32_t n, prev, off, i;
    if (b.n < 12 || (n = ut_u32(b.p)) == 0 || n > UT_MAX_LINES ||
        (uint64_t)(n + 2) * 4 > b.n || !ut_offset(b, 1, &prev) || prev < (n + 2) * 4) return 0;
    for (i = 2; i <= n + 1; i++) {
        if (!ut_offset(b, i, &off) || off < prev) return 0;
        prev = off;
    }
    *count = n;
    return 1;
}
static int ut_section(UtBuf b, uint32_t i, UtBuf *out) {
    uint32_t first, last;
    if (!ut_offset(b, i, &first) || !ut_offset(b, i + 1, &last) || last < first) return 0;
    out->p = b.p + first;
    out->n = last - first;
    return 1;
}
/* Same bounded byte-pair stream format as menu_labels.c's ml_unpack_bytes. */
static int ut_unpack(UtBuf b, UtBuf *out) {
    size_t p = 8, end, n = 0;
    uint32_t size, packed;
    uint8_t *dst;
    if (b.n < 8 || (size = ut_u32(b.p)) == 0 || size > UT_MAX_UNPACKED ||
        (packed = ut_u32(b.p + 4)) == 0 || packed > b.n - 8) return 0;
    end = 8 + packed;
    dst = malloc(size);
    if (dst == NULL) return 0;
    while (p < end) {
        uint8_t left[256], right[256], stack[256];
        size_t block, k;
        int ch = 0, i, sp;
        for (i = 0; i < 256; i++) { left[i] = (uint8_t)i; right[i] = 0; }
        while (ch < 256) {
            int count;
            if (p >= end) goto fail;
            count = b.p[p++];
            if (count > 127) { ch += count - 127; count = 0; }
            if (ch == 256) break;
            for (i = 0; i <= count; i++) {
                if (ch >= 256 || p >= end) goto fail;
                left[ch] = b.p[p++];
                if (ch != left[ch]) { if (p >= end) goto fail; right[ch] = b.p[p++]; }
                ch++;
            }
        }
        if (end - p < 2) goto fail;
        block = (size_t)b.p[p] * 256 + b.p[p + 1]; p += 2;
        if (end - p < block) goto fail;
        for (k = 0; k < block; k++) {
            sp = 0; stack[sp++] = b.p[p + k];
            while (sp) {
                uint8_t v = stack[--sp];
                if (left[v] == v) { if (n >= size) goto fail; dst[n++] = v; }
                else { if (sp + 2 > 256) goto fail; stack[sp++] = right[v]; stack[sp++] = left[v]; }
            }
        }
        p += block;
    }
    if (p != end || n != size) goto fail;
    out->p = dst; out->n = n;
    return 1;
fail:
    free(dst);
    return 0;
}
static int ut_line(UtBuf b) {
    size_t p;
    if (b.n < 4 || b.p[0] != 0xff || b.p[1] != 0xfe) return 0;
    for (p = 2; p + 1 < b.n; p += 2) {
        uint16_t c = ut_u16(b.p + p);
        if (c == 0) return 1;
        if (c >= 0xd800 && c <= 0xdbff) {
            if (p + 3 >= b.n || ut_u16(b.p + p + 2) < 0xdc00 || ut_u16(b.p + p + 2) > 0xdfff) return 0;
            p += 2;
        } else if (c >= 0xdc00 && c <= 0xdfff) return 0;
    }
    return 0;
}
static void ut_json_string(FILE *fp, UtBuf b) {
    size_t p;
    fputc('"', fp);
    for (p = 2; p + 1 < b.n; p += 2) {
        uint32_t c = ut_u16(b.p + p);
        if (!c) break;
        if (c >= 0xd800 && c <= 0xdbff) { c = 0x10000 + ((c - 0xd800) << 10) + ut_u16(b.p + p + 2) - 0xdc00; p += 2; }
        if (c == '"' || c == '\\') { fputc('\\', fp); fputc((int)c, fp); }
        else if (c < 0x20) fprintf(fp, "\\u%04x", c);
        else if (c < 0x80) fputc((int)c, fp);
        else if (c < 0x800) { fputc(0xc0 | (c >> 6), fp); fputc(0x80 | (c & 63), fp); }
        else if (c < 0x10000) { fputc(0xe0 | (c >> 12), fp); fputc(0x80 | ((c >> 6) & 63), fp); fputc(0x80 | (c & 63), fp); }
        else { fputc(0xf0 | (c >> 18), fp); fputc(0x80 | ((c >> 12) & 63), fp); fputc(0x80 | ((c >> 6) & 63), fp); fputc(0x80 | (c & 63), fp); }
    }
    fputc('"', fp);
}
/* A line found: into the catalog, or kept (a copy: the walk frees what it unpacked). */
static void ut_emit(UtWriter *w, const char *id, UtBuf line, unsigned tableLines) {
    UtLine *l;
    size_t n;
    if (w->fp != NULL) {
        fprintf(w->fp, "%s    \"%s\": {\"original\": ", w->count++ ? ",\n" : "", id);
        ut_json_string(w->fp, line);
        fputs(", \"text\": ", w->fp);
        ut_json_string(w->fp, line);
        fputc('}', w->fp);
        return;
    }
    if (w->failed) return;
    for (n = 2; n + 1 < line.n && ut_u16(line.p + n) != 0; n += 2) {}
    n += 2; /* (the closing zero: ut_line has seen it) */
    if (w->nLines == w->capLines) {
        unsigned cap = w->capLines ? w->capLines * 2 : 4096;
        UtLine *more = realloc(w->lines, cap * sizeof(*more));
        if (more == NULL) { w->failed = 1; return; }
        w->lines = more;
        w->capLines = cap;
    }
    l = &w->lines[w->nLines];
    l->id = strdup(id);
    l->text = malloc(n);
    if (l->id == NULL || l->text == NULL) { free(l->id); free(l->text); w->failed = 1; return; }
    memcpy(l->text, line.p, n);
    l->n = n;
    l->tableLines = tableLines;
    w->nLines++;
}
static void ut_walk(UtWriter *w, UtBuf b, const char *path, int depth) {
    uint32_t n, i, lines = 0;
    UtBuf line;
    if (depth > 6) return;
    if (!ut_archive(b, &n)) {
        UtBuf unpacked;
        /* The separate mode packs are compressed as a whole, rather than archive members. */
        if (ut_unpack(b, &unpacked)) {
            ut_walk(w, unpacked, path, depth + 1);
            free((void *)unpacked.p);
        }
        return;
    }
    for (i = 1; i <= n; i++) if (!ut_section(b, i, &line) || !ut_line(line)) break;
    if (i > n) {
        for (i = 1; i <= n; i++) {
            char id[128];
            ut_section(b, i, &line);
            if (snprintf(id, sizeof(id), "%s/%u", path, i - 1) >= (int)sizeof(id)) return;
            ut_emit(w, id, line, n);
        }
        return;
    }
    for (i = 1; i <= n; i++) lines += ut_section(b, i, &line) && ut_line(line);
    for (i = 1; i <= n; i++) {
        UtBuf part, unpacked;
        uint32_t childCount;
        char child[128];
        if (ut_section(b, i, &part) && ut_line(part)) { /* a line among other sections (the HUD's file 5: the pause menu) */
            char id[128];
            if (snprintf(id, sizeof(id), "%s/%u", path, i - 1) >= (int)sizeof(id)) continue;
            ut_emit(w, id, part, lines);
            continue;
        }
        if (!ut_section(b, i, &part) || part.n < 12 ||
            snprintf(child, sizeof(child), "%s/%u", path, i) >= (int)sizeof(child)) continue;
        if (ut_archive(part, &childCount)) { ut_walk(w, part, child, depth + 1); }
        else if (ut_unpack(part, &unpacked)) { ut_walk(w, unpacked, child, depth + 1); free((void *)unpacked.p); }
    }
}
static uint8_t *ut_read(const char *path, size_t *size) {
    FILE *fp = Port_PackFopen(path, "rb");
    long n;
    uint8_t *data;
    if (fp == NULL) return NULL;
    if (fseek(fp, 0, SEEK_END) || (n = ftell(fp)) < 12 || n > UT_MAX_FILE || fseek(fp, 0, SEEK_SET)) { fclose(fp); return NULL; }
    data = malloc((size_t)n);
    if (data == NULL) { fclose(fp); return NULL; }
    if (fread(data, 1, (size_t)n, fp) != (size_t)n) { free(data); fclose(fp); return NULL; }
    fclose(fp); *size = (size_t)n; return data;
}
/* Version 3 adds whole compressed packs, all mode menus and the story dialogue tables. */
#define UT_SOURCE "extracted USA game text, version 3"
/* Wii extraction supplies 44 spoken lines and 66 menu/dialog messages.
   Insert their stable IDs when a UI catalog is first generated. */
static unsigned ut_append_wii_fragment(FILE *fp, const char *data, const char *locale) {
    char path[1280], fragment[65536];
    FILE *source;
    size_t size;
    unsigned count = 0;
    const char *p;
    if (snprintf(path, sizeof(path), "%s/dragon-net-battle/ui-fragment-%s.json", data, locale) >= (int)sizeof(path)) return 0;
    source = Port_PackFopen(path, "rb");
    if (source == NULL) return 0;
    size = fread(fragment, 1, sizeof(fragment) - 1, source);
    if (!feof(source) || ferror(source)) { fclose(source); return 0; }
    fclose(source);
    fragment[size] = 0;
    for (p = fragment; (p = strstr(p, "\"wii-dragon-net/")) != NULL; p++) count++;
    if (count != 110 || fragment[0] != ',') return 0;
    fwrite(fragment, 1, size, fp);
    return count;
}
/* An English catalog this program extracted in an earlier version, or the unedited bundled override copy. */
static int ut_files_same(const char *aPath, const char *bPath);
static int ut_stale(const char *path) {
    char head[512], bundled[1200];
    size_t n;
    FILE *fp = Port_PackFopen(path, "rb");
    if (fp == NULL) return 0;
    n = fread(head, 1, sizeof(head) - 1, fp);
    fclose(fp);
    head[n] = 0;
    if ((strstr(head, "\"source\": \"extracted USA game text\"") != NULL ||
         strstr(head, "\"source\": \"extracted USA game text, version ") != NULL) &&
        strstr(head, "\"source\": \"" UT_SOURCE "\"") == NULL) return 1;
    /* The short catalog bundled with the program is an override layer, not the disc base. */
    return MenuTranslation_BundledUiCatalogPath("en", bundled, sizeof(bundled)) &&
        ut_files_same(path, bundled);
}
static void *ut_worker(void *arg) {
    UtJob *job = arg;
    char temp[1280], path[1280];
    UtWriter w = {0};
    size_t size;
    int i;
    if (snprintf(temp, sizeof(temp), "%s.part", job->output) >= (int)sizeof(temp)) goto done;
    w.fp = Port_PackFopen(temp, "wb");
    if (w.fp == NULL) goto done;
    fputs("{\n  \"schema_version\": 1,\n  \"labels_version\": 1,\n  \"translation_version\": 1,\n  \"locale\": \"en\",\n  \"name\": \"English\",\n  \"source\": \"" UT_SOURCE "\",\n  \"labels\": {\n", w.fp);
    for (i = 0; i < (int)(sizeof(kUtTextFiles) / sizeof(kUtTextFiles[0])); i++) {
        uint8_t *data;
        UtBuf b;
        if (snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", job->data, kUtTextFiles[i]) >= (int)sizeof(path)) continue;
        data = ut_read(path, &size);
        if (data == NULL) continue;
        b.p = data; b.n = size; w.file = kUtTextFiles[i];
        { char id[32]; snprintf(id, sizeof(id), "pzs3us1/%05d", w.file); ut_walk(&w, b, id, 0); }
        free(data);
    }
    w.count += ut_append_wii_fragment(w.fp, job->data, "en");
    fprintf(w.fp, "\n  },\n  \"labels_count\": %u\n}\n", w.count);
    if (fclose(w.fp) == 0 && w.count > 0) {
#ifdef _WIN32
        if (access(job->output, 0) != 0 && rename(temp, job->output) == 0) goto done;
#else
        /* link is atomic and cannot overwrite a user's catalog created meanwhile. */
        if (link(temp, job->output) == 0) { remove(temp); goto done; }
#endif
    }
    remove(temp);
done:
    free(job);
    return NULL;
}
void UiText_ExtractOnStartup(const char *gameTextDir) {
    const char *data = getenv("ZENKAIPOWER_DATA");
    UtJob *job;
    pthread_t thread;
    char parent[1200], dir[1200];
    if (gameTextDir == NULL) return;
    pthread_mutex_lock(&sUtLock);
    if (sUtStarted) { pthread_mutex_unlock(&sUtLock); return; }
    sUtStarted = 1;
    pthread_mutex_unlock(&sUtLock);
    job = calloc(1, sizeof(*job));
    if (job == NULL) return;
    if (snprintf(job->data, sizeof(job->data), "%s", data && data[0] ? data : "gamedata") >= (int)sizeof(job->data) ||
        snprintf(parent, sizeof(parent), "%s/translations", gameTextDir) >= (int)sizeof(parent) ||
        snprintf(dir, sizeof(dir), "%s/ui", parent) >= (int)sizeof(dir) ||
        snprintf(job->output, sizeof(job->output), "%s/en.json", dir) >= (int)sizeof(job->output)) { free(job); return; }
    if (ut_stale(job->output)) remove(job->output); /* (made by an older release: made again, with what it lacked) */
    if (getenv("ZENKAIPOWER_PREPARE_TEXT") ? access(job->output,0)==0 : Port_PackExists(job->output)) { free(job); return; }
#ifdef _WIN32
    _mkdir(gameTextDir); _mkdir(parent); _mkdir(dir);
#else
    mkdir(gameTextDir, 0755); mkdir(parent, 0755); mkdir(dir, 0755);
#endif
    if (getenv("ZENKAIPOWER_PREPARE_TEXT")) { ut_worker(job); return; }
    if (pthread_create(&thread, NULL, ut_worker, job) != 0) { free(job); return; }
    pthread_detach(thread);
}

/* ---- the European disc's languages ------------------------------------------------------------------------------
   The European release (SLES-54945) has the menus, the battle HUD and their text once per language: English,
   Spanish, German, French, Italian. The setup (or port/tools/extract_eu.py) copies those archive entries into
   gamedata/pzs3eu1 as they are; from them the catalogs ui/es.json, de.json, fr.json and it.json are made here, keyed
   by the USA disc's IDs. A line is the same line on both discs where the European English is the USA English, or
   where its table has as many lines on both (the European English is then the USA line reworded: bullets added, a
   typo fixed, "Fight Again?"). Lines without their like stay English. */
#define UT_EU_SOURCE "European disc, version 2"
#define UT_EU_STRIDE 31  /* the menu archives: USA 448 + k is European 451 + k + 31 * language */
#define UT_EU_FIRST 451
#define UT_EU_HUD 5      /* the HUD: USA 5 is European 5 + language */
static const struct { const char *locale, *name; } kUtEuLanguages[4] = {
    {"es", "Espa\xc3\xb1ol"}, {"de", "Deutsch"}, {"fr", "Fran\xc3\xa7" "ais"}, {"it", "Italiano"}};

static void ut_lines_free(UtWriter *w) {
    unsigned i;
    for (i = 0; i < w->nLines; i++) { free(w->lines[i].id); free(w->lines[i].text); }
    free(w->lines);
    memset(w, 0, sizeof(*w));
}
static int ut_line_order(const void *a, const void *b) {
    return strcmp(((const UtLine *)a)->id, ((const UtLine *)b)->id);
}
static const UtLine *ut_line_find(const UtWriter *w, const char *id) {
    UtLine key;
    key.id = (char *)id;
    return bsearch(&key, w->lines, w->nLines, sizeof(*w->lines), ut_line_order);
}
static int ut_line_same(const UtLine *a, const UtLine *b) {
    return a->n == b->n && memcmp(a->text, b->text, a->n) == 0;
}
static int ut_files_same(const char *aPath, const char *bPath) {
    unsigned char a[4096], b[4096];
    FILE *aFile = Port_PackFopen(aPath, "rb");
    FILE *bFile = Port_PackFopen(bPath, "rb");
    int same = 1;
    if (aFile == NULL || bFile == NULL) {
        if (aFile != NULL) fclose(aFile);
        if (bFile != NULL) fclose(bFile);
        return 0;
    }
    for (;;) {
        size_t aSize = fread(a, 1, sizeof(a), aFile);
        size_t bSize = fread(b, 1, sizeof(b), bFile);
        if (aSize != bSize || memcmp(a, b, aSize) != 0) { same = 0; break; }
        if (aSize == 0) break;
    }
    if (ferror(aFile) || ferror(bFile)) same = 0;
    fclose(aFile);
    fclose(bFile);
    return same;
}
/* The lines of one disc: the USA one's (bank -1, gamedata/pzs3us1) or a European language bank's (0 English ...). */
static int ut_collect(const char *data, int bank, UtWriter *w) {
    int i;
    for (i = 0; i < (int)(sizeof(kUtTextFiles) / sizeof(kUtTextFiles[0])); i++) {
        char path[1300], id[32];
        size_t size;
        uint8_t *bytes;
        UtBuf b;
        int usa = kUtTextFiles[i];
        /* EU story English begins at 656, then 50 entries per language (verified against SLES-54945). */
        int entry = bank < 0 ? usa : usa == UT_EU_HUD ? UT_EU_HUD + bank :
                    usa >= 560 ? 656 + (usa - 560) + bank * 50 :
                    UT_EU_FIRST + (usa - 448) + bank * UT_EU_STRIDE;
        if (snprintf(path, sizeof(path), "%s/%s/%05d.bin", data, bank < 0 ? "pzs3us1" : "pzs3eu1", entry) >= (int)sizeof(path)) return 0;
        if ((bytes = ut_read(path, &size)) == NULL) return 0;
        b.p = bytes; b.n = size;
        snprintf(id, sizeof(id), "pzs3us1/%05d", usa);
        ut_walk(w, b, id, 0);
        free(bytes);
    }
    return !w->failed && w->nLines > 0;
}
/* A catalog of ours that is current (made by this version from the disc), or one that is someone else's (written by
   a person, or by an older tool): left as it is. Missing, or ours from another version: made. */
static int ut_eu_wanted(const char *path, const char *locale) {
    char head[512];
    char bundled[1200];
    size_t n;
    FILE *fp = Port_PackFopen(path, "rb");
    if (fp == NULL) return 1;
    n = fread(head, 1, sizeof(head) - 1, fp);
    fclose(fp);
    head[n] = 0;
    if (strstr(head, "\"source\": \"European disc, version ") != NULL)
        return strstr(head, "\"source\": \"" UT_EU_SOURCE "\"") == NULL;
    return MenuTranslation_BundledUiCatalogPath(locale, bundled, sizeof(bundled)) &&
        ut_files_same(path, bundled);
}
int UiText_EuropeanOnStartup(const char *gameTextDir, char *why, size_t whyLen) {
    const char *env = getenv("ZENKAIPOWER_DATA");
    char data[1100], dir[1300], path[1400], temp[1420];
    UtWriter usa, eu[5];
    unsigned i, matched = 0;
    int k, want = 0, made = 0, result = -1;

    snprintf(why, whyLen, "%s", "");
    snprintf(data, sizeof(data), "%s", env != NULL && env[0] ? env : "gamedata");
    if (gameTextDir == NULL || snprintf(path, sizeof(path), "%s/pzs3eu1/%05d.bin", data, UT_EU_HUD) >= (int)sizeof(path) || !Port_PackExists(path) ||
        snprintf(dir, sizeof(dir), "%s/translations/ui", gameTextDir) >= (int)sizeof(dir)) {
        return 0; /* (no European disc taken) */
    }
    for (k = 0; k < 4; k++) {
        snprintf(path, sizeof(path), "%s/%s.json", dir, kUtEuLanguages[k].locale);
        want |= ut_eu_wanted(path, kUtEuLanguages[k].locale) << k;
    }
    if (want == 0) return 0;
    memset(&usa, 0, sizeof(usa));
    memset(eu, 0, sizeof(eu));
    if (!ut_collect(data, -1, &usa)) {
        snprintf(why, whyLen, "the USA disc's menu archives cannot be read from %s/pzs3us1", data);
        goto done;
    }
    for (k = 0; k < 5; k++) {
        if (!ut_collect(data, k, &eu[k])) {
            snprintf(why, whyLen, "the European disc's archives in %s/pzs3eu1 are incomplete or damaged; import the European disc again to add mode and story text", data);
            goto done;
        }
        qsort(eu[k].lines, eu[k].nLines, sizeof(*eu[k].lines), ut_line_order);
    }
    for (i = 0; i < usa.nLines; i++) {
        const UtLine *e = ut_line_find(&eu[0], usa.lines[i].id);
        matched += e != NULL && ut_line_same(e, &usa.lines[i]);
    }
    if (matched * 10 < usa.nLines * 7) { /* (another disc's archives, numbered otherwise) */
        snprintf(why, whyLen, "the archives in %s/pzs3eu1 do not match the USA disc's (%u of %u lines)", data, matched, usa.nLines);
        goto done;
    }
#ifdef _WIN32
    { char parent[1300]; snprintf(parent, sizeof(parent), "%s/translations", gameTextDir); _mkdir(gameTextDir); _mkdir(parent); _mkdir(dir); }
#else
    { char parent[1300]; snprintf(parent, sizeof(parent), "%s/translations", gameTextDir); mkdir(gameTextDir, 0755); mkdir(parent, 0755); mkdir(dir, 0755); }
#endif
    for (k = 0; k < 4; k++) {
        FILE *fp;
        unsigned written = 0, translated = 0;
        if (!(want >> k & 1)) continue;
        snprintf(path, sizeof(path), "%s/%s.json", dir, kUtEuLanguages[k].locale);
        snprintf(temp, sizeof(temp), "%s.part", path);
        if ((fp = Port_PackFopen(temp, "wb")) == NULL) {
            snprintf(why, whyLen, "%s cannot be written", temp);
            goto done;
        }
        fprintf(fp, "{\n  \"schema_version\": 1,\n  \"labels_version\": 1,\n  \"translation_version\": 1,\n  \"locale\": \"%s\",\n"
                    "  \"name\": \"%s\",\n  \"source\": \"" UT_EU_SOURCE "\",\n  \"labels\": {\n",
                kUtEuLanguages[k].locale, kUtEuLanguages[k].name);
        for (i = 0; i < usa.nLines; i++) {
            const UtLine *u = &usa.lines[i], *e0 = ut_line_find(&eu[0], u->id), *e = ut_line_find(&eu[k + 1], u->id);
            UtBuf original = {u->text, u->n}, text = original;
            if (e != NULL && e0 != NULL && (ut_line_same(e0, u) || e0->tableLines == u->tableLines)) {
                text.p = e->text;
                text.n = e->n;
                translated += !ut_line_same(e, u);
            }
            fprintf(fp, "%s    \"%s\": {\"original\": ", written++ ? ",\n" : "", u->id);
            ut_json_string(fp, original);
            fputs(", \"text\": ", fp);
            ut_json_string(fp, text);
            fputc('}', fp);
        }
        written += ut_append_wii_fragment(fp, data, kUtEuLanguages[k].locale);
        fprintf(fp, "\n  },\n  \"labels_count\": %u\n}\n", written);
        if (fclose(fp) != 0) {
            remove(temp);
            snprintf(why, whyLen, "%s cannot be written", temp);
            goto done;
        }
        remove(path); /* (Windows' rename does not replace) */
        if (rename(temp, path) != 0) {
            remove(temp);
            snprintf(why, whyLen, "%s cannot be written", path);
            goto done;
        }
        made++;
        fprintf(stderr, "zenkaipower: European disc: %s, %u of %u lines translated\n", path, translated, written);
    }
    result = made;
    snprintf(why, whyLen, "%d languages made", made);
done:
    ut_lines_free(&usa);
    for (k = 0; k < 5; k++) ut_lines_free(&eu[k]);
    return result;
}

/* Character packs contain all five Western skill-list scripts even on the USA
   disc. Read only those sections, directly from loose packs or AFS slices. */
#define UT_SKILL_SOURCE "USA embedded skill scripts, version 1"
static uint8_t *ut_skill_script(FILE *fp, int64_t base, int64_t size, const uint8_t *table,
                                unsigned section, size_t *bytes) {
    uint32_t first=ut_u32(table+section*4)&~3u, last=ut_u32(table+(section+1)*4)&~3u;
    uint8_t *text;
    size_t end;
    if(first<0x35*4 || last<=first || last>(uint64_t)size || last-first>128*1024) return NULL;
    text=malloc(last-first);
    if(!text) return NULL;
    if(Port_PackSeek(fp,base+first,SEEK_SET) || fread(text,1,last-first,fp)!=last-first ||
       !ut_line((UtBuf){text,last-first})) {free(text);return NULL;}
    for(end=2;end+1<last-first && ut_u16(text+end);end+=2) {}
    *bytes=end+2;
    return text;
}


/* Publish all five catalogs before removing their loose copies. Existing entries
   are read through Port_PackFopen, so a partial update preserves other locales. */
static int ut_skill_package(const char *dir, const char *data) {
    static const char *const names[]={"en.json","es.json","de.json","fr.json","it.json"};
    FILE *inputs[5]={0},*out=NULL;
    int64_t sizes[5],offset=12;
    char path[1280],package[1280],temporary[1300];
    int k,ok=0;
    snprintf(package,sizeof(package),"%s/game-skills.pak",data);
    snprintf(temporary,sizeof(temporary),"%s.part",package);
    for(k=0;k<5;k++) {
        snprintf(path,sizeof(path),"%s/%s",dir,names[k]);
        inputs[k]=Port_PackFopen(path,"rb");
        if(!inputs[k] || Port_PackSeek(inputs[k],0,SEEK_END) ||
           (sizes[k]=Port_PackTell(inputs[k]))<1 || sizes[k]>32*1024*1024 ||
           Port_PackSeek(inputs[k],0,SEEK_SET)) goto done;
        offset+=20+strlen(names[k]);
    }
    out=fopen(temporary,"wb");
    if(!out) goto done;
    fwrite("BT3TEXT1",1,8,out);
    for(int n=0;n<4;n++) fputc((5u>>(8*n))&255,out);
    for(k=0;k<5;k++) {
        uint64_t words[]={strlen(names[k]),(uint64_t)offset,(uint64_t)sizes[k]};
        for(int w=0;w<3;w++) for(int n=0;n<(w ? 8:4);n++) fputc((int)(words[w]>>(8*n))&255,out);
        fwrite(names[k],1,strlen(names[k]),out);
        offset+=sizes[k];
    }
    for(k=0;k<5;k++) if(!Port_PackCopy(inputs[k],out,sizes[k])) goto done;
    if(ferror(out) || fflush(out)) goto done;
    if(fclose(out)) {out=NULL;goto done;}
    out=NULL;
    for(k=0;k<5;k++) {fclose(inputs[k]);inputs[k]=NULL;}
#ifdef _WIN32
    if(!MoveFileExA(temporary,package,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) goto done;
#else
    if(rename(temporary,package)) goto done;
#endif
    ok=1;
    for(k=0;k<5;k++) {snprintf(path,sizeof(path),"%s/%s",dir,names[k]);remove(path);}
    rmdir(dir); /* succeeds only when no other user files remain */
done:
    if(out) fclose(out);
    for(k=0;k<5;k++) if(inputs[k]) fclose(inputs[k]);
    if(!ok) {remove(temporary);fprintf(stderr,"zenkaipower: skill text packaging failed; source catalogs retained\n");}
    return ok;
}

void UiText_ExtractSkills(const char *gameTextDir) {
    static const char *const locales[]={"en","es","de","fr","it"};
    static const unsigned sections[]={0x2d,0x30,0x31,0x32,0x33};
    FILE *outputs[5]={0};
    unsigned counts[5]={0};
    char paths[5][1280],temps[5][1280],dir[1200],data[1200],path[1280];
    UtWriter seen={0};
    const char *env=getenv("ZENKAIPOWER_DATA");
    int k,character,costume,wanted=0,failed=0,loose=0;
    if(!gameTextDir) return;
    pthread_mutex_lock(&sUtLock);
    snprintf(data,sizeof(data),"%s",env ? env:gameTextDir);
    if(!env) {char *slash=strrchr(data,'/'),*back=strrchr(data,'\\');if(back && (!slash || back>slash)) slash=back;if(slash) *slash=0;else snprintf(data,sizeof(data),"gamedata");}
    snprintf(dir,sizeof(dir),"%s/translations",gameTextDir);
#ifdef _WIN32
    _mkdir(gameTextDir);_mkdir(dir);
#else
    mkdir(gameTextDir,0755);mkdir(dir,0755);
#endif
    snprintf(dir,sizeof(dir),"%s/translations/skills",gameTextDir);
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir,0755);
#endif
    for(k=0;k<5;k++) {
        char head[512];size_t n;FILE *fp;
        snprintf(paths[k],sizeof(paths[k]),"%s/%s.json",dir,locales[k]);
        fp=fopen(paths[k],"rb");if(fp) {loose=1;fclose(fp);}
        fp=Port_PackFopen(paths[k],"rb");
        if(fp) {n=fread(head,1,sizeof(head)-1,fp);fclose(fp);head[n]=0;
            /* Preserve edited catalogs and current generated ones. */
            if(!strstr(head,"USA embedded skill scripts, version ") || strstr(head,UT_SKILL_SOURCE)) continue;
        }
        wanted|=1<<k;
        snprintf(temps[k],sizeof(temps[k]),"%s.part",paths[k]);
        outputs[k]=Port_PackFopen(temps[k],"wb");
        if(!outputs[k]) {failed=1;break;}
        fprintf(outputs[k],"{\n  \"schema_version\": 1,\n  \"locale\": \"%s\",\n  \"source\": \"" UT_SKILL_SOURCE "\",\n  \"labels\": {\n",locales[k]);
    }
    if(!wanted || failed) goto done;
    for(character=0;character<161 && !failed;character++) for(costume=0;costume<8;costume++) {
        unsigned entry=0x58f+character*10+costume,j;
        uint8_t table[0x35*4],*scripts[5]={0};size_t sizes[5]={0};
        int64_t base=0,size=0;FILE *fp;
        snprintf(path,sizeof(path),"%s/pzs3us1/%05u.bin",data,entry);
        fp=fopen(path,"rb");
        if(fp) {if(Port_PackSeek(fp,0,SEEK_END) || (size=Port_PackTell(fp))<sizeof(table) || Port_PackSeek(fp,0,SEEK_SET)) {fclose(fp);continue;}}
        else fp=Port_PackOpen(path,&base,&size);
        if(!fp) continue;
        if(size<sizeof(table) || fread(table,1,sizeof(table),fp)!=sizeof(table) || ut_u32(table)<0x34) {fclose(fp);continue;}
        for(k=0;k<5;k++) scripts[k]=ut_skill_script(fp,base,size,table,sections[k],&sizes[k]);
        fclose(fp);
        if(scripts[0]) {
            for(j=0;j<seen.nLines;j++) if(seen.lines[j].n==sizes[0] && !memcmp(seen.lines[j].text,scripts[0],sizes[0])) break;
            if(j==seen.nLines) {
                char id[96];
                snprintf(id,sizeof(id),"skills/%03d/%d",character,costume);
                ut_emit(&seen,id,(UtBuf){scripts[0],sizes[0]},1);
                if(seen.failed) failed=1;
                for(k=0;k<5;k++) if(outputs[k]) {
                    fprintf(outputs[k],"%s    \"%s\": {\"original\": ",counts[k]++ ? ",\n":"",id);
                    ut_json_string(outputs[k],(UtBuf){scripts[0],sizes[0]});
                    fputs(", \"text\": ",outputs[k]);
                    ut_json_string(outputs[k],(UtBuf){scripts[k] ? scripts[k]:scripts[0],scripts[k] ? sizes[k]:sizes[0]});
                    fputc('}',outputs[k]);
                }
            }
        }
        for(k=0;k<5;k++) free(scripts[k]);
    }
done:
    for(k=0;k<5;k++) if(outputs[k]) {
        int ok;
        fprintf(outputs[k],"\n  },\n  \"labels_count\": %u\n}\n",counts[k]);
        ok=fclose(outputs[k])==0;
        if(ok && !failed && counts[k]) {remove(paths[k]);if(rename(temps[k],paths[k])) remove(temps[k]);}
        else remove(temps[k]);
    }
    for(unsigned j=0;j<seen.nLines;j++) {free(seen.lines[j].id);free(seen.lines[j].text);}
    free(seen.lines);
    if(!failed && (wanted || loose)) ut_skill_package(dir,data);
    else if(!wanted && !loose) rmdir(dir);
    pthread_mutex_unlock(&sUtLock);
}
