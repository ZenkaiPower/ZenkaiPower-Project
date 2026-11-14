/* cc -std=gnu99 -O1 -ffunction-sections -fdata-sections -Iport/include -Iport/src \
 * port/tests/ui/game_version.c port/src/plat_file.c port/src/plat_songs.c port/src/plat_settings.c \
 * -Wl,--gc-sections -lSDL3 -lm -pthread -o /tmp/game-version-test
 * /tmp/game-version-test <data> <isolated-settings-file> <expected-mask>
 * Mask: 1 = METEOR logo selected, 2 = Japanese music, 4 = Japanese credits. */
#include <assert.h>
#include <stdlib.h>
extern int Port_TitleLogoEnabled(void), PortSongs_JapaneseSoundtrack(void), Port_UseJapaneseCredits(void);
extern void Port_SettingSave(const char *name, int value);
int PortStages_Alias(const char *rel, char *out, unsigned size) {
    (void)rel; (void)out; (void)size; return 0;
}
static int selection(void) {
    return Port_TitleLogoEnabled() | PortSongs_JapaneseSoundtrack() * 2 | Port_UseJapaneseCredits() * 4;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    setenv("ZENKAIPOWER_DATA", argv[1], 1);
    setenv("ZENKAIPOWER_SETTINGS", argv[2], 1);
    unsetenv("ZENKAIPOWER_FILE_ALIAS");
    assert(selection() == atoi(argv[3]));
    Port_SettingSave("game_version", 1);
    assert(selection() == 0);
    Port_SettingSave("game_version", 0);
    assert(Port_TitleLogoEnabled());
    return 0;
}
