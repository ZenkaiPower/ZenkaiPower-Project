/* gcc -std=gnu99 -g -fsanitize=address,undefined -ffunction-sections -fdata-sections \
 *   port/tests/ui/menu_mouse.c port/src/gs/gs_input.c -Wl,--gc-sections -pthread -lm -o /tmp/menu_mouse_test
 * /tmp/menu_mouse_test
 * Synthetic window events exercise the real input mailbox without a display. */
#include <SDL3/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../../../include/menu/port_menu_mouse.h"

extern void PortInput_MenuMouseEvent(const SDL_Event *, int);
extern int gPortOverlayOpen, gPortInputCapture, gPortChatTyping;
volatile int gUiPresentX = 200, gUiPresentY = 100, gUiPresentW = 1600, gUiPresentH = 1200;
static int focused = 1, mouseSupport = 1;
int Port_Setting(const char *name, int fallback) { (void)name; (void)fallback; return mouseSupport; }
int GsDraw_OutputSize(int *w,int *h) { *w=2000;*h=1400;return 1; }
SDL_Window *GsDraw_Window(void) { return (SDL_Window *)1; }
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *window) { (void)window; return focused ? SDL_WINDOW_INPUT_FOCUS : 0; }
bool SDL_GetWindowSize(SDL_Window *window, int *w, int *h) { (void)window; *w = 1000; *h = 700; return true; }
bool SDL_GetWindowSizeInPixels(SDL_Window *window, int *w, int *h) { (void)window; *w = 2000; *h = 1400; return true; }

