#!/usr/bin/env python3
"""The added Controller plate follows the real row throughout native motion."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
code = (root / 'src/sys/gfxm_c.c').read_text()
start = code.index('static void Flash_OptionsPlateAt(')
end = code.index('\n#endif', start)
source = r'''
#include <assert.h>
#include <string.h>
typedef int s32;typedef unsigned u32;typedef float f32;
#define FLASH_CLIP_VISIBLE 2
#define FLASH_OV_POS 2
typedef struct { struct { float m[9]; } mtx;float alpha;unsigned flags; } FlashProp;
typedef struct { FlashProp prop;unsigned flags; } FlashClip;
typedef struct { FlashClip *clip; } FlashSlot;
typedef struct { FlashClip *clips; } Flash;
static int sMenuArtScreen=8,sSelectionMouseScope=9,sMenuArtDrawDepth=3,sFlashMouseMarker=4;
static int sOptionsCaptionAlpha[2];
static int sOptionsPlateDim,sOptionsPlateCopy,draws;
static const char *sOptionsPlateLabel;
static const char *expectedLabel="Mouse/Keyboard";
static FlashProp drawn;
static FlashClip *FlashClipList_FindByName(FlashClip *clip,char *name) { return clip; }
static void Flash_OptionsDisabledProp(FlashProp *) {}
static void Flash_MenuMarker(int value) { sFlashMouseMarker=value; }
static void FlashSlot_Draw(FlashSlot *slot,void *,Flash *) {
    assert(sOptionsPlateCopy && !strcmp(sOptionsPlateLabel,expectedLabel));
    drawn=slot->clip->prop;draws++;
}
''' + code[start:end] + r'''
int main(void) {
    FlashClip copy={},live={};Flash movie={&copy},reference={&live};
    copy.prop.mtx.m[2]=32;copy.prop.mtx.m[5]=204;
    FlashClip original=copy;
    live.flags=FLASH_CLIP_VISIBLE;
    float positions[]={800,400,72,-200,-800};
    for(float x:positions) {
        live.prop.mtx.m[2]=x;live.prop.mtx.m[5]=116;
        live.prop.mtx.m[0]=0.8f;live.prop.mtx.m[4]=1.2f;live.prop.alpha=0.3f;
        Port_FlashOptionsControllerPlate(&movie,&reference);
        assert(drawn.mtx.m[2]==x-20 && drawn.mtx.m[5]==160);
        assert(drawn.mtx.m[0]==0.8f && drawn.mtx.m[4]==1.2f && drawn.alpha==0.3f);
        assert(!memcmp(&original,&copy,sizeof(copy)));
        assert(sMenuArtScreen==8 && sSelectionMouseScope==9 && sMenuArtDrawDepth==3 && sFlashMouseMarker==4);
    }
    expectedLabel="Gamepad";
    live.prop.mtx.m[2]=400;live.prop.mtx.m[5]=116;
    Port_FlashOptionsGamepadPlate(&movie,&reference);
    assert(drawn.mtx.m[2]==360 && drawn.mtx.m[5]==204 && drawn.alpha==0.3f);
    assert(!memcmp(&original,&copy,sizeof(copy)));
    expectedLabel="Mouse/Keyboard";
    for(int row=0;row<4;row++) {
        live.prop.mtx.m[2]=95-20*row;
        assert(Port_FlashOptionsRowSlide(&reference,row)==0);
        live.prop.mtx.m[2]+=420;
        assert(Port_FlashOptionsRowSlide(&reference,row)==420);
        live.flags=0; /* hidden native Controller rows still supply motion */
        assert(Port_FlashOptionsRowSlide(&reference,row)==420);
    }
    live.flags=0;int count=draws;Port_FlashOptionsControllerPlate(&movie,&reference);
    assert(draws==count); /* parked/removed native rows do not leave a static copy */
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'controller.cpp'
    exe = Path(tmp) / 'controller'
    src.write_text(source)
    subprocess.run(['c++', '-std=c++17', '-Wno-write-strings', '-fsanitize=undefined',
                    str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Controller native animation inheritance checks passed')
