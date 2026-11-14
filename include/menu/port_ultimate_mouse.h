#ifndef MENU_PORT_ULTIMATE_MOUSE_H
#define MENU_PORT_ULTIMATE_MOUSE_H
#ifdef PORT
#include "sys/pad.h"
#include "menu/port_selection_mouse.h"

/* Keep native handlers responsible for confirmation and rule validation. */
static void Port_UbMouseBegin(s32 ready) {
    Port_FlashMenuMouseBegin();
    PortMenuMouse_Ready(ready && !(gProgress->flags & MPROG_FREEZE) &&
                        ColorFade_IsInDone() && !ColorFade_IsFadingOut());
}
static s32 Port_UbMouseRow(PortMenuMouse *mouse, s32 scope, const char *format,
                          s32 first, s32 count, f32 width, f32 height, f32 pivot,
                          s32 *cursor, void (*clip)(s32, s32, char *), s32 movie, s32 kind) {
    s32 hit = Port_SelectionMouseHit(mouse, scope, format, first, count, width, height, pivot);
    if (hit >= first && *cursor != hit - first) {
        clip(movie, kind, "fl_off_start");
        *cursor = hit - first;
        clip(movie, kind, "fl_on_start");
        Snd_PlaySe(1, 0);
    }
    return hit;
}
static void Port_UbMouseArrows(PortMenuMouse *mouse, s32 scope, f32 pivot) {
    s32 up = Port_SelectionMouseHit(mouse, scope, "mc_yajirusi_up", 0, 1, 32, 32, pivot);
    s32 down = Port_SelectionMouseHit(mouse, scope, "mc_yajirusi_down", 0, 1, 32, 32, pivot);
    if (mouse->confirm && (up >= 0 || down >= 0)) {
        gPad[0].gameRepeat |= up >= 0 ? PADG_UP : PADG_DOWN;
        mouse->confirm = 0;
    }
}
#endif
#endif
