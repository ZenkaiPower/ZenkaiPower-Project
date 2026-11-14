/* cc -std=gnu99 -O1 -w -DPORT -ffunction-sections -fdata-sections -Iinclude -Iport/include \
 * -include port_compat.h port/tests/ui/dragon_net_proportions.c src/menu/port_screen.c \
 * -Wl,--gc-sections -lm -o /tmp/dragon-net-proportions && /tmp/dragon-net-proportions */
#include "menu/port_screen.h"
#include <assert.h>
#include <math.h>
static int keep = 1, whole, aspect = 1778, marker, flushes;
int Port_TitleMainChars43(void) { return keep; }
int Port_Menus43(void) { return whole; }
int Port_PageAspectMilli(void) { return whole ? 1333 : aspect; }
void Port_GsMarker(int effect) { marker = effect; }
void Font_FlushAll(void) { flushes++; }
static void near(float a, float b) { assert(fabsf(a - b) < 0.001f); }
int main(void) {
    int aspects[] = {1333, 1778, 2389, 3556};
    for (int i = 0; i < 4; i++) {
        aspect = aspects[i];
        Ps_SetShift(0);
        Ps_PageSet();
        float left = Ps_X(0), right = Ps_X(512);
        Ps_Foreground43Begin();
        if (aspect <= 1340) {
            near(Ps_X(0), left); near(Ps_X(512), right);
            Ps_Foreground43End();
            continue;
        }
        assert(marker == 0x19);
        Ps_Foreground43Anchor(1); assert(marker == 0x1A);
        Ps_Foreground43Anchor(2); assert(marker == 0x1B);
        Ps_Foreground43Anchor(0); assert(marker == 0x19);
        near(Ps_X(0), 0); near(Ps_X(512), 512); near(Ps_Y(448), 448);
        Ps_SetShift(20); near(Ps_X(100), 120); Ps_SetShift(0);
        Ps_Foreground43End();
        assert(marker == 0x13);
        near(Ps_X(0), left); near(Ps_X(512), right);
    }
    assert(flushes == 3);
    keep = 0; marker = 0;
    Ps_PageSet(); float left = Ps_X(0);
    Ps_Foreground43Begin(); near(Ps_X(0), left);
    Ps_Foreground43End(); assert(marker == 0 && flushes == 3);
    keep = 1; whole = 1;
    Ps_Foreground43Begin(); Ps_Foreground43End();
    assert(marker == 0 && flushes == 3); /* Whole-page 4:3 must not be narrowed twice. */
    whole = 0; marker = 0;
    Ps_ForegroundStretchBegin();
    near(Ps_X(0), 0); near(Ps_X(512), 512); near(Ps_Y(448), 448);
    Ps_Foreground43Anchor(1); assert(marker == 0);
    Ps_Foreground43End(); assert(marker == 0 && flushes == 3);
    assert(Ps_X(0) > 0); /* Normal page mapping restored after stretched keyboard. */
    return 0;
}
