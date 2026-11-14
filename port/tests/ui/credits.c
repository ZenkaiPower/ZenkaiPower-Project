/* cc -std=gnu99 -O1 -ffunction-sections -fdata-sections -Iport/include -Iport/src \
 * port/tests/ui/credits.c port/src/plat_file.c port/src/plat_songs.c \
 * -Wl,--gc-sections -lm -pthread -o /tmp/credits-test
 * /tmp/credits-test gamedata 1 1
 * /tmp/credits-test gamedata 0 0
 * /tmp/credits-test /missing/zenkaipower-data 1 0 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
extern int Port_UseJapaneseCredits(void), Port_UseJapaneseOpening(void);
static int japanese;
int Port_Setting(const char *name, int fallback) {
    return !strcmp(name, "game_version") ? !japanese : fallback;
}
int PortStages_Alias(const char *rel, char *out, unsigned size) {
    (void)rel; (void)out; (void)size; return 0;
}
int main(int argc, char **argv) {
    assert(argc == 4 || argc == 5);
    setenv("ZENKAIPOWER_DATA", argv[1], 1);
    unsetenv("ZENKAIPOWER_FILE_ALIAS");
    japanese = atoi(argv[2]);
    assert(Port_UseJapaneseCredits() == atoi(argv[3]));
    if (argc == 5) assert(Port_UseJapaneseOpening() == atoi(argv[4]));
    return 0;
}
