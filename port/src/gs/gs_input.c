/*
 * Controller input for the windowed PC build: fills the game's pad buffer (the libpad2 layout the game reads in
 * src/sys/pad.c) from the keyboard and from SDL gamepads, through bindings the settings window (ui.cpp) edits and
 * the settings file keeps.
 *
 * Each player has: one key per action (the 16 buttons and the 8 stick directions), one controller button or
 * trigger per pad button, and a controller slot (the n-th connected controller, or none). Defaults:
 *   player 1   first controller; arrows = d-pad, W A S D = left stick, Enter = START,
 *              Space = cross, L = circle, J = square, I = triangle, Q = L1, O = R1,
 *              Shift/E = L2, 9 = R2, Ctrl = L3, Alt = R3
 *   player 2   second controller, no keys
 *   controller the usual layout (south = cross, east = circle, west = square, north = triangle, triggers = L2 / R2)
 * The sticks of a controller are passed through, or given a round deadzone first (stick_bytes, below). The pressure
 * bytes of the buffer are left at zero. The game's motor commands go to the player's controller (Port_PadVibrate).
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"
#include "ui.h"
#include "../../../include/menu/port_pc_bindings.h"
#include "../../../include/sys/port_zenkai.h"
#include "../../../include/menu/port_menu_mouse.h"
#include "../../../include/menu/port_net_selection_mouse.h"
#include "../../../include/menu/port_game_mouse.h"
#include "../../../include/menu/port_native_keyboard.h"

int gPortOverlayOpen, gPortInputCapture, gPortChatTyping;
extern int gPortBootSkipMouse; /* src/sys/pad.c: boot logos and opening video */
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern void Port_SettingSave(const char *name, int value), Port_SettingsWrite(void);

/* The window thread owns SDL text input; the game thread consumes an ordered
 * mailbox. Characters never also reach the configurable controller bindings. */
static pthread_mutex_t sNativeKeyLock = PTHREAD_MUTEX_INITIALIZER;
static int sNativeKeyActive, sNativeKeys[128], sNativeKeyCount;
static SDL_Window *sNativeTextOwner;
static int sNativeLowercase = 1;

static int native_keyboard_lowercase(SDL_Keymod modifiers) {
    return !((!!(modifiers & SDL_KMOD_SHIFT)) ^ (!!(modifiers & SDL_KMOD_CAPS)));
}

int Port_NativeKeyboardPage(void) {
    int page;
    pthread_mutex_lock(&sNativeKeyLock);
    page = sNativeKeyActive && Port_Setting("native_keyboard", 1) &&
        !gPortOverlayOpen && !gPortChatTyping && !gPortInputCapture ? sNativeLowercase : -1;
    pthread_mutex_unlock(&sNativeKeyLock);
    return page;
}

void Port_NativeKeyboardSet(int active) {
    pthread_mutex_lock(&sNativeKeyLock);
    active = active && Port_Setting("native_keyboard", 1);
    if (active != sNativeKeyActive) {
        sNativeKeyCount = 0;
        PortInput_SwallowKeys();
    }
    sNativeKeyActive = active;
    pthread_mutex_unlock(&sNativeKeyLock);
}

int PortInput_NativeKeyboardActive(void) {
    int active;
    pthread_mutex_lock(&sNativeKeyLock);
    active = sNativeKeyActive && Port_Setting("native_keyboard", 1);
    pthread_mutex_unlock(&sNativeKeyLock);
    return active;
}

int Port_NativeKeyboardPop(void) {
    int key = 0;
    pthread_mutex_lock(&sNativeKeyLock);
    if (!Port_Setting("native_keyboard", 1) || gPortOverlayOpen || gPortChatTyping || gPortInputCapture)
        sNativeKeyCount = 0;
    if (sNativeKeyActive && sNativeKeyCount) {
        key = sNativeKeys[0];
        memmove(sNativeKeys, sNativeKeys + 1, --sNativeKeyCount * sizeof(*sNativeKeys));
    }
    pthread_mutex_unlock(&sNativeKeyLock);
    return key;
}

/* Both graphics backends call this before polling events, on the window thread. */
void PortInput_NativeKeyboardUpdate(void) {
    SDL_Window *window = GsDraw_Window();
    int active = PortInput_NativeKeyboardActive() && !gPortOverlayOpen && !gPortChatTyping && !gPortInputCapture;
    pthread_mutex_lock(&sNativeKeyLock);
    sNativeLowercase = native_keyboard_lowercase(SDL_GetModState());
    pthread_mutex_unlock(&sNativeKeyLock);
    if (active && window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
        if (!SDL_TextInputActive(window)) SDL_StartTextInput(window);
        sNativeTextOwner = window;
    } else if (sNativeTextOwner) {
        if (!gPortOverlayOpen && !gPortChatTyping) SDL_StopTextInput(sNativeTextOwner);
        sNativeTextOwner = NULL;
    }
}

/* Shared by SDL text input and clipboard paste; controls (including newlines)
 * never become confirm/delete actions. Each menu validates its own alphabet. */
static void native_keyboard_text(const char *value) {
    const unsigned char *text = (const unsigned char *)value;
    if (text == NULL) return;
    for (; *text && sNativeKeyCount < 128; text++)
        if (*text >= 32 && *text < 127) sNativeKeys[sNativeKeyCount++] = *text;
}

static void native_keyboard_event(const SDL_Event *ev, int consumed) {
    int key = 0;
    pthread_mutex_lock(&sNativeKeyLock);
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP)
        sNativeLowercase = native_keyboard_lowercase(ev->key.mod);
    if (consumed || !sNativeKeyActive || !Port_Setting("native_keyboard", 1) ||
        gPortOverlayOpen || gPortChatTyping || gPortInputCapture || ev->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        sNativeKeyCount = 0;
    } else if (ev->type == SDL_EVENT_KEY_DOWN) {
        if (ev->key.scancode == SDL_SCANCODE_V && (ev->key.mod & SDL_KMOD_CTRL) &&
            !(ev->key.mod & SDL_KMOD_ALT)) {
            if (!ev->key.repeat) {
                char *clipboard = SDL_GetClipboardText();
                native_keyboard_text(clipboard);
                SDL_free(clipboard);
            }
        } else if (ev->key.scancode == SDL_SCANCODE_RETURN || ev->key.scancode == SDL_SCANCODE_KP_ENTER)
            key = ev->key.repeat ? 0 : PORT_KEYBOARD_CONFIRM;
        else if (ev->key.scancode == SDL_SCANCODE_BACKSPACE) key = PORT_KEYBOARD_DELETE;
        if (key && sNativeKeyCount < 128) sNativeKeys[sNativeKeyCount++] = key;
    } else if (ev->type == SDL_EVENT_TEXT_INPUT) {
        native_keyboard_text(ev->text.text);
    }
    pthread_mutex_unlock(&sNativeKeyLock);
}

/* Mouse events belong to the window thread; menu input belongs to the game
   thread. Keep the small mailbox under one lock, including focus/overlay resets. */
static pthread_mutex_t sMenuMouseLock = PTHREAD_MUTEX_INITIALIZER;
static PortMenuMouse sMenuMouse;
static int sMenuMouseEnabled, sMenuMouseReady, sMenuMouseUsed, sAnyButtonPressed;
static float sMenuMouseClickX, sMenuMouseClickY;
static int sMenuMouseHoverBlocked, sMenuMouseHovered;
static int sMenuMouseHoverPending, sMenuMouseHoverCheck;
static float sMenuMouseScrollX, sMenuMouseScrollY;
static float sMenuMouseWheelRemainder;
/* Separate from pointer navigation: battle/loading input works with no menu open. */
static unsigned sGameMouseHeld, sGameMousePressed, sGameMouseBlocked;
static pthread_mutex_t sPcLock=PTHREAD_MUTEX_INITIALIZER;
static unsigned sPcMouseHeld,sPcMouseBlocked,sPcMousePressed;
static int sGameMouseMode;

/* Window-thread snapshot: keyboard input already uses SDL's pumped state.
   Sample mouse buttons there too, even when no menu/overlay event is handled. */
static void game_mouse_sample(unsigned held, int allowed) {
    unsigned pressed = held & ~sGameMouseHeld;
    if (!allowed) {
        sGameMouseBlocked |= held;
        sGameMouseHeld = held;
        sGameMousePressed = 0;
        return;
    }
    sGameMousePressed |= pressed;
    sGameMouseHeld = held;
    sGameMouseBlocked &= held;
}

static void game_mouse_poll(void) {
    SDL_Window *window = GsDraw_Window();
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    int focused = window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
    pthread_mutex_lock(&sPcLock);
    sPcMousePressed|=(focused ? buttons:0)&~sPcMouseHeld;
    sPcMouseHeld=focused ? buttons:0;
    if(!focused || !Port_Setting("mouse_support",1) || gPortOverlayOpen || gPortInputCapture || gPortChatTyping) { sPcMouseBlocked|=sPcMouseHeld;sPcMousePressed=0; }
    sPcMouseBlocked&=sPcMouseHeld;
    pthread_mutex_unlock(&sPcLock);
    unsigned held = focused ? ((buttons & SDL_BUTTON_LMASK ? PORT_MOUSE_LEFT : 0) |
                               (buttons & SDL_BUTTON_RMASK ? PORT_MOUSE_RIGHT : 0)) : 0;
    pthread_mutex_lock(&sMenuMouseLock);
    game_mouse_sample(held, focused && Port_Setting("mouse_support", 1) &&
        !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping);
    pthread_mutex_unlock(&sMenuMouseLock);
}

static unsigned game_mouse_buttons(int mode) {
    unsigned buttons;
    pthread_mutex_lock(&sMenuMouseLock);
    if (mode != sGameMouseMode) {
        sGameMouseMode = mode;
        /* A fresh click belongs to the new context. Only buttons already
           held before it must wait for release. */
        sGameMouseBlocked |= sGameMouseHeld & ~sGameMousePressed;
    }
    if (!Port_Setting("mouse_support", 1) || gPortOverlayOpen || gPortInputCapture || gPortChatTyping) {
        sGameMouseBlocked |= sGameMouseHeld;
        sGameMousePressed = 0;
    }
    buttons = Port_GameMouseMap(mode,
        (sGameMouseHeld | sGameMousePressed) & ~sGameMouseBlocked,
        sGameMousePressed & ~sGameMouseBlocked);
    sGameMousePressed = 0;
    pthread_mutex_unlock(&sMenuMouseLock);
    return buttons;
}

void PortMenuMouse_Enable(int enabled) {
    pthread_mutex_lock(&sMenuMouseLock);
    sMenuMouseEnabled = enabled;
    sAnyButtonPressed = 0;
    sMenuMouseReady = sMenuMouseUsed = sMenuMouseHoverBlocked = sMenuMouseHovered = 0;
    sMenuMouseWheelRemainder = 0;
    sMenuMouseHoverPending = sMenuMouseHoverCheck = 0;
    memset(&sMenuMouse, 0, sizeof(sMenuMouse));
    pthread_mutex_unlock(&sMenuMouseLock);
}

int PortMenuMouse_Enabled(void) {
    int enabled;
    pthread_mutex_lock(&sMenuMouseLock);
    enabled = sMenuMouseEnabled;
    pthread_mutex_unlock(&sMenuMouseLock);
    return enabled;
}

int PortMenuMouse_LeftHeld(void) {
    int held;
    pthread_mutex_lock(&sMenuMouseLock);
    held=sMenuMouseEnabled && sMenuMouseReady && Port_Setting("mouse_support",1) &&
        !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping &&
        (sGameMouseHeld & PORT_MOUSE_LEFT) && !(sGameMouseBlocked & PORT_MOUSE_LEFT);
    pthread_mutex_unlock(&sMenuMouseLock);
    return held;
}

void PortMenuMouse_Ready(int ready) {
    pthread_mutex_lock(&sMenuMouseLock);
    sMenuMouseReady = ready;
    /* Publish the completed game-thread hit check as one update. Window
       motion never exposes the intermediate no-hit state to the cursor. */
    if (sMenuMouseHoverCheck) {
        sMenuMouseHovered = sMenuMouseHoverPending;
        sMenuMouseHoverCheck = 0;
    }
    if (!ready) {
        sMenuMouseHoverPending = sMenuMouseHoverCheck = 0;
        sMenuMouseHovered = 0;
        memset(&sMenuMouse, 0, sizeof(sMenuMouse));
        sMenuMouseWheelRemainder = 0;
    }
    pthread_mutex_unlock(&sMenuMouseLock);
}

int PortMenuMouse_Visible(void) {
    int visible;
    pthread_mutex_lock(&sMenuMouseLock);
    visible = Port_Setting("mouse_support", 1) && sMenuMouseEnabled && sMenuMouseUsed;
    pthread_mutex_unlock(&sMenuMouseLock);
    return visible;
}

void PortMenuMouse_SetHover(int hovered) {
    pthread_mutex_lock(&sMenuMouseLock);
    if (sMenuMouseHoverCheck) sMenuMouseHoverPending = hovered != 0;
    else sMenuMouseHovered = hovered != 0;
    pthread_mutex_unlock(&sMenuMouseLock);
}

