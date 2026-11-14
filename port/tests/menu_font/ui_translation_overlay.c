/* Bundled UI entries override matching disc-generated entries, without hiding the rest.
   cc -std=gnu99 -Wall -Wextra -DUI_TEXT_HOST_ALLOC -Iport/src/gs \
      port/tests/menu_font/ui_translation_overlay.c port/src/gs/ui_text_translate.c -o ui_translation_overlay_test */
#define _GNU_SOURCE
#include "ui_text_translate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char sBundled[256];

static void table(uint8_t *buf) {
    uint32_t *words = (uint32_t *)buf;
    uint16_t *play = (uint16_t *)(buf + 16), *quit = (uint16_t *)(buf + 32);
    int i;
    memset(buf, 0, 64);
    words[0] = 2; words[1] = 16; words[2] = 32;
    play[0] = quit[0] = 0xfeff;
    for (i = 0; i < 4; i++) { play[i + 1] = "Play"[i]; quit[i + 1] = "Quit"[i]; }
}

int MenuTranslation_BundledUiCatalogPath(const char *locale, char *path, size_t size) {
    if ((strcmp(locale, "es") && strcmp(locale, "en")) || !sBundled[0] ||
        snprintf(path, size, "%s", sBundled) >= (int)size) return 0;
    return 1;
}

/* Optional integration check against a prepared release's packed English catalog. */
static int release_catalog_test(void) {
    static const uint16_t original[] = {0xfeff, 'M', 'E', 'M', 'O', 'R', 'Y', ' ', 'C', 'A', 'R', 'D',
                                        ' ', 's', 'l', 'o', 't', ' ', '1', 0};
    const uint16_t *text;
    char byId[64];
    if (!getenv("ZENKAIPOWER_UI_REAL_TEST")) return 0;
    snprintf(sBundled, sizeof(sBundled), "port/translations/ui/en.json");
    if (!UiText_Load("release/linux-x64/gamedata/game-text", "en")) return 1;
    text = UiText_TranslateString(original);
    return text == original || text[1] != 'S' || text[2] != 'a' ||
        !UiText_TranslateIdUtf8("pzs3us1/00448/1/25/6", 0, byId, sizeof(byId)) || strcmp(byId, "Save storage");
}

int main(void) {
    static const char generated[] =
        "{\"schema_version\":1,\"locale\":\"es\",\"labels\":{\"one/0\":{\"original\":\"Play\",\"text\":\"Generated\"},"
        "\"one/1\":{\"original\":\"Quit\",\"text\":\"Salir\"}}}";
    static const char bundled[] =
        "{\"schema_version\":1,\"locale\":\"es\",\"labels\":{\"one/0\":{\"original\":\"Play\",\"text\":\"Override\"}}}";
    uint8_t lines[64];
    const uint16_t *text;
    char dir[] = "/tmp/zenkaipower-ui-overlay-XXXXXX", path[300];
    FILE *fp;
    int result = 1;
    if (release_catalog_test()) goto done;
    if (!mkdtemp(dir)) return 1;
    snprintf(path, sizeof(path), "%s/translations", dir);
    if (mkdir(path, 0700)) goto done;
    snprintf(path, sizeof(path), "%s/translations/ui", dir);
    if (mkdir(path, 0700)) goto done;
    snprintf(path, sizeof(path), "%s/translations/ui/es.json", dir);
    if (!(fp = fopen(path, "wb"))) goto done;
    if (fwrite(generated, 1, sizeof(generated) - 1, fp) != sizeof(generated) - 1 || fclose(fp)) goto done;
    snprintf(sBundled, sizeof(sBundled), "%s/bundled.json", dir);
    if (!(fp = fopen(sBundled, "wb"))) goto done;
    if (fwrite(bundled, 1, sizeof(bundled) - 1, fp) != sizeof(bundled) - 1 || fclose(fp)) goto done;
    if (!UiText_Load(dir, "es")) goto done;
    table(lines);
    text = UiText_TranslateLine((const uint32_t *)lines, 0, (const uint16_t *)(lines + 16));
    if (text == (const uint16_t *)(lines + 16) || text[1] != 'O') goto done;
    text = UiText_TranslateLine((const uint32_t *)lines, 1, (const uint16_t *)(lines + 32));
    if (text[1] != 'S') goto done;
    result = 0;
done:
    if (!strncmp(sBundled, "/tmp/zenkaipower-ui-overlay-", 20)) unlink(sBundled);
    snprintf(path, sizeof(path), "%s/translations/ui/es.json", dir); unlink(path);
    snprintf(path, sizeof(path), "%s/translations/ui", dir); rmdir(path);
    snprintf(path, sizeof(path), "%s/translations", dir); rmdir(path);
    rmdir(dir);
    if (result) fputs("UI translation overlay test failed\n", stderr);
    return result;
}
