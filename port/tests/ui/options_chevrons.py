#!/usr/bin/env python3
"""Verify shared Options chevron entrance, looping motion and re-entry."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
code = (root / 'src/sys/gfxm_c.c').read_text()
a = code.index('static s32 sOptionsChevronFrame,')
b = code.index('\n#endif', a)
source = r'''
#include <assert.h>
#include <string.h>
#include <math.h>
typedef int s32;typedef float f32;typedef unsigned char u8;
typedef struct { float pt[4][2];struct { int u0,u1,v0,v1; } uv;int rgba[4]; } FlashQuad;
typedef struct { void *tex[35]; } Flash;
static FlashQuad drawn;
void Flash_MenuMarker(int) {}
void FlashDraw_Quad(void *,FlashQuad *q) { drawn=*q; }
''' + code[a:b] + r'''
int main() {
    Flash movie={};float positions[4][85]={};
    for(int frame=0;frame<85;frame++) {
        Port_FlashOptionsChevronFrame(27);
        for(int direction=0;direction<4;direction++) {
            float x=direction<2 ? 272:direction==2 ? 360:480;
            float y=direction<2 ? (direction ? 252:64):88;
            Port_FlashOptionsChevron(&movie,direction,x,y);
            assert(drawn.rgba[3]==(direction>=2 ? 128:frame<19 ? 0:frame>=34 ? 128:(int)(128*((float)(frame-19)/15))));
            assert(drawn.uv.v0==(direction<2 ? 32:0));
            positions[direction][frame]=direction<2 ? (drawn.pt[0][1]+drawn.pt[3][1])/2:
                (drawn.pt[0][0]+drawn.pt[3][0])/2;
            if(frame>=34 || direction>=2) {
                assert(fabsf((drawn.pt[3][0]-drawn.pt[0][0])-32)<0.001f);
                assert(fabsf((drawn.pt[3][1]-drawn.pt[0][1])-32)<0.001f);
            }
        }
    }
    for(int direction=0;direction<4;direction++) {
        assert(fabsf(positions[direction][40]-positions[direction][80])<0.001f);
        assert(fabsf(positions[direction][40]-positions[direction][59])>5.9f);
    }
    Port_FlashOptionsChevronFrame(127); /* Keyboard to Gamepad restarts entrance */
    Port_FlashOptionsChevron(&movie,1,272,252);assert(!drawn.rgba[3]);
    for(int frame=0;frame<2;frame++) Port_FlashOptionsChevronFrame(127); /* hidden */
    Port_FlashOptionsChevron(&movie,1,272,252);assert(sOptionsChevronMotion[1].age==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / 'chevrons.cpp', Path(tmp) / 'chevrons'
    src.write_text(source)
    subprocess.run(['c++', '-std=c++17', '-fsanitize=undefined', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Options chevron animation checks passed')