int PortMenuMouse_Hovered(void) {
    int hovered;
    pthread_mutex_lock(&sMenuMouseLock);
    hovered = sMenuMouseHovered && sMenuMouseEnabled && sMenuMouseReady &&
        Port_Setting("mouse_support", 1) && !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping;
    pthread_mutex_unlock(&sMenuMouseLock);
    return hovered;
}

void PortMenuMouse_Read(PortMenuMouse *out) {
    pthread_mutex_lock(&sMenuMouseLock);
    *out = sMenuMouse;
    if (out->confirm) {
        out->x = sMenuMouseClickX;
        out->y = sMenuMouseClickY;
    }
    if (!Port_Setting("mouse_support", 1) || !sMenuMouseEnabled || !sMenuMouseReady || gPortOverlayOpen || gPortInputCapture || gPortChatTyping)
        memset(out, 0, sizeof(*out));
    if (out->moved || out->confirm || out->wheel) {
        sMenuMouseHoverPending = 0;
        sMenuMouseHoverCheck = 1;
    }
    sMenuMouse.moved = sMenuMouse.confirm = sMenuMouse.cancel = sMenuMouse.wheel = 0;
    pthread_mutex_unlock(&sMenuMouseLock);
}

int PortInput_AnyButtonPressed(void) {
    int pressed;
    pthread_mutex_lock(&sMenuMouseLock);
    pressed = sAnyButtonPressed;
    sAnyButtonPressed = 0;
    pthread_mutex_unlock(&sMenuMouseLock);
    return pressed;
}

/* This state belongs to the physical mouse, outside saved/rolled-back game
   memory. Wait for the native cursor to acknowledge each synchronized step. */
static PortMouseNetSelector sNetMouseSelector,sSavedMouseSelector;
static int sSavedMouseEnabled;
static struct {
    int phase,target,active,waiting,position,release,waitTicks;
    unsigned confirm;
} sNetMouse;
void PortMenuMouse_NetSelector(PortMouseNetSelector selector) {
    sNetMouseSelector=selector;
    memset(&sNetMouse,0,sizeof(sNetMouse));
}
/* A restored game stack can resume inside a selector's loop, bypassing its
   entry code. Its host input owner is not part of that restored snapshot.
   Repair ownership without restarting navigation on every ordinary frame. */
void PortMenuMouse_NetEnsureSelector(PortMouseNetSelector selector) {
    int enabled=PortMenuMouse_Enabled();
    if(enabled && sNetMouseSelector==selector) return;
    if(!enabled) PortMenuMouse_Enable(1);
    PortMenuMouse_NetSelector(selector);
}
void PortMenuMouse_SessionSave(void) {
    sSavedMouseEnabled=PortMenuMouse_Enabled();
    sSavedMouseSelector=sNetMouseSelector;
    PortMenuMouse_NetSelector(NULL);
    PortMenuMouse_Enable(0);
}
void PortMenuMouse_SessionRestore(void) {
    PortMenuMouse_NetSelector(sSavedMouseSelector);
    PortMenuMouse_Enable(sSavedMouseEnabled);
    pthread_mutex_lock(&sMenuMouseLock);
    sGameMouseMode=PORT_MOUSE_GAME_OFF;
    sGameMouseBlocked|=sGameMouseHeld;
    sGameMousePressed=0;
    pthread_mutex_unlock(&sMenuMouseLock);
}
void PortMenuMouse_NetSample(unsigned char *pad) {
    PortMenuMouse mouse;
    PortMouseNetNav nav;
    unsigned button=0;
    if(!sNetMouseSelector) return;
    if(!PortMenuMouse_Enabled() || !Port_Setting("mouse_support",1) ||
       gPortOverlayOpen || gPortInputCapture || gPortChatTyping) {
        memset(&sNetMouse,0,sizeof(sNetMouse));return;
    }
    PortMenuMouse_Read(&mouse);
    /* Keyboard/controller input takes ownership over a pending pointer target. */
    if(pad[0]!=255 || pad[1]!=255 || pad[2]<100 || pad[2]>155 ||
       pad[3]<100 || pad[3]>155 || pad[4]<100 || pad[4]>155 || pad[5]<100 || pad[5]>155) {
        memset(&sNetMouse,0,sizeof(sNetMouse));return;
    }
    memset(&nav,0,sizeof(nav));nav.target=-1;
    sNetMouseSelector(&mouse,&nav);
    if(!nav.ready) {memset(&sNetMouse,0,sizeof(sNetMouse));return;}
    if(nav.phase!=sNetMouse.phase) {memset(&sNetMouse,0,sizeof(sNetMouse));sNetMouse.phase=nav.phase;}
    if(mouse.cancel) {sNetMouse.active=0;button=0x1000;}
    else {
        if(mouse.moved || mouse.confirm || mouse.wheel) {
            if(nav.target>=0 && !mouse.wheel) {
                sNetMouse.active=1;sNetMouse.target=nav.target;
                if(mouse.confirm) sNetMouse.confirm=nav.action ? nav.action:0x4000;
                else sNetMouse.confirm=0;
            } else {
                sNetMouse.active=0;sNetMouse.confirm=0;
                if(mouse.confirm || mouse.wheel) button=nav.action;
            }
        }
        if(sNetMouse.release) {sNetMouse.release=0;return;}
        if(sNetMouse.waiting) {
            if(nav.current==sNetMouse.position && ++sNetMouse.waitTicks<60) return;
            sNetMouse.waiting=0;
        }
        if(!button && sNetMouse.active) {
            if(nav.current!=sNetMouse.target) button=nav.current>sNetMouse.target ? nav.negative:nav.positive;
            else {button=sNetMouse.confirm;sNetMouse.active=0;sNetMouse.confirm=0;}
        }
    }
    if(button) {
        pad[0]&=(unsigned char)~button;pad[1]&=(unsigned char)~(button>>8);
        sNetMouse.release=1;
        sNetMouse.waiting=button!=0x4000 && button!=0x1000;
        sNetMouse.position=nav.current;sNetMouse.waitTicks=0;
    }
}

/* Window points -> game picture; shared by menus and the native exit prompt. */
int PortInput_MousePoint(float x, float y, float *gameX, float *gameY) {
    SDL_Window *window = GsDraw_Window();
    int ww, wh, pw, ph;
    if (window == NULL || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) return 0;
    if (!SDL_GetWindowSize(window, &ww, &wh) ||
        ww <= 0 || wh <= 0 || gUiPresentW <= 0 || gUiPresentH <= 0) return 0;
    /* The picture rectangle uses backend output pixels. Android can keep a
       smaller swapchain after going fullscreen, stretching it to the window;
       SDL's window pixel size then differs from the image actually presented. */
    if (!GsDraw_OutputSize(&pw, &ph) && !SDL_GetWindowSizeInPixels(window, &pw, &ph)) return 0;
    x = (x * (float)pw / ww - gUiPresentX) * 512.0f / gUiPresentW;
    y = (y * (float)ph / wh - gUiPresentY) * 448.0f / gUiPresentH;
    if (x < 0 || x >= 512 || y < 0 || y >= 448) return 0;
    *gameX = x; *gameY = y;
    return 1;
}

static int pc_wheel_binding(const SDL_Event *ev) {
    float sign=ev->wheel.direction==SDL_MOUSEWHEEL_FLIPPED ? -1.0f:1.0f;
    float x=ev->wheel.x*sign,y=ev->wheel.y*sign;
    if(y>0) return 6;if(y<0) return 7;
    if(x<0) return 8;if(x>0) return 9;
    return 0;
}
/* Called after the overlay has had first refusal, by both graphics backends.
   Never poll SDL mouse state on the game thread (the window can live elsewhere). */
void PortInput_MenuMouseEvent(const SDL_Event *ev, int consumed) {
    native_keyboard_event(ev, consumed);
    if(ev->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
        pthread_mutex_lock(&sPcLock);sPcMouseHeld=sPcMousePressed=sPcMouseBlocked=0;pthread_mutex_unlock(&sPcLock);
    }
    if(ev->type==SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        pthread_mutex_lock(&sPcLock);
        unsigned bit=SDL_BUTTON_MASK(ev->button.button);
        if(ev->type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
            sPcMouseHeld|=bit;
            if(!consumed && !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping && Port_Setting("mouse_support",1))
                sPcMousePressed|=bit;
            else sPcMouseBlocked|=bit;
        } else { sPcMouseHeld&=~bit;sPcMouseBlocked&=~bit; }
        pthread_mutex_unlock(&sPcLock);
    }
    if(ev->type==SDL_EVENT_MOUSE_WHEEL && !consumed && !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping && Port_Setting("mouse_support",1)) {
        SDL_Window *wheelWindow=GsDraw_Window();int binding=pc_wheel_binding(ev);
        if(binding && wheelWindow && (SDL_GetWindowFlags(wheelWindow)&SDL_WINDOW_INPUT_FOCUS)) {
            pthread_mutex_lock(&sPcLock);sPcMousePressed|=SDL_BUTTON_MASK(binding);pthread_mutex_unlock(&sPcLock);
        }
    }
    SDL_Window *window = GsDraw_Window();
    float x, y;
    pthread_mutex_lock(&sMenuMouseLock);
    if (!consumed && !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping &&
        ((ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) ||
         (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && Port_Setting("mouse_support", 1)))) {
        sAnyButtonPressed = 1;
    }
    /* UI consumption only affects menu navigation. It must not clear held
       battle buttons for an unrelated consumed event. */
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        unsigned bit = ev->button.button == SDL_BUTTON_LEFT ? PORT_MOUSE_LEFT :
                       ev->button.button == SDL_BUTTON_RIGHT ? PORT_MOUSE_RIGHT : 0;
        unsigned held = ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? sGameMouseHeld | bit : sGameMouseHeld & ~bit;
        int allowed = window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
            Port_Setting("mouse_support", 1) && !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping;
        game_mouse_sample(held, allowed);
    }
    if (!Port_Setting("mouse_support", 1) || consumed || ev->type == SDL_EVENT_WINDOW_FOCUS_LOST ||
        ev->type == SDL_EVENT_WINDOW_MOUSE_LEAVE || gPortOverlayOpen || gPortInputCapture || gPortChatTyping) {
        memset(&sMenuMouse, 0, sizeof(sMenuMouse));
        sMenuMouseWheelRemainder = 0;
        sMenuMouseHovered = 0;
        sMenuMouseHoverPending = sMenuMouseHoverCheck = 0;
        if (ev->type == SDL_EVENT_WINDOW_FOCUS_LOST || ev->type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
            sGameMouseHeld = sGameMousePressed = sGameMouseBlocked = 0;
        }
        if (ev->type == SDL_EVENT_WINDOW_FOCUS_LOST) sMenuMouseUsed = 0;
        goto done;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
        (ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && abs(ev->gaxis.value) > 16000)) {
        sMenuMouseUsed = 0;
        sMenuMouseHovered = 0;
        sMenuMouseHoverPending = sMenuMouseHoverCheck = 0;
        sMenuMouseWheelRemainder = 0;
        memset(&sMenuMouse, 0, sizeof(sMenuMouse));
        goto done;
    }
    if (!sMenuMouseEnabled || window == NULL) goto done;
    if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        if (ev->motion.xrel == 0 && ev->motion.yrel == 0) goto done;
        x = ev->motion.x; y = ev->motion.y;
    } else if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        x = ev->button.x; y = ev->button.y;
    } else if (ev->type == SDL_EVENT_MOUSE_WHEEL) {
        x = ev->wheel.mouse_x; y = ev->wheel.mouse_y;
    } else goto done;
    sMenuMouseUsed = 1;
    if (!sMenuMouseReady || !PortInput_MousePoint(x, y, &x, &y)) goto done;
    sMenuMouse.x = x; sMenuMouse.y = y;
    if (ev->type == SDL_EVENT_MOUSE_MOTION) {
        float dx = x - sMenuMouseScrollX, dy = y - sMenuMouseScrollY;
        /* Wheel navigation owns selection until the pointer deliberately moves.
           Ignore tiny wheel/trackpad jitter and motion in the same event batch. */
        if (sMenuMouse.wheel) {
            sMenuMouseScrollX = x; sMenuMouseScrollY = y;
        } else if (!sMenuMouseHoverBlocked || dx * dx + dy * dy >= 16.0f) {
            sMenuMouseHoverBlocked = 0;
            sMenuMouse.moved = 1;
        }
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button == SDL_BUTTON_LEFT && !sMenuMouse.confirm) {
        sMenuMouse.confirm = 1;
        sMenuMouseClickX = x; sMenuMouseClickY = y;
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button == SDL_BUTTON_RIGHT) sMenuMouse.cancel = 1;
    if (ev->type == SDL_EVENT_MOUSE_WHEEL && ev->wheel.y != 0) {
        float wheel = ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -ev->wheel.y : ev->wheel.y;
        /* Two wheel units per step; preserve fractional trackpad movement.
           Reversing direction starts fresh, and bursts never queue extra steps. */
        if (wheel * sMenuMouseWheelRemainder < 0) sMenuMouseWheelRemainder = 0;
        sMenuMouseWheelRemainder += wheel;
        if (fabsf(sMenuMouseWheelRemainder) >= 2.0f) {
            sMenuMouse.wheel = wheel > 0 ? -1 : 1;
            sMenuMouseWheelRemainder = 0;
        }
        sMenuMouse.moved = 0;
        sMenuMouseHoverBlocked = 1;
        sMenuMouseScrollX = x; sMenuMouseScrollY = y;
    }
