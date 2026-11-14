#!/usr/bin/env python3
"""Exercise remaining menu mouse adapters without rebuilding the game."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
def extract(file,name):
    source=(root/file).read_text()
    start=source.index('static void '+name+'(')
    return source[start:source.index('\n}\n',start)+3]
def character_reference_helpers():
    source=(root/'src/menu/menu_h_b.c').read_text()
    start=source.index('static s32 sCharRefMouseDragging;')
    return source[start:source.index('static void CharRef_MouseInput(void)',start)]
common=r'''


#undef Snd_PlaySe
#include "common.h"
#include "sys/pad.h"
extern void *memset(void *,s32,u32);
extern s32 sprintf(char *,const char *,...);
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
static int fading, faded=1, scope, reads;
void PortMenuMouse_Read(PortMenuMouse *out) { reads++; *out=input; if (!ready) memset(out,0,sizeof(*out)); }
void PortMenuMouse_Ready(int on) { ready=on; }
void PortMenuMouse_SetHover(int on) {}
void Port_FlashMenuMouseBegin(void) {}
s32 ColorFade_IsInDone(void) { return faded; }
s32 ColorFade_IsFadingOut(void) { return fading; }
int Port_FlashMenuMouseHit(int screen,const char *name,float w,float h,float pivot,float x,float y) {
    if(screen!=scope) return 0;
    return target && strcmp(target,name)==0;
}
s32 Snd_PlaySe(u32 a,s32 b) { sounds++; return 0; }
void Voice_PlayWithSubtitle(void *a,s32 b,s32 c) { voices++; }
void Port_DialogMouseSelect(int n) { selected=n; }
void Port_ItemPanelMouseInput(s32 side,s32 pad,PortMenuMouse *mouse) { selected=9; }
static void clear(void) { memset(gPad,0,sizeof(gPad)); memset(&input,0,sizeof(input)); target=0; }

'''
cases=[]
def case(header,file,name,setup,args,checks):
    cases.append((name,'#include "'+header+'"\n#include "menu/menu_a.h"\n'+common+setup+extract(file,name)+'\nint main(void) {\n'+checks+'\nreturn 0;\n}\n'))

case('menu/menu_h.h','src/menu/menu_h_b.c','CharRef_MouseInput',r"""
CharRef menu,*gCharRef=&menu;
void CharRef_ClipGoto(s32 movie,s32 kind,char *label) { highlights++; }
static int held,thumb;static float thumbU=.5f,thumbV=.5f;
int PortMenuMouse_LeftHeld(void) {return held;}
int Port_FlashMenuMousePosition(int scope,const char *name,float w,float h,float pivot,float x,float y,float *u,float *v) {
    *u=thumbU;*v=thumbV;return thumb;
}
"""+character_reference_helpers(), '',r"""
scope=67;menu.state=CHARREF_ST_LIST;menu.count=10;menu.cursor=5;menu.row=1;
input.moved=1;target="mc_menu_plate2_4";CharRef_MouseInput();CHECK(menu.cursor==7 && menu.row==3);
clear();menu.state=CHARREF_ST_DETAILS;input.confirm=1;target="mc_menu_plate_3";CharRef_MouseInput();CHECK(menu.menuCursor==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;target="mc_yajirusi_right";CharRef_MouseInput();CHECK((gPad[0].gameRepeat&PADG_RIGHT) && !gPad[0].gamePressed);
clear();menu.state=CHARREF_ST_PROFILE;menu.textHeight=300;input.wheel=1;CharRef_MouseInput();CHECK(menu.scroll==24);
clear();menu.state=CHARREF_ST_VIEW;input.cancel=1;CharRef_MouseInput();CHECK((gPad[0].gamePressed&PADG_TRIANGLE) && !(gPad[0].gamePressed&PADG_CROSS));
clear();menu.state=CHARREF_ST_PROFILE;menu.scroll=48;input.confirm=1;target="mc_yajirusi_icon_up";
CharRef_MouseInput();CHECK(menu.scroll==24 && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_yajirusi_icon_down";CharRef_MouseInput();CHECK(menu.scroll==48 && !gPad[0].gamePressed);
clear();menu.state=CHARREF_ST_COSTUME;input.confirm=1;target="mc_yajirusi_num_up";CharRef_MouseInput();CHECK((gPad[0].gamePressed&PADG_UP) && !(gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;target="mc_yajirusi_icon_down";CharRef_MouseInput();CHECK((gPad[0].gamePressed&PADG_DOWN) && !(gPad[0].gamePressed&PADG_CROSS));
clear();menu.state=CHARREF_ST_LIST;menu.count=20;menu.cursor=8;menu.row=1;thumb=held=1;input.confirm=1;
CharRef_MouseInput();CHECK(sCharRefMouseDragging && menu.cursor==8 && !gPad[0].gamePressed);
clear();thumbV=1.5f;CharRef_MouseInput();CHECK(menu.cursor==12 && menu.row==1 && !gPad[0].gamePressed);
clear();thumbV=100;CharRef_MouseInput();CHECK(menu.cursor==17);
clear();held=0;CharRef_MouseInput();CHECK(!sCharRefMouseDragging);

""")
case('menu/menu_x.h','src/menu/menu_w_c.c','EvoTop_MouseInput',r"""
void EvoTop_SetPlate(EvoTop *m,s32 on) {highlights++;}
void EvoTop_PlayVoice(EvoTop *m,s32 n) {voices++;}
EvoTop menu;
""",'',r"""
scope=50;input.confirm=1;target="mc_menu_plate_3";EvoTop_MouseInput(&menu);CHECK(menu.cursor==2 && voices==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;EvoTop_MouseInput(&menu);CHECK(!gPad[0].gamePressed);
""")
case('menu/menu_w.h','src/menu/menu_v_d.c','Shop_MouseInput',r"""
Shop menu,*gShop=&menu;
void Shop_SetPlate(s32 movie,s32 level,s32 tab,char *label) {highlights++;}
""",'',r"""
scope=54;input.confirm=1;target="menu_plate_2";Shop_MouseInput();CHECK(menu.cursor[0]==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();scope=55;menu.level=SHOP_LV_BUY;menu.list[0].rows=4;menu.list[0].count[0]=6;menu.list[0].top[0]=2;
input.confirm=1;target="mc_list_plate_3";Shop_MouseInput();CHECK(menu.list[0].cur[0]==4 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.list[0].count[0]=3;input.confirm=1;target="mc_list_plate_3";Shop_MouseInput();CHECK(!gPad[0].gamePressed);
clear();input.confirm=1;target="mc_tab_btn_1";Shop_MouseInput();CHECK(menu.list[0].tab==1 && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_menu_yajirusi_up";Shop_MouseInput();CHECK((gPad[0].gameRepeat&PADG_UP) && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_menu_yajirusi_down";Shop_MouseInput();CHECK((gPad[0].gameRepeat&PADG_DOWN) && !gPad[0].gamePressed);
clear();scope=56;menu.level=SHOP_LV_ALL;input.confirm=1;target="mc_menu_yajirusi_down";Shop_MouseInput();CHECK((gPad[0].gameRepeat&PADG_DOWN) && !gPad[0].gamePressed);
clear();scope=58;menu.level=SHOP_LV_CONFIRM;input.confirm=1;target="mc_reconfir_plate_3";Shop_MouseInput();CHECK(menu.cursor[4]==2 && (gPad[0].gamePressed&PADG_CROSS));
""")
case('menu/menu_z.h','src/menu/menu_y_b.c','DcList_MouseInput',r"""
void DcView_LightRow(DcView *v,DcChars *list,s32 on) {highlights++;}
void DcView_LightMenu(DcView *v,s32 row,s32 on) {highlights++;}
void DcList_LightItemRow(DcView *v,s32 row,s32 on) {highlights++;}
s32 DcList_IsRowSkipped(u16 *items,s32 row) {return row==3;}
DcList menu;
""",'',r"""
scope=64;menu.state=DCLIST_ST_LIST;input.confirm=1;target="mc_menu_plate2_2";DcList_MouseInput(&menu);CHECK(menu.chars.row==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.state=DCLIST_ST_DIALOG;input.confirm=1;reads=0;DcList_MouseInput(&menu);CHECK(reads==0);
clear();scope=65;menu.state=DCLIST_ST_ITEMS;input.confirm=1;target="mc_list_plate_4";DcList_MouseInput(&menu);CHECK(!gPad[0].gamePressed);
""")
case('menu/menu_v.h','src/menu/menu_v_b.c','EvoZ_MouseInput',r"""
s32 ChrGrid_IsSelectable(VChrCell *cells,s32 index) {return index!=3;}
void EvoZ_ClipGoto(EvoZ *m,s32 movie,s32 kind,s32 sub,char *label) {highlights++;}
void EvoZ_RefreshStatus(EvoZ *m) {images++;}
void EvoZ_RequestFace(EvoZ *m) {images++;}
EvoZ menu;
EvoZSide side;
""",'',r"""
scope=11;menu.side=&side;menu.state=EVOZ_ST_GRID;side.chip[2]=42;input.confirm=1;target="mc_chara_chip_002";
EvoZ_MouseInput(&menu);CHECK(side.col==2 && side.chara==42 && images==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;target="mc_chara_chip_003";EvoZ_MouseInput(&menu);CHECK(side.col==2 && !gPad[0].gamePressed);
clear();scope=51;menu.state=EVOZ_ST_MENU;input.confirm=1;target="menu_plate_2";EvoZ_MouseInput(&menu);CHECK(menu.cur[1]==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();scope=53;menu.state=EVOZ_ST_SLOTS;menu.rowsUsed=2;input.confirm=1;target="mc_list_plate_4";EvoZ_MouseInput(&menu);CHECK(!gPad[0].gamePressed);
clear();input.confirm=1;target="mc_custom_yajirusi/mc_yajirusi_left";EvoZ_MouseInput(&menu);
CHECK((gPad[0].gameRepeat&PADG_LEFT) && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_custom_yajirusi/mc_yajirusi_right";EvoZ_MouseInput(&menu);
CHECK((gPad[0].gameRepeat&PADG_RIGHT) && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_custom_yajirusi";EvoZ_MouseInput(&menu);CHECK(!gPad[0].gameRepeat && !gPad[0].gamePressed);
clear();menu.state=EVOZ_ST_LIST;menu.list.count[0]=20;input.confirm=1;target="mc_menu_yajirusi_down";
EvoZ_MouseInput(&menu);CHECK((gPad[0].gameRepeat&PADG_DOWN) && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_menu_yajirusi_up";
EvoZ_MouseInput(&menu);CHECK((gPad[0].gameRepeat&PADG_UP) && !gPad[0].gamePressed);
clear();scope=51;menu.state=EVOZ_ST_ZP;input.confirm=1;target="mc_num_zp_3/mc_yajirusi_num_up";EvoZ_MouseInput(&menu);
CHECK(menu.cur[5]==3 && (gPad[0].gameRepeat&PADG_UP) && !gPad[0].gamePressed);
clear();input.confirm=1;target="mc_num_zp_3/mc_yajirusi_num_dwn";EvoZ_MouseInput(&menu);
CHECK((gPad[0].gameRepeat&PADG_DOWN) && !gPad[0].gamePressed);
""")

case('battle/hud_0.h','src/battle/hud_0_b.c','BtlMenu_MouseInput',r"""
static int enabled=1,online,me,skills,controls;
int PortMenuMouse_Enabled(void) {return enabled;}
void PortMenuMouse_Enable(int on) { enabled=on; memset(&input,0,sizeof(input)); }
int Port_NetActive(void) {return online;}
int Port_NetMe(void) {return me;}
s32 BtlMenu_GetWidth(BtlMenu *m) {return 200;}
s32 BtlMenu_GetHeight(BtlMenu *m) {return 3*28+20;}
s32 BtlMenu_IsSkillList(BtlMenu *m) {return skills;}
s32 BtlMenu_IsControls(BtlMenu *m) {return controls;}
BtlMenu menu;BtlMenuItem items[3];BtlMenuWork work,*gBtlMenu=&work;
""",'',r"""
menu.items=items;menu.count=3;menu.padY=10;menu.scale=1;menu.state=BTL_MENU_STATE_ACTIVE;
input.confirm=1;input.x=256;input.y=225;BtlMenu_MouseInput(&menu,0);
CHECK(menu.cursor==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();items[2].disabled=1;input.confirm=1;input.x=256;input.y=260;
BtlMenu_MouseInput(&menu,0);CHECK(menu.cursor==1 && !gPad[0].gamePressed);
clear();input.wheel=1;BtlMenu_MouseInput(&menu,1);CHECK(gPad[1].gamePressed&PADG_DOWN);
clear();menu.state=BTL_MENU_STATE_OPENING;input.confirm=1;BtlMenu_MouseInput(&menu,0);CHECK(!gPad[0].gamePressed);
clear();menu.state=BTL_MENU_STATE_ACTIVE;ready=1;input.confirm=1;online=1;me=1;reads=0;
BtlMenu_MouseInput(&menu,0);CHECK(reads==0);
clear();online=0;controls=1;input.cancel=1;BtlMenu_MouseInput(&menu,0);CHECK(gPad[0].gamePressed&PADG_TRIANGLE);
""")
case('menu/menu_z.h','src/menu/menu_z_d.c','DcPass_MouseInput',r"""
void DcPass_GetClipPos(DcKeyPos *key,DcKeyPos *out) {*out=*key;}
void DcPass_LightKey(MFlash *f,DcKeyPos *key,s32 on) {highlights++;}
void DcPass_LightRow(DcPassView *v,DcPassList *list,s32 on) {highlights++;}
DcPass menu;
""",'',r"""
scope=62;menu.state=DCPASS_ST_TYPE;input.confirm=1;target="mc_input_code_2_04";
DcPass_MouseInput(&menu);CHECK(menu.keyPos.row==1 && menu.keyPos.col==3 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.wheel=1;DcPass_MouseInput(&menu);CHECK((gPad[0].gamePressed&PADG_CIRCLE) && !gPad[0].gameRepeat);
clear();scope=63;menu.state=DCPASS_ST_LIST;input.confirm=1;target="mc_menu_plate2_3";
DcPass_MouseInput(&menu);CHECK(menu.list.cursor==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.state=DCPASS_ST_ASK_OVER;input.confirm=1;reads=0;DcPass_MouseInput(&menu);CHECK(reads==0);
""")
# Shenron uses the main executable's ViewProgress layout, rather than MenuProgress.
shen_common=common.replace('#include "menu/port_ultimate_mouse.h"','#include "menu/port_selection_mouse.h"').replace(
    'static union { MenuProgress menu; u8 bytes[8192]; } progressStorage;\n#define progress progressStorage.menu\nMenuProgress *gProgress=&progress;', '')
for name,file,setup,checks in [
    ('Shen_MouseInput','src/sys/late_a.c',r"""
void Shen_SetPlate(Flash *f,s32 row,s32 on) {highlights++;}
ShenWork menu;
""",r"""
scope=69;input.confirm=1;target="mc_menu_plate_3";Shen_MouseInput(&menu);CHECK(menu.cursor==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();input.confirm=1;target="mc_yajirusi_down";Shen_MouseInput(&menu);CHECK((gPad[0].gameRepeat&PADG_DOWN) && !gPad[0].gamePressed);
clear();menu.state=SHEN_STATE_GET;input.confirm=1;Shen_MouseInput(&menu);CHECK(gPad[0].gamePressed&PADG_CROSS);
"""),
    ('ShenCfm_MouseInput','src/sys/late_a_b.c',r"""
ShenCfm menu,*gShenCfm=&menu;
static void ShenCfm_SetPlate(ShenCfm *m,s32 on) {highlights++;}
""",r"""
scope=69;input.confirm=1;target="mc_reconfir_plate_2";ShenCfm_MouseInput();CHECK(menu.cursor==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.detail=1;input.confirm=1;ShenCfm_MouseInput();CHECK(gPad[0].gamePressed&PADG_CROSS);
""")]:
    cases.append((name,'#include "sys/late_a.h"\n'+shen_common+setup+extract(file,name)+'\nint main(void) {\n'+checks+'\nreturn 0;\n}\n'))
# Dragon Net rows are captured during drawing: verify the same anchor transform
# at 4:3, 16:9 and ultrawide, rather than approximating a fixed screen rectangle.
cases.append(('TboGeometry','#include "menu/menu_a.h"\n'+common+r"""
typedef struct TbMouseRow {s32 mode,index;f32 x0,y0,x1,y1;} TbMouseRow;
static TbMouseRow sTbMouseRows[16];static s32 sTbMouseCount;
static struct {s32 mode;} tb,*sTb=&tb;
static s32 aspect=1333;
s32 Port_PageAspectMilli(void) {return aspect;}
s32 Port_TitleMainChars43(void) {return 1;}
s32 Port_Menus43(void) {return 0;}
f32 Ps_X(f32 x) {return x;}
f32 Ps_Y(f32 y) {return y;}
"""+extract('src/menu/port_tbo.c','Tb_MouseRow')+r"""
int main(void) {
TbMouseRow *r=&sTbMouseRows[0];
Tb_MouseRow(1,100,50,200,40,0,0);CHECK(r->x0==100 && r->x1==300);
sTbMouseCount=0;aspect=1778;Tb_MouseRow(1,100,50,200,40,0,0);
CHECK(r->x0>138 && r->x0<140 && r->x1>288 && r->x1<290 && r->y0==50 && r->y1==90);
sTbMouseCount=0;Tb_MouseRow(1,100,50,200,40,1,0);CHECK(r->x0>74 && r->x0<76 && r->x1>224 && r->x1<226);
sTbMouseCount=0;aspect=2400;Tb_MouseRow(1,100,50,200,40,0,0);CHECK(r->x0>169 && r->x0<170);
return 0;
}
"""))

case('menu/menu_a.h','src/menu/port_tbo.c','Tb_MouseInput',r"""
#define TB_WII_HOME 9
#define TB_WII_BATTLE 10
#define TB_WII_FRIENDS 11
#define TB_WII_SETTINGS 12
#define TB_WII_RANKING 13
#define TB_WII_NAME 14
#define TB_WII_CUSTOM 15
#define TB_WII_VS 16
#define TB_WII_FRIEND_MENU 17
#define TB_WII_FRIEND_FORMAT 18
#define TB_WII_FRIEND_CODE 19
#define TB_WII_REWARD 20
typedef struct TbMouseRow { s32 mode,index; f32 x0,y0,x1,y1; } TbMouseRow;
static TbMouseRow sTbMouseRows[16];static s32 sTbMouseCount;
static struct {
 s32 mode,wiiNextMode,accountWaiting,wiiBattleIntro,wiiHomeT,wiiBattleT,wiiNameExit,disconnectReason,accountPrompt;
 s32 lower,keyRow,keyCol,wiiCur,wiiBattleCur,wiiCustomCur,wiiSubCur,wiiPrevCur,wiiSelectT,wiiFocusFrame,wiiIdleT,wiiDialogTouched;
} tb,*sTb=&tb;
static s32 art=1;
s32 Port_WiiArtAvailable(void) {return art;}
s32 Port_DcNameReady(void) {return 1;}
s32 Port_DcNameMouse(s32 page,s32 *row,s32 *col,PortMenuMouse *m) {return 0;}
static s32 Tb_WiiLine(void) {return 0;}
static void Tb_WiiDialogSet(s32 line) {voices++;}
""",'',r"""
sTb->mode=TB_WII_HOME;sTb->wiiHomeT=40;sTbMouseCount=1;
sTbMouseRows[0]=(TbMouseRow){TB_WII_HOME,2,100,100,300,140};
input.confirm=1;input.x=200;input.y=120;Tb_MouseInput(1);
CHECK(sTb->wiiCur==2 && voices==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();sTb->wiiHomeT=0;input.confirm=1;input.x=200;input.y=120;Tb_MouseInput(1);CHECK(!ready && !gPad[0].gamePressed);
clear();sTb->wiiHomeT=40;input.confirm=1;input.x=400;input.y=120;Tb_MouseInput(1);CHECK(!gPad[0].gamePressed);
clear();input.cancel=1;Tb_MouseInput(1);CHECK(gPad[0].gamePressed&PADG_TRIANGLE);
clear();sTb->disconnectReason=1;sTb->accountPrompt=1;reads=0;input.confirm=1;Tb_MouseInput(1);CHECK(reads==0);
clear();sTb->disconnectReason=sTb->accountPrompt=0;sTb->mode=TB_WII_RANKING;input.wheel=1;
Tb_MouseInput(1);CHECK(gPad[0].gameRepeat&PADG_DOWN);
""")
case('menu/menu_u.h','src/menu/menu_u_g.c','EvoZ_MouseDialog',r"""
UEvoZ menu,*gEvoZ=&menu;
void Port_DialogMemoryMouse(void) {reads++;gPad[0].gamePressed|=PADG_CROSS;}
""",'',r"""
menu.dialogState=0;EvoZ_MouseDialog();CHECK(!reads && !gPad[0].gamePressed);
menu.dialogState=6;EvoZ_MouseDialog();CHECK(reads==1 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.dialogState=2;EvoZ_MouseDialog();CHECK(reads==2 && (gPad[0].gamePressed&PADG_CROSS));
clear();menu.dialogState=7;EvoZ_MouseDialog();CHECK(reads==2 && !gPad[0].gamePressed);
""")
# The previous frame's dialog regions must survive until the modal reads them.
evo_loop=(root/'src/menu/menu_u_g.c').read_text().split('s32 EvoZ_Run(')[1]
assert evo_loop.index('EvoZ_MouseDialog();') < evo_loop.index('Port_UbMouseBegin(') < evo_loop.index('EvoZ_UpdateDialog(gEvoZ);')

with tempfile.TemporaryDirectory() as folder:
    for name,source in cases:
        path=Path(folder)/(name+'.c');path.write_text(source);exe=path.with_suffix('')
        subprocess.run(['cc','-std=gnu99','-DPORT','-fno-builtin','-Iinclude','-Iport/include','-include','port_compat.h',str(path),'-o',str(exe)],cwd=root,check=True)
        subprocess.run([str(exe)],check=True)
        print(name+': passed')
