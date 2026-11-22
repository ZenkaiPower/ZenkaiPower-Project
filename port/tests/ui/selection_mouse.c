/* cc -std=c99 -Iinclude port/tests/ui/selection_mouse.c -o /tmp/selection-mouse */
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
typedef float f32;
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define PADG_UP 8
#define PADG_DOWN 4
static struct { int gamePressed,gameRepeat; } gPad[2];
#include "menu/port_selection_mouse.h"
#include "menu/port_selection_net_target.h"
static int calls;
static const char *forcedHit;
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) {
    calls++;
    if (forcedHit) return strcmp(name,forcedHit)==0;
    return scope==11 && strcmp(name,"mc_chara_chip_003")==0 && x==100 && y==100;
}
void Port_ItemPanelNetTarget(s32 side,PortMenuMouse *mouse,PortMouseNetNav *nav) {
    nav->target=-1;
}
int main(void) {
    PortMenuMouse mouse={100,100,1,0,0,0};
    assert(Port_SelectionMouseHit(&mouse,11,"mc_chara_chip_%03d",0,7,32,32,0)==3);
    assert(Port_SelectionMouseHit(&mouse,12,"mc_chara_chip_%03d",0,7,32,32,512)==-1);
    calls=0;mouse.wheel=1;
    assert(Port_SelectionMouseHit(&mouse,11,"mc_chara_chip_%03d",0,7,32,32,0)==-1 && calls==0);
    Port_SelectionMouseButtons(&mouse,0,0);assert(gPad[0].gameRepeat==PADG_DOWN);
    mouse.wheel=0;mouse.moved=0;mouse.confirm=1;
    assert(Port_SelectionMouseHit(&mouse,11,"mc_chara_chip_%03d",0,7,32,32,0)==3);
    Port_SelectionMouseButtons(&mouse,1,0);assert(gPad[1].gamePressed==0);
    Port_SelectionMouseButtons(&mouse,1,1);assert(gPad[1].gamePressed==PADG_CROSS);
    mouse.confirm=0;mouse.cancel=1;Port_SelectionMouseButtons(&mouse,0,0);
    assert(gPad[0].gamePressed==PADG_TRIANGLE);
    memset(gPad,0,sizeof(gPad));mouse.cancel=0;mouse.confirm=1;
    Port_SelectionScreenButtons(&mouse,0,0);
#ifdef __ANDROID__
    assert(gPad[0].gamePressed==PADG_CROSS);
#else
    assert(!gPad[0].gamePressed);
#endif
    /* Ordinary menus still require a hit, including on Android. */
    Port_SelectionMouseButtons(&mouse,1,0);assert(!gPad[1].gamePressed);
    memset(gPad,0,sizeof(gPad));mouse.confirm=0;
    Port_SelectionScreenButtons(&mouse,0,0);assert(!gPad[0].gamePressed);
    mouse.wheel=1;
    Port_SelectionScreenButtons(&mouse,0,0);
    assert(!gPad[0].gamePressed && gPad[0].gameRepeat==PADG_DOWN);
    /* Online selectors must emit ordinary confirm without moving a cursor. */
    mouse.x=mouse.y=0;mouse.wheel=0;mouse.confirm=1;
    for (int state=0;state<=10;state++) {
        PortMouseNetView view={0};PortMouseNetNav nav={0};
        view.state=state;view.current=2;
        Port_SelectionNetTarget(&mouse,&nav,&view);
        assert(nav.target==-1);
#ifdef __ANDROID__
        if(state!=8) assert(nav.action==0x4000);
        else assert(!nav.action); /* a finished side does not accept again */
#else
        assert(nav.action==(state==4 ? 0x4000:0));
#endif
    }
    {
        PortMouseNetView view={0};PortMouseNetNav nav={0};view.state=9;
        forcedHit="mc_yajirusi_up";
        Port_SelectionNetTarget(&mouse,&nav,&view);
        assert(nav.action==0x10 && nav.target==-1);
        forcedHit=0;
    }
    puts("Selection mouse input checks passed.");
    return 0;
}