done:
    pthread_mutex_unlock(&sMenuMouseLock);
}

/* bits of the two button bytes together (byte 0 low), as include/sys/pad.h has them; 1 = pressed here */
enum { B_SELECT = 0x0001, B_L3 = 0x0002, B_R3 = 0x0004, B_START = 0x0008, B_UP = 0x0010, B_RIGHT = 0x0020, B_DOWN = 0x0040,
       B_LEFT = 0x0080, B_L2 = 0x0100, B_R2 = 0x0200, B_L1 = 0x0400, B_R1 = 0x0800, B_TRIANGLE = 0x1000, B_CIRCLE = 0x2000,
       B_CROSS = 0x4000, B_SQUARE = 0x8000 };

/* The actions in the order the settings window lists them. 16..23: left stick up, down, left, right, then right. */
static const struct { const char *name, *label; unsigned bit; int key; int pad; } kActions[PORT_ACTIONS] = {
    {"up", "D-pad up", B_UP, SDL_SCANCODE_UP, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"down", "D-pad down", B_DOWN, SDL_SCANCODE_DOWN, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"left", "D-pad left", B_LEFT, SDL_SCANCODE_LEFT, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"right", "D-pad right", B_RIGHT, SDL_SCANCODE_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"cross", "Cross", B_CROSS, SDL_SCANCODE_SPACE, SDL_GAMEPAD_BUTTON_SOUTH},
    {"circle", "Circle", B_CIRCLE, SDL_SCANCODE_L, SDL_GAMEPAD_BUTTON_EAST},
    {"square", "Square", B_SQUARE, SDL_SCANCODE_J, SDL_GAMEPAD_BUTTON_WEST},
    {"triangle", "Triangle", B_TRIANGLE, SDL_SCANCODE_I, SDL_GAMEPAD_BUTTON_NORTH},
    {"l1", "L1", B_L1, SDL_SCANCODE_Q, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"r1", "R1", B_R1, SDL_SCANCODE_O, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"l2", "L2", B_L2, SDL_SCANCODE_LSHIFT, PORT_PAD_LT},
    {"r2", "R2", B_R2, SDL_SCANCODE_9, PORT_PAD_RT},
    {"l3", "L3", B_L3, SDL_SCANCODE_LCTRL, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"r3", "R3", B_R3, SDL_SCANCODE_LALT, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"start", "START", B_START, SDL_SCANCODE_RETURN, SDL_GAMEPAD_BUTTON_START},
    {"select", "SELECT", B_SELECT, 0, SDL_GAMEPAD_BUTTON_BACK},
    {"ls_up", "Left stick up", 0, SDL_SCANCODE_W, -1}, {"ls_down", "Left stick down", 0, SDL_SCANCODE_S, -1},
    {"ls_left", "Left stick left", 0, SDL_SCANCODE_A, -1}, {"ls_right", "Left stick right", 0, SDL_SCANCODE_D, -1},
    {"rs_up", "Right stick up", 0, 0, -1}, {"rs_down", "Right stick down", 0, 0, -1},
    {"rs_left", "Right stick left", 0, 0, -1}, {"rs_right", "Right stick right", 0, 0, -1},
};

static int sKeys[2][PORT_ACTIONS], sPadBtn[2][PORT_PAD_BUTTONS], sPadSlot[2];
/* Per player: the deadzone of the left and of the right stick (-1 = the game's own, as the pad's bytes always were
   passed; 0..PORT_DEADZONE_MAX = a round one of that many percent, stick_bytes below), and vibration (on, strength
   in percent). */
static int sDeadzone[2][2], sVib[2][2];
/* What opens and closes the settings window: a key (F1) and a controller button (none), both of the settings
   window's list; and START + SELECT of any controller together, always. */
static int sOverlayKey = SDL_SCANCODE_F1, sOverlayPad = -1;
static int sChatKey = SDL_SCANCODE_TAB; /* opens the online chat's line (setting key_chat; Tab by default) */
static int sComboLatch; /* START + SELECT has opened or closed the window and is still held */
static int sSwallow; /* a controller opened or closed the window: nothing of the controllers reaches the game until those buttons are let go */
static volatile int sKeySwallow; /* the chat's line was sent or put away: nothing of the keyboard reaches the game until no bound key is held
                                    (Enter, which sends it, is START) */
static pthread_once_t sLoadOnce = PTHREAD_ONCE_INIT;

/* Native Mouse/Keyboard bindings are semantic battle actions. Each action
   uses a key or mouse button; controller mappings and menu keys are separate. */

#define SPCDEFAULTKEYS_INIT {SDL_SCANCODE_LSHIFT,SDL_SCANCODE_R,SDL_SCANCODE_F,0, \
    SDL_SCANCODE_Q,SDL_SCANCODE_E,0,SDL_SCANCODE_SPACE, \
    SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_A,SDL_SCANCODE_D, \
    SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_C,SDL_SCANCODE_LALT,SDL_SCANCODE_LCTRL}
static const int sPcDefaultKeys[PORT_PC_STORAGE_ACTIONS]=SPCDEFAULTKEYS_INIT;
static int sPcKeys[PORT_PC_STORAGE_ACTIONS]=SPCDEFAULTKEYS_INIT;
#define SPCDEFAULTMOUSE_INIT {[3]=SDL_BUTTON_LEFT,[6]=SDL_BUTTON_RIGHT}
static const int sPcDefaultMouse[PORT_PC_STORAGE_ACTIONS]=SPCDEFAULTMOUSE_INIT;
static int sPcMouse[PORT_PC_STORAGE_ACTIONS]=SPCDEFAULTMOUSE_INIT;
static int sPcCapture=-1,sPcResult,sPcRelease,sPcGamepad,sGamepadRelease;
static int sGamepadPending[3],sGamepadPendingHeld[3],sGamepadPendingCount;
static SDL_JoystickID sGamepadCaptureDevice;
/* Buttons are SDL index + 1; axes are 100 + axis * 2 + positive. */
#define SGAMEPADDEFAULTS_INIT { \
    109,SDL_GAMEPAD_BUTTON_LEFT_SHOULDER+1,SDL_GAMEPAD_BUTTON_NORTH+1,SDL_GAMEPAD_BUTTON_WEST+1, \
    111,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER+1,SDL_GAMEPAD_BUTTON_EAST+1,SDL_GAMEPAD_BUTTON_SOUTH+1, \
    102,103,100,101,104,105,SDL_GAMEPAD_BUTTON_BACK+1, \
    (PORT_GAMEPAD_CHORD | (SDL_GAMEPAD_BUTTON_LEFT_STICK+1) | ((SDL_GAMEPAD_BUTTON_RIGHT_STICK+1)<<8)),SDL_GAMEPAD_BUTTON_RIGHT_STICK+1}
static const int sGamepadDefaults[PORT_GAMEPAD_BINDINGS]=SGAMEPADDEFAULTS_INIT;
static int sGamepadBindings[PORT_GAMEPAD_BINDINGS]=SGAMEPADDEFAULTS_INIT;
extern int Port_NetOptionsControlsEnabled(void);
static int options_controls_enabled(void) {
    return Port_ZenkaiEnabled() || Port_NetOptionsControlsEnabled();
}
void PortInput_PcDefaults(void) {
    if(!Port_ZenkaiEnabled()) return;
    pthread_mutex_lock(&sPcLock);
    memcpy(sPcKeys,sPcDefaultKeys,sizeof(sPcKeys));
    memcpy(sPcMouse,sPcDefaultMouse,sizeof(sPcMouse));
    pthread_mutex_unlock(&sPcLock);
}
void PortInput_GamepadDefaults(void) {
    if(!Port_ZenkaiEnabled()) return;
    pthread_mutex_lock(&sPcLock);
    memcpy(sGamepadBindings,sGamepadDefaults,sizeof(sGamepadBindings));
    pthread_mutex_unlock(&sPcLock);
}
static int gamepad_binding_valid(int b) {
    return Port_GamepadBindingValid((unsigned)b);
}
void PortInput_GamepadGet(int *out) {
    pthread_mutex_lock(&sPcLock);memcpy(out,sGamepadBindings,sizeof(sGamepadBindings));pthread_mutex_unlock(&sPcLock);
}
void PortInput_GamepadSet(const int *in) {
    int i;pthread_mutex_lock(&sPcLock);
    for(i=0;i<PORT_GAMEPAD_BINDINGS;i++) if(gamepad_binding_valid(in[i])) sGamepadBindings[i]=in[i];
    pthread_mutex_unlock(&sPcLock);
}
void PortInput_GamepadBindingName(int action,char *out,int size) {
    int b=0,i,count,offset=0;
    static const char *const buttons[]={"Cross","Circle","Square","Triangle",
        "Select","PS","Start","L3","R3","L1","R1",
        "D-pad Up","D-pad Down","D-pad Left","D-pad Right","Function","Right Back 1","Left Back 1","Right Back 2","Left Back 2","Touchpad",
        "Function 2","Function 3","Function 4","Function 5","Function 6"};
    static const char *const axes[]={"Left Stick Left","Left Stick Right","Left Stick Up","Left Stick Down",
        "Right Stick Left","Right Stick Right","Right Stick Up","Right Stick Down","L2","L2","R2","R2"};
    if(size<=0) return;
    pthread_mutex_lock(&sPcLock);if(action>=0 && action<PORT_GAMEPAD_BINDINGS) b=sGamepadBindings[action];pthread_mutex_unlock(&sPcLock);
    count=(b&PORT_GAMEPAD_CHORD) ? ((b&0xFF0000) ? 3:2):1;
    out[0]=0;
    for(i=0;i<count && offset<size;i++) {
        int token=(b&PORT_GAMEPAD_CHORD) ? ((b>>(8*i))&255):b;
        const char *name=!token ? "Unbound":token>=100 ? axes[token-100]:buttons[token-1];
        int n=snprintf(out+offset,size-offset,"%s%s",i ? " + ":"",name);
        if(n<0) break;
        offset+=n;
    }
}

void PortInput_GamepadCaptureStart(int action) {
    if(!Port_ZenkaiEnabled() || action<0 || action>=PORT_PC_ACTIONS) return;
    pthread_mutex_lock(&sPcLock);
    sGamepadPendingCount=0;sGamepadCaptureDevice=0;
    memset(sGamepadPendingHeld,0,sizeof(sGamepadPendingHeld));
    sPcGamepad=1;sPcCapture=action;sPcResult=0;gPortInputCapture=1;
    pthread_mutex_unlock(&sPcLock);
}
static int gamepad_held(SDL_Gamepad *g,int b) {
    if(b&PORT_GAMEPAD_CHORD) return gamepad_held(g,b&255) && gamepad_held(g,(b>>8)&255) &&
        (!(b&0xFF0000) || gamepad_held(g,(b>>16)&255));
    if(b>=100) { int v=SDL_GetGamepadAxis(g,(SDL_GamepadAxis)((b-100)/2));return (b&1) ? v>PORT_TRIGGER_AT:v < -PORT_TRIGGER_AT; }
    return b>0 && SDL_GetGamepadButton(g,(SDL_GamepadButton)(b-1));
}
static void stick_bytes(int deadzone,int x,int y,unsigned char *bx,unsigned char *by);
static unsigned gamepad_battle_buttons(SDL_Gamepad *g,int socket,unsigned char *lx,unsigned char *ly,unsigned char *rx) {
    unsigned btn=0;int a,axes[3]={0,0,0};pthread_mutex_lock(&sPcLock);
    for(a=0;a<PORT_PC_ACTIONS;a++) {
        int b=sGamepadBindings[a];
        if(a>=8 && a<14) {
            int strength=127,direction=(a==8 || a==10 || a==12) ? -1:1;
            int target=a<10 ? 1:a<12 ? 0:2;
            if(direction<0) strength=128;
            if(!(b&PORT_GAMEPAD_CHORD) && b>=100) {
                unsigned char value,unused;
                int raw=SDL_GetGamepadAxis(g,(SDL_GamepadAxis)((b-100)/2));
                if((b&1) ? raw<=0:raw>=0) continue;
                stick_bytes(sDeadzone[socket][a>=12],raw,0,&value,&unused);
                strength=(b&1) ? value-128:128-value;
            } else if(!gamepad_held(g,b)) continue;
            axes[target]+=direction*strength;
        } else if(gamepad_held(g,b)) btn|=a==14 ? B_SELECT:a==15 ? B_L3|B_R3:a==16 ? B_R3:Port_PcActionMask(a);
    }
    *lx=axes[0]<-128 ? 0:axes[0]>127 ? 255:128+axes[0];
    *ly=axes[1]<-128 ? 0:axes[1]>127 ? 255:128+axes[1];
    *rx=axes[2]<-128 ? 0:axes[2]>127 ? 255:128+axes[2];
    pthread_mutex_unlock(&sPcLock);return btn;
}

