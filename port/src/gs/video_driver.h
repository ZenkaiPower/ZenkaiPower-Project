#ifndef PORT_VIDEO_DRIVER_H
#define PORT_VIDEO_DRIVER_H
#include <SDL3/SDL.h>
/* Apply before either renderer initializes video. User hints/environment win. */
static inline void Port_VideoPreferWayland(void) {
#if defined(__linux__) && !defined(__ANDROID__)
    if (!SDL_GetHint(SDL_HINT_VIDEO_DRIVER)) {
        const char *legacy = SDL_getenv("SDL_VIDEODRIVER");
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,
            legacy ? legacy : "wayland,x11", SDL_HINT_DEFAULT);
    }
#endif
}
#endif
