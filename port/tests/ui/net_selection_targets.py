#!/usr/bin/env python3
"""Check local-side-only online targets using the production selector callbacks."""
import ast
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
tree=ast.parse((root/'port/tests/ui/remaining_mouse.py').read_text())
common=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(v,ast.Name) and v.id=='common' for v in n.targets))
setup=r'''
#include "menu/port_selection_net_target.h"
static int me,session=1,panelSide=-1;
int Port_NetMe(void) {return me;}
int Port_NetSession(void) {return session;}
int Port_NetActive(void) {return session;}
s32 gPortNetStagePicker;
s32 StgGrid_IsSelectable(s32 *ids,s32 index) {return index!=5;}
void Port_ItemPanelNetTarget(s32 side,PortMenuMouse *mouse,PortMouseNetNav *nav) {panelSide=side;}
'''
with tempfile.TemporaryDirectory() as tmp:
    for prefix,file,header,sideType,stageType in [
        ('CharSel','menu_c_e','menu_d','CharSelSide','CharSelStage'),
        ('TeamSel','menu_e_b','menu_f','TeamSelSide','TeamSelStage')]:
        text=(root/f'src/menu/{file}.c').read_text()
        start=text.index(f'static void {prefix}_NetMouse(')
        function=text[start:text.index('\n}\n',start)+3]
        structure=prefix
        shared=f'''\n{structure} menu,*g{prefix}=&menu;
{sideType} sides[2];{stageType} stage; s32 stageIds[42];
'''
        initial='''sides[0].col=0;sides[1].col=1;''' if prefix=='CharSel' else '''sides[0].member[0].col=0;sides[1].member[0].col=1;'''
        flags='CHARSEL_STAGE_PHASE' if prefix=='CharSel' else 'TEAMSEL_STAGE'
        stageState='CHARSEL_ST_STAGE' if prefix=='CharSel' else 'TEAMSEL_STAGE_GRID'
        checks=f'''
int main(void) {{
PortMouseNetNav nav={{0}};
menu.side[0]=&sides[0];menu.side[1]=&sides[1];menu.stage=&stage;menu.stageIds=stageIds;
menu.flash[0].flags=MFLASH_PAD;{initial}
input.moved=1;target="mc_chara_chip_003";scope=11;
{prefix}_NetMouse(&input,&nav);CHECK(nav.ready && nav.current==0 && nav.target==3);
CHECK(!gPad[0].gamePressed && !gPad[1].gamePressed); /* read-only target generation */
me=1;scope=12;target="mc_chara_chip_002";
memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(nav.ready && nav.current==1 && nav.target==2);
menu.flags={flags};stage.state={stageState};stage.col=2;gPortNetStagePicker=0;
memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(!nav.ready); /* remote owns stage */
gPortNetStagePicker=1;scope=10;target="mc_map_chip_04";
memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(nav.ready && nav.current==2 && nav.target==4);
target="mc_map_chip_05";memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(nav.target==-1); /* locked stage */
target="background";input.confirm=1;
memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);
CHECK(nav.target==-1);
#ifdef __ANDROID__
CHECK(nav.action==0x4000); /* native confirm, no pointer target */
#else
CHECK(!nav.action);
#endif
target="mc_yajirusi_up";memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);
CHECK(nav.action==0x10); /* an arrow never confirms */
input.confirm=0;
progress.flags=MPROG_FREEZE;memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(!nav.ready);
progress.flags=0;session=0;memset(&nav,0,sizeof(nav));{prefix}_NetMouse(&input,&nav);CHECK(!nav.ready);
return 0;
}}
'''
        code=f'#include "menu/{header}.h"\n'+common+setup+shared+function+checks
        path=Path(tmp)/(prefix+'.c');path.write_text(code);exe=path.with_suffix('')
        for platform in ([], ['-D__ANDROID__']):
            subprocess.run(['cc','-std=gnu99',*platform,'-fno-builtin','-Iinclude','-Iport/include','-include','port_compat.h',str(path),'-o',str(exe)],cwd=root,check=True)
            subprocess.run([str(exe)],check=True)
        print(prefix+' online mouse targets passed')