void PortInput_PcGet(int *bindings) {
    pthread_mutex_lock(&sPcLock);
    memcpy(bindings,sPcKeys,sizeof(sPcKeys));memcpy(bindings+PORT_PC_STORAGE_ACTIONS,sPcMouse,sizeof(sPcMouse));
    pthread_mutex_unlock(&sPcLock);
}
void PortInput_PcSet(const int *bindings) {
    int i;
    pthread_mutex_lock(&sPcLock);
    for(i=0;i<PORT_PC_ACTIONS;i++) {
        if(bindings[i]>=0 && bindings[i]<SDL_SCANCODE_COUNT) sPcKeys[i]=bindings[i];
        if(bindings[i+PORT_PC_STORAGE_ACTIONS]>=0 && bindings[i+PORT_PC_STORAGE_ACTIONS]<=9) sPcMouse[i]=bindings[i+PORT_PC_STORAGE_ACTIONS];
        if(sPcMouse[i]) sPcKeys[i]=0; /* mouse wins when migrating an old dual binding */
    }
    pthread_mutex_unlock(&sPcLock);
}
void PortInput_PcBindingName(int action,char *out,int size) {
    const char *key;int k,m;
    if(action<0 || action>=PORT_PC_ACTIONS) { snprintf(out,size,"Unbound");return; }
    pthread_mutex_lock(&sPcLock);k=sPcKeys[action];m=sPcMouse[action];pthread_mutex_unlock(&sPcLock);
    key=k ? SDL_GetScancodeName((SDL_Scancode)k):"";
    static const char *const names[]={"","Left Mouse","Middle Mouse","Right Mouse","Mouse 4","Mouse 5","Scroll Up","Scroll Down","Scroll Left","Scroll Right"};
    if(m) snprintf(out,size,"%s",names[m]);
    else snprintf(out,size,"%s",k ? key:"Unbound");
}
void PortInput_PcCaptureStart(int action) {
    if(!Port_ZenkaiEnabled() || action<0 || action>=PORT_PC_ACTIONS) return;
    pthread_mutex_lock(&sPcLock);
    sPcGamepad=0;sPcCapture=action;sPcResult=0;gPortInputCapture=1;
    pthread_mutex_unlock(&sPcLock);
}
int PortInput_PcCapturing(void) {
    int active;pthread_mutex_lock(&sPcLock);active=sPcCapture>=0 || sPcResult!=0;pthread_mutex_unlock(&sPcLock);return active;
}
int PortInput_PcCapturePoll(void) {
    int result;
    pthread_mutex_lock(&sPcLock);result=sPcResult;
    if(result) { sPcResult=0;gPortInputCapture=0;sPcRelease=1;sGamepadRelease=sPcGamepad;sPcMousePressed=0;sPcMouseBlocked|=sPcMouseHeld; }
    pthread_mutex_unlock(&sPcLock);
    return result;
}
int PortInput_PcCaptureEvent(const SDL_Event *ev) {
    int used=0;
    pthread_mutex_lock(&sPcLock);
    if(sPcCapture>=0 || sPcResult) {
        used=ev->type==SDL_EVENT_KEY_DOWN || ev->type==SDL_EVENT_KEY_UP ||
            ev->type==SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type==SDL_EVENT_MOUSE_BUTTON_UP ||
            ev->type==SDL_EVENT_MOUSE_WHEEL || ev->type==SDL_EVENT_TEXT_INPUT;
        if(sPcGamepad) {
            int binding=0;
            used=used || ev->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type==SDL_EVENT_GAMEPAD_BUTTON_UP || ev->type==SDL_EVENT_GAMEPAD_AXIS_MOTION;
            if(sPcCapture>=0 && ev->type==SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
                if(ev->key.scancode==SDL_SCANCODE_ESCAPE) sPcResult=-1;
                else if(ev->key.scancode==SDL_SCANCODE_DELETE || ev->key.scancode==SDL_SCANCODE_BACKSPACE) {
                    sGamepadBindings[sPcCapture]=0;sPcResult=1;
                }
            } else if(ev->type==SDL_EVENT_WINDOW_FOCUS_LOST) sPcResult=-1;
            if(sPcCapture>=0 && (ev->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type==SDL_EVENT_GAMEPAD_BUTTON_UP || ev->type==SDL_EVENT_GAMEPAD_AXIS_MOTION)) {
                SDL_JoystickID device=ev->type==SDL_EVENT_GAMEPAD_AXIS_MOTION ? ev->gaxis.which:ev->gbutton.which;
                int i,held=0;
                if(sGamepadPendingCount && device!=sGamepadCaptureDevice) { pthread_mutex_unlock(&sPcLock);return used; }
                if(ev->type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) binding=ev->gbutton.button+1;
                if(ev->type==SDL_EVENT_GAMEPAD_AXIS_MOTION &&
                    (ev->gaxis.value>PORT_TRIGGER_AT || ev->gaxis.value < -PORT_TRIGGER_AT))
                    binding=100+ev->gaxis.axis*2+(ev->gaxis.value>0);
                if(binding && Port_GamepadTokenValid(binding)) {
                    for(i=0;i<sGamepadPendingCount;i++) if(sGamepadPending[i]==binding) break;
                    if(i<3) {
                        if(i==sGamepadPendingCount) sGamepadPendingCount++;
                        sGamepadPending[i]=binding;sGamepadPendingHeld[i]=1;sGamepadCaptureDevice=device;
                    }
                }
                for(i=0;i<sGamepadPendingCount;i++) {
                    int token=sGamepadPending[i];
                    if(ev->type==SDL_EVENT_GAMEPAD_BUTTON_UP && token==ev->gbutton.button+1) sGamepadPendingHeld[i]=0;
                    if(ev->type==SDL_EVENT_GAMEPAD_AXIS_MOTION && token>=100 && (token-100)/2==ev->gaxis.axis &&
                       ((token&1) ? ev->gaxis.value<=PORT_TRIGGER_AT:ev->gaxis.value>=-PORT_TRIGGER_AT)) sGamepadPendingHeld[i]=0;
                    held|=sGamepadPendingHeld[i];
                }
                if(sGamepadPendingCount && !held) {
                    int j;binding=0;
                    for(i=0;i<sGamepadPendingCount;i++) for(j=i+1;j<sGamepadPendingCount;j++)
                        if(sGamepadPending[j]<sGamepadPending[i]) { int t=sGamepadPending[i];sGamepadPending[i]=sGamepadPending[j];sGamepadPending[j]=t; }
                    for(i=0;i<sGamepadPendingCount;i++) binding|=sGamepadPending[i]<<(8*i);
                    if(sGamepadPendingCount>1) binding|=PORT_GAMEPAD_CHORD;
                    for(i=0;i<PORT_GAMEPAD_BINDINGS;i++) if(i!=sPcCapture && sGamepadBindings[i]==binding) sGamepadBindings[i]=0;
                    sGamepadBindings[sPcCapture]=binding;sPcResult=1;
                }
            } else if(ev->type==SDL_EVENT_GAMEPAD_REMOVED && ev->gdevice.which==sGamepadCaptureDevice) sPcResult=-1;

        } else if(sPcCapture>=0 && ev->type==SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            if(ev->key.scancode==SDL_SCANCODE_ESCAPE) sPcResult=-1;
            else if(ev->key.scancode==SDL_SCANCODE_DELETE) { sPcKeys[sPcCapture]=sPcMouse[sPcCapture]=0;sPcResult=1; }
            else if(ev->key.scancode==SDL_SCANCODE_BACKSPACE) { sPcKeys[sPcCapture]=sPcMouse[sPcCapture]=0;sPcResult=1; }
            else if(ev->key.scancode!=SDL_SCANCODE_F1) {
                int i;for(i=0;i<PORT_PC_ACTIONS;i++) if(i!=sPcCapture && sPcKeys[i]==ev->key.scancode) sPcKeys[i]=0;
                sPcKeys[sPcCapture]=ev->key.scancode;sPcMouse[sPcCapture]=0;sPcResult=1;
            }
        } else if(sPcCapture>=0 && ev->type==SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button>=1 && ev->button.button<=5) {
            int i;for(i=0;i<PORT_PC_ACTIONS;i++) if(i!=sPcCapture && sPcMouse[i]==ev->button.button) sPcMouse[i]=0;
            sPcMouse[sPcCapture]=ev->button.button;sPcKeys[sPcCapture]=0;sPcResult=1;
        } else if(sPcCapture>=0 && ev->type==SDL_EVENT_MOUSE_WHEEL && pc_wheel_binding(ev)) {
            int binding=pc_wheel_binding(ev),i;
            for(i=0;i<PORT_PC_ACTIONS;i++) if(i!=sPcCapture && sPcMouse[i]==binding) sPcMouse[i]=0;
            sPcMouse[sPcCapture]=binding;sPcKeys[sPcCapture]=0;sPcResult=1;
        } else if(sPcCapture>=0 && ev->type==SDL_EVENT_WINDOW_FOCUS_LOST) sPcResult=-1;
        if(sPcResult) sPcCapture=-1;
    }
    pthread_mutex_unlock(&sPcLock);
    return used;
}
static void pc_mouse_context(int mode) {
    static int previous;
    pthread_mutex_lock(&sPcLock);
    if(mode!=previous) { sPcMouseBlocked|=sPcMouseHeld&~sPcMousePressed;previous=mode; }
    if(mode!=PORT_MOUSE_GAME_FIGHT && mode!=PORT_MOUSE_GAME_PAUSED) {
        sPcMousePressed=0;sPcMouseBlocked|=sPcMouseHeld;
    }
    pthread_mutex_unlock(&sPcLock);
}
static int pc_binding_held(int action,const bool *keys,unsigned mouse) {
    return (sPcKeys[action]>0 && keys[sPcKeys[action]]) ||
        (sPcMouse[action]>0 && (mouse & SDL_BUTTON_MASK(sPcMouse[action])));
}
static unsigned pc_battle_buttons(const bool *keys,unsigned char *lx,unsigned char *ly,unsigned char *rx,unsigned char *ry,int *strike) {
    unsigned buttons=0,mouse;int a;
    pthread_mutex_lock(&sPcLock);mouse=(sPcMouseHeld|sPcMousePressed)&~sPcMouseBlocked;sPcMousePressed=0;
    *strike=pc_binding_held(3,keys,mouse);
    for(a=0;a<8;a++) if(pc_binding_held(a,keys,mouse)) buttons|=Port_PcActionMask(a);
    if(pc_binding_held(8,keys,mouse)) *ly=0;
    if(pc_binding_held(9,keys,mouse)) *ly=255;
    if(pc_binding_held(10,keys,mouse)) *lx=0;
    if(pc_binding_held(11,keys,mouse)) *lx=255;
    if(pc_binding_held(12,keys,mouse)) *rx=0;
    if(pc_binding_held(13,keys,mouse)) *rx=255;
    if(pc_binding_held(14,keys,mouse)) buttons|=B_SELECT;
    if(pc_binding_held(15,keys,mouse)) buttons|=B_L3|B_R3;
    if(pc_binding_held(16,keys,mouse)) buttons|=B_R3;
    pthread_mutex_unlock(&sPcLock);return buttons;
}
static int pc_release_blocked(const bool *keys) {
    int held=0,i;
    pthread_mutex_lock(&sPcLock);
    if(sPcRelease) {
        for(i=1;i<SDL_SCANCODE_COUNT;i++) held|=keys[i];
        held|=sPcMouseHeld!=0;held|=sGamepadRelease!=0;sPcRelease=held;
    }
    pthread_mutex_unlock(&sPcLock);return held;
}

static unsigned keyboard_secondary_buttons(const bool *keys) {
    return (keys[SDL_SCANCODE_K] ? B_CROSS : 0) |
           (keys[SDL_SCANCODE_F] ? B_TRIANGLE : 0) |
           (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_RSHIFT] ? B_L2 : 0) |
           (keys[SDL_SCANCODE_RCTRL] ? B_L3 : 0) |
           (keys[SDL_SCANCODE_RALT] ? B_R3 : 0);
}

static void defaults(int player) {
    int a;
    for (a = 0; a < PORT_ACTIONS; a++) {
        sKeys[player][a] = player == 0 ? kActions[a].key : 0;
        if (a < PORT_PAD_BUTTONS) {
            sPadBtn[player][a] = kActions[a].pad;
        }
    }
    sPadSlot[player] = player;
    if (player == 1 && getenv("ZENKAIPOWER_PAD_SHARED") != NULL) {
        sPadSlot[player] = 0; /* ZENKAIPOWER_PAD_SHARED: both players on controller 1 (testing 1P vs 2P with one pad) */
    }
    sDeadzone[player][0] = sDeadzone[player][1] = -1;
    sVib[player][0] = 1;
    sVib[player][1] = 100;
}

