#ifndef BATTLE_PAUSE_TEXT_STYLE_H
#define BATTLE_PAUSE_TEXT_STYLE_H
/* Shared by the pause menu and the PC player-name overlay. GS alpha: 128 is opaque. */
enum {
    PAUSE_TEXT_R = 0x89, PAUSE_TEXT_G = 0xC3, PAUSE_TEXT_B = 0xE5, PAUSE_TEXT_A = 0x80,
    PAUSE_TEXT_SHADOW_R = 0, PAUSE_TEXT_SHADOW_G = 0, PAUSE_TEXT_SHADOW_B = 0,
    PAUSE_TEXT_SHADOW_A = 0x40, PAUSE_TEXT_SHADOW_MODE = 2,
    PAUSE_TEXT_SHADOW_DX = 1, PAUSE_TEXT_SHADOW_DY = 2
};
/* Font's eight-copy outline, in draw order. Used by both renderers. */
static inline int Font_OutlineOffsetX(int pass) {
    static const signed char offsets[8] = {1, -1, 1, -1, 1, -1, 0, 0};
    return offsets[pass];
}
static inline int Font_OutlineOffsetY(int pass) {
    static const signed char offsets[8] = {1, 1, -1, -1, 0, 0, 1, -1};
    return offsets[pass];
}
#endif
