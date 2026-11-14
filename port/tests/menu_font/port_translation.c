/* Port catalog rows and the Wii's stable text indexes must stay independent.
 * gcc -g -O1 -DUI_TEXT_HOST_ALLOC -fsanitize=address,undefined -fno-sanitize-recover=all \
 *   port/tests/menu_font/port_translation.c port/src/gs/ui_text_extract.c \
 *   port/src/gs/ui_text_translate.c port/src/plat_settings.c -lm -lpthread -lSDL3 -o /tmp/port_translation_test
 * Run from the repository root: /tmp/port_translation_test
 */
#include "../../src/gs/menu_translation.c"
#include "../../../include/menu/port_video_options.h"
#include <assert.h>

int MenuText_FirstFontId(int setting) { (void)setting; return 0; }
int Port_ZenkaiEnabled(void) { return 0; }

int main(void) {
    TrPortCatalog catalog;
    char out[1024];
    int i, j, optionRows = 0;
    assert(tr_port_read("port", "en", &catalog));
    for (i = 0; i < TR_PORT_TEXT_COUNT; i++) {
        assert(kPortText[i].id && kPortText[i].original);
        if (!strncmp(kPortText[i].id, "options/", 8)) {
            assert(catalog.present[i] && !strcmp(catalog.text[i], kPortText[i].original));
            optionRows++;
        }
        if (!strcmp(kPortText[i].id, "music/meteor/01")) {
            assert(!strcmp(kPortText[i].original, "ニューヒーロー登場"));
            assert(catalog.present[i] && !strcmp(catalog.text[i], "A New Hero Appears"));
        }
        if (!strcmp(kPortText[i].id, "options/category/visual"))
            assert(catalog.present[i] && !strcmp(catalog.text[i], "Visual Settings"));
        if (!strcmp(kPortText[i].id, "options/value/off"))
            assert(catalog.present[i] && !strcmp(catalog.text[i], "Off"));
    }
    assert(optionRows == 86);
    tr_port_load("port", "en");
    strcpy(sLocale, "en");
    __atomic_store_n(&sReady, 1, __ATOMIC_RELEASE);
    for (i = 0; i < 24; i++) assert(MenuTranslation_MeteorSong(i) && MenuTranslation_MeteorSong(i)[0]);
    assert(MenuTranslation_MeteorSong(-1) == NULL && MenuTranslation_MeteorSong(24) == NULL);
    assert(!strcmp(MenuTranslation_MeteorSong(1), "A New Hero Appears"));
    for (i = 0; i < TR_PORT_TEXT_COUNT; i++) {
        if (!strcmp(kPortText[i].id, "music/meteor/01")) {
            strcpy(sPortText[i], "A custom translated song title");
            assert(!strcmp(MenuTranslation_MeteorSong(1), "A custom translated song title"));
            sPortText[i][0] = 0; /* Existing local catalogs without the new rows use English. */
            assert(!strcmp(MenuTranslation_MeteorSong(1), "A New Hero Appears"));
        }
    }
    assert(MenuTranslation_PortText("unknown/key") == NULL);
    assert(MenuTranslation_PortText("net/connecting") == NULL);
    assert(MenuTranslation_PortText("account/invalid-name") != NULL);
    assert(!strcmp(MenuTranslation_PortText("options/category/visual"), "Visual Settings"));
    assert(!strcmp(MenuTranslation_PortText("options/setting/autosave"), "Autosave"));
    for (i = 0; i < PORT_OPTION_CATEGORY_COUNT; i++)
        assert(MenuTranslation_PortText(sPortOptionCategoryIds[i]) != NULL);
    for (i = 0; i < PORT_OPTION_SETTING_COUNT; i++) {
        const PortVideoOption *option = Port_OptionsDescriptor(i);
        assert(MenuTranslation_PortText(option->labelId) != NULL);
        assert(MenuTranslation_PortText(option->helpId) != NULL);
        for (j = 0; j < option->count; j++)
            assert(MenuTranslation_PortText(option->nameIds[j]) != NULL);
    }
    assert(MenuTranslation_PortTextCopy("options/setting/autosave", "Autosave", out, sizeof(out)));
    assert(!strcmp(out, "Autosave"));
    assert(MenuTranslation_PortTextCopy("options/unknown", "Fallback", out, sizeof(out)));
    assert(!strcmp(out, "Fallback"));
    PortTbo_WiiTextCopy(0, out, sizeof(out));
    assert(!strcmp(out, "Dragon Net Battle"));
    PortTbo_WiiTextCopy(33, out, sizeof(out));
    assert(!strcmp(out, "Dragon Net Battle"));
    for (i = 0; i < TR_PORT_WII_TEXT_COUNT; i++) {
        PortTbo_WiiTextCopy(i, out, sizeof(out));
        assert(out[0] && !strncmp(kPortWiiTextIds[i], "dragon-net/", 11));
    }
    PortTbo_ResultTextCopy(0, out, sizeof(out));
    assert(!strcmp(out, "Rematch Request"));
    PortTbo_ResultTextCopy(1, out, sizeof(out));
    assert(!strcmp(out, "End Battle"));
    /* A translation edit must reach the intended button, not an adjacent row. */
    for (i = 0; i < TR_PORT_TEXT_COUNT; i++) {
        if (!strcmp(kPortText[i].id, "dragon-net/results/rematch"))
            strcpy(sPortText[i], "Try again");
        if (!strcmp(kPortText[i].id, "dragon-net/home/title"))
            strcpy(sPortText[i], "Online battles");
    }
    PortTbo_ResultTextCopy(0, out, sizeof(out));
    assert(!strcmp(out, "Try again"));
    PortTbo_WiiTextCopy(0, out, sizeof(out));
    assert(!strcmp(out, "Online battles"));
    return 0;
}