static int range(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void load(void) {
    char name[32];
    int p, a;
    for (p = 0; p < 2; p++) {
        defaults(p);
        for (a = 0; a < PORT_ACTIONS; a++) {
            snprintf(name, sizeof(name), "key_p%d_%s", p + 1, kActions[a].name);
            sKeys[p][a] = Port_Setting(name, sKeys[p][a]);
            if (a < PORT_PAD_BUTTONS) {
                snprintf(name, sizeof(name), "pad_p%d_%s", p + 1, kActions[a].name);
                sPadBtn[p][a] = Port_Setting(name, sPadBtn[p][a]);
            }
        }
        if (p == 0) {
            if (sKeys[p][4] == SDL_SCANCODE_K) sKeys[p][4] = SDL_SCANCODE_SPACE;
            if (sKeys[p][8] == SDL_SCANCODE_U) sKeys[p][8] = SDL_SCANCODE_Q;
            if (sKeys[p][10] == SDL_SCANCODE_7) sKeys[p][10] = SDL_SCANCODE_LSHIFT;
            if (sKeys[p][12] == 0) sKeys[p][12] = SDL_SCANCODE_LCTRL;
            if (sKeys[p][13] == 0) sKeys[p][13] = SDL_SCANCODE_LALT;
            if (sKeys[p][15] == SDL_SCANCODE_RSHIFT) sKeys[p][15] = 0;
        }
        snprintf(name, sizeof(name), "padslot_p%d", p + 1);
        sPadSlot[p] = Port_Setting(name, sPadSlot[p]);
        if (p == 1 && getenv("ZENKAIPOWER_PAD_SHARED") != NULL) {
            sPadSlot[p] = 0; /* overrides the saved setting too */
        }
        for (a = 0; a < 2; a++) {
            snprintf(name, sizeof(name), "deadzone_p%d_%s", p + 1, a == 0 ? "ls" : "rs");
            sDeadzone[p][a] = range(Port_Setting(name, -1), -1, PORT_DEADZONE_MAX);
        }
        snprintf(name, sizeof(name), "vibration_p%d", p + 1);
        sVib[p][0] = Port_Setting(name, 1) != 0;
        snprintf(name, sizeof(name), "vibration_p%d_strength", p + 1);
        sVib[p][1] = range(Port_Setting(name, 100), 0, 100);
    }
    sOverlayKey = Port_Setting("key_overlay", SDL_SCANCODE_F1);
    if (sOverlayKey == SDL_SCANCODE_ESCAPE) sOverlayKey = SDL_SCANCODE_F1; /* migrate the former default */
    sOverlayPad = Port_Setting("pad_overlay", -1);
    sChatKey = Port_Setting("key_chat", SDL_SCANCODE_TAB);
    /* A relaunched Dragon Net session has an isolated save card. Carry the
       source player's in-game Options bindings across that process boundary. */
    if (Port_NetOptionsControlsEnabled()) {
        const char *encoded=getenv("ZENKAIPOWER_NET_PC_BINDINGS");
        int bindings[PORT_PC_BINDINGS];
        if(encoded && strlen(encoded)==PORT_PC_BINDINGS*8) {
            for(a=0;a<PORT_PC_BINDINGS;a++) {
                char word[9];memcpy(word,encoded+a*8,8);word[8]=0;
                if(strspn(word,"0123456789abcdefABCDEF")!=8) break;
                bindings[a]=(int)strtoul(word,NULL,16);
            }
            if(a==PORT_PC_BINDINGS) PortInput_PcSet(bindings);
        }
        encoded=getenv("ZENKAIPOWER_NET_GAMEPAD_BINDINGS");
        if(encoded && strlen(encoded)==PORT_GAMEPAD_BINDINGS*8) {
            int bindingsPad[PORT_GAMEPAD_BINDINGS];
            for(a=0;a<PORT_GAMEPAD_BINDINGS;a++) {
                char word[9];memcpy(word,encoded+a*8,8);word[8]=0;
                if(strspn(word,"0123456789abcdefABCDEF")!=8) break;
                bindingsPad[a]=(int)strtoul(word,NULL,16);
            }
            if(a==PORT_GAMEPAD_BINDINGS) PortInput_GamepadSet(bindingsPad);
        }
    }
}

const char *PortInput_ActionLabel(int action) { return kActions[action].label; }
int *PortInput_Keys(int player) { pthread_once(&sLoadOnce, load); return sKeys[player]; }
int *PortInput_PadButtons(int player) { pthread_once(&sLoadOnce, load); return sPadBtn[player]; }
int *PortInput_PadSlot(int player) { pthread_once(&sLoadOnce, load); return &sPadSlot[player]; }
int *PortInput_Deadzone(int player) { pthread_once(&sLoadOnce, load); return sDeadzone[player]; }
int *PortInput_Vibration(int player) { pthread_once(&sLoadOnce, load); return sVib[player]; }
int *PortInput_OverlayKey(void) { pthread_once(&sLoadOnce, load); return &sOverlayKey; }
int *PortInput_OverlayPad(void) { pthread_once(&sLoadOnce, load); return &sOverlayPad; }

/* 1 = this event opens or closes the settings window. */
int PortInput_Hotkey(const SDL_Event *ev) {
    if (!Port_SettingsEnabled()) return 0;
    pthread_once(&sLoadOnce, load);
    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        (ev->gbutton.button == SDL_GAMEPAD_BUTTON_START || ev->gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) {
        sComboLatch = 0;
        return 0;
    }
    if (gPortInputCapture) { /* (a binding is being taken, or the online screen is up) */
        return 0;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
        if (ev->key.scancode == SDL_SCANCODE_ESCAPE && Ui_ConsumesEscape()) return 0;
        return sOverlayKey > 0 && (int)ev->key.scancode == sOverlayKey;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *g = SDL_GetGamepadFromID(ev->gbutton.which);
        int b = (int)ev->gbutton.button;
        if (sOverlayPad >= 0 && b == sOverlayPad) {
            sSwallow = 1;
            return 1;
        }
        /* START + SELECT counts once per press of the two (sComboLatch, cleared when either is let go). Pressed
           in the same moment, both "down" events come in one round and each already finds the other held:
           without the latch the window opened and closed again at once, and only "hold one, press the other"
           worked. */
        if (g != NULL && (b == SDL_GAMEPAD_BUTTON_START || b == SDL_GAMEPAD_BUTTON_BACK) &&
            SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_START) && SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_BACK)) {
            sSwallow = 1;
            if (sComboLatch) {
                return 0;
            }
            sComboLatch = 1;
            return 1;
        }
    }
    return 0;
}
/* The settings window closed, or its exit question was answered, with a controller button: that press is not the game's. */
void PortInput_Swallow(void) { sSwallow = 2; }
void PortInput_SwallowKeys(void) { sKeySwallow = 1; }

int *PortInput_ChatKey(void) { pthread_once(&sLoadOnce, load); return &sChatKey; }
void PortInput_ResetDefaults(int player) { pthread_once(&sLoadOnce, load); defaults(player); }

void PortInput_Save(void) {
    char name[32];
    int p, a;
    for (p = 0; p < 2; p++) {
        for (a = 0; a < PORT_ACTIONS; a++) {
            snprintf(name, sizeof(name), "key_p%d_%s", p + 1, kActions[a].name);
            Port_SettingSave(name, sKeys[p][a]);
            if (a < PORT_PAD_BUTTONS) {
                snprintf(name, sizeof(name), "pad_p%d_%s", p + 1, kActions[a].name);
                Port_SettingSave(name, sPadBtn[p][a]);
            }
        }
        snprintf(name, sizeof(name), "padslot_p%d", p + 1);
        Port_SettingSave(name, sPadSlot[p]);
        for (a = 0; a < 2; a++) {
            snprintf(name, sizeof(name), "deadzone_p%d_%s", p + 1, a == 0 ? "ls" : "rs");
            Port_SettingSave(name, sDeadzone[p][a]);
        }
        snprintf(name, sizeof(name), "vibration_p%d", p + 1);
        Port_SettingSave(name, sVib[p][0]);
        snprintf(name, sizeof(name), "vibration_p%d_strength", p + 1);
        Port_SettingSave(name, sVib[p][1]);
    }
    Port_SettingSave("key_overlay", sOverlayKey);
    Port_SettingSave("pad_overlay", sOverlayPad);
    Port_SettingSave("key_chat", sChatKey);
    Port_SettingsWrite();
}

/* Testing without a controller or a person. ZENKAIPOWER_STICK_TEST="lx,ly,rx,ry[,lt,rt]" (the sticks -32768..32767, + =
   right / down; the triggers 0..32767) and ZENKAIPOWER_RUMBLE_LOG=1 each attach a controller made by SDL itself ("virtual"),
   which becomes controller 1 and the only one (the real ones are left out): its sticks are held at those values,
   and what SDL hands its motors is printed (ZENKAIPOWER_RUMBLE_SLOW=ms: it keeps the caller waiting that long). The rest of
   the program does not know it from a real one. */
static bool SDLCALL test_pad_rumble(void *userdata, Uint16 low, Uint16 high) {
    const char *slow = getenv("ZENKAIPOWER_RUMBLE_SLOW"); /* ms the controller keeps the caller waiting (a slow one) */
    (void)userdata;
    fprintf(stderr, "rumble: the test controller's motors: low %u, high %u\n", low, high);
    if (slow != NULL) {
        SDL_Delay((Uint32)atoi(slow));
    }
    return true;
}

static SDL_Joystick *sTestPad;

/* (Said again after the program has opened the controller: an axis SDL has not seen move reads as resting.) */
static void test_pad_axes(void) {
    const char *t = getenv("ZENKAIPOWER_STICK_TEST");
    int v[6] = {0, 0, 0, 0, 0, 0}, i, pass;

    if (sTestPad == NULL) {
        return;
    }
    if (t != NULL) {
        sscanf(t, "%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    }
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < 6; i++) { /* (a trigger is an axis whose rest is its lowest value) */
            int x = range(i < 4 ? v[i] : v[i] * 2 - 32768, -32768, 32767);
            SDL_SetJoystickVirtualAxis(sTestPad, i, (Sint16)(pass == 0 ? x ^ 1 : x));
        }
        SDL_UpdateJoysticks();
    }
}

static void test_pad(void) {
    SDL_VirtualJoystickDesc desc;
    SDL_JoystickID id;

    if (getenv("ZENKAIPOWER_STICK_TEST") == NULL && getenv("ZENKAIPOWER_RUMBLE_LOG") == NULL) {
        return;
    }
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = "Test controller";
    desc.Rumble = test_pad_rumble;
    id = SDL_AttachVirtualJoystick(&desc);
    sTestPad = id != 0 ? SDL_OpenJoystick(id) : NULL;
    if (sTestPad == NULL) {
        fprintf(stderr, "zenkaipower: no test controller: %s\n", SDL_GetError());
    }
}

/* The connected controllers, in SDL's order (the order the settings window lists them in), kept open. */
#define MAX_PADS 8
static struct { SDL_JoystickID id; SDL_Gamepad *pad; } sOpen[MAX_PADS];
static SDL_JoystickID sIds[MAX_PADS];
static int sIdCount, sInit;

static void pads_refresh(void) {
    static unsigned calls;
    SDL_JoystickID *ids;
    int n = 0, i, k;

    if (!sInit) {
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        sInit = 1;
        test_pad();
    } else if (calls++ % 120 != 0) { /* look for plugged and unplugged controllers about once a second */
        return;
    }
#ifdef __ANDROID__
    /* DragonRise PS2-style USB pads use Android BUTTON_1..12. SDL's
       Android driver exposes those at b20..31, with hats at b11..14. */
    {
        static SDL_JoystickID mapped;
        int count=0;
        SDL_JoystickID *joysticks=SDL_GetJoysticks(&count);
        for(i=0;joysticks && i<count;i++) {
            SDL_JoystickID id=joysticks[i];
            if(id!=mapped && SDL_GetJoystickVendorForID(id)==0x0810 &&
               SDL_GetJoystickProductForID(id)==0x0001) {
                char guid[33],mapping[512];
                SDL_GUIDToString(SDL_GetJoystickGUIDForID(id),guid,sizeof(guid));
                snprintf(mapping,sizeof(mapping),
                    "%s,USB PS2 Gamepad,a:b22,b:b21,x:b23,y:b20,back:b28,start:b29,"
                    "leftshoulder:b24,rightshoulder:b25,lefttrigger:b26,righttrigger:b27,"
                    "leftstick:b30,rightstick:b31,leftx:a0,lefty:a1,rightx:a2,righty:a3,"
                    "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,platform:Android,",guid);
                if(SDL_AddGamepadMapping(mapping)>=0) {
                    mapped=id;
                    fprintf(stderr,"zenkaipower: Android USB PS2 gamepad mapping installed (%s)\n",guid);
                } else fprintf(stderr,"zenkaipower: Android gamepad mapping failed: %s\n",SDL_GetError());
            }
        }
        SDL_free(joysticks);
    }
#endif
    ids = SDL_GetGamepads(&n);
    sIdCount = 0;
    for (i = 0; ids != NULL && i < n && sIdCount < MAX_PADS; i++) {
        if (sTestPad != NULL && !SDL_IsJoystickVirtual(ids[i])) {
            continue; /* (a test is never felt on a real controller) */
        }
        sIds[sIdCount++] = ids[i];
    }
#ifdef __ANDROID__
    /* A USB/Bluetooth pad plugged in after touch play should own player 1. */
    for (i = 0; i + 1 < sIdCount; i++) {
        const char *name = SDL_GetGamepadNameForID(sIds[i]);
        if (name && strcmp(name, "ZenkaiPower Touch Pad") == 0) {
            SDL_JoystickID touch = sIds[i];
            for (k = i; k + 1 < sIdCount; k++) sIds[k] = sIds[k + 1];
            sIds[sIdCount - 1] = touch;
            break;
        }
    }
#endif
    SDL_free(ids);
    for (k = 0; k < MAX_PADS; k++) { /* close the ones that are gone */
        for (i = 0; i < sIdCount && sIds[i] != sOpen[k].id; i++) {
        }
        if (sOpen[k].pad != NULL && i == sIdCount) {
            SDL_CloseGamepad(sOpen[k].pad);
            sOpen[k].pad = NULL;
        }
    }
    for (i = 0; i < sIdCount; i++) { /* open the new ones */
        for (k = 0; k < MAX_PADS && !(sOpen[k].pad != NULL && sOpen[k].id == sIds[i]); k++) {
        }
        if (k == MAX_PADS) {
            for (k = 0; k < MAX_PADS && sOpen[k].pad != NULL; k++) {
            }
            if (k < MAX_PADS) {
                sOpen[k].id = sIds[i];
                sOpen[k].pad = SDL_OpenGamepad(sIds[i]);
#ifdef __ANDROID__
                fprintf(stderr,"zenkaipower: Android controller %u: %s (%s)\n",(unsigned)sIds[i],
                    SDL_GetGamepadNameForID(sIds[i]),sOpen[k].pad ? "opened" : SDL_GetError());
#endif
            }
        }
    }
    if (calls < 400) {
        test_pad_axes();
    }
}

