/* Copy to port/tests/ui/dragon_net_row_aspect.c and run from repository root:
 * cc -std=gnu99 -O1 -w -DPORT -ffunction-sections -fdata-sections -Iinclude -Iport/include -Iport/src \
 * -include port_compat.h -include math.h port/tests/ui/dragon_net_row_aspect.c \
 * -Wl,--gc-sections -lm -o /tmp/dragon-net-row-aspect && /tmp/dragon-net-row-aspect */
#include "../../src/menu/port_tbo.c"
#include <assert.h>
#include <math.h>

static int page;
int Port_AspectMilli(void) { return 2389; }
int Port_PageAspectMilli(void) { return page; }
int Port_TitleMainChars43(void) { return 1; }
int Port_Menus43(void) { return 0; }
s32 Port_WiiHomeMotionQuarterMilli(s32 track, s32 frame) {
    (void)track; (void)frame;
    return 120000;
}
static void near(float a, float b) { assert(fabsf(a - b) < 0.002f); }
int main(void) {
    TboScreen screen = {0};
    sTb = &screen;

    /* New regression: 4:3 must not shift a Battle / Custom / Friend row
       or its selection radar 120px to the right. */
    page = 1333;
    screen.mode = TB_WII_BATTLE;
    for (int i = 0; i < 6; i++) {
        float resting = 170.0f + i * TB_WII_BATTLE_ROW_STEP_X;
        float radar = 196.0f + i * TB_WII_BATTLE_ROW_STEP_X
                      - Tb_RowInset() + Tb_RowOffset();
        near(Tb_RowX(resting, resting), resting);
        near(Tb_RowX(512.0f, resting), 512.0f);
        near(Tb_RowX((resting + 512.0f) * 0.5f, resting), (resting + 512.0f) * 0.5f);
        near(radar - Tb_RowX(resting, resting), 26.0f);
    }
    screen.mode = TB_WII_HOME;
    near(Tb_HomeRowX(512.0f), 512.0f);
    near(Tb_HomeRowX(120.0f), 162.0f);

    /* Existing regression: keep the output ultrawide while the visible
       page changes, and preserve the parked right-edge positions. */
    for (int direction = 0; direction < 2; direction++) {
        for (int i = 0; i <= 20; i++) {
            page = 1341 + (2389 - 1341) * (direction ? 20 - i : i) / 20;
            float scale = 1333.333f / page;
            screen.mode = TB_WII_BATTLE;
            near(Tb_RowX(512.0f, 170.0f) * scale, 512.0f);
            screen.mode = TB_WII_HOME;
            near(256.0f + (Tb_HomeRowX(512.0f) - 256.0f) * scale, 512.0f);
            near(Tb_HomeRowX(120.0f), 162.0f);
        }
    }
    return 0;
}
