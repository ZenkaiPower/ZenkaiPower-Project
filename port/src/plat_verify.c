#include "port_pack.h"
#include "port_online_presence.h"
/*
 * Is the game data the original disc's?
 *
 * The port is written against one disc, the USA release (SLUS-21678). The installer checks the two programs; the
 * data (the three AFS archives, unpacked into folders of loose files, and the streams) it takes as it is. A
 * modified disc image, or files replaced in the data folder or through <root>/mods/, therefore installs and
 * runs, and what breaks with it looks like the port's fault in a screenshot. So the game says so itself:
 *
 *   at start a background thread compares every data file with the table of plat_verify_tab.h (size and CRC-32
 *   of the original's 68,605 files, made by port/tools/make_verify.py), and when something differs
 *     - the log says how many files and names the first ones,
 *     - the window title gets " [modified game data]" (so that a screenshot of the window carries it),
 *     - a notice is shown once (the F1 menu's notice line),
 *     - a crash report has the line (plat_crash.c).
 *
 * The comparison reads about 3 GB: a few seconds, once. Its result is kept in <root>/.verified with a signature
 * of every file's size and modification time, so later starts only look at those (and that too is done on the
 * thread: the start of the game does not wait for any of it).
 *
 * What does NOT count: texture packs, added songs, stages and characters live outside the data folder.
 * ZENKAIPOWER_VERIFY=0 turns the check off; without a window (ZENKAIPOWER_GS=none) it only runs with ZENKAIPOWER_VERIFY=1.
 *
 * The same thread computes the gameplay fingerprint (plat_fprint.c), which asks the other question: not "is every
 * file the original's" but "does this data compute the same fight". It is computed with ZENKAIPOWER_VERIFY=0 too.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef __ANDROID__
#include <sys/resource.h>
#include <time.h>
/* The scan shares storage with foreground gameplay. Yield between small
 * batches and run on Android's background CPU priority, including fingerprinting. */
static void verify_yield(unsigned n) {
    if (!(n & 31)) { struct timespec pause={0,2000000}; nanosleep(&pause,NULL); }
}
#else
#define verify_yield(n) ((void)0)
#endif
#include "plat_verify_tab.h"
#include "gs/net_sign.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

extern const char *Port_FileRoot(void); /* plat_file.c */
extern void Port_UiNotice(const char *text); /* gs/ui.cpp */
extern void PortFp_Run(int detail); /* plat_fprint.c: the gameplay fingerprint, computed on this thread */
static int sCheck = 1; /* 0 with ZENKAIPOWER_VERIFY=0: only the fingerprint */

enum { DATA_CHECKING = 0, DATA_ORIGINAL = 1, DATA_MODIFIED = 2, DATA_OFF = 3 };
static volatile int sState = DATA_OFF;
static int sDiffer, sMissing, sMods, sTotal;
static char sFirst[4][40];
static int sAtLeast; /* counted by size only (the quick pass): files of the same size were not read */
static char sSummary[160];

int Port_DataState(void) { return sState; }
/* One line for the log, the notice and the crash report; "" unless the data is modified. */
const char *Port_DataSummary(void) {
    return sState == DATA_MODIFIED ? sSummary : sState == DATA_CHECKING ? "game data: not checked yet (the check was still running)" : "";
}

/* CRC-32 (the zlib one), eight bytes a step. */
static uint32_t sTab[8][256];
static void crc_build(void) {
    uint32_t i, k, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        sTab[0][i] = c;
    }
    for (i = 0; i < 256; i++) {
        for (k = 1; k < 8; k++) {
            sTab[k][i] = (sTab[k - 1][i] >> 8) ^ sTab[0][sTab[k - 1][i] & 0xFF];
        }
    }
}
static pthread_once_t sCrcOnce = PTHREAD_ONCE_INIT;
static void crc_init(void) { pthread_once(&sCrcOnce, crc_build); }
static uint32_t crc_add(uint32_t c, const uint8_t *p, size_t n) {
    c = ~c;
    while (n >= 8) {
        uint32_t a, b;
        memcpy(&a, p, 4);
        memcpy(&b, p + 4, 4);
        a ^= c;
        c = sTab[7][a & 0xFF] ^ sTab[6][(a >> 8) & 0xFF] ^ sTab[5][(a >> 16) & 0xFF] ^ sTab[4][a >> 24] ^
            sTab[3][b & 0xFF] ^ sTab[2][(b >> 8) & 0xFF] ^ sTab[1][(b >> 16) & 0xFF] ^ sTab[0][b >> 24];
        p += 8;
        n -= 8;
    }
    while (n-- > 0) {
        c = sTab[0][(c ^ *p++) & 0xFF] ^ (c >> 8);
    }
    return ~c;
}