static SDL_Gamepad *pad_of(int player) {
    int slot = sPadSlot[player], k;
    if (slot < 0 || slot >= sIdCount) {
        return NULL;
    }
    for (k = 0; k < MAX_PADS; k++) {
        if (sOpen[k].pad != NULL && sOpen[k].id == sIds[slot]) {
            return sOpen[k].pad;
        }
    }
    return NULL;
}

/* ZENKAIPOWER_PAD="frame:button,frame:button,...": scripted presses for player 1, each held for 4 frames from that frame
   (frame = the renderer's frame counter). Buttons: start select up down left right cross circle square triangle
   l1 r1 l2 r2. For tests without a person at the keyboard. */
static unsigned script_buttons(void) {
    static const struct { const char *name; unsigned bit; } names[] = {
        {"start", B_START}, {"select", B_SELECT}, {"up", B_UP}, {"down", B_DOWN}, {"left", B_LEFT}, {"right", B_RIGHT},
        {"cross", B_CROSS}, {"circle", B_CIRCLE}, {"square", B_SQUARE}, {"triangle", B_TRIANGLE}, {"l1", B_L1}, {"r1", B_R1},
        {"l2", B_L2}, {"r2", B_R2},
    };
    static struct { unsigned frame, bit; } ev[256];
    static int count = -1;
    unsigned btn = 0;
    int i;

    if (count < 0) {
        const char *p = getenv("ZENKAIPOWER_PAD");
        count = 0;
        while (p != NULL && *p != 0 && count < 256) {
            char name[16];
            unsigned frame;
            int used = 0;
            size_t k;
            if (sscanf(p, "%u:%15[a-z0-9]%n", &frame, name, &used) != 2) {
                break;
            }
            for (k = 0; k < sizeof(names) / sizeof(names[0]); k++) {
                if (strcmp(name, names[k].name) == 0) {
                    ev[count].frame = frame;
                    ev[count++].bit = names[k].bit;
                }
            }
            p += used;
            if (*p == ',') { p++; }
        }
    }
    for (i = 0; i < count; i++) {
        if (gGsFrame >= ev[i].frame && gGsFrame < ev[i].frame + 4) {
            btn |= ev[i].bit;
        }
    }
    return btn;
}

/* Fills `data` (18 bytes) for controller port `socket`. Returns 0 when there is no window (the caller keeps the
   idle pad). */
/* Clash tap input: always available to keyboard/mouse; optional for gamepads
 * (setting clash_square, off by default; ZENKAIPOWER_CLASH_SQUARE=1).
 *
 * In a clash (two beams or two rushes meeting) the game counts one thing: the frames in which a direction is newly
 * pressed (input 51 = BTLC_DIR_P; BtlAct_ClashStruggleHandler and BtlAct_ClashBlowsHandler). Turning the stick is
 * the usual way to make them, four a turn; tapping directions makes them as well. When tapping Strike, while this
 * player's fighter is in a clash action, Strike is taken off the pad and each press of it holds the stick in the
 * next of up, right, down, left, then advances on the next three frames: four newly pressed directions, unlike a single tap of
 * the d-pad gives. In Strike mode, stick and d-pad directions are neutralized
 * during the clash so each press of Strike is the struggle input.
 *
 * Each new Strike press emits four successive directions on consecutive input frames.
 * Holding does not repeat after the fourth direction. One tap covers a full turn.
 *
 * This is done to the pad's own bytes, before they reach the game or the other player of an online match: the
 * game sees an ordinary pad, so the two copies of an online match compute the same fight whoever has it on. */
static int sClashSquare = -1;

int Port_ClashSquare(void) {
    if (sClashSquare < 0) {
        sClashSquare = getenv("ZENKAIPOWER_CLASH_SQUARE") != NULL ? atoi(getenv("ZENKAIPOWER_CLASH_SQUARE")) != 0 : Port_Setting("clash_square", 0) != 0;
    }
    return sClashSquare;
}

void Port_ClashSquareSet(int on) {
    sClashSquare = on != 0;
    Port_SettingSave("clash_square", sClashSquare);
    Port_SettingsWrite();
}
void Port_ClashSquareApply(int on) { sClashSquare = on != 0; }

/* The last active device chooses the local clash prompt when gamepad rotation is selected. */
static int sClashPcPrompt;
int Port_ClashTapPrompt(void) { return Port_ClashSquare() || __atomic_load_n(&sClashPcPrompt, __ATOMIC_RELAXED); }

static void clash_square(int socket, unsigned *btn, unsigned char *lx, unsigned char *ly, int pcStrike, unsigned strikeBit) {
    extern void *BtlChar_FindByObjId(int objId); /* the game: the fighter of a side, NULL outside a fight */
    extern int BtlAct_GetCurrent(void *chr);     /* its action */
    extern int Port_NetActive(void), Port_NetMe(void);
    static int dir[2], was[2], remaining[2];
    int side = Port_NetActive() ? Port_NetMe() : socket, in = 0;
    int enabled = Port_ClashSquare() || pcStrike || remaining[socket], down = Port_ClashSquare() ? (*btn & strikeBit) != 0 : pcStrike;
    void *chr;

    if (!enabled) {
        was[socket] = remaining[socket] = 0;
        return;
    }
    if (getenv("ZENKAIPOWER_CLASH_TEST") != NULL) {
        in = 1; /* testing: as if in a clash all the time */
    } else if ((chr = BtlChar_FindByObjId(side)) != NULL) {
        int action = BtlAct_GetCurrent(chr);
        in = (action >= 0x130 && action <= 0x132) || action == 0xFA || action == 0xFB || action == 0xFC;
    }
    if (!in) {
        remaining[socket] = 0;
        was[socket] = down; /* (a Square held into the clash is not a press in it) */
        return;
    }
    int pulse = remaining[socket] > 0;
    if (down && !was[socket]) remaining[socket] += 4;
    if (remaining[socket] > 0) {
        dir[socket] = (dir[socket] + 1) & 3;
        remaining[socket]--;
        pulse = 1;
    }
    was[socket] = down;
    *btn &= ~(strikeBit|B_UP|B_RIGHT|B_DOWN|B_LEFT);
    *lx = *ly = 0x80;
    if (down || pulse) {
        switch (dir[socket]) {
        case 0: *ly = 0x00; break; /* up */
        case 1: *lx = 0xFF; break; /* right */
        case 2: *ly = 0xFF; break; /* down */
        default: *lx = 0x00; break; /* left */
        }
    }
}

/* ZENKAIPOWER_CLASH_LOG=1: one line for every point a clash counts from a newly pressed direction (the stick, the d-pad, or
   Square with the option above), and the total as a beam clash ends. Called from the game's clash handlers. */
void Port_ClashLog(int player, const char *what, int fromInput, int total, int frame) {
    static int on = -1;

    if (on < 0) {
        on = getenv("ZENKAIPOWER_CLASH_LOG") != NULL;
    }
    if (!on) {
        return;
    }
    if (fromInput) {
        fprintf(stderr, "clash: player %d, %s, frame %d: +1 from input, total %d%s\n", player + 1, what, frame, total,
                Port_ClashSquare() ? " (Square option on)" : "");
    } else {
        fprintf(stderr, "clash: player %d, %s ended after %d frames with %d points\n", player + 1, what, frame, total);
    }
}

/* The sticks.
 *
 * The game (src/sys/pad.c, Pad_Update) takes each axis as a byte, 0..255 with the rest between 127 and 128, and
 * gives every axis by itself a deadzone of PAD_STICK_DEADZONE = 50 of the 127.5 (39%): value = (|byte - 127.5| -
 * 50) / 77.5, zero below. The pair is then brought inside the unit circle, and the stick counts as a direction
 * (the menus, dashes, the clash counter) when the larger of the two values is beyond 0.5: 69.6% of the way on an
 * axis. Its zone of rest is therefore a square, and a diagonal starts late.
 *
 * Deadzone -1 ("Original") passes the controller's bytes as they are: that. A deadzone d of 0..PORT_DEADZONE_MAX
 * percent is a round one instead: with m the stick's distance from the centre (0..1, the corners of a square gate
 * cut to 1), nothing below d, and from there
 *     out = (m - d) / (1 - d)        in the stick's own direction
 * which is zero at the edge of the zone and 1 at full tilt. The bytes are then the ones the game's own formula
 * turns back into `out` (0x80 for zero, else 127.5 +- (50 + |out| * 77.5), to the nearest of the 78 steps a side),
 * so the game is left as it is and its direction test, 0.5 of `out`, falls at d + (1 - d) / 2 of the way: half way
 * between the zone's edge and full tilt, in every direction. It is done to the pad's bytes, before they reach the
 * game or the other player of an online match (as the Square option above).
 */
static unsigned char game_byte(double v) {
    double a = fabs(v) * 77.5;
    int k;

    if (a < 0.25) { /* (nearer to nothing than to the first step, 0.5) */
        return 0x80;
    }
    k = range((int)a, 0, 77); /* the steps are at k + 0.5 */
    return (unsigned char)(v > 0.0 ? 178 + k : 77 - k);
}

static void stick_bytes(int deadzone, int x, int y, unsigned char *bx, unsigned char *by) {
    double fx = x < -32767 ? -1.0 : x / 32767.0, fy = y < -32767 ? -1.0 : y / 32767.0;
    double m = sqrt(fx * fx + fy * fy), d = deadzone / 100.0, out;

    if (deadzone < 0) {
        *bx = (unsigned char)((x + 32768) >> 8);
        *by = (unsigned char)((y + 32768) >> 8);
        return;
    }
    if (m <= d || m <= 0.0) {
        *bx = *by = 0x80;
        return;
    }
    out = ((m > 1.0 ? 1.0 : m) - d) / (1.0 - d);
    *bx = game_byte(fx / m * out);
    *by = game_byte(fy / m * out);
}

/* What the game makes of a stick's two bytes: Pad_StickToFloat and Pad_Update's steps (src/sys/pad.c), done again
   here in the same single-precision arithmetic. (The game's own function cannot be called from this side: its
   floats are not passed as the host's are. ZENKAIPOWER_STICK_TEST prints this beside what the game itself then has.) */
static float game_axis(unsigned char raw) {
    float value = (float)raw - 127.5f;

    if (0.0f < value) {
        value -= 50.0f;
        if (value < 0.0f) {
            value = 0.0f;
        }
    } else {
        value += 50.0f;
        if (0.0f < value) {
            value = 0.0f;
        }
    }
    return value / (127.5f - 50.0f);
}

static void game_stick(const unsigned char b[2], float out[2], int *dir) {
    float len;

    out[0] = game_axis(b[0]);
    out[1] = game_axis(b[1]);
    len = out[0] * out[0] + out[1] * out[1];
    if (len > 1.0f) {
        len = 1.0f / sqrtf(len);
        out[0] *= len;
        out[1] *= len;
    }
    if (fabsf(out[0]) > fabsf(out[1])) {
        *dir = out[0] > 0.5f ? 4 : out[0] < -0.5f ? 3 : 0;
    } else {
        *dir = out[1] > 0.5f ? 2 : out[1] < -0.5f ? 1 : 0;
    }
}

