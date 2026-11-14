#ifndef PORT_NET_SELECTION_MOUSE_H
#define PORT_NET_SELECTION_MOUSE_H
#include "port_menu_mouse.h"
/* Host-only navigation state: only ordinary pad bytes enter rollback/network
   history. Callbacks inspect the local selector but never change game state. */
typedef struct PortMouseNetNav {
    int ready, phase, current, target;
    unsigned negative, positive, action;
} PortMouseNetNav;
typedef void (*PortMouseNetSelector)(PortMenuMouse *, PortMouseNetNav *);
void PortMenuMouse_NetSelector(PortMouseNetSelector selector);
void PortMenuMouse_NetEnsureSelector(PortMouseNetSelector selector);
void PortMenuMouse_NetSample(unsigned char *pad);
void PortMenuMouse_SessionSave(void);
void PortMenuMouse_SessionRestore(void);
#endif
