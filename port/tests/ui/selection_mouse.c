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
static int calls;
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) {
    calls++;
    return scope==11 && strcmp(name,"mc_chara_chip_003")==0 && x==100 && y==100;
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
    puts("Selection mouse input checks passed.");
    return 0;
}
