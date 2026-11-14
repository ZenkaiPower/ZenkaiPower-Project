#!/usr/bin/env python3
"""Exercise production Training/Tournament mouse adapters without building the game."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
def handler(file,name):
    source=(root/('src/menu/'+file+'.c')).read_text()
    start=source.index('static void '+name+'_MouseInput(void) {')
    return source[start:source.index('\n}\n',start)+3]
common=r'''

#undef Snd_PlaySe
#include "common.h"
#include "sys/pad.h"
#include "menu/port_ultimate_mouse.h"
extern void abort(void);
extern int strcmp(const char *, const char *);
#define CHECK(x) do { if (!(x)) abort(); } while (0)
Pad gPad[PAD_COUNT];
static union { MenuProgress menu; u8 bytes[8192]; } progressStorage;
#define progress progressStorage.menu
MenuProgress *gProgress=&progress;
static PortMenuMouse input;
static const char *target;
static int ready=1, sounds, highlights, images, voices, selected=-1;
static int fading, faded=1, scope;
void PortMenuMouse_Read(PortMenuMouse *out) { *out=input; if (!ready) memset(out,0,sizeof(*out)); }
void PortMenuMouse_Ready(int on) { ready=on; }
void Port_FlashMenuMouseBegin(void) {}
s32 ColorFade_IsInDone(void) { return faded; }
s32 ColorFade_IsFadingOut(void) { return fading; }
int Port_FlashMenuMouseHit(int screen,const char *name,float w,float h,float pivot,float x,float y) {
    CHECK(screen == scope || screen == 4);
    return target && strcmp(target,name)==0;
}
s32 Snd_PlaySe(u32 a,s32 b) { sounds++; return 0; }
void Voice_PlayWithSubtitle(void *a,s32 b,s32 c) { voices++; }
void Port_DialogMouseSelect(int n) { selected=n; }
void Port_ItemPanelMouseInput(s32 side,s32 pad,PortMenuMouse *mouse) { selected=9; }
static void clear(void) { memset(gPad,0,sizeof(gPad)); memset(&input,0,sizeof(input)); target=0; }
'''
cases=[]
def case(header,file,name,setup,checks):
    cases.append((name,'#include "menu/'+header+'.h"\n'+common+setup+handler(file,name)+'\nint main(void) {\n'+checks+'\nreturn 0;\n}\n'))
case('menu_i','menu_h_d','Train',r'''
Train menu,*gTrain=&menu;
void Train_CursorGoto(s32 on) { highlights++; }
void Train_Plate2Goto(s32 row,s32 on) { highlights++; }
static inline void Train_Say(s32 line) { menu.voiceLine=line;voices++; }
''',r'''
scope=60;input.moved=1;target="mc_menu_plate_2";
Train_MouseInput();CHECK(menu.sel[0]==1 && voices==1 && highlights==2);
Train_MouseInput();CHECK(voices==1);
clear();menu.level=TRAIN_LV_CLASS;input.confirm=1;target="mc_menu_plate_3";
Train_MouseInput();CHECK(menu.sel[1]==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.level=TRAIN_LV_LESSON;menu.sel[1]=0;menu.count[0]=4;menu.top=2;
input.moved=1;target="mc_menu_plate_2_2";Train_MouseInput();CHECK(menu.row==1);
clear();input.confirm=1;target="mc_menu_plate_2_3";Train_MouseInput();CHECK(!gPad[0].gamePressed);
clear();input.wheel=1;input.moved=1;target="mc_menu_plate_2_1";
Train_MouseInput();CHECK(menu.row==1 && (gPad[0].gameRepeat&PADG_DOWN));
clear();input.confirm=1;target="mc_yajirusi_up";Train_MouseInput();CHECK((gPad[0].gameRepeat&PADG_UP) && !gPad[0].gamePressed);
clear();menu.level=TRAIN_LV_PAGES;input.confirm=1;Train_MouseInput();CHECK(gPad[0].gamePressed&PADG_CROSS);
clear();input.cancel=1;Train_MouseInput();CHECK(gPad[0].gamePressed&PADG_TRIANGLE);
progress.flags=MPROG_FREEZE;Port_UbMouseBegin(1);clear();input.confirm=1;Train_MouseInput();CHECK(!gPad[0].gamePressed);
''')
case('menu_j','menu_j_b','TourMenu',r'''
TourMenu menu,*gTourMenu=&menu;
#define TM_CUR (gTourMenu->cursor[gTourMenu->level])
static s32 sPortTourMouseDialogue;
void TourMenu_ClipGoto(s32 movie,s32 kind,char *label) { highlights++; }
''',r'''
scope=31;input.moved=1;target="mc_menu_plate_02";TourMenu_MouseInput();CHECK(menu.cursor[0]==1 && highlights==2);
clear();menu.level=TOURMENU_LV_TOUR;input.confirm=1;target="mc_taikai_plate_05";
TourMenu_MouseInput();CHECK(menu.cursor[1]==4 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.level=TOURMENU_LV_LEVEL;input.moved=1;target="mc_dragonball_off_03";
TourMenu_MouseInput();CHECK(menu.cursor[3]==2);
clear();input.confirm=1;target="mc_dragonball_off_01";TourMenu_MouseInput();
CHECK(menu.cursor[3]==0 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;target="mc_dragonball_off_03";TourMenu_MouseInput();
CHECK(menu.cursor[3]==2 && (gPad[0].gamePressed&PADG_CROSS));
menu.cursor[0]=0;target="mc_dragonball_off_01";TourMenu_MouseInput();CHECK(menu.cursor[3]==2); /* locked native difficulty */
clear();menu.level=TOURMENU_LV_NUM;input.wheel=1;TourMenu_MouseInput();CHECK(gPad[0].gameRepeat&PADG_RIGHT);
clear();menu.cursor[0]=1;input.confirm=1;target="mc_yajirusi_l";TourMenu_MouseInput();CHECK((gPad[0].gameRepeat&PADG_LEFT) && !gPad[0].gamePressed);
clear();input.confirm=1;menu.seq=1;TourMenu_MouseInput();CHECK(sPortTourMouseDialogue && (gPad[0].gamePressed&PADG_CROSS));
''')
case('menu_j','menu_i_d','EntrySel',r'''
ESel menu,*gEntrySel=&menu;ESelState state;ESelCell cells[14];
#define ES_CUR (gEntrySel->sel->entry[gEntrySel->sel->cur])
#define ES_CELL (gEntrySel->grid[ES_CUR.row*ESEL_COLS+ES_CUR.col])
static int locked=-1,changed;
s32 ChrGrid_IsSelectable(ESelCell *grid,s32 i) { return i!=locked; }
void EntrySel_ClipGoto(s32 movie,s32 kind,char *label) { highlights++; }
void EntrySel_ChangeImage(void) { changed++; }
''',r'''
scope=23;menu.sel=&state;menu.grid=cells;state.rowChara[2]=42;
input.moved=1;target="mc_chara_chip_002";EntrySel_MouseInput();CHECK(ES_CUR.col==2 && state.image==42 && changed==1);
locked=3;target="mc_chara_chip_003";input.confirm=1;EntrySel_MouseInput();CHECK(ES_CUR.col==2 && !gPad[0].gamePressed);
clear();state.step=ESEL_STEP_FORM;state.rowChara[1]=ESEL_ID_NONE;input.confirm=1;target="mc_chara_chip_001";
EntrySel_MouseInput();CHECK(!gPad[0].gamePressed);
clear();state.step=ESEL_STEP_CUSTOM;menu.customCount=4;input.confirm=1;target="mc_custom_plate_3/mc_yajirusi";
EntrySel_MouseInput();CHECK(ES_CUR.custom==2 && (gPad[0].gamePressed&PADG_RIGHT) && !(gPad[0].gamePressed&PADG_CROSS));
clear();state.step=ESEL_STEP_COLOR;menu.colorCount=3;input.confirm=1;target="mc_color_plate_2";
EntrySel_MouseInput();CHECK(ES_CUR.costume==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();state.step=ESEL_STEP_PANEL;input.cancel=1;EntrySel_MouseInput();CHECK(selected==9 && (gPad[0].gamePressed&PADG_TRIANGLE));
clear();state.step=ESEL_STEP_CHARA;TOUR_PROG->t.tour=TOUR_YAMCHA;input.confirm=1;
EntrySel_MouseInput();CHECK(gPad[0].gamePressed&PADG_CROSS); /* confirms native random choice */
''')
case('menu_k','menu_k_b','Bracket',r'''
Bracket menu,*gBracket=&menu;
static s32 sPortBracketMouseDialogue;
static int leftHeld;
int PortMenuMouse_LeftHeld(void) { return leftHeld; }
''',r'''
scope=33;menu.flags=BRK_INPUT;input.wheel=1;Bracket_MouseInput();CHECK(menu.scroll==-64);
clear();input.confirm=1;target="mc_yajirusi_r";Bracket_MouseInput();
CHECK(menu.scroll==-112 && !gPad[0].gamePressed && sounds==1);
clear();leftHeld=1;target="mc_yajirusi_r";Bracket_MouseInput();CHECK(menu.scroll==-115 && !gPad[0].gamePressed);
leftHeld=0;clear();input.confirm=1;target="mc_yajirusi_l";Bracket_MouseInput();CHECK(menu.scroll==-67 && !gPad[0].gamePressed);
clear();input.wheel=1;
menu.scroll=-500;Bracket_MouseInput();CHECK(menu.scroll==-512);
clear();input.wheel=-1;menu.scroll=-5;Bracket_MouseInput();CHECK(menu.scroll==0);
clear();input.confirm=1;Bracket_MouseInput();CHECK((gPad[0].gamePressed&PADG_CROSS) && !sPortBracketMouseDialogue);
clear();menu.flags=0;input.confirm=1;Bracket_MouseInput();CHECK(sPortBracketMouseDialogue && (gPad[0].gamePressed&PADG_CROSS));
''')
with tempfile.TemporaryDirectory() as folder:
    for name,source in cases:
        path=Path(folder)/(name+'.c');path.write_text(source);exe=path.with_suffix('')
        subprocess.run(['cc','-std=gnu99','-DPORT','-fno-builtin','-Iinclude','-Iport/include','-include','port_compat.h',str(path),'-o',str(exe)],cwd=root,check=True)
        subprocess.run([str(exe)],check=True)
print('Training/Tournament mouse checks passed (4 adapters)')
