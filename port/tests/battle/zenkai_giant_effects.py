#!/usr/bin/env python3
"""Exercise production effect entry points without rebuilding the game."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
def function(path, name):
    source = (ROOT / path).read_text()
    start = re.search(r'^(?:static )?(?:void|f32|s32) ' + re.escape(name) + r'\(', source, re.M).start()
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

code = r"""
#include <assert.h>
#define PORT 1
typedef float f32;
typedef int s32;
typedef struct { int unused; } EftEmitState, EftEmitNodes, Vec4;
typedef struct { int node; } Param;
typedef struct { int objId; Param *param; } Owner;
typedef struct { Owner *owner; } EftEmitSet;
typedef struct { float scale; } BtlSceneRate;
static struct { BtlSceneRate rates[2]; } scene;
static typeof(scene) *gBtlScene = &scene;
static int gBtlSceneCharCount = 2;
static float factor = 1, received;
float Port_ZenkaiGiantEffectScale(int id) { return id == 0 ? factor : 1; }
void EftEmit_Spawn(EftEmitSet *s, EftEmitState *st, EftEmitNodes *n,
 Vec4 *p, Vec4 *d, int id, int node, int own, int type, int idx, int flags, float scale) {
 assert(id==0 && node==17 && own==1); received=scale;
}
"""
code += function('src/battle/eft_i.c', 'EftEmit_SpawnOwn')
code += '\n' + function('src/battle/btl_scene.c', 'BtlScene_GetCharScale')
code += r"""
int main(void) {
 Param p={17}; Owner o={0,&p}; EftEmitSet set={&o};
 scene.rates[0].scale=1.5f; scene.rates[1].scale=2;
 EftEmit_SpawnOwn(&set,0,0,0,0,0,0,0,2); assert(received==2);
 factor=4;
 EftEmit_SpawnOwn(&set,0,0,0,0,0,0,0,2); assert(received==8);
 EftEmit_SpawnOwn(&set,0,0,0,0,0,0,0,2); assert(received==8);
 assert(BtlScene_GetCharScale(0)==6);
 assert(BtlScene_GetCharScale(0)==6); /* no accumulated multiplication */
 assert(BtlScene_GetCharScale(1)==2);
 assert(BtlScene_GetCharScale(-1)==1 && BtlScene_GetCharScale(2)==1);
 factor=1; assert(BtlScene_GetCharScale(0)==1.5f);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as d:
    source, exe = Path(d)/'effects.c', Path(d)/'effects'
    source.write_text(code)
    subprocess.run(['cc','-std=gnu99','-fsanitize=undefined',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Giant effect entry-point checks passed.')

# Exercise actual sphere/capsule collision sizing, including aliased endpoints.
source = r"""
#include "common.h"
#include "battle/eft_a.h"
#include <assert.h>
static float factor=4;
float Port_ZenkaiGiantEffectScale(int id) { return factor; }
"""
for name in ('EftHit_GetRadiusA','EftHit_GetRadiusB','EftHit_SetRadiusA','EftHit_SetRadiusB','Port_EftHitScaleGiant'):
    source += '\n' + function('src/battle/eft_a.c', name)
source += r"""
int main(void) {
 EftHitRec rec={0}; EftHitSrc owner={0};
 EftHitSphere a={0},b={0}; EftHitBox c={0},d={0};
 rec.src=&owner;rec.shape.a=&a;rec.shape.b=&b;a.radius=2;b.radius=3;
 Port_EftHitScaleGiant(&rec);assert(a.radius==8&&b.radius==12);
 rec.shape.a=rec.shape.b=&a;a.radius=2;
 Port_EftHitScaleGiant(&rec);assert(a.radius==8); /* not 16x */
 rec.shape.type=1;rec.shape.a=&c;rec.shape.b=&d;c.radius=2;d.radius=3;
 Port_EftHitScaleGiant(&rec);assert(c.radius==8&&d.radius==12);
 factor=1;c.radius=2;d.radius=3;Port_EftHitScaleGiant(&rec);assert(c.radius==2&&d.radius==3);
 factor=4;rec.src=0;Port_EftHitScaleGiant(&rec);assert(c.radius==2&&d.radius==3);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as d:
    src, exe = Path(d)/'hit.c', Path(d)/'hit'
    src.write_text(source)
    subprocess.run(['cc','-std=gnu99','-fsanitize=undefined','-fno-builtin',
                    '-Iinclude','-Iport/include','-include','port_compat.h',str(src),'-o',str(exe)],cwd=ROOT,check=True)
    subprocess.run([str(exe)],check=True)
print('Giant technique collision-width checks passed.')

# Prop models have their own matrix path, independent of particle emitters.
source = r"""
#include "common.h"
#include "battle/eft_j.h"
#include <assert.h>
#include <math.h>
#include <string.h>
static float factor=2.5f, received;
static int events;
f32 Port_ZenkaiGiantEffectScale(int id) { assert(id==7);return factor; }
s32 EftShot_TestBits(s32 id,s32 bits) { return events & bits; }
void Vec4_Copy(void *d,void *s) { *(EftJVec *)d=*(EftJVec *)s; }
f32 EftMath_WrapAngle(f32 a) { return a; }
f32 Mathf_Cos(f32 a) { return cosf(a); }
f32 Mathf_Asin(f32 a) { return asinf(a); }
void Mtx_StoreIdentity(Mtx44 *m) { }
void Mtx_ScaleDiagUniform(Mtx44 *d,Mtx44 *s,f32 v) { received=v; }
void Mtx_RotateX(Mtx44 *d,Mtx44 *s,f32 a) { }
void Mtx_RotateY(Mtx44 *d,Mtx44 *s,f32 a) { }
void Mtx_Translate(Mtx44 *d,Mtx44 *s,void *v) { }
void EftObj_SetMtx(s32 id,Mtx44 *m) { assert(id==9); }
"""
source += function('src/battle/eft_j.c', 'EftPropShot_UpdateProp')
source += r"""
int main(void) {
 EftJTask task={0};EftPropShot work={0};EftJSrc src={0};
 task.work=&work;work.src=&src;src.objId=7;
 work.prop.obj=9;work.prop.flags=0x10;work.prop.scale=3;work.prop.flyScale=2;
 events=2;EftPropShot_UpdateProp(&task);assert(received==7.5f);
 events=0;EftPropShot_UpdateProp(&task);assert(received==7.5f);assert(work.prop.scale==3);
 events=4;EftPropShot_UpdateProp(&task);assert(received==5);
 events=0;EftPropShot_UpdateProp(&task);assert(received==5);assert(work.prop.flyScale==2);
 factor=1;EftPropShot_UpdateProp(&task);assert(received==2);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as d:
    src, exe = Path(d)/'prop.c', Path(d)/'prop'
    src.write_text(source)
    subprocess.run(['cc','-std=gnu99','-fsanitize=undefined','-fno-builtin',
                    '-Iinclude','-Iport/include','-include','port_compat.h',str(src),'-lm','-o',str(exe)],cwd=ROOT,check=True)
    subprocess.run([str(exe)],check=True)
print('Giant held/flying prop model checks passed.')