static SDL_Event motion(float x, float y) {
    SDL_Event ev = {0};
    ev.type = SDL_EVENT_MOUSE_MOTION;
    ev.motion.x = x; ev.motion.y = y; ev.motion.xrel = 1;
    return ev;
}
static SDL_Event click(int button, float x, float y) {
    SDL_Event ev = {0};
    ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.button = button; ev.button.x = x; ev.button.y = y;
    return ev;
}
static void empty(void) {
    PortMenuMouse mouse;
    PortMenuMouse_Read(&mouse);
    assert(!mouse.moved && !mouse.confirm && !mouse.cancel && !mouse.wheel);
}
int main(void) {
    PortMenuMouse mouse;
    SDL_Event ev = motion(500, 350);
    PortInput_MenuMouseEvent(&ev, 0);
    assert(!PortMenuMouse_Visible()); empty();
    PortMenuMouse_Enable(1);
    PortInput_MenuMouseEvent(&ev, 0);
    assert(PortMenuMouse_Visible()); empty(); /* opening animation is not ready */
    PortMenuMouse_Ready(1);
    PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse);
    assert(mouse.moved && fabsf(mouse.x - 256) < 0.01f && fabsf(mouse.y - 224) < 0.01f);
    empty(); /* motion and clicks are consumed only once */
    PortMenuMouse_SetHover(1); PortMenuMouse_Ready(1); assert(PortMenuMouse_Hovered());
    empty(); assert(PortMenuMouse_Hovered()); /* stationary */
    PortInput_MenuMouseEvent(&ev, 0); assert(PortMenuMouse_Hovered());
    PortMenuMouse_Read(&mouse); assert(mouse.moved && PortMenuMouse_Hovered());
    PortMenuMouse_SetHover(1); assert(PortMenuMouse_Hovered());
    PortMenuMouse_Ready(1); assert(PortMenuMouse_Hovered()); /* still inside */
    PortInput_MenuMouseEvent(&ev, 0); PortMenuMouse_Read(&mouse);
    assert(PortMenuMouse_Hovered()); /* no intermediate purple frame */
    PortMenuMouse_Ready(1); assert(!PortMenuMouse_Hovered()); /* completed miss */
    PortMenuMouse_SetHover(1);
    PortMenuMouse_Ready(0); assert(!PortMenuMouse_Hovered());
    PortMenuMouse_Ready(1);

    ev = click(SDL_BUTTON_LEFT, 300, 200);
    PortInput_MenuMouseEvent(&ev, 0);
    assert(PortMenuMouse_LeftHeld());
    ev.type = SDL_EVENT_MOUSE_BUTTON_UP; PortInput_MenuMouseEvent(&ev, 0);
    assert(!PortMenuMouse_LeftHeld());
    ev = motion(600, 400); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse);
    assert(mouse.confirm && fabsf(mouse.x - 128) < 0.01f && fabsf(mouse.y - 112) < 0.01f);
    empty(); /* use the click's coordinates, not subsequent motion */
    ev = click(SDL_BUTTON_LEFT, 50, 200); PortInput_MenuMouseEvent(&ev, 0); empty(); /* side bar */
    ev = click(SDL_BUTTON_RIGHT, 300, 200); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(mouse.cancel && !mouse.confirm); empty();

    ev.type = SDL_EVENT_MOUSE_WHEEL;
    ev.wheel.mouse_x = 500; ev.wheel.mouse_y = 350; ev.wheel.y = 0.25f; ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    PortInput_MenuMouseEvent(&ev, 0); PortMenuMouse_Read(&mouse); assert(!mouse.wheel);
    ev.wheel.y = 1.75f;
    PortInput_MenuMouseEvent(&ev, 0); PortMenuMouse_Read(&mouse); assert(mouse.wheel == -1);
    ev.wheel.y = 2;
    ev.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    PortInput_MenuMouseEvent(&ev, 0); PortMenuMouse_Read(&mouse); assert(mouse.wheel == 1);
    empty();

    /* Wheel owns selection across reads, even with small pointer jitter. */
    ev = motion(501, 350); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(!mouse.moved);
    ev = motion(502, 350); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(!mouse.moved);
    ev = click(SDL_BUTTON_LEFT, 502, 350); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(mouse.confirm); /* clicks still work */
    ev = motion(520, 350); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(mouse.moved);

    ev = click(SDL_BUTTON_LEFT, 300, 200);
    PortInput_MenuMouseEvent(&ev, 0); PortInput_MenuMouseEvent(&ev, 1); empty(); /* overlay first refusal */
    gPortOverlayOpen = 1; PortInput_MenuMouseEvent(&ev, 0); empty(); gPortOverlayOpen = 0;
    gPortInputCapture = 1; PortInput_MenuMouseEvent(&ev, 0); empty(); gPortInputCapture = 0;
    gPortChatTyping = 1; PortInput_MenuMouseEvent(&ev, 0); empty(); gPortChatTyping = 0;
    PortInput_MenuMouseEvent(&ev, 0); gPortOverlayOpen = 1; empty(); gPortOverlayOpen = 0; empty();
    focused = 0; PortInput_MenuMouseEvent(&ev, 0); empty(); focused = 1;
    PortInput_MenuMouseEvent(&ev, 0); ev.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    PortInput_MenuMouseEvent(&ev, 0); empty(); assert(!PortMenuMouse_Visible());
    ev = motion(500, 350); PortInput_MenuMouseEvent(&ev, 0); assert(PortMenuMouse_Visible());
    ev.type = SDL_EVENT_KEY_DOWN; PortInput_MenuMouseEvent(&ev, 0); empty(); assert(!PortMenuMouse_Visible());
    ev = click(SDL_BUTTON_LEFT, 300, 200); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Ready(0); PortMenuMouse_Ready(1); empty(); /* a click cannot survive a transition */
    PortInput_MenuMouseEvent(&ev, 0); mouseSupport = 0;
    empty(); assert(!PortMenuMouse_Visible()); /* pending click is discarded immediately */
    PortInput_MenuMouseEvent(&ev, 0); empty(); /* no new input while disabled */
    mouseSupport = 1; empty();
    ev = motion(500, 350); PortInput_MenuMouseEvent(&ev, 0);
    PortMenuMouse_Read(&mouse); assert(mouse.moved && PortMenuMouse_Visible());
    PortMenuMouse_Enable(0); PortInput_MenuMouseEvent(&ev, 0); empty(); assert(!PortMenuMouse_Visible());
    puts("Menu mouse input checks passed");
    return 0;
}
