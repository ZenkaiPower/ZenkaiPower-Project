"""Check production pause-menu hit tests across displayed page ratios."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[3]
text = (ROOT / "src/battle/hud_0_b.c").read_text()
a = text.index("static void BtlMenu_MouseInput(")
b = text.index("\n}\n", a) + 3
source = r'''
#include <assert.h>
#include "common.h"
#include "battle/hud_0.h"
#include "sys/pad.h"
#include "menu/port_menu_mouse.h"
Pad gPad[PAD_COUNT];
static BtlMenuWork work;
BtlMenuWork *gBtlMenu=&work;
static PortMenuMouse pending;
static int aspect,skills,hover,held;
int PortMenuMouse_LeftHeld(void) {return held;}
int Port_PageAspectMilli(void) {return aspect;}
int Port_NetActive(void) {return 0;}
int Port_NetMe(void) {return 0;}
int PortMenuMouse_Enabled(void) {return 1;}
void PortMenuMouse_Enable(int on) {}
void PortMenuMouse_Ready(int on) {}
void PortMenuMouse_Read(PortMenuMouse *out) {*out=pending;}
void PortMenuMouse_SetHover(int on) {hover=on;}
s32 BtlMenu_GetWidth(BtlMenu *menu) {return 360;}
s32 BtlMenu_GetHeight(BtlMenu *menu) {return 400;}
s32 BtlMenu_IsSkillList(BtlMenu *menu) {return skills;}
s32 BtlMenu_IsControls(BtlMenu *menu) {return 0;}
s32 Snd_PlaySe(u32 mask,s32 id) {return 0;}
''' + text[a:b] + r'''
int main(void) {
    BtlMenu menu={0};BtlMenuItem items[2]={{0}};
    menu.items=items;
    int ratios[]={1333,1778,2333},r,page,pad;
    menu.state=BTL_MENU_STATE_ACTIVE;menu.scale=1;menu.padX=10;
    pending.confirm=1;
    for(r=0;r<3;r++) for(pad=0;pad<2;pad++) {
        aspect=ratios[r];skills=1;work.list[pad].pages=3;
        for(page=0;page<3;page++) {
            float nativeX=(512-360)*.5f+20+page*50+25;
            work.list[pad].page=(page+1)%3;hover=0;
            pending.x=aspect>1340 ? 256+(nativeX-256)*1333.333f/aspect:nativeX;
            pending.y=(448-400)*.5f+12;
            BtlMenu_MouseInput(&menu,pad);
            assert(work.list[pad].page==page && hover);
        }
        pending.y=(448-400)*.5f+26;hover=0;
        BtlMenu_MouseInput(&menu,pad);assert(!hover); /* below tab */
        /* Track taps and thumb dragging scroll the active tab, including endpoints. */
        work.list[pad].count[page=work.list[pad].page]=20;
        {
            float nativeX=76+360-10+4;
            pending.x=aspect>1340 ? 256+(nativeX-256)*1333.333f/aspect:nativeX;
            pending.y=24+250;pending.confirm=1;
            BtlMenu_MouseInput(&menu,pad);
            assert(work.list[pad].cursor[page]==19 && work.list[pad].scroll[page]==13);
            held=1;pending.y=24+64+(int)(19*190.0f/26)+2;
            BtlMenu_MouseInput(&menu,pad);
            pending.confirm=0;pending.moved=1;pending.y=0;
            BtlMenu_MouseInput(&menu,pad);
            assert(work.list[pad].cursor[page]==0 && work.list[pad].scroll[page]==0);
            pending.y=448;
            BtlMenu_MouseInput(&menu,pad);
            assert(work.list[pad].cursor[page]==19 && work.list[pad].scroll[page]==13);
            held=0;pending.confirm=1;pending.moved=0;
        }
        skills=0;menu.count=2;menu.padY=10;menu.cursor=0;
        pending.x=256;pending.y=(448-400)*.5f+10+28+12;
        BtlMenu_MouseInput(&menu,pad);
        assert(menu.cursor==1 && (gPad[pad].gamePressed&PADG_CROSS));
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory=Path(directory)
    (directory/"test.c").write_text(source)
    subprocess.run(["gcc","-std=gnu99","-fno-builtin","-fsanitize=undefined",
                    "-Iinclude","-Iport/include","-include","port_compat.h",
                    str(directory/"test.c"),"-o",str(directory/"test")],cwd=ROOT,check=True)
    subprocess.run([str(directory/"test")],check=True)
print("Pause pointer checks passed: tabs, rows, scrollbar taps and dragging, both pads, 4:3, 16:9 and 21:9.")
