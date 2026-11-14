#!/usr/bin/env python3
"""Verify Options pointer coordinates follow rendered quads, not clip origins."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/sys/gfxm_c.c').read_text()
a=s.index('int Port_FlashMenuMousePosition(')
b=s.index('\nint Port_FlashMenuMouseHit(',a)
source=r'''
#include <assert.h>
#include <string.h>
#include <math.h>
typedef int s32; typedef float f32;
typedef struct { float pt[3][2]; int valid; } PortMousePlate;
typedef struct { char name[64]; int scope; int prop; void *flash; PortMousePlate drawn; } PortSelectionMouseClip;
static PortSelectionMouseClip sSelectionMouseClips[2];
static int sSelectionMouseCount=1;
/* An estimated region must never be used for these Options controls. */
static void Flash_MouseRegion(PortMousePlate *p,int a,float b,float c,float d,void *e,void *f) { assert(0); }
'''+s[a:b]+r'''
int main(void) {
    float u,v;
    PortSelectionMouseClip *c=sSelectionMouseClips;
    c->scope=68;strcpy(c->name,"mc_bgm_scroll_bar");
    c->drawn=(PortMousePlate){{{450,120},{466,120},{450,180}},1};
    assert(Port_FlashMenuMousePosition(68,c->name,16,35,256,458,135,&u,&v));
    assert(fabsf(u-.5f)<.0001f && fabsf(v-.25f)<.0001f);
    /* Capture continues outside the quad during a drag. */
    assert(Port_FlashMenuMousePosition(68,c->name,16,35,256,440,240,&u,&v));
    assert(u<0 && v==2);
    c->drawn.valid=0;
    assert(!Port_FlashMenuMousePosition(68,c->name,16,35,256,458,135,&u,&v));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'geometry.c';path.write_text(source)
    exe=Path(tmp)/'geometry'
    subprocess.run(['cc','-std=gnu99','-fsanitize=undefined',str(path),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Options rendered mouse geometry checks passed')