#define FILES ((int)(sizeof(kVerify) / sizeof(kVerify[0])))
/* The path of file number n under the data root, as the game asks for it. */
static void rel_of(int n, char *out, size_t size) {
    int part;
    for (part = 0; part < 3; part++) {
        if (n < kVerifyCount[part]) {
            snprintf(out, size, "pzs3us%d/%05d.bin", part, n);
            return;
        }
        n -= kVerifyCount[part];
    }
    snprintf(out, size, "%s", kVerifyLoose[n]);
}

static void note(const char *rel) {
    int seen = sDiffer + sMissing + sMods;
    if (seen == 0 && sState == DATA_CHECKING) {
        /* The first difference found: known from here on, whatever happens before the count is finished (a crash
           report written a second after the start has the line). */
        snprintf(sSummary, sizeof(sSummary), "modified game data: files are not the original disc's (%s first; still counting)", rel);
        sState = DATA_MODIFIED;
    }
    if (seen < 4) {
        snprintf(sFirst[seen], sizeof(sFirst[seen]), "%s", rel);
    }
}

static void finish(void) {
    int bad = sDiffer + sMissing + sMods, k;
    if (bad == 0) {
        fprintf(stderr, "zenkaipower: game data: the original disc's (%d files checked)\n", sTotal);
        sState = DATA_ORIGINAL;
        return;
    }
    snprintf(sSummary, sizeof(sSummary), "modified game data: %s%d of %d files are not the original disc's",
             sAtLeast ? "at least " : "", bad, sTotal);
    fprintf(stderr, "zenkaipower: %s (%d changed, %d missing, %d replaced through mods/). This build supports the original USA "
                    "disc (SLUS-21678); glitches are expected. The first:", sSummary, sDiffer, sMissing, sMods);
    for (k = 0; k < bad && k < 4; k++) {
        fprintf(stderr, " %s", sFirst[k]);
    }
    fprintf(stderr, "\n");
    sState = DATA_MODIFIED;
    Port_UiNotice("Modified game data: this build supports the original USA disc. Glitches are expected.");
}

