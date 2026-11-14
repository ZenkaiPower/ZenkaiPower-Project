#!/usr/bin/env python3
"""Compile Ultimate Battle's production mouse adapters without a game build."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]


def handler(suffix, name):
    text = (root / f'src/menu/menu_{suffix}.c').read_text()
    start = text.index(f'static void {name}_MouseInput(void) {{')
    return text[start:text.index('\n}\n', start) + 3]


common = r'''
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

cases = []

def case(header, suffix, name, setup, checks):
    callback = f'void {name}_ClipGoto(s32 a,s32 b,char *label) {{ highlights++; }}\n'
    cases.append((name, f'#include "menu/{header}.h"\n' + common + setup + callback + handler(suffix, name) +
                  '\nint main(void) {\n' + checks + '\nreturn 0;\n}\n'))

case('menu_p','p_c','UbMenu', r'''
#undef Snd_PlaySe
UbMenu menu, *gUbMenu=&menu;
s32 gUbMenuPlate3Open;
#define UM_SAY_PLATE() Voice_PlayWithSubtitle(0,0,0)
''', r'''
scope=17; input.moved=1; target="mc_menu_plate_2";
UbMenu_MouseInput(); CHECK(menu.cur[0]==1 && voices==1 && highlights==2);
UbMenu_MouseInput(); CHECK(voices==1 && highlights==2);
target="mc_menu_plate_3"; input.confirm=1;
UbMenu_MouseInput(); CHECK(menu.cur[0]==1 && !gPad[0].gamePressed);
gUbMenuPlate3Open=1; UbMenu_MouseInput(); CHECK(menu.cur[0]==2 && (gPad[0].gamePressed & PADG_CROSS));
clear(); input.confirm=1; target="empty"; UbMenu_MouseInput(); CHECK(!gPad[0].gamePressed);
clear(); input.wheel=1; target="mc_menu_plate_1"; input.moved=1;
UbMenu_MouseInput(); CHECK(menu.cur[0]==2 && (gPad[0].gameRepeat & PADG_DOWN));
clear(); menu.script=21; input.confirm=1; UbMenu_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
clear(); menu.script=0; input.cancel=1; UbMenu_MouseInput(); CHECK(gPad[0].gamePressed & PADG_TRIANGLE);
progress.flags=MPROG_FREEZE; Port_UbMouseBegin(1); CHECK(!ready);
progress.flags=0; fading=1; Port_UbMouseBegin(1); CHECK(!ready);
fading=0; faded=0; Port_UbMouseBegin(1); CHECK(!ready);
faded=1; Port_UbMouseBegin(0); CHECK(!ready);
input.confirm=1; target="mc_menu_plate_1"; UbMenu_MouseInput(); CHECK(!(gPad[0].gamePressed & PADG_CROSS));
Port_UbMouseBegin(1); CHECK(ready);
''')

for suffix,name,header,obj,field,level,screen in [
    ('o_d','MisSel','menu_o','gMisSel','cursor','level',19),
    ('s_b','SurvSel','menu_s','gSurvSel','cur','level',18),
    ('n_b','UbzSel','menu_n','gUbzSel','cursor','step',46),
]:
    extra = ''
    if name == 'MisSel':
        extra = r'''
clear(); input.confirm=1; target="mc_yajirusi_r";
MisSel_MouseInput(); CHECK((gPad[0].gameRepeat & PADG_RIGHT) && !gPad[0].gamePressed);
'''
    case(header,suffix,name,f'{name} menu, *{obj}=&menu;\n',f'''
scope={screen}; input.moved=1; target="mc_window_plate02";
{name}_MouseInput(); CHECK(menu.{field}[0]==1 && highlights==2);
{name}_MouseInput(); CHECK(highlights==2);
input.confirm=1; {name}_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
clear(); input.confirm=1; {name}_MouseInput(); CHECK(!gPad[0].gamePressed);
{extra}
clear(); menu.{level}=1; input.confirm=1; {name}_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
clear(); input.cancel=1; {name}_MouseInput(); CHECK(gPad[0].gamePressed & PADG_TRIANGLE);
clear(); menu.voiceReq=1; input.confirm=1; {name}_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
''')

case('menu_n','n_c','UbRank',r'''
UbRank menu, *gUbRank=&menu;
NSave save, *gSaveData=&save;
''',r'''
scope=47; menu.flags=UBRANK_STARTED; save.rank=4; menu.top=0; menu.cursor[0]=4;
input.moved=1; input.confirm=1; target="mc_ranking_plate01";
UbRank_MouseInput(); CHECK(menu.cursor[0]==4 && !gPad[0].gamePressed);
target="mc_ranking_plate03"; UbRank_MouseInput(); CHECK(menu.cursor[0]==3 && (gPad[0].gamePressed & PADG_CROSS));
clear(); input.wheel=-1; UbRank_MouseInput(); CHECK(gPad[0].gameRepeat & PADG_UP);
clear(); menu.state=UBRANK_ST_VS; input.confirm=1; UbRank_MouseInput(); CHECK(!gPad[0].gamePressed);
menu.state=UBRANK_ST_WAIT; UbRank_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
''')

for team in [False, True]:
    name='UbTeamSel' if team else 'SoloSel'
    suffix='m_b' if team else 'l_b'
    prefix='UBTEAM' if team else 'SOLOSEL'
    state='UbTeamState' if team else 'SoloSelState'
    cur='UT_CUR' if team else 'SS_CUR'
    custom='plate' if team else 'custom'
    color='color' if team else 'costume'
    count='plateCount' if team else 'customCount'
    setup=f'''
{name} menu, *g{name}=&menu;
{state} state;
#define {cur} (g{name}->sel->{'team.member[gUbTeamSel->sel->cur]' if team else 'entry'})
s32 ChrGrid_IsSelectable({'UbCell' if team else 'LChrCell'} *cells,s32 idx) {{ return idx != 2; }}
void {name}_{'RequestFace' if team else 'ChangeImage'}(void) {{ images++; }}
'''
    checks=f'''
scope=23; menu.sel=&state; state.step={prefix}_STEP_CHARA;
input.moved=1; target="mc_chara_chip_001";
{name}_MouseInput(); CHECK({cur}.col==1 && images==1);
{name}_MouseInput(); CHECK(images==1);
target="mc_chara_chip_002"; input.confirm=1;
{name}_MouseInput(); CHECK({cur}.col==1 && !gPad[0].gamePressed);
clear(); state.step={prefix}_STEP_{'PLATE' if team else 'CUSTOM'}; menu.{count}=4;
input.confirm=1; target="mc_custom_plate_3/mc_yajirusi";
{name}_MouseInput(); CHECK({cur}.{custom}==2 && (gPad[0].gamePressed & PADG_RIGHT) && !(gPad[0].gamePressed & PADG_CROSS));
clear(); state.step={prefix}_STEP_COLOR; menu.colorCount=3;
input.confirm=1; target="mc_color_plate_2";
{name}_MouseInput(); CHECK({cur}.{color}==1 && (gPad[0].gamePressed & PADG_CROSS));
clear(); state.step={prefix}_STEP_PANEL; {name}_MouseInput(); CHECK(selected==9);
clear(); state.step={prefix}_STEP_HELP; input.confirm=1; {name}_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
'''
    if team:
        checks+='''
clear(); state.step=UBTEAM_STEP_TEAM; menu.teamMax=5; state.memberCount=2;
input.confirm=1; target="mc_team_2"; UbTeamSel_MouseInput(); CHECK(state.cur==2 && (gPad[0].gamePressed & PADG_CROSS));
clear(); input.confirm=1; target="mc_team_4"; UbTeamSel_MouseInput(); CHECK(state.cur==2 && !gPad[0].gamePressed);
'''
    case('menu_m' if team else 'menu_l',suffix,name,setup,checks)

case('menu_q','q_b','SimDay',r'''
SimDay menu, *gSimDay=&menu;
const SimTrain gSimTrain0[5] = {{.param=5}};
s32 gSimCardRank, gSimCardNext;
u8 gSimCardPicked;
#define QPROG ((QProgress *)gProgress)
s32 SimDay_CountItems(void) { return 0; }
void SimEv28_SelectCard(SimDay *p) { highlights++; }
void SimEv28_UnselectCard(SimDay *p) { highlights++; }
void SimPopo_CursorOn(SimDay *p) { highlights++; }
void SimPopo_CursorOff(SimDay *p) { highlights++; }
''',r'''
scope=24; input.moved=1; target="mc_sim_botan02";
SimDay_MouseInput(); CHECK(menu.cur[0]==2 && menu.msgLine==0x52);
clear(); input.cancel=1; SimDay_MouseInput(); CHECK(gPad[0].gamePressed & PADG_START);
clear(); menu.state=SIMDAY_ST_TRAIN; input.confirm=1; target="mc_sim_botan07";
SimDay_MouseInput(); CHECK(menu.cur[menu.state]==1 && (gPad[0].gamePressed & PADG_CROSS));
clear(); menu.state=SIMDAY_ST_CONFIRM; input.confirm=1; target="mc_menu_plate_2";
SimDay_MouseInput(); CHECK(selected==1 && (gPad[0].gamePressed & PADG_CROSS));
clear(); scope=25; menu.state=SIMDAY_ST_SCRIPT; menu.event=28; menu.unkB68[0]=15;
input.confirm=1; target="mc_card_anime03"; SimDay_MouseInput();
CHECK(menu.cur[menu.state]==3 && (gPad[0].gamePressed & PADG_CROSS));
clear(); gSimCardPicked=1<<2; input.confirm=1; target="mc_card_anime02";
SimDay_MouseInput(); CHECK(menu.cur[menu.state]==3 && !gPad[0].gamePressed);
clear(); scope=26; menu.event=34; menu.unkB68[0]=16; input.confirm=1; target="mc_popo01";
SimDay_MouseInput(); CHECK(menu.cur[menu.state]==1 && (gPad[0].gamePressed & PADG_CROSS));
clear(); menu.event=32; input.confirm=1; SimDay_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
clear(); menu.event=1; menu.flags=SIMDAY_WAIT_KEY; input.confirm=1;
SimDay_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
''')

case('menu_o','o_b','DiscFusion',r'''
DiscFusion menu, *gDiscFusion=&menu;
void DiscFusion_PlateGoto(s32 a,s32 b,char *s) { highlights++; }
''',r'''
scope=20; input.confirm=1; target="mc_disk_plate01";
DiscFusion_MouseInput(); CHECK(menu.cursor[0]==1 && menu.talkStep==4 && (gPad[0].gamePressed & PADG_CROSS));
clear(); menu.talkStep=0; menu.dlgStep=DISCFUSION_DLG_CONFIRM; input.confirm=1;
DiscFusion_MouseInput(); CHECK(!gPad[0].gamePressed);
target="mc_menu_plate_1"; DiscFusion_MouseInput(); CHECK(selected==0 && (gPad[0].gamePressed & PADG_CROSS));
''')

case('menu_r','r_d','SimTop',r'''
SimTop menu, *gSimTop=&menu;
const s32 gSimTopPlateVoice[SIMTOP_ROWS]={1,2,3};
''',r'''
scope=21; input.confirm=1; target="mc_sim_plate03";
SimTop_MouseInput(); CHECK(menu.cur[0]==2 && menu.voiceReq==3 && (gPad[0].gamePressed & PADG_CROSS));
clear(); menu.voiceReq=0; menu.level=SIMTOP_LV_RANKING; input.confirm=1; target="mc_menu_yajirusi_down";
SimTop_MouseInput(); CHECK((gPad[0].gameRepeat & PADG_DOWN) && !gPad[0].gamePressed);
clear(); menu.level=SIMTOP_LV_HELP; input.confirm=1; SimTop_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
''')

case('menu_r','r_c','SimResult',r'''
SimResult menu, *gSimResult=&menu;
void SimResult_YesNoGoto(char *s) { highlights++; }
''',r'''
scope=28; menu.state=SIMRESULT_ST_CONTINUE; input.confirm=1; target="mc_yes_no_text02";
SimResult_MouseInput(); CHECK(menu.pick[menu.state]==1 && highlights==2 && (gPad[0].gamePressed & PADG_CROSS));
clear(); input.confirm=1; SimResult_MouseInput(); CHECK(!gPad[0].gamePressed);
clear(); menu.state=SIMRESULT_ST_PAGES; input.wheel=1; SimResult_MouseInput(); CHECK(gPad[0].gameRepeat & PADG_DOWN);
clear(); input.confirm=1; SimResult_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
''')

with tempfile.TemporaryDirectory() as tmp:
    for name, source in cases:
        path=Path(tmp)/f'{name}.c'; exe=path.with_suffix('')
        path.write_text(source)
        subprocess.run(['cc','-std=gnu99','-DPORT','-fno-builtin','-Iinclude','-Iport/include',
                        '-include','port_compat.h',str(path),'-o',str(exe)],cwd=root,check=True)
        subprocess.run([str(exe)],check=True)
print(f'Ultimate Battle mouse checks passed ({len(cases)} adapters)')
