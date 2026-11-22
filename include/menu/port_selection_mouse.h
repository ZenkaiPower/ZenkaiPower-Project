#ifndef MENU_PORT_SELECTION_MOUSE_H
#define MENU_PORT_SELECTION_MOUSE_H
#include "menu/port_menu_mouse.h"
/* Menus supply their current state and keep all native validation/confirmation. */
static s32 Port_SelectionMouseHit(PortMenuMouse *mouse, s32 scope, const char *format,
                                s32 first, s32 count, f32 width, f32 height, f32 pivot) {
    char name[64];
    s32 n;
    if (!(mouse->moved || mouse->confirm) || mouse->wheel) return -1;
    for (n = first; n < first + count; ++n) {
        sprintf(name, format, n);
        if (Port_FlashMenuMouseHit(scope, name, width, height, pivot, mouse->x, mouse->y)) return n;
    }
    return -1;
}
static s32 Port_SelectionMusicHit(PortMenuMouse *mouse, s32 grid) {
    if (Port_SelectionMouseHit(mouse,10,"mc_bgm_now",0,1,432,32,256) >= 0) return 1;
    return grid && Port_SelectionMouseHit(mouse,10,"mc_stage_music",0,1,64,64,256) >= 0;
}

static void Port_SelectionMouseButtons(PortMenuMouse *mouse, s32 pad, s32 hit) {
    if (mouse->confirm && hit) gPad[pad].gamePressed |= PADG_CROSS;
    if (mouse->cancel) gPad[pad].gamePressed |= PADG_TRIANGLE;
    if (mouse->wheel) gPad[pad].gameRepeat |= mouse->wheel < 0 ? PADG_UP : PADG_DOWN;
}
/* Only character/stage/music selectors opt in. Navigation controls consume
   confirm before reaching here so a tap never both navigates and accepts. */
static void Port_SelectionScreenButtons(PortMenuMouse *mouse, s32 pad, s32 hit) {
#ifdef __ANDROID__
    if (!mouse->wheel) hit = 1;
#endif
    Port_SelectionMouseButtons(mouse, pad, hit);
}
#endif
