#!/usr/bin/env python3
"""Exercise Training's cloud scroll and actual GS UV packet without a game build."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/sys/gfxm_c.c').read_text()

def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}\n', start) + 3]

code = r'''
#include <assert.h>
#include <math.h>
#include <string.h>
#include "sys/gfxm_c.h"
#include "menu/port_training_art.h"
static int sTrainingCloudTree, sTrainingCloudFraction;
static int aspect=1778, proportional=1, menus43;
int Port_PageAspectMilli(void) { return aspect; }
int Port_TitleMainChars43(void) { return proportional; }
int Port_Menus43(void) { return menus43; }
#define FLASH_RGBA(q) 0
#define FLASH_XY(x,y) 0
''' + function('f32 Port_FlashTrainingCloudScroll(f32 offset, f32 speed) {') + function('u64 *FlashDraw_PutSprite(u64 *p, FlashQuad *q, u64 tex0) {') + r'''
int main(void) {
 FlashQuad q={0}; u64 packet[16]={0}; float speed;
 q.uv.u0=511; q.uv.u1=1023; q.uv.v1=128;
 speed=Port_FlashTrainingCloudScroll(511.5f,0.047407407f);
 assert(fabsf(speed*1778/1333.333f-0.047407407f)<0.000001f);
 sTrainingCloudTree=1;
 FlashDraw_PutSprite(packet,&q,0);
 assert((packet[6]&0xffff)==512*16+8);
 assert((packet[8]&0xffff)==1022*16+8);
 assert((packet[10]&0xffff)==512*16+8);
 assert((packet[12]&0xffff)==1022*16+8);
 sTrainingCloudTree=0;
 FlashDraw_PutSprite(packet,&q,0);
 assert((packet[6]&0xffff)==512*16); /* other artwork remains untouched */
 aspect=1333;
 assert(Port_FlashTrainingCloudScroll(511.5f,0.047407407f)==0.047407407f);
 assert(sTrainingCloudFraction==0);
 aspect=1778;menus43=1;
 assert(Port_FlashTrainingCloudScroll(511.5f,0.047407407f)==0.047407407f);
 assert(sTrainingCloudFraction==0);
 menus43=0;proportional=0;
 assert(Port_FlashTrainingCloudScroll(511.5f,0.047407407f)==0.047407407f);
 assert(sTrainingCloudFraction==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='training-cloud-') as folder:
    folder = Path(folder)
    (folder / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c99', '-DPORT', '-Iinclude', '-fsanitize=undefined',
                    str(folder / 'test.c'), '-lm', '-o', str(folder / 'test')], cwd=root, check=True)
    subprocess.run([str(folder / 'test')], check=True)
print('Training cloud checks passed: fractional UV packets, proportional speed, native scoping.')
