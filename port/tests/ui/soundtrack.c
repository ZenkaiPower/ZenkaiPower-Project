/* cc -std=gnu99 -O1 -ffunction-sections -fdata-sections -Iport/include -Iport/src \
 * port/tests/ui/soundtrack.c port/src/plat_songs.c -Wl,--gc-sections -lm -pthread -o /tmp/soundtrack-test
 * /tmp/soundtrack-test gamedata
 * /tmp/soundtrack-test /missing/zenkaipower-data
 * Real-disc integration: compares decoded artwork with the extracted Japanese sheets.
 * No copyrighted assets are embedded in this test. */
#include <assert.h>
#include "../../src/gs/menu_labels.c"
#include "../../include/plat_songs.h"

static int japanese;
int Port_Setting(const char *name, int fallback) {
    return strcmp(name, "game_version") == 0 ? !japanese : fallback;
}
const char *Port_FileRoot(void) {
    const char *data = getenv("ZENKAIPOWER_DATA");
    return data ? data : "gamedata";
}

int main(int argc, char **argv) {
    char path[512];
    int bank, index;
    MlCtx *c;
    MlBuf select;
    assert(argc == 2);
    setenv("ZENKAIPOWER_DATA", argv[1], 1);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/64974.bin") == 0);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/64997.bin") == 23);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/64998.bin") == -1);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/64973.bin") == -1);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/64974.bin.bak") == -1);
    assert(PortSongs_SoundtrackSlot("gamedata/mods/pzs3us2/64974.bin") == -1);
    assert(PortSongs_SoundtrackSlot("gamedata/pzs3us2/73000.bin") == -1);
    assert(!PortSongs_SoundtrackPath("gamedata/pzs3us2/64974.bin", path, sizeof(path)));
    japanese = 1;
    if (!PortSongs_SoundtrackAvailable()) {
        assert(!PortSongs_JapaneseSoundtrack());
        assert(!PortSongs_SoundtrackPath("gamedata/pzs3us2/64974.bin", path, sizeof(path)));
        puts("soundtrack: missing-data fallback passed");
        return 0;
    }
    for (index = 0; index < 24; index++) {
        char original[100], expected[512];
        snprintf(original, sizeof(original), "gamedata/pzs3us2/%05d.bin", 64974 + index);
        snprintf(expected, sizeof(expected), "%s/soundtrack/meteor/%02d.adx", argv[1], index);
        assert(PortSongs_SoundtrackPath(original, path, sizeof(path)));
        assert(!strcmp(path, expected));
    }
    assert(!PortSongs_SoundtrackPath("gamedata/pzs3us2/64974.bin", path, 4));
    c = calloc(1, sizeof(*c));
    assert(c && ml_read_one(c, argv[1], ml_file(448), 1) == MENU_LABELS_MADE);
    assert(ml_screen(c, 448, 2, &select));
    for (bank = 0; bank < 2; bank++) {
        MlBuf us, jp;
        unsigned char data[431424];
        FILE *fp;
        assert(ml_section(c, select, bank ? 3 : 4, &us));
        {
            MlTex locked = {0};
            int first, selected;
            assert(ml_texture(c, us, 3, &locked) > 0);
            assert(MenuLabels_SoundtrackArtInfo(locked.rgba, locked.w, locked.h, &first, &selected));
            assert(first == 24 && selected == bank);
        }
        snprintf(path, sizeof(path), "%s/soundtrack/meteor/names-%s.bin", argv[1], bank ? "selected" : "normal");
        fp = Port_PackFopen(path, "rb");
        assert(fp && fread(data, 1, sizeof(data), fp) == sizeof(data));
        fclose(fp);
        jp.p = data; jp.n = sizeof(data);
        for (index = 0; index < 3; index++) {
            MlTex original = {0}, expected = {0};
            unsigned char *copy = malloc(512 * 256 * 4);
            unsigned before, after;
            assert(ml_texture(c, us, index, &original) > 0);
            assert(ml_texture(c, jp, index, &expected) > 0);
            memcpy(copy, original.rgba, 512 * 256 * 4);
            japanese = 0; before = MenuLabels_ArtKey();
            assert(!MenuLabels_SoundtrackArtSwap(copy, 512, 256));
            assert(!memcmp(copy, original.rgba, 512 * 256 * 4));
            japanese = 1; after = MenuLabels_ArtKey();
            assert(before != after); /* Switching back also invalidates the renderer's cache. */
            assert(MenuLabels_SoundtrackArtSwap(copy, 512, 256));
            assert(!memcmp(copy, expected.rgba, 512 * 256 * 4));
            {
                int first = -1, selected = -1;
                assert(MenuLabels_SoundtrackArtInfo(copy, 512, 256, &first, &selected));
                assert(first == index * 8 && selected == bank);
            }
            free(copy);
        }
    }
    /* Sound test uses another copy of the same six name sheets. */
    assert(ml_read_one(c, argv[1], ml_file(453), 1) == MENU_LABELS_MADE);
    assert(ml_screen(c, 453, 1, &select));
    for (bank = 0; bank < 2; bank++) {
        MlBuf sheet;
        assert(ml_section(c, select, bank ? 4 : 5, &sheet));
        {
            MlTex locked = {0};
            int first, selected;
            assert(ml_texture(c, sheet, 3, &locked) > 0);
            assert(MenuLabels_SoundtrackArtInfo(locked.rgba, locked.w, locked.h, &first, &selected));
            assert(first == 24 && selected == bank);
        }
        for (index = 0; index < 3; index++) {
            MlTex tex = {0};
            assert(ml_texture(c, sheet, index, &tex) > 0);
            assert(MenuLabels_SoundtrackArtSwap(tex.rgba, tex.w, tex.h));
        }
    }
    ml_free(c); free(c);
    puts("soundtrack: 24 mappings, both choices, mod/custom preservation, and selection/sound-test name sheets passed");
    return 0;
}