static void *verify_thread(void *arg) {
    const char *root = Port_FileRoot();
    char path[700], rel[64], cache[700];
    uint32_t sig = 0;
    struct stat st;
    uint8_t *buf;
    FILE *f;
    int n;

    (void)arg;
#ifdef __ANDROID__
    setpriority(PRIO_PROCESS,0,19);
#endif
    if (!sCheck) {
        PortFp_Run(0);
        return NULL;
    }
    crc_init();
    sTotal = FILES;
    /* the signature: every file's size and time, and whether mods/ has one for it */
    for (n = 0; n < FILES; n++) {
        verify_yield((unsigned)n);
        uint64_t rec[4] = { 0, 0, 0, 0 };
        rel_of(n, rel, sizeof(rel));
        snprintf(path, sizeof(path), "%s/%s", root, rel);
        if (Port_PackStat(path, &st) == 0) {
            rec[0] = (uint64_t)st.st_size + 1;
            rec[1] = (uint64_t)st.st_mtime;
        }
        snprintf(path, sizeof(path), "%s/mods/%s", root, rel);
        if (Port_PackStat(path, &st) == 0) {
            rec[2] = (uint64_t)st.st_size + 1;
            rec[3] = (uint64_t)st.st_mtime;
        }
        /* the quick pass: a replaced, missing or resized file shows without reading anything */
        if (rec[2] != 0) {
            note(rel);
            sMods++;
        } else if (rec[0] == 0) {
            note(rel);
            sMissing++;
        } else if (rec[0] - 1 != kVerify[n][0]) {
            note(rel);
            sDiffer++;
        }
        sig = crc_add(sig, (const uint8_t *)rec, sizeof(rec));
    }
    if (sDiffer + sMissing + sMods > 0) {
        /* Enough to say so, and at once: a modified disc's data can stop the game within seconds of its start
           (seen with a romhack's models), long before 3 GB are read. Files of the original's size are not read
           then, so the number is a lower bound. */
        sAtLeast = 1;
        finish();
        PortFp_Run(0); /* (reads the files that are loaded, those under mods/ too) */
        return NULL;
    }
    snprintf(cache, sizeof(cache), "%s/.verified", root);
    f = Port_PackFopen(cache, "r");
    if (f != NULL) {
        unsigned int was = 0;
        int ver = 0, got;
        got = fscanf(f, "%d %x %d %d %d", &ver, &was, &sDiffer, &sMissing, &sMods);
        for (n = 0; n < 4 && got == 5; n++) {
            if (fscanf(f, "%39s", sFirst[n]) != 1) {
                sFirst[n][0] = '\0';
            }
        }
        fclose(f);
        if (got == 5 && ver == 2 && was == sig) {
            finish();
            PortFp_Run(0);
            return NULL;
        }
    }
    /* The fingerprint first: a few hundred megabytes of the 3 GB, known within a second or two; the comparison
       below then finds those files in the system's cache. */
    PortFp_Run(0);
    sDiffer = sMissing = sMods = 0;
    memset(sFirst, 0, sizeof(sFirst));
    buf = malloc(1 << 20);
    if (buf == NULL) {
        sState = DATA_OFF;
        return NULL;
    }
    for (n = 0; n < FILES; n++) {
        uint32_t crc = 0;
        uint64_t size = 0;
        size_t got;
        int64_t packedBase=0,packedSize=-1;
        verify_yield((unsigned)n);

        rel_of(n, rel, sizeof(rel));
        snprintf(path, sizeof(path), "%s/mods/%s", root, rel);
        if (Port_PackStat(path, &st) == 0) {
            note(rel);
            sMods++;
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", root, rel);
        f = fopen(path, "rb");
        if (!f) f = Port_PackOpen(path,&packedBase,&packedSize);
        if (f == NULL) {
            note(rel);
            sMissing++;
            continue;
        }
        if ((packedSize>=0 && (uint64_t)packedSize!=kVerify[n][0]) ||
            (packedSize<0 && fseek(f,0,SEEK_END)==0 && (uint64_t)ftell(f)!=kVerify[n][0])) {
            fclose(f);
            note(rel);
            sDiffer++;
            continue;
        }
        if (Port_PackSeek(f,packedSize>=0 ? packedBase : 0,SEEK_SET)) {
            fclose(f); note(rel); sMissing++; continue;
        }
        while (packedSize<0 || size<(uint64_t)packedSize) {
            size_t wanted=1<<20;
            if (packedSize>=0 && (uint64_t)packedSize-size<wanted) wanted=(size_t)((uint64_t)packedSize-size);
            got=fread(buf,1,wanted,f);
            if (!got) break;
            crc = crc_add(crc, buf, got);
            size += got;
            verify_yield(0);
        }
        fclose(f);
        if (size != kVerify[n][0] || crc != kVerify[n][1]) {
            note(rel);
            sDiffer++;
        }
    }
    free(buf);
    f = Port_PackFopen(cache, "w");
    if (f != NULL) {
        fprintf(f, "2 %08x %d %d %d\n", (unsigned int)sig, sDiffer, sMissing, sMods);
        for (n = 0; n < 4 && sFirst[n][0] != '\0'; n++) {
            fprintf(f, "%s\n", sFirst[n]);
        }
        fclose(f);
    }
    finish();
    return NULL;
}

/* Called once at start (headless.c). */
void Port_VerifyStart(void) {
    const char *v = getenv("ZENKAIPOWER_VERIFY"), *gs = getenv("ZENKAIPOWER_GS");
    static int once;
    pthread_t th;
    pthread_attr_t attr;

    if (once) { /* (the caller runs again when the game restarts itself) */
        return;
    }
    once = 1;
    if (getenv("ZENKAIPOWER_FINGERPRINT") != NULL) {
        /* ZENKAIPOWER_FINGERPRINT=1: the gameplay fingerprint with every differing file named, then the program ends (a
           tool: port/tools/make_fingerprint.py is the other implementation of the same definition). */
        PortFp_Run(1);
        fflush(stderr);
        exit(0);
    }
    if (v == NULL && gs != NULL && strcmp(gs, "none") == 0) {
        return;
    }
    if (v != NULL && atoi(v) == 0) {
        sCheck = 0; /* no comparison with the disc, no notice; the fingerprint is still computed */
    }
    sState = sCheck ? DATA_CHECKING : DATA_OFF;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&th, &attr, verify_thread, NULL) != 0) {
        sState = DATA_OFF;
    }
    pthread_attr_destroy(&attr);
}

