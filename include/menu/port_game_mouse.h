#ifndef MENU_PORT_GAME_MOUSE_H
#define MENU_PORT_GAME_MOUSE_H

/* Shared by the SDL input producer and game-thread context query. */
enum {
    PORT_MOUSE_GAME_OFF,
    PORT_MOUSE_GAME_LOADING,
    PORT_MOUSE_GAME_FIGHT,
    PORT_MOUSE_GAME_SCENE,
    PORT_MOUSE_GAME_DIALOG,
    PORT_MOUSE_GAME_PAUSED
};
#define PORT_MOUSE_LEFT 1u
#define PORT_MOUSE_RIGHT 2u

#ifdef __cplusplus
extern "C" {
#endif
int Port_GameMouseMode(void);
#ifdef __cplusplus
}
#endif

/* Raw PS2 pad bits, before the usual game/battle remapping and net exchange.
   Scenes use a fresh click, never a held Start that could pause the next fight. */
static unsigned Port_GameMouseMap(int mode, unsigned held, unsigned pressed) {
    if (mode == PORT_MOUSE_GAME_LOADING || mode == PORT_MOUSE_GAME_DIALOG)
        return pressed & PORT_MOUSE_LEFT ? 0x4000u : 0;
    if (mode == PORT_MOUSE_GAME_SCENE)
        return pressed & PORT_MOUSE_LEFT ? 0x4008u : 0;
    if (mode == PORT_MOUSE_GAME_FIGHT)
        return (held & PORT_MOUSE_LEFT ? 0x8000u : 0) |
               (held & PORT_MOUSE_RIGHT ? 0x2000u : 0);
    return 0;
}
#endif
