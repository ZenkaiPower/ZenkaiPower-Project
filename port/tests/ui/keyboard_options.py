#!/usr/bin/env python3
"""Exercise the native Controller entry, scrolling binding rows and capture lock."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
menu = (root / 'src/menu/menu_x_c.c').read_text()
# Derive the page from the native transition, rather than duplicating its value.
entry = menu.index('} else if (gOption->state == OPT_ITEM_CTRL) {')
page = int(re.search(r'gOption->page = (\d+);', menu[entry:]).group(1))
start = menu.index('Port_FlashOptionsViewport(Port_ZenkaiEnabled()')
viewport_call = menu[start:menu.index(';', start) + 1]
source = '#define NATIVE_CONTROLLER_PAGE ' + str(page) + '\n' + r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned u32;typedef int s32;typedef float f32;typedef unsigned short u16;
#include "menu/port_pc_bindings.h"
#include "menu/port_video_options.h"
#define OPT_ITEM_CTRL 4
#define OPT_ITEM_KEYS 17
#define OPT_ITEM_VIB 18
#define OPT_ITEM_CTRL_RESET 19
#define OPT_CLIP_BOTTOM 3
#define OPT_CLIP_ROW_CTRL 6
#define PADG_UP 8
#define PADG_DOWN 4
#define PADG_LEFT 1
#define PADG_RIGHT 2
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define PS_PLAIN 0
typedef struct { float x,y;int moved,confirm,cancel,wheel; } PortMenuMouse;
typedef struct {} MFlashRef;
typedef struct { int x0,y0,x1,y1; } MFlashUv;
static struct { int gamePressed,gameRepeat; } gPad[1];
static struct { int state,cursor,dirty,page,voiceLine,resetStep;void *res,*dialogMsg;void *flash; } option,*gOption=&option;
static int sOptionPcGamepad,gamepadCapture=-1,pcDefaults,padDefaults;
extern "C" void PortInput_PcDefaults(void) { pcDefaults++; }
extern "C" void PortInput_GamepadDefaults(void) { padDefaults++; }
static int unbound,captionKind,spriteCount,lastAlpha;
static u32 lastTint;
static const char *captionText;
static float rowSlide[4],captionSlide,focusY,lastY;
static int zenkai=1,lastSound=-1,capture=-1,result,draws,footer,focuses,chevrons;
int Port_ZenkaiEnabled(void) { return zenkai; }
static int viewport;
void Port_FlashOptionsViewport(int mode) { viewport=mode; }
void Snd_PlaySe(int a,int b) { lastSound=b; }
void Flash_GotoLabel(void *m,const char *label,int restart) { assert(!strcmp(label,"fl_menu_change")); }
extern "C" void PortInput_PcCaptureStart(int action) { capture=action; }
extern "C" void PortInput_GamepadCaptureStart(int action) { gamepadCapture=action; }
extern "C" void PortInput_GamepadBindingName(int action,char *out,int size) { snprintf(out,size,"Left Trigger"); }
extern "C" int PortInput_PcCapturePoll(void) { int r=result;result=0;return r; }
extern "C" void PortInput_PcBindingName(int action,char *out,int size) { snprintf(out,size,"%s",unbound ? "Unbound":"Left Shift"); }
int Port_FlashMenuMouseHit(int scope,const char *name,int w,int h,int pivot,float x,float y) { return y>=280 && !strcmp(name,gOption->state==PORT_PC_OPTIONS_PAGE ? "mc_video_default":"mc_bottom_plate_1"); }
int Option_PortHit(PortMenuMouse *m,float y0,float y1) { return !m->wheel && (m->moved || m->confirm) && m->y>=y0 && m->y<y1; }
void Port_FlashOptionsVideoPicker(int row) { assert(row==-1); }
void Port_FlashOptionsVideoFooter(int mode) { footer=mode; }
void Option_SetBottomText(void *,MFlashRef *,const char *,MFlashUv uv) { assert(uv.x1==256); }
static int voiceCount,lastVoice,plateChanges,sOptionPlateSelected[4],sOptionPlateMovies[4];
#define OPT_VOICE(n) (voiceCount++,lastVoice=(n))
void Option_PlateGoto(int,int,const char *) { plateChanges++; }
void Flash_FindLabel(int *,void *,const char *,MFlashRef *) {}
void Flash_ClipGotoLabel(int *,MFlashRef *,const char *) {}
void Flash_Advance(int *) {}
void Port_FlashOptionsControllerPlate(void *,void *) { draws++;focuses+=sOptionPlateSelected[2]>0; }
float Port_FlashOptionsRowSlide(void *,int row) { return rowSlide[row]; }
void Port_FlashOptionsGamepadPlate(void *,void *) { draws++;focuses+=sOptionPlateSelected[3]>0; }
void Option_PortArtInit(void) {}
void Ps_SetShift(int) {}
int Port_PageAspectMilli(void) { return 1333; }
int Port_TitleMainChars43(void) { return 1; }
int Port_Menus43(void) { return 0; }
void PortMenuMouse_SetHover(int) {}
void Port_FlashOptionsChevron(void *,int down,float x,float y) {
    assert(x==272 && y==(down ? 252:64));chevrons|=1<<down;
}
void Option_PortButton(int index,void *,float x,float y,char *label,int selected) {
    assert(index>=0 && index<4);assert(x==92-20*index+rowSlide[index]);captionSlide=rowSlide[index];assert(y==72+44*index);draws++;focuses+=selected>0;
}
int PortTbo_TextId(const char *text,int kind) { captionText=text;captionKind=kind;return 1; }
void Port_FlashOptionsCaptionMotion(int layer,float *x,float *y) { *x=0;*y=layer ? focusY:0; }
static int focusAlpha=96;
int Port_FlashOptionsCaptionStyle(int layer,u32 *tint) { *tint=0x808080;return layer ? focusAlpha:128; }
int PortTbo_TextInfo(int,int what) { return what==7 ? 10000 : what==8 ? 1000 : 0; }
void Port_GsMarker(int) {}
void Ps_SpriteScreen(int,float x0,float y0,float x1,float,float,float,float,float,int tint,int alpha) {
    spriteCount++;lastY=y0;lastTint=tint;lastAlpha=alpha;
    assert(x0>=363+captionSlide && x1>x0); /* long binding captions fit the value column */
}
int MsgWin_PortLineRoom(int) { return 400; }
void Port_MsgWinDrawString43(u16 *) {}
void Option_PortWrapHelp(const char *,u16 *,int,int) {}
void Option_UpdateReset(void);
#include "menu/port_keyboard_options.h"
#define PORT 1
#define MPACK_AT(a,b) 0
#define YOPT_SCR_RESET 10
#define YOPT_ADJUST_RESET 11
#define YOPT_SND_RESET 16
#define YOPT_CTRL_RESET 19
#define YOPT_SCREEN 2
#define YOPT_ADJUST 14
#define YOPT_SOUND 3
#define YOPT_CTRL 4
#define SAVE_FLAG_VOICE 32
static struct { int screenX,screenY,unk1694,unk1698,soundMode,bgmVolume,seVolume,flags,keyEdit[2][8]; } save,*gSaveData=&save;
static int dialogAnswer,dialogStarts,dialogCursor;
void Dialog_Init(int,void *,int) {}
void Dialog_SetCursor(int cursor) { dialogCursor=cursor; }
void Dialog_SetMsgTable(void *) {}
void Dialog_SetMsg(int) {}
void Dialog_SetChoices(int) {}
void Dialog_Start(int closing) { dialogStarts++; }
int Dialog_Input(int) { int answer=dialogAnswer;dialogAnswer=0;return answer; }
int Dialog_IsClosed(void) { return 1; }
void SndOpt_Apply(void) {}
void Option_PortDefaults(void) { assert(0); }

''' + menu[menu.index('void Option_UpdateReset(void) {'):menu.index('/* Gives the two text clips', menu.index('void Option_UpdateReset(void) {'))] + r'''
''' + 'static void prepare_viewport(void) {' + viewport_call + '}\n' + r'''
static int key(int repeat,int pressed) {
    gPad[0].gameRepeat=repeat;gPad[0].gamePressed=pressed;return Option_PcInput();
}
int main(void) {
    assert(!strcmp(sPortPcLabels[15],"Switch Character") && !strcmp(sPortPcLabels[16],"Transform/Progress"));
    option.state=OPT_ITEM_CTRL;option.page=NATIVE_CONTROLLER_PAGE;option.cursor=OPT_ITEM_KEYS;
    assert(PORT_PC_CONTROLLER_PAGE==NATIVE_CONTROLLER_PAGE);
    prepare_viewport();assert(viewport==2); /* native rows must remain visible */
    assert(key(0,PADG_CROSS)==0); /* original Controller Type picker */
    key(PADG_DOWN,0);assert(option.cursor==OPT_ITEM_VIB && !key(0,PADG_CROSS) && lastVoice==24 && voiceCount==1 && plateChanges==2);
    key(PADG_DOWN,0);assert(option.cursor==PORT_PC_CONTROLLER_ENTRY);
    assert(Option_PcHelp() && Option_PcDraw() && draws==2 && focuses==1);
    key(0,PADG_CROSS);assert(option.state==PORT_PC_OPTIONS_PAGE);
    prepare_viewport();assert(viewport==1); /* binding rows replace native list */
    draws=focuses=0;assert(Option_PcDraw() && draws==4 && focuses==1);
    for(int frame=0;frame<12;frame++) {
        for(int row=0;row<4;row++) rowSlide[row]=(12-frame)*(35+row*2);
        chevrons=0;
        assert(Option_PcDraw()); /* captions and action plates move together */
        assert(chevrons==2); /* scrolling remains visible with hidden native rows moving */
    }
    for(int row=0;row<4;row++) rowSlide[row]=0;
    unbound=1;spriteCount=0;Option_PcDraw();
    assert(spriteCount==8 && lastTint==0x585858 && lastAlpha==96 && captionKind==4); /* focus color stays dimmed */
    unbound=0;sOptionPcRow=3;sOptionPcWaiting=1;spriteCount=0;Option_PcDraw();
    assert(spriteCount==8 && !strcmp(captionText,"...") && captionKind==4 && lastAlpha==96);
    float originalY=lastY;focusY=-4;focusAlpha=32;Option_PcDraw();assert(lastAlpha==32 && lastY==originalY-4); /* dots sample the native pulse every frame */
    sOptionPcWaiting=0;sOptionPcRow=0;
    assert(Option_PcFooter() && footer==1);
    { PortMenuMouse arrow={272,252,1,1,0,0};
      gPad[0].gamePressed=gPad[0].gameRepeat=0;Option_PcMouse(&arrow);Option_PcInput();
      assert(sOptionPcFirst==1 && sOptionPcRow==4);
      arrow.y=64;gPad[0].gamePressed=gPad[0].gameRepeat=0;Option_PcMouse(&arrow);Option_PcInput();
      assert(sOptionPcFirst==0 && sOptionPcRow==0);
    }
    { PortMenuMouse m={400,132,1,1,0,0};
      gPad[0].gamePressed=gPad[0].gameRepeat=0;Option_PcMouse(&m);Option_PcInput();
      assert(capture==1 && sOptionPcWaiting);
      key(PADG_DOWN,0);assert(sOptionPcRow==1 && sOptionPcWaiting);
      m.y=220;Option_PcMouse(&m);assert(sOptionPcRow==1);
      result=-1;key(0,0);assert(!sOptionPcWaiting && !option.dirty && lastSound==2);
      key(0,PADG_CROSS);assert(capture==1);
      result=1;key(0,0);assert(!sOptionPcWaiting && option.dirty && lastSound==1);
    }
    for(int i=1;i<PORT_PC_ACTIONS;i++) key(PADG_DOWN,0);
    assert(sOptionPcRow==PORT_PC_ACTIONS && sOptionPcFirst==PORT_PC_ACTIONS-4);
    key(0,PADG_CROSS);assert(option.state==PORT_PC_OPTIONS_PAGE && sOptionPcReset && pcDefaults==0 && padDefaults==0 && lastSound==1);
    key(0,0);assert(dialogCursor==1 && option.resetStep==1 && !pcDefaults);
    key(PADG_DOWN,0);assert(sOptionPcRow==PORT_PC_ACTIONS);
    dialogAnswer=-1;key(0,0);key(0,0);key(0,0);assert(!sOptionPcReset && !pcDefaults);
    key(0,PADG_CROSS);key(0,0);dialogAnswer=1;key(0,0);key(0,0);
    assert(pcDefaults==1 && !padDefaults && option.dirty);
    key(0,0);key(0,0);assert(!sOptionPcReset);
    key(0,PADG_TRIANGLE);assert(option.state==OPT_ITEM_CTRL && option.cursor==PORT_PC_CONTROLLER_ENTRY);
    key(PADG_DOWN,0);assert(option.cursor==PORT_GAMEPAD_CONTROLLER_ENTRY);
    key(0,PADG_CROSS);assert(option.state==PORT_PC_OPTIONS_PAGE && sOptionPcGamepad);
    key(0,PADG_CROSS);assert(gamepadCapture==0 && sOptionPcWaiting);
    result=-1;key(0,0);assert(!sOptionPcWaiting);
    sOptionPcRow=PORT_PC_ACTIONS;key(0,PADG_CROSS);
    assert(sOptionPcReset && padDefaults==0 && pcDefaults==1 && option.state==PORT_PC_OPTIONS_PAGE && lastSound==1);
    key(0,0);dialogAnswer=1;key(0,0);key(0,0);key(0,0);key(0,0);
    assert(!sOptionPcReset && padDefaults==1 && pcDefaults==1);
    key(0,PADG_TRIANGLE);assert(option.cursor==PORT_GAMEPAD_CONTROLLER_ENTRY);
    key(PADG_DOWN,0);assert(option.cursor==OPT_ITEM_CTRL_RESET && lastVoice==10);
    key(PADG_DOWN,0);assert(option.cursor==OPT_ITEM_KEYS && lastVoice==23);
    option.cursor=PORT_PC_CONTROLLER_ENTRY;key(0,PADG_CROSS);
    key(0,PADG_TRIANGLE);assert(option.state==OPT_ITEM_CTRL && lastSound==2);
    option.cursor=PORT_PC_CONTROLLER_ENTRY;key(0,PADG_CROSS);
    { PortMenuMouse defaults={400,300,1,0,0,0};
      gPad[0].gamePressed=gPad[0].gameRepeat=0;lastSound=-1;
      Option_PcMouse(&defaults);assert(lastSound==0 && sOptionPcRow==PORT_PC_ACTIONS);
      lastSound=-1;Option_PcMouse(&defaults);assert(lastSound==-1); /* no repeated hover sound */
      defaults.confirm=1;Option_PcMouse(&defaults);Option_PcInput();
      assert(option.state==PORT_PC_OPTIONS_PAGE && sOptionPcReset && pcDefaults==1 && padDefaults==1 && lastSound==1);
      key(0,0);dialogAnswer=-1;key(0,0);key(0,0);key(0,0);
      assert(!sOptionPcReset && pcDefaults==1);
      defaults.confirm=0;defaults.cancel=1;Option_PcMouse(&defaults);Option_PcInput();
      assert(option.state==OPT_ITEM_CTRL && lastSound==2);
    }
    zenkai=0;option.state=OPT_ITEM_CTRL;prepare_viewport();assert(!viewport);
    assert(!key(PADG_DOWN,PADG_CROSS) && !Option_PcDraw() && !Option_PcHelp());
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'keyboard.cpp'
    exe = Path(tmp) / 'keyboard'
    src.write_text(source)
    subprocess.run(['c++', '-std=c++17', '-Wno-write-strings', '-fsanitize=undefined',
                    '-I' + str(root / 'include'), str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Native Mouse/Keyboard menu checks passed')
