#!/usr/bin/env python3
"""Exercise production Options pointer routing with native input-step stubs."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
text=(root/'src/menu/menu_x_c.c').read_text()
a=text.index('static s32 sOptionMouseStepping;')
b=text.index('\n#endif',a)
helpers=text[a:b]
constants='\n'.join(line for line in (root/'include/menu/menu_x.h').read_text().splitlines() if line.startswith('#define OPT_') or line.startswith('#define OPTION_VOICE_BASE'))
source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
typedef unsigned u32;
typedef float f32;
#define PADG_LEFT 1
#define PADG_RIGHT 2
#define PADG_DOWN 4
#define PADG_UP 8
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define MPROG_FREEZE 0x100
struct { u32 gamePressed,gameRepeat,gameHeld; } gPad[2];
struct Option { int state,mcBusy,cursor,voiceLine,greeted,subtitles,resetStep,bgmTop,bgmBottom,bgmExtra,bgmCursor,bgmCount,value,pad,keyOld,keyRow,keyCursor; } option,*gOption=&option;
struct { unsigned flags; } progress,*gProgress=&progress;
struct { int bgmVolume,seVolume,keyEdit[2][8]; } save,*gSaveData=&save;
static int selected=-1,opened=1,steps,reads,fading;
int ColorFade_IsFadingOut(void) { return fading; }
static const char *target="";
#include "menu/port_menu_mouse.h"
#include "menu/port_video_options.h"
static PortMenuMouse input;
static int portVideoOpened,sOptionPcReset,zenkai,pcMouseCalls;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Option_PortTopMouse(PortMenuMouse *mouse) { return -1; }
int Option_PortHit(PortMenuMouse *mouse,float y0,float y1) { return 0; }
void Option_PortVideoMouse(PortMenuMouse *mouse) { portVideoOpened++; }
void Option_PcMouse(PortMenuMouse *) { pcMouseCalls++; }
#define OPT_ITEM_CTRL 4
#define PORT_PC_OPTIONS_PAGE 27
void Option_PortPickerMouse(PortMenuMouse *mouse) { portVideoOpened++; }
static int held, geometry,hovered;
static float thumbU,thumbV;
int PortMenuMouse_LeftHeld(void) { return held; }
int Port_FlashMenuMousePosition(int scope,const char *name,float w,float h,float pivot,float x,float y,float *u,float *v) {
    *u=thumbU;*v=thumbV;return geometry;
}
void PortMenuMouse_Read(PortMenuMouse *out) { *out=input; reads++; }
void PortMenuMouse_SetHover(int value) { if(value) hovered=1; }
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) { int hit=!strcmp(name,target);if(hit) PortMenuMouse_SetHover(1);return hit; }
void Port_DialogMouseSelect(int choice) { selected=choice; }
int Dialog_IsOpen(void) { return opened; }
void Option_PlateGoto(int unused,int clip,const char *label) {}
void Option_SetKeyMark(int row) {}
void Voice_PlayWithSubtitle(int subtitles,int base,int line) {}
void Snd_PlaySe(int mask,int id) {}
int Option_Input(void) {
    steps++;
    if(gOption->state==13) {
        if(gPad[0].gamePressed&PADG_UP) gOption->value=0;
        if(gPad[0].gamePressed&PADG_DOWN) gOption->value=1;
        if(gPad[0].gameRepeat&PADG_RIGHT) { if(gOption->value) save.seVolume++;else save.bgmVolume++; }
        if(gPad[0].gameRepeat&PADG_LEFT) { if(gOption->value) save.seVolume--;else save.bgmVolume--; }
    } else if(gPad[0].gamePressed&PADG_RIGHT) gOption->value^=1;
    return 1;
}
#include "menu/port_selection_mouse.h"
#define OPT_PLATE(kind,label) Option_PlateGoto(0,kind,label)
'''+constants+'\n'+helpers+r'''
static void pointer(int state,const char *name,int click) {
    memset(&input,0,sizeof(input));memset(gPad,0,sizeof(gPad));
    input.moved=1;input.confirm=click;input.x=256;input.y=200;
    hovered=0;option.state=state;target=name;
}
int main(void) {
    pointer(0,"mc_menu_plate_3",0);Option_MouseInput();
    assert(option.cursor==3 && option.voiceLine==3 && !gPad[0].gamePressed);
    pointer(0,"mc_bottom_plate_1",1);Option_MouseInput();assert(option.cursor==5 && (gPad[0].gamePressed&PADG_CROSS));
    pointer(2,"mc_menu_plate_2",1);Option_MouseInput();assert(option.cursor==8);
    pointer(3,"mc_menu_plate_3",0);Option_MouseInput();assert(option.cursor==14 && option.voiceLine==17);
    pointer(4,"mc_menu_plate_2",0);Option_MouseInput();assert(option.cursor==18 && option.voiceLine==24);
    pointer(1,"mc_select_plate_2",1);option.value=0;Option_MouseInput();assert(option.value==1 && (gPad[0].gamePressed&PADG_CROSS));
    pointer(13,"mc_vol_plate_se_7",0);option.value=0;save.seVolume=2;Option_MouseInput();assert(option.value==1 && save.seVolume==2);
    pointer(13,"mc_vol_plate_se_7",1);Option_MouseInput();assert(save.seVolume==7 && (gPad[0].gamePressed&PADG_CROSS));
    pointer(14,"mc_bgm_plate_3",0);option.bgmTop=6;option.bgmCount=20;Option_MouseInput();assert(option.bgmCursor==8 && !gPad[0].gamePressed);
    pointer(14,"mc_bgm_plate_3",1);Option_MouseInput();assert(gPad[0].gamePressed&PADG_CROSS);
    pointer(14,"mc_bgm_plate_1",1);input.y=40;Option_MouseInput();assert(!gPad[0].gamePressed);
    pointer(14,"mc_yajirusi_down",1);Option_MouseInput();assert(gPad[0].gameRepeat&PADG_DOWN);
    pointer(14,"mc_yajirusi_up",0);Option_MouseInput();assert(hovered && !gPad[0].gameRepeat && !gPad[0].gamePressed);
    pointer(14,"mc_yajirusi_down",0);Option_MouseInput();assert(hovered && !gPad[0].gameRepeat);
    pointer(13,"mc_yajirusi_left",0);Option_MouseInput();assert(hovered && !gPad[0].gameRepeat);
    pointer(13,"mc_yajirusi_right",0);Option_MouseInput();assert(hovered && !gPad[0].gameRepeat);
    pointer(7,"mc_yajirusi_left",0);Option_MouseInput();assert(hovered && !gPad[0].gamePressed);
    pointer(7,"mc_yajirusi_right",0);Option_MouseInput();assert(hovered && !gPad[0].gamePressed);
    pointer(7,"mc_yajirusi_left",1);Option_MouseInput();assert(gPad[0].gamePressed&PADG_LEFT);
    pointer(18,"mc_select_plate_2",1);option.value=0;Option_MouseInput();assert(option.value==1 && (gPad[0].gamePressed&PADG_DOWN));
    pointer(21,"mc_select_plate_4",0);option.value=0;Option_MouseInput();assert(option.value==0);
    pointer(21,"mc_select_plate_4",1);Option_MouseInput();assert(option.value==1 && (gPad[0].gamePressed&PADG_CROSS));
    pointer(22,"mc_button_text_6",1);option.keyOld=-1;Option_MouseInput();assert(option.keyRow==0 && option.keyCursor==6 && (gPad[0].gameHeld&PADG_CROSS) && (gPad[0].gamePressed&PADG_RIGHT));
    pointer(10,"mc_menu_plate_2",1);option.resetStep=1;Option_MouseInput();assert(selected==1 && (gPad[0].gamePressed&PADG_CROSS));
    pointer(10,"mc_menu_plate_1",1);opened=0;Option_MouseInput();assert(!gPad[0].gamePressed);opened=1;
    zenkai=1;sOptionPcReset=1;
    pointer(PORT_PC_OPTIONS_PAGE,"mc_menu_plate_1",1);option.resetStep=1;
    Option_MouseInput();assert(selected==0 && !pcMouseCalls && (gPad[0].gamePressed&PADG_CROSS));
    pointer(PORT_PC_OPTIONS_PAGE,"",0);input.cancel=1;Option_MouseInput();
    assert(gPad[0].gamePressed&PADG_TRIANGLE);
    sOptionPcReset=0;pointer(PORT_PC_OPTIONS_PAGE,"",0);Option_MouseInput();assert(pcMouseCalls==1);
    zenkai=0;
    pointer(20,"",0);input.cancel=1;Option_MouseInput();assert(gPad[0].gamePressed&PADG_UP);
    pointer(9,"",1);input.x=20;Option_MouseInput();assert(gPad[0].gameRepeat&PADG_LEFT);
    pointer(9,"",1);Option_MouseInput();assert(gPad[0].gamePressed&PADG_CROSS);
    pointer(0,"mc_menu_plate_1",1);option.mcBusy=1;int old=reads;Option_MouseInput();assert(reads==old && !gPad[0].gamePressed);option.mcBusy=0;
    progress.flags=MPROG_FREEZE;Option_MouseInput();assert(reads==old);progress.flags=0;
    fading=1;Option_MouseInput();assert(reads==old);fading=0;
    /* Grab without jumping, then drag while keeping the selected visible row. */
    pointer(14,"",1);geometry=held=1;thumbU=thumbV=0.5f;
    option.bgmTop=3;option.bgmCursor=5;option.bgmCount=20;
    Option_MouseInput();assert(sOptionMouseDragging && option.bgmTop==3 && !gPad[0].gamePressed);
    input.confirm=0;thumbV=1.5f;Option_MouseInput();
    assert(option.bgmTop==9 && option.bgmCursor==11 && option.bgmBottom==15 && option.bgmExtra==9);
    /* Drag remains captured outside the thumb and clamps at either end. */
    thumbU=4;thumbV=100;Option_MouseInput();assert(option.bgmTop==14 && option.bgmCursor==16);
    thumbV=-100;Option_MouseInput();assert(option.bgmTop==0 && option.bgmCursor==2);
    held=0;thumbV=10;Option_MouseInput();assert(!sOptionMouseDragging && option.bgmTop==0);
    pointer(14,"",1);held=1;thumbU=thumbV=0.5f;option.bgmCount=6;
    Option_MouseInput();assert(!sOptionMouseDragging && option.bgmTop==0);
    option.bgmCount=20;Option_MouseInput();assert(sOptionMouseDragging);
    geometry=0;Option_MouseInput();assert(!sOptionMouseDragging);
    geometry=1;Option_MouseInput();assert(sOptionMouseDragging);
    pointer(3,"",0);Option_MouseInput();assert(!sOptionMouseDragging);
    old=reads;
    sOptionMouseStepping=1;Option_MouseInput();assert(reads==old);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 src=Path(tmp)/'options.c';exe=Path(tmp)/'options';src.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=undefined','-I'+str(root/'include'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Options mouse routing checks passed')
