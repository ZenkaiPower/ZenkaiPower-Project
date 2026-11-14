/* cc -std=gnu99 -ffunction-sections -fdata-sections port/tests/ui/settings_key.c \
 * port/src/gs/gs_input.c -Wl,--gc-sections -lSDL3 -lm -pthread -o /tmp/settings_key_test
 * /tmp/settings_key_test && /tmp/settings_key_test legacy */
#include <SDL3/SDL.h>
#include <assert.h>
#include <string.h>
#include "../../src/gs/ui.h"
static int consumed, legacy, settingsEnabled;
int Port_SettingsEnabled(void) { return settingsEnabled; }
const char *Port_GetEnv(const char *name) { (void)name; return NULL; }
int Ui_ConsumesEscape(void) { return consumed; }
int Port_Setting(const char *name, int fallback) {
    return legacy && strcmp(name, "key_overlay") == 0 ? SDL_SCANCODE_ESCAPE : fallback;
}
extern int PortInput_Hotkey(const SDL_Event *event);
int main(int argc, char **argv) {
    SDL_Event event;
    (void)argv;
    legacy = argc > 1;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_F1;
    assert(!PortInput_Hotkey(&event)); /* F1 is off without --settings. */
    settingsEnabled=1;
    assert(PortInput_Hotkey(&event));
    consumed = 1;
    assert(PortInput_Hotkey(&event)); /* Escape consumption does not block F1. */
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    assert(!PortInput_Hotkey(&event));
    event.key.scancode = SDL_SCANCODE_F1;
    consumed = 0;
    event.key.repeat = 1;
    assert(!PortInput_Hotkey(&event));
    event.key.repeat = 0;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    assert(!PortInput_Hotkey(&event));
    return 0;
}
