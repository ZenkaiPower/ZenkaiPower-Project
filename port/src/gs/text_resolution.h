#ifndef GS_TEXT_RESOLUTION_H
#define GS_TEXT_RESOLUTION_H
#include "../../../include/menu/port_text_resolution.h"
#include "../../../include/menu/port_build_flags.h"
/* Follow means the game's Resolution option. HUD info overrides the main flag. */
static inline int PortText_FollowsResolution(int info) {
    return info ? !!ZENKAIPOWER_HUD_INFO_FOLLOW_RES : !!ZENKAIPOWER_TEXT_FOLLOW_RES;
}
static inline int PortText_UsesScreen(void) {
    return !ZENKAIPOWER_TEXT_FOLLOW_RES || !ZENKAIPOWER_HUD_INFO_FOLLOW_RES;
}
#endif
