/* Build like fuzz.c, replacing fuzz.c with recovery.c. Run from the repository root. */
#include <time.h>
static time_t retryClock = 1;
static time_t recovery_time(time_t *out) { if (out) *out = retryClock; return retryClock; }
#define time recovery_time
#include "../../src/gs/menu_text.c"
#undef time
#include <assert.h>

int main(void) {
    char dir[] = "/tmp/zenkaipower-font-recovery-XXXXXX", path[1200];
    FILE *input, *output;
    assert(mkdtemp(dir));
    snprintf(sFontDir, sizeof(sFontDir), "%s", dir);
    snprintf(path, sizeof(path), "%s/fonts.tsv", dir);
    output = fopen(path, "wb"); assert(output);
    fputs("restored.ttf\tRestored\t7\n", output); fclose(output);
    sFontsLoaded = 1;
    fonts_read();
    assert(sFontCount == 1);
    int id = font_id(1);
    sLabelsThere = sLabelsLoaded = 1;
    sCount = 1; /* Isolate font-file recovery from the disc label fixture. */
    MenuText_SetFontId(id);
    MenuText_SetBattleFontId(id);
    assert(MenuText_GetFont() == 0 && MenuText_GetBattleFont() == 0);
    assert(MenuText_FontIdToSave() == id && MenuText_BattleFontIdToSave() == id);
    /* Applying unrelated settings must retain the pending request. */
    MenuText_SetFont(MenuText_GetFont());
    MenuText_SetBattleFont(MenuText_GetBattleFont());
    assert(MenuText_FontIdToSave() == id && MenuText_BattleFontIdToSave() == id);
    sMakeResult = 0; sLabelsOld = 0;
    labels_take(); /* Failed label generation must retain it too. */
    assert(MenuText_FontIdToSave() == id);
    sLabelsThere = 1;
    snprintf(path, sizeof(path), "%s/restored.ttf", dir);
    input = fopen("port/third_party/fonts/dejavu/DejaVuSansCondensed-Bold.ttf", "rb"); assert(input);
    output = fopen(path, "wb"); assert(output);
    assert(fseek(input, 0, SEEK_END) == 0);
    long size = ftell(input); rewind(input);
    assert(Port_PackCopy(input, output, size)); fclose(input); fclose(output);
    MenuText_Poll();
    assert(MenuText_GetFont() == 1 && MenuText_GetBattleFont() == 1);
    assert(MenuText_FontIdToSave() == id && MenuText_BattleFontIdToSave() == id);
    MenuText_SetFont(0); MenuText_SetBattleFont(0);
    retryClock++;
    MenuText_Poll();
    assert(MenuText_GetFont() == 0 && MenuText_FontIdToSave() == 0);
    assert(MenuText_GetBattleFont() == 0 && MenuText_BattleFontIdToSave() == 0);
    unlink(path);
    snprintf(path, sizeof(path), "%s/fonts.tsv", dir); unlink(path); rmdir(dir);
    free(sFonts);
    puts("Font recovery checks passed");
    return 0;
}
