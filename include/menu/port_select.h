/* Online selection uses simulation frames, so both peers share the deadline. */
#ifndef MENU_PORT_SELECT_H
#define MENU_PORT_SELECT_H
#include "menu/tbo_art.h"
#include "menu/port_screen.h"
#define PORT_SELECT_FRAMES (60 * 60)
static s32 Port_SelectCostume(s32 chara) {
    s32 count = ChrTbl_WrapCostume(chara, NULL);
    return count > 1 ? Rand_Range(count) : 0;
}
static void Port_SelectTimerTick(s32 *frames, s32 *slide, s32 stage) {
    if (*frames > 0) --*frames;
    if (stage && *slide < 20) ++*slide;
    else if (!stage && *slide > 0) --*slide;
}
/* Wii DragonNe section 2, resource 47: 4-column 32px digits and 64px frame.
   Keep the original cyan fill, black outline and decorative frame. */
static void Port_SelectTimerDraw(s32 frames, s32 slide) {
    s32 seconds = (frames + 59) / 60;
    /* Visible alpha bounds in each 32px cell; the atlas has uneven padding. */
    static const s32 widths[10] = {26, 20, 25, 25, 24, 25, 26, 22, 25, 26};
    s32 i;
    f32 x = 256.0f + 192.0f * (f32)slide / 20.0f;
    f32 y = 200.0f + 84.0f * (f32)slide / 20.0f;
    Ps_SpriteScreen(TBO_WII_SELECT_TIMER_FRAME, x - 28.0f, y - 32.0f, x + 36.0f, y + 32.0f,
                    0, 0, 1, 1, PS_PLAIN, 0x80);
    /* Fixed 26px slots meet at the frame center; narrow digits never shift the pair. */
    f32 left = x - 26.0f;
    for (i = 0; i < 2; i++) {
        s32 digit = i ? seconds % 10 : seconds / 10;
        s32 inset = digit == 1 ? 1 : 0;
        f32 u = (f32)((digit % 4) * 32 + inset) / 128.0f;
        f32 v = (f32)(digit / 4) / 4.0f;
        f32 width = (f32)widths[digit];
        Ps_SpriteScreen(TBO_WII_SELECT_TIMER_DIGITS, left, y - 13.5f, left + 26.0f, y + 13.5f,
                        u, v, u + width / 128.0f, v + 27.0f / 128.0f, PS_PLAIN, 0x80);
        left += 26.0f;
    }
}
#endif
