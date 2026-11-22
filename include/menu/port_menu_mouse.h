#ifndef PORT_MENU_MOUSE_H
#define PORT_MENU_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Game-thread reads consume edges once; the window thread supplies SDL events.
   Coordinates are in the displayed 512 x 448 game picture, excluding bars. */
typedef struct PortMenuMouse {
    float x, y;
    int moved, confirm, cancel, wheel;
    float dragX, dragY;
    int dragStart, dragHeld, dragEnd;
} PortMenuMouse;

void PortMenuMouse_Enable(int enabled);
int PortMenuMouse_Enabled(void);
int PortMenuMouse_LeftHeld(void);
void PortMenuMouse_Ready(int ready);
void PortMenuMouse_Read(PortMenuMouse *out);
int PortInput_AnyButtonPressed(void);
int PortMenuMouse_Visible(void);
/* Accepted control hits publish cursor feedback across the game/window threads. */
void PortMenuMouse_SetHover(int hovered);
int PortMenuMouse_Hovered(void);

/* Selection screens capture composed clip transforms from the last draw. */
void Port_FlashMenuMouseBegin(void);
int Port_FlashMenuMouseHit(int scope, const char *name, float width, float height,
                          float pivot, float x, float y);

/* Unbounded coordinates in the drawn clip's unit rectangle, for dragging. */
int Port_FlashMenuMousePosition(int scope,const char *name,float width,float height,
                               float pivot,float x,float y,float *u,float *v);
int Port_ItemListMouseScroll(PortMenuMouse *mouse, int scope, int rows, int count, int *top);
void Port_DialogMouseSelect(int choice);

/* Drawn menu controls, including their current animation transforms. */
int Port_FlashMainMouseRow(float x, float y);
int Port_FlashTitleMouseRow(float x, float y);
/* -1 up, +1 down, 0 outside either chevron. */
int Port_FlashMainMouseArrow(float x, float y);
void Port_FlashQuitMouseCapture(int on);
int Port_FlashQuitMouseRow(int x16, int y16);

#ifdef __cplusplus
}
#endif
#endif