/* For the settings window: the sticks and triggers of a player's controller now, and what the game would get. */
void PortInput_StickView(int player, PortStickView *out) {
    static const SDL_GamepadAxis axes[2][2] = {{SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY}, {SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY}};
    SDL_Gamepad *g;
    int s, i;

    pthread_once(&sLoadOnce, load);
    memset(out, 0, sizeof(*out));
    g = pad_of(player);
    out->connected = g != NULL;
    out->canVibrate = g != NULL && SDL_GetBooleanProperty(SDL_GetGamepadProperties(g), SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    for (s = 0; s < 2; s++) {
        int v[2] = {0, 0};
        for (i = 0; i < 2 && g != NULL; i++) {
            v[i] = SDL_GetGamepadAxis(g, axes[s][i]);
            out->raw[s][i] = v[i] < -32767 ? -1.0f : (float)v[i] / 32767.0f;
        }
        stick_bytes(sDeadzone[player][s], v[0], v[1], &out->bytes[s][0], &out->bytes[s][1]);
        game_stick(out->bytes[s], out->game[s], &out->dir[s]);
        if (g != NULL) {
            int t = SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            out->trigger[s] = (float)t / 32767.0f;
            out->triggerHeld[s] = t > PORT_TRIGGER_AT;
        }
    }
}

/* Vibration.
 *
 * The game drives the DualShock 2's two motors itself: every frame Pad_Update (src/sys/pad.c) ends with
 * Pad_SendActuator for each port, which calls sceVibSetActParam with two bytes: bit 0 = the small motor (on / off),
 * bits 1..7 = the large motor's power (0..127). What it sends is what the fight asked for in that frame
 * (BtlChar_UpdateVibration -> Pad_AddVibration: hits taken and given, clashes, charging; nothing in a replay, for a
 * CPU side, or with the game's own Options > Controller > Vibration off for that player), and zero otherwise:
 * a motor stops by the game sending zero. That call comes here (plat_stub.c).
 *
 * The large motor is SDL's low-frequency one (its power, of 127, becomes SDL's, of 65535), the small motor the
 * high-frequency one (all or nothing), each times the player's strength. The game runs 30 steps a second and
 * sends a new command with every one for as long as an effect lasts: the large motor's power swings between 50%
 * and 100% of what was asked, six times a second, and the small motor is on in every other step. That is passed on
 * as it comes; a command that only repeats the last one is not sent again until half its time is over.
 * Nothing is sent while the settings window is open or the game's window is not the one being used, with the
 * player's setting off, or in a frame that is only being re-run (online play's rollback). Every command is given
 * RUMBLE_MS to live, so a controller is never left humming by a game that stopped coming here; and the
 * controllers are stopped as the program ends.
 *
 * A command is sent on the game's frame, and some controllers keep the caller waiting for it (generic pads through
 * DirectInput, some Bluetooth links): 30 of them a second of 50 ms each and the game stands still. Each call is
 * timed: a controller that takes RUMBLE_SLOW_NS or more is sent at most one command per ten times its longest call
 * (each living long enough to reach the next), and one that takes RUMBLE_OFF_NS or more twice (once could be the
 * system's own pause) is not sent any more until another controller is this player's, with a notice and a line in
 * the log.
 *
 * Nothing here is seen by the game: two bytes are read, nothing is written and the result is the constant it
 * always was. A fight, a recording of one and both sides of an online match are the same with it on or off. Online,
 * both ports' commands are computed on both sides: only this side's (Port_NetMe) are felt, on player 1's
 * controller, which is the one that plays there.
 */
#define RUMBLE_MS 250
#define RUMBLE_SLOW_NS 4000000ull /* 4 ms */
#define RUMBLE_OFF_NS 50000000ull /* 50 ms */
static struct {
    SDL_Gamepad *pad;
    Uint16 low, high;
    Uint64 at;
    int lastLarge, lastSmall;
    Uint64 cost; /* the controller's longest call, ns (0: never slow) */
    int stalls;  /* its calls of RUMBLE_OFF_NS or more */
    int off;     /* the controller held the game up: nothing more is sent to it */
} sRumble[2];

static void rumble_send(int player, SDL_Gamepad *g, Uint16 low, Uint16 high, int why) {
    Uint64 now = SDL_GetTicks(), gap, took;
    int ms = RUMBLE_MS;

    if (sRumble[player].pad != g && sRumble[player].pad != NULL && (sRumble[player].low | sRumble[player].high) != 0) {
        int k; /* another controller became this player's: the old one, if it is still there, is stopped */
        for (k = 0; k < MAX_PADS; k++) {
            if (sOpen[k].pad == sRumble[player].pad) {
                SDL_RumbleGamepad(sOpen[k].pad, 0, 0, 0);
            }
        }
        sRumble[player].low = sRumble[player].high = 0;
    }
    if (sRumble[player].pad != g) {
        sRumble[player].cost = 0;
        sRumble[player].stalls = 0;
        sRumble[player].off = 0;
    }
    sRumble[player].pad = g;
    if (sRumble[player].off) {
        return;
    }
    if (sRumble[player].cost >= RUMBLE_SLOW_NS) { /* a slow controller: one command per ten times its longest call */
        gap = sRumble[player].cost * 10 / 1000000;
        if (gap < RUMBLE_MS / 2) {
            gap = RUMBLE_MS / 2;
        }
        if (now - sRumble[player].at < gap) {
            return;
        }
        ms = (int)gap + RUMBLE_MS;
    }
    if (low == sRumble[player].low && high == sRumble[player].high && ((low | high) == 0 || now - sRumble[player].at < (Uint64)ms / 2)) {
        return; /* (the same again: only said again before it would run out) */
    }
    if (getenv("ZENKAIPOWER_RUMBLE_LOG") != NULL) {
        extern unsigned gPortVBlanks;
        static const char *const whys[] = {"", " (the settings window is open)", " (the window is not in use)", " (vibration is off)"};
        fprintf(stderr, "rumble: vblank %u, player %d: small %d, large %d/127 -> %s(low %u, high %u, %d ms)%s\n", gPortVBlanks, player + 1,
                sRumble[player].lastSmall, sRumble[player].lastLarge, g != NULL ? "SDL_RumbleGamepad" : "no controller", low, high,
                (low | high) != 0 ? ms : 0, whys[why]);
    }
    if (g != NULL) {
        took = SDL_GetTicksNS();
        SDL_RumbleGamepad(g, low, high, (low | high) != 0 ? ms : 0);
        took = SDL_GetTicksNS() - took;
        if (took >= RUMBLE_OFF_NS && ++sRumble[player].stalls >= 2) {
            extern void Port_UiNotice(const char *text);
            sRumble[player].off = 1;
            fprintf(stderr, "rumble: player %d's controller (%s) took %llu ms for a vibration command: vibration is off for it\n",
                    player + 1, SDL_GetGamepadName(g) != NULL ? SDL_GetGamepadName(g) : "?", (unsigned long long)(took / 1000000));
            Port_UiNotice("Vibration turned off: the controller held the game up");
        } else if (took >= RUMBLE_SLOW_NS && took > sRumble[player].cost) {
            if (sRumble[player].cost == 0) {
                fprintf(stderr, "rumble: player %d's controller (%s) took %.1f ms for a vibration command: fewer are sent\n", player + 1,
                        SDL_GetGamepadName(g) != NULL ? SDL_GetGamepadName(g) : "?", (double)took / 1e6);
            }
            sRumble[player].cost = took;
        }
    }
    sRumble[player].low = low;
    sRumble[player].high = high;
    sRumble[player].at = now;
}

#ifdef __ANDROID__
/* Expiry (milliseconds) and amplitude are published together for the Java UI.
   A stalled game must never leave the device vibrating indefinitely. */
static Uint64 sAndroidVibration;
#endif

static void rumble_stop(void) {
#ifdef __ANDROID__
    __atomic_store_n(&sAndroidVibration,0,__ATOMIC_RELEASE);
#endif
    int k;
    for (k = 0; k < MAX_PADS; k++) {
        if (sOpen[k].pad != NULL) {
            SDL_RumbleGamepad(sOpen[k].pad, 0, 0, 0);
        }
    }
}

/* The game's motor command for a port: `small` 0 / 1, `large` 0..127. Once per port and frame. */
void Port_PadVibrate(int socket, int small, int large) {
    extern int Port_NetActive(void), Port_NetMe(void);
    extern int gPortResim;
    static int atExit;
    SDL_Window *w;
    int player = socket, why = 0;
    unsigned low, high;

    if (!sInit || gPortResim || socket < 0 || socket > 1) {
        return;
    }
    if (Port_NetActive()) {
        if (socket != Port_NetMe()) {
            return;
        }
        player = 0;
    }
    if (!atExit) {
        atExit = 1;
        atexit(rumble_stop);
    }
    sRumble[player].lastLarge = large;
    sRumble[player].lastSmall = small;
    low = (unsigned)large * 65535u / 127u * (unsigned)sVib[player][1] / 100u;
    high = small ? 65535u * (unsigned)sVib[player][1] / 100u : 0;
    w = GsDraw_Window();
    if (gPortOverlayOpen || gPortInputCapture) {
        why = 1;
    } else if (w != NULL && !(SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) && getenv("ZENKAIPOWER_RUMBLE_LOG") == NULL) {
        why = 2; /* (the test's window, which nobody sees, never is) */
    } else if (!sVib[player][0]) {
        why = 3;
    }
    if (why != 0) {
        low = high = 0;
    }
#ifdef __ANDROID__
    if (player == 0) {
        unsigned strength = low > high ? low : high;
        unsigned amplitude = (strength * 255u + 32767u) / 65535u;
        Uint64 request = amplitude ? ((SDL_GetTicks() + RUMBLE_MS) << 8) | amplitude : 0;
        __atomic_store_n(&sAndroidVibration,request,__ATOMIC_RELEASE);
    }
#endif
    rumble_send(player, pad_of(player), (Uint16)low, (Uint16)high, why);
}

/* 1 with ZENKAIPOWER_RUMBLE_TEST set: the game's rule "no vibration in a replay" is lifted (src/battle/btl_char_get.c). */
int Port_RumbleTest(void) {
    static int on = -1;
    if (on < 0) {
        on = getenv("ZENKAIPOWER_RUMBLE_TEST") != NULL;
    }
    return on;
}

/* The settings window's "Test": half a second of both motors at the player's strength (the one thing that is felt
   while that window is open). */
void PortInput_VibrationTest(int player) {
    SDL_Gamepad *g = pad_of(player);
    Uint16 s = (Uint16)(65535u * (unsigned)sVib[player][1] / 100u);

    if (getenv("ZENKAIPOWER_RUMBLE_LOG") != NULL) {
        fprintf(stderr, "rumble: test, player %d -> %s(low %u, high %u, 500 ms)\n", player + 1, g != NULL ? "SDL_RumbleGamepad" : "no controller", s, s);
    }
    if (g != NULL) {
        SDL_RumbleGamepad(g, s, s, 500);
    }
}

/* The controllers' and the keyboard's state as it is at this moment (plat_stub.c, "Low input lag": the pads are
   read long after the window's events were last taken). The events themselves stay queued for the window. */
void PortInput_Fresh(void) {
    if (Gs_RenderThreaded() && !Gs_OnRenderThread()) {
        return; /* (the window is the render thread's: it takes the events, every millisecond it waits: PortInput_Pump) */
    }
    SDL_PumpEvents();
    game_mouse_poll();
}

/* The render thread (ZENKAIPOWER_GS_THREAD): the window's events, taken while it waits for work or for a picture's time.
   The controllers' and the keys' state the game's thread reads is then never more than a millisecond old. */
void PortInput_Pump(void) {
    SDL_PumpEvents();
    game_mouse_poll();
}

/* ZENKAIPOWER_LAG_LOG (plat_stub.c, Port_LagMark): how old a press is when the game first reads it. An event watch notes
   when each button or key press arrived (the event's own time: the system's or SDL's reading of the controller);
   the read that first has a new press logs "lag P <time> <frame> <microseconds since the press arrived>". */
static uint64_t sPressNs; /* the latest press's arrival, SDL_GetTicksNS's clock */
static bool SDLCALL press_watch(void *unused, SDL_Event *ev) {
    (void)unused;
    if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat)) {
        __atomic_store_n(&sPressNs, ev->common.timestamp, __ATOMIC_RELEASE);
    }
    return true;
}
static void press_age(int socket, unsigned btn) {
    static unsigned prev[2];
    static int on = -1;
    if (on < 0) {
        on = getenv("ZENKAIPOWER_LAG_LOG") != NULL;
        if (on) {
            SDL_AddEventWatch(press_watch, NULL);
        }
    }
    if (on && (btn & ~prev[socket]) != 0) {
        /* (each press counted once: a direction from a stick is a new bit without a press of its own, and was given
           the age of the last button press, seconds old) */
        static uint64_t used;
        uint64_t at = __atomic_load_n(&sPressNs, __ATOMIC_ACQUIRE), now = SDL_GetTicksNS();
        extern void Port_LagNote(int what, unsigned long long value);
        if (at != 0 && at != used && now >= at && now - at < 1000000000ull) {
            used = at;
            Port_LagNote('P', (unsigned long long)((now - at) / 1000ull));
        }
    }
    prev[socket] = btn;
}

