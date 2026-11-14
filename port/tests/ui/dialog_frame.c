/* cc -std=gnu99 -g -fsanitize=address,undefined -ffunction-sections -fdata-sections \
 *    port/tests/ui/dialog_frame.c port/src/gs/menu_labels.c -Wl,--gc-sections -lm -pthread -o /tmp/dialog_frame_test
 * /tmp/dialog_frame_test [gamedata]
 * Reads the user's resources; no disc artwork is stored in the test. */
#include "../../src/gs/menu_labels.h"
#include <assert.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int width = 99, height = 99;
    unsigned char *pixels = MenuLabels_DialogFrame("/missing/zenkaipower-dialog-resources", &width, &height);
    assert(pixels == NULL && width == 0 && height == 0);
    if (argc > 1) {
        pixels = MenuLabels_DialogFrame(argv[1], &width, &height);
        assert(pixels && width == 256 && height == 32);
        assert(pixels[3] == 0); /* Transparent region outside the ornate corner. */
        assert(pixels[(37 * 4) + 3] > 0); /* The native top border. */
        assert(pixels[((31 * width + 255) * 4) + 3] == 204); /* Native translucent body: GS alpha 102 becomes host alpha 204. */
        free(pixels);
        {
            unsigned char *normal = MenuLabels_DialogButton(argv[1], 0, &width, &height);
            unsigned char *highlight = MenuLabels_DialogButton(argv[1], 1, &width, &height);
            size_t center = ((size_t)20 * 128 + 60) * 4;
            assert(normal && highlight && width == 128 && height == 64);
            assert(normal[center + 1] != highlight[center + 1]);
            assert(normal[((size_t)63 * 128 + 127) * 4 + 3] == 0);
            free(normal);
            free(highlight);
        }
    }
    return 0;
}
