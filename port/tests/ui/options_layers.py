#!/usr/bin/env python3
"""Exercise the production native-row insertion hook and its recursion guard."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/sys/gfxm_c.c').read_text()
a=s.index('void Port_FlashOptionsViewport(')
b=s.index('static const char *sOptionsPlateLabel;',a)
source=r'''
#include <assert.h>
#include <string.h>
typedef int s32;
static int sOptionsViewport,sOptionsViewportDrawn,sMenuArtScreen=68;
static int order[8],count;
'''+s[a:b]+r'''
void Port_OptionsTopDraw(void) {
    assert(!Flash_OptionsViewportRow("mc_menu_plate_4")); /* template recursion */
    order[count++]=1;
}
int main(void) {
    Port_FlashOptionsViewport(1);
    assert(Flash_OptionsViewportRow("mc_menu_plate_1"));
    assert(Flash_OptionsViewportRow("mc_menu_plate_2"));
    assert(count==1);
    assert(!Flash_OptionsViewportRow("mc_guide_dende")); /* guide remains in native foreground */
    assert(!Flash_OptionsViewportRow("mc_dimmer"));order[count++]=2;
    assert(!Flash_OptionsViewportRow("mc_select_plate_1"));order[count++]=3;
    assert(!Flash_OptionsViewportRow("mc_bottom_plate_1")); /* preserve EXIT */
    assert(order[0]==1 && order[1]==2 && order[2]==3);
    Port_FlashOptionsViewport(0);
    assert(!Flash_OptionsViewportRow("mc_menu_plate_1"));
    Port_FlashOptionsViewport(1);sMenuArtScreen=4;
    assert(!Flash_OptionsViewportRow("mc_menu_plate_1")); /* dialogs unaffected */
    sMenuArtScreen=68;
    assert(Flash_OptionsViewportRow("mc_menu_plate_1"));assert(count==4);
    Port_FlashOptionsViewport(2);
    assert(!Flash_OptionsViewportRow("mc_menu_plate_1"));
    assert(!Flash_OptionsViewportRow("mc_menu_plate_2"));
    assert(count==5); /* Controller native rows remain, insertion runs once. */
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'layers.c';exe=Path(tmp)/'layers';src.write_text(source)
    subprocess.run(['cc','-std=c99','-fsanitize=undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Options native layering checks passed')