static int sVirtualGamepad=1;
int Port_VirtualGamepad(void) { return __atomic_load_n(&sVirtualGamepad,__ATOMIC_ACQUIRE); }
void Port_VirtualGamepadApply(int on) {
    __atomic_store_n(&sVirtualGamepad,on!=0,__ATOMIC_RELEASE);
#ifdef __ANDROID__
    if (!on) __atomic_store_n(&sAndroidVibration,0,__ATOMIC_RELEASE);
#endif
}

#ifdef __ANDROID__
static int sAndroidTouchActive;
static int sAndroidPauseRequest;
int Port_AndroidBattleActive(void) {
    return __atomic_load_n(&sAndroidTouchActive, __ATOMIC_ACQUIRE);
}
void Port_AndroidPauseRequest(void) {
    if(Port_AndroidBattleActive()) __atomic_store_n(&sAndroidPauseRequest,1,__ATOMIC_RELEASE);
}
/* Java reads only this published snapshot, never live game state. */
int Port_AndroidTouchControlsActive(void) {
    return Port_AndroidBattleActive() && Port_VirtualGamepad();
}
int Port_AndroidTouchVibration(void) {
    Uint64 request=__atomic_load_n(&sAndroidVibration,__ATOMIC_ACQUIRE);
    return Port_AndroidTouchControlsActive() && SDL_GetTicks() < (request >> 8)
        ? (int)(request & 255u) : 0;
}
#endif

int Port_PadRead(int socket, unsigned char *data) {
    unsigned btn = 0;
    unsigned strikeBit = B_SQUARE;
    unsigned char rx = 0x80, ry = 0x80, lx = 0x80, ly = 0x80;
    SDL_Gamepad *g;
    int a, pcStrike = 0, pcActivity = 0, padActivity = 0;

    if (!GsGpu_Enabled() || socket < 0 || socket > 1) {
        return 0;
    }
    pthread_once(&sLoadOnce, load);
    pads_refresh();
    if(socket==0) {
        int mode = Port_GameMouseMode();
        pc_mouse_context(mode);
#ifdef __ANDROID__
        __atomic_store_n(&sAndroidTouchActive, mode == PORT_MOUSE_GAME_FIGHT &&
            !gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping, __ATOMIC_RELEASE);
#endif
    }
    if (sSwallow) { /* the buttons that opened or closed the settings window are not the game's (START would pause) */
        int k, b, held = 0;
        for (k = 0; k < MAX_PADS && sSwallow == 2; k++) { /* (2: whichever button it was) */
            for (b = 0; sOpen[k].pad != NULL && b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                if (SDL_GetGamepadButton(sOpen[k].pad, (SDL_GamepadButton)b)) {
                    held = 2;
                }
            }
        }
        for (k = 0; k < MAX_PADS; k++) {
            if (sOpen[k].pad != NULL &&
                (SDL_GetGamepadButton(sOpen[k].pad, SDL_GAMEPAD_BUTTON_START) || SDL_GetGamepadButton(sOpen[k].pad, SDL_GAMEPAD_BUTTON_BACK) ||
                 (sOverlayPad >= 0 && sOverlayPad < SDL_GAMEPAD_BUTTON_COUNT && SDL_GetGamepadButton(sOpen[k].pad, (SDL_GamepadButton)sOverlayPad)))) {
                held = held != 0 ? held : 1;
            }
        }
        sSwallow = held;
    }
    /* (the settings window is worked with the controller too while it is open: nothing of it for the game) */
    g = gPortInputCapture || gPortOverlayOpen || sSwallow ? NULL : pad_of(socket);
    if(sGamepadRelease) {
        int held=0,k;
        for(k=0;k<MAX_PADS;k++) if(sOpen[k].pad) {
            for(a=1;a<=SDL_GAMEPAD_BUTTON_COUNT;a++) held|=gamepad_held(sOpen[k].pad,a);
            for(a=100;a<100+SDL_GAMEPAD_AXIS_COUNT*2;a++) held|=gamepad_held(sOpen[k].pad,a);
        }
        pthread_mutex_lock(&sPcLock);sGamepadRelease=held;pthread_mutex_unlock(&sPcLock);
        if(held) g=NULL;
    }
    if (g != NULL) {
        int battle=options_controls_enabled() && Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT;
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            if(battle && kActions[a].bit!=B_START && !(kActions[a].bit&(B_UP|B_DOWN|B_LEFT|B_RIGHT))) continue;
            int src = sPadBtn[socket][a];
            if (src == PORT_PAD_LT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > PORT_TRIGGER_AT :
                src == PORT_PAD_RT ? SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > PORT_TRIGGER_AT :
                src >= 0 && SDL_GetGamepadButton(g, (SDL_GamepadButton)src)) {
                btn |= kActions[a].bit;
            }
        }
        if(battle) btn|=gamepad_battle_buttons(g,socket,&lx,&ly,&rx);
        else {
            stick_bytes(sDeadzone[socket][0], SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX), SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY), &lx, &ly);
            stick_bytes(sDeadzone[socket][1], SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX), SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY), &rx, &ry);
        }
        padActivity = btn != 0 || lx != 0x80 || ly != 0x80 || rx != 0x80 || ry != 0x80;
    }
    int pcBlocked=pc_release_blocked(SDL_GetKeyboardState(NULL));
    if (pcBlocked) btn=0;
    if (sKeySwallow) {
        const bool *ks = SDL_GetKeyboardState(NULL);
        int p, held = 0;
        for (p = 0; p < 2; p++) {
            for (a = 0; a < PORT_ACTIONS; a++) {
                held = held || (sKeys[p][a] > 0 && sKeys[p][a] < SDL_SCANCODE_COUNT && ks[sKeys[p][a]]);
            }
        }
        sKeySwallow = held || keyboard_secondary_buttons(ks) != 0;
    }
    /* the settings window takes the keyboard while it is open, and the online chat while a line is typed (the
       controller still plays then) */
    if (!gPortOverlayOpen && !gPortInputCapture && !gPortChatTyping && !sKeySwallow && !pcBlocked && !PortInput_NativeKeyboardActive()) {
        const bool *ks = SDL_GetKeyboardState(NULL);
        const int *k = sKeys[socket];
        int pcBattle=socket==0 && options_controls_enabled() && Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT;
        if(pcBattle) {
            unsigned char oldLx=lx,oldLy=ly,oldRx=rx,oldRy=ry;
            unsigned pcButtons=pc_battle_buttons(ks,&lx,&ly,&rx,&ry,&pcStrike);
            btn|=pcButtons;
            pcActivity = pcButtons != 0 || lx != oldLx || ly != oldLy || rx != oldRx || ry != oldRy;
            strikeBit = Port_PcActionMask(3);
        } else {
        int menu=socket==0 && Port_GameMouseMode()!=PORT_MOUSE_GAME_FIGHT;
        if(socket==0) {
            unsigned secondary=keyboard_secondary_buttons(ks);
            btn|=menu ? secondary&~(B_CROSS|B_TRIANGLE):secondary;
            if(menu) {
                if(ks[SDL_SCANCODE_RETURN] || ks[SDL_SCANCODE_KP_ENTER]) btn|=gPortBootSkipMouse ? B_START : B_CROSS;
                if(ks[SDL_SCANCODE_BACKSPACE]) btn|=B_TRIANGLE;
            }
        }
        for (a = 0; a < PORT_PAD_BUTTONS; a++) {
            if(menu && (kActions[a].bit==B_CROSS || kActions[a].bit==B_TRIANGLE ||
                (kActions[a].bit==B_START && (k[a]==SDL_SCANCODE_RETURN || k[a]==SDL_SCANCODE_KP_ENTER)))) continue;
            if (k[a] > 0 && k[a] < SDL_SCANCODE_COUNT && ks[k[a]]) {
                btn |= kActions[a].bit;
                if (kActions[a].bit == B_SQUARE && Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT) pcStrike=1;
                pcActivity=1;
            }
        }
#define HELD(n) (k[n] > 0 && k[n] < SDL_SCANCODE_COUNT && ks[k[n]])
        if (HELD(16)) { ly = 0x00; }
        if (HELD(17)) { ly = 0xFF; }
        if (HELD(18)) { lx = 0x00; }
        if (HELD(19)) { lx = 0xFF; }
        if (HELD(20)) { ry = 0x00; }
        if (HELD(21)) { ry = 0xFF; }
        if (HELD(22)) { rx = 0x00; }
        if (HELD(23)) { rx = 0xFF; }
#undef HELD
        }
    }
    if (socket == 0 && PortInput_NativeKeyboardActive() && !gPortOverlayOpen && !gPortChatTyping && !gPortInputCapture) {
        const bool *ks = SDL_GetKeyboardState(NULL);
        if (ks[SDL_SCANCODE_UP]) btn |= B_UP;
        if (ks[SDL_SCANCODE_DOWN]) btn |= B_DOWN;
        if (ks[SDL_SCANCODE_LEFT]) btn |= B_LEFT;
        if (ks[SDL_SCANCODE_RIGHT]) btn |= B_RIGHT;
    }
    if (socket == 0) {
        /* Esc follows menu Back, but Start during boot playback, fights, and pause.
           A key used by an overlay/text field must be released before play. */
        static int escapeBlocked;
        const bool *ks = SDL_GetKeyboardState(NULL);
        int mode = Port_GameMouseMode();
        if (!ks[SDL_SCANCODE_ESCAPE]) escapeBlocked = 0;
        else if (gPortOverlayOpen || gPortInputCapture || gPortChatTyping || pcBlocked ||
                 sKeySwallow || PortInput_NativeKeyboardActive()) escapeBlocked = 1;
        else if (!escapeBlocked)
            btn |= gPortBootSkipMouse || mode == PORT_MOUSE_GAME_FIGHT || mode == PORT_MOUSE_GAME_PAUSED ? B_START : B_TRIANGLE;
        if(!(options_controls_enabled() && mode==PORT_MOUSE_GAME_FIGHT)) {
            unsigned mouseButtons=game_mouse_buttons(mode);
            btn |= mouseButtons;
            if (mode==PORT_MOUSE_GAME_FIGHT) {
                pcStrike |= (mouseButtons & B_SQUARE) != 0;
                pcActivity |= mouseButtons != 0;
            }
        }
        btn |= script_buttons();
    }
    if (socket == 0 && getenv("ZENKAIPOWER_STICK_TEST") != NULL) { /* the numbers, once: what was read, what was passed, what the game has */
        static const char *const dirs[5] = {"-", "up", "down", "left", "right"};
        extern unsigned char gPad[]; /* the game's (include/sys/pad.h): 0x130 left x, y, 0x138 right x, y, 0x148 held */
        static int reads;
        if (++reads == 40 && g != NULL) {
            PortStickView v;
            int s;
            PortInput_StickView(0, &v);
            for (s = 0; s < 2; s++) {
                fprintf(stderr, "sticks: %s: raw %6d %6d (%+.3f %+.3f), deadzone %d -> bytes %3u %3u -> game %+.4f %+.4f, direction %s\n",
                        s == 0 ? "left " : "right", SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFTX : SDL_GAMEPAD_AXIS_RIGHTX),
                        SDL_GetGamepadAxis(g, s == 0 ? SDL_GAMEPAD_AXIS_LEFTY : SDL_GAMEPAD_AXIS_RIGHTY), v.raw[s][0], v.raw[s][1],
                        sDeadzone[0][s], v.bytes[s][0], v.bytes[s][1], v.game[s][0], v.game[s][1], dirs[v.dir[s]]);
            }
        } else if (reads == 44) {
            float f[4];
            unsigned held;
            memcpy(f, gPad + 0x130, sizeof(f));
            memcpy(&held, gPad + 0x148, sizeof(held));
            fprintf(stderr, "sticks: the game's pad 1 has left %+.4f %+.4f, right %+.4f %+.4f, held %08X\n", f[0], f[1], f[2], f[3], held);
        }
    }
    if (socket==0 && Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT) {
        if (pcActivity) __atomic_store_n(&sClashPcPrompt, 1, __ATOMIC_RELAXED);
        else if (padActivity) __atomic_store_n(&sClashPcPrompt, 0, __ATOMIC_RELAXED);
        else if (g==NULL) __atomic_store_n(&sClashPcPrompt, 1, __ATOMIC_RELAXED);
    }
    clash_square(socket, &btn, &lx, &ly, pcStrike, strikeBit);
#ifdef __ANDROID__
    if(socket==0 && __atomic_exchange_n(&sAndroidPauseRequest,0,__ATOMIC_ACQ_REL) &&
       Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT) btn|=B_START;
#endif
    press_age(socket, btn);
    memset(data, 0, 18);
    data[0] = (unsigned char)(~btn & 0xFF);       /* active low */
    data[1] = (unsigned char)(~(btn >> 8) & 0xFF);
    data[2] = rx; data[3] = ry; data[4] = lx; data[5] = ly;
    return 18;
}