/* Online v2: hash build binaries, only check presence of required logical data files. */
int Port_BuildSignature(const char *exe, char out[65]) {
    static const char *const roles[] = {"exe", "mem", "data", "sdlwin", "sdllinux", "sdlmac"};
    char paths[6][1600], directory[1600], records[1024], hash[65];
    unsigned char digest[32];
    size_t used;
    const char *alias = getenv("ZENKAIPOWER_FILE_ALIAS");
    out[0] = 0;
    if (!exe || strlen(exe) > 1400 || (alias && alias[0])) return 0;
    snprintf(directory, sizeof(directory), "%s", exe);
    char *slash = strrchr(directory, '/');
#ifdef _WIN32
    char *backslash = strrchr(directory, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
#endif
    if (slash) *slash = 0; else strcpy(directory, ".");
    snprintf(paths[0], sizeof(paths[0]), "%s", exe);
    snprintf(paths[1], sizeof(paths[1]), "%s.mem", exe);
    snprintf(paths[2], sizeof(paths[2]), "%s", exe);
    size_t length = strlen(paths[2]);
    if (length > 4 && !strcmp(paths[2] + length - 4, ".exe")) paths[2][length - 4] = 0;
    strcat(paths[2], ".dat");
    snprintf(paths[3], sizeof(paths[3]), "%s/SDL3.dll", directory);
    snprintf(paths[4], sizeof(paths[4]), "%s/lib/libSDL3.so.0", directory);
    snprintf(paths[5], sizeof(paths[5]), "%s/lib/libSDL3.0.dylib", directory);
#ifdef __ANDROID__
    const char *metadata = getenv("ZENKAIPOWER_ANDROID_METADATA");
    if (!metadata || !*metadata) return 0;
    snprintf(paths[1], sizeof(paths[1]), "%s/libzenkaipower.so.mem", metadata);
    snprintf(paths[2], sizeof(paths[2]), "%s/libzenkaipower.so.dat", metadata);
    snprintf(paths[4], sizeof(paths[4]), "%s/libSDL3.so", directory);
#endif
    if (!Port_OnlineRequiredPresent(Port_FileRoot(), kVerifyCount, kVerifyLoose, 4)) return 0;
    used = (size_t)snprintf(records, sizeof(records), "zenkaipower-binaries-v2\nfiles=%d\n", FILES);
    for (int i = 0; i < 6; i++) {
        struct stat st;
        if (stat(paths[i], &st) == 0) {
            if (!Sign_FileSha256(paths[i], hash)) return 0;
        } else {
            if (i == 0) return 0;
            strcpy(hash, "-");
        }
        int written = snprintf(records + used, sizeof(records) - used, "%s=%s\n", roles[i], hash);
        if (written < 0 || (size_t)written >= sizeof(records) - used) return 0;
        used += written;
    }
    Sign_Sha256((const uint8_t *)records, used, digest);
    for (int n = 0; n < 32; n++) sprintf(out + n * 2, "%02x", digest[n]);
    fprintf(stderr, "zenkaipower: online data: %d required files present; binary signature %s\n", FILES, out);
    return 1;
}

int Port_DataSignature(char out[65]) {
    char exe[1600];
#ifdef __ANDROID__
    const char *library = getenv("ZENKAIPOWER_ANDROID_ENGINE");
    if (!library || strlen(library) >= sizeof(exe)) return 0;
    strcpy(exe, library);
    long length = (long)strlen(exe);
#elif defined(_WIN32)
    long length = (long)GetModuleFileNameA(NULL, exe, sizeof(exe) - 1);
#elif defined(__APPLE__)
    uint32_t capacity = sizeof(exe) - 1;
    long length = _NSGetExecutablePath(exe, &capacity) == 0 ? (long)strlen(exe) : -1;
#else
    long length = (long)readlink("/proc/self/exe", exe, sizeof(exe) - 1);
#endif
    out[0] = 0;
    if (length <= 0 || length >= (long)sizeof(exe) - 1) return 0;
    exe[length] = 0;
    return Port_BuildSignature(exe, out);
}
