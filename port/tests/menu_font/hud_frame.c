/* Disc-dependent integration test: hud-frame-test /path/to/extracted/gamedata. */
#include "../../src/gs/menu_labels.h"
#include "../../src/gs/gs_draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
const char *Port_FileRoot(void) { return "gamedata"; }
int PortSongs_JapaneseSoundtrack(void) { return 0; }
int Port_NetSession(void) { return 0; }
GsRenderScale gsScale = 2;
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /path/to/extracted/gamedata\n", argv[0]);
        return 2;
    }
    int fw, fh;
    unsigned char *frame = MenuLabels_HudNameFrame(argv[1], &fw, &fh);
    assert(frame && fw == 128 && fh == 32);
    /* The inset gauge rails are gone; the original angled edge remains untouched. */
    for (int channel = 0; channel < 4; channel++)
        assert(frame[(8 * fw + 50) * 4 + channel] == frame[(8 * fw + 24) * 4 + channel]);
    assert(frame[(8 * fw + 20) * 4 + 3] > 0);
    assert(frame[(19 * fw + 93) * 4 + 3] == 0);
    free(frame);
    puts("PASS: original frame contour without inset gauge rails");
}
