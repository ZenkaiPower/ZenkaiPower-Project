#!/usr/bin/env python3
"""Exercise production giant sizing and collision helpers without rebuilding the game."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]

def function(source, name):
    start = re.search(r'^(?:static )?(?:void|f32|s32) ' + re.escape(name) + r'\(', source, re.M).start()
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

source = (ROOT / 'src/battle/bobj_a.c').read_text()
# Compile the actual implementations with small native math/resource substitutes.
body = '\n'.join(function(source, n) for n in (
    'Port_BtlObjMeleeReachBounds', 'Port_BtlObjMeleeWorldBox', 'Port_BtlObjObbScale', 'Port_BtlObjBodyPartSphere', 'BtlObjBody_Init', 'BtlObjXf_Update', 'BtlObjXf_SetMtx', 'BtlObjXf_Reset',
    'BtlObjObb_SetMtx', 'BtlObjBody_SetCenter', 'BtlObjBody_UpdateParts'))
collision = (ROOT / 'src/battle/col_b.c').read_text()
body = '\n'.join(function(collision, n) for n in ('ColSphere_TestSphere', 'ColBounds_AddSphere')) + '\n' + body
body += '\n' + function((ROOT / 'src/battle/btl_char_cam_modes.c').read_text(), 'Port_ChrCamGiantFocus')
body += '\n' + function((ROOT / 'src/battle/btl_char_cam.c').read_text(), 'ChrCam_GetOffset')
body += '\n' + function((ROOT / 'src/battle/btl_char_move.c').read_text(), 'BtlMove_GetApproachHeightDiff')
harness = r'''
#include "common.h"
#include "battle/bobj_a.h"
#include "sys/port_zenkai.h"
#include <assert.h>
#include <math.h>
typedef struct { Vec4 pos; f32 radius, radiusSq; f32 padding[2]; } CpSphere;
typedef BobjAabb CpBox;
typedef Vec4 CpVec;
void ColBounds_AddSphere(BobjAabb *, CpSphere *);
void BtlObjBody_SetCenter(Vec4 *, Vec4 *);
void ColObb_Init(BobjObb *, Vec4 *, f32, f32, f32);
void ColObb_Update(BobjObb *, Mtx44 *);
s32 ColObb_Overlaps(BobjObb *, BobjObb *);
typedef struct { s32 objId; } ChrCamChr;
typedef ChrCamChr BtlMoveChr;
f32 BtlOpp_GetHalfHeightDiff(BtlMoveChr *chr) { (void)chr;return 15; }
f32 BtlCharApi_GetHeight(s32 id) { (void)id;return 10; }
static void *testFocusObj, *testOwnCameraObj;
void *BtlChar_GetObj(ChrCamChr *chr) { (void)chr;return testOwnCameraObj; }
void *BtlOpp_GetObj(ChrCamChr *chr) { (void)chr;return testFocusObj; }
static int active, session, online;
static BobjObj *testOpponent;
s32 BtlCharApi_GetOpponentObjId(s32 id) { (void)id;return testOpponent ? 1 : -1; }
BobjObj *BtlObj_Get(s32 id) { (void)id;return testOpponent; }
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return session; }
int Port_NetActive(void) { return online; }
static int cinematic;
s32 EftCam_IsActive(void) { return cinematic; }
extern void *memset(void *, int, u32);
extern int memcmp(const void *, const void *, u32);
static Vec4 D_002EC2C0;
static Mtx44 current;
void Vec4_Copy(Vec4 *d, Vec4 *s) { *d = *s; }
void Vec4_Sub(Vec4 *d, Vec4 *a, Vec4 *b) { Vec4 v={a->x-b->x,a->y-b->y,a->z-b->z,a->w-b->w};*d=v; }
void Vec3_Sub(Vec4 *d, Vec4 *a, Vec4 *b) { Vec4_Sub(d,a,b);d->w=1; }
void Vec3_Copy(Vec4 *d,Vec4 *s) { d->x=s->x;d->y=s->y;d->z=s->z; }
f32 Vec3_Dot(Vec4 *a,Vec4 *b) { return a->x*b->x+a->y*b->y+a->z*b->z; }
f32 Vec3_LengthSq(Vec4 *v) { return v->x*v->x+v->y*v->y+v->z*v->z; }
void Mtx_MulVec4(Vec4 *d, Mtx44 *m, Vec4 *v) {
 int i;Vec4 out;for(i=0;i<4;i++)((f32 *)&out)[i]=m->m[0][i]*v->x+m->m[1][i]*v->y+m->m[2][i]*v->z+m->m[3][i]*v->w;*d=out;
}
void StgAabb_SetEmpty(BobjAabb *b) { int i;for(i=0;i<3;i++){b->min[i]=1e30f;b->max[i]=-1e30f;} }
void ColBox_AddPoint(BobjAabb *b, Vec4 *v) { int i;for(i=0;i<3;i++){f32 f=((f32 *)v)[i];if(f<b->min[i])b->min[i]=f;if(f>b->max[i])b->max[i]=f;} }
void Vu0Cur_LoadIdentity(void) { int i; memset(&current,0,sizeof current); for(i=0;i<4;i++)current.m[i][i]=1; }
void Vu0Cur_ScaleDiagUniform(f32 f) { int i; for(i=0;i<3;i++) current.m[i][i]*=f; }
void Vu0Cur_RotateZXY(Vec4 *v) { (void)v; }
void Vu0Cur_Translate(Vec4 *v) { current.m[3][0]=v->x; current.m[3][1]=v->y; current.m[3][2]=v->z; }
void Vu0Cur_StoreMtx(Mtx44 *m) { *m=current; }
void Mtx_Copy(Mtx44 *d, Mtx44 *s) { *d=*s; }
void Mtx_InverseRT(Mtx44 *d, Mtx44 *m) {
 int i,j; memset(d,0,sizeof *d); d->m[3][3]=1;
 for(i=0;i<3;i++)for(j=0;j<3;j++)d->m[i][j]=m->m[j][i];
 for(i=0;i<3;i++)for(j=0;j<3;j++)d->m[3][i]-=m->m[3][j]*d->m[j][i];
}
s32 BtlObjBody_Exists(BobjObj *o) { return o->type==0 && o->mdl.body!=0; }
void BtlObjBody_Reset(BobjObj *o) { (void)o; }
static BobjPart *testHead;
BobjPart *BtlObj_GetNode(BobjObj *o,s32 n) { (void)o;return n==0x30?testHead:0; }
BobjBone *BtlObj_FindBound(BobjObj *o,s32 n) { (void)o;(void)n;return 0; }
'''
tests = r'''
int main(void) {
 BobjObj o; BobjParam param; BobjModelHdr model; BobjObb box, attackBox, targetBox; BobjBodyPart hurt; BobjBone bone; BobjPart node; BobjObj small; BobjParam smallParam; BobjAabb strike, targetBounds; CpSphere shot; Vec4 target, cameraPos, cameraDelta; ChrCamChr viewer={0}; Vec4 normalOffset, giantOffset; f32 normalPitch, giantPitch; int i, mode;
 memset(&o,0,sizeof o); memset(&param,0,sizeof param); memset(&model,0,sizeof model);
 o.mdl.param=&param; o.mdl.model=&model; ((u8 *)&param)[2]=4;
 assert(Port_ZenkaiGiantScale(0,&param)==1); active=1;
 assert(Port_ZenkaiGiantScale(0,&param)==2.5f);
 testOpponent=&o;
 assert(Port_ZenkaiGiantEffectScale(0)==2.5f);
 cinematic=1;assert(Port_ZenkaiGiantAnimationScale(0,&param)==1);
 assert(Port_ZenkaiGiantEffectScale(0)==1);assert(Port_ZenkaiGiantScale(0,&param)==2.5f);
 cinematic=0;assert(Port_ZenkaiGiantEffectScale(0)==2.5f);

 assert(Port_ZenkaiGiantEffectScale(-1)==1.0f);
 session=1; assert(Port_ZenkaiGiantEffectScale(0)==1.0f);session=0;
 online=1; assert(Port_ZenkaiGiantEffectScale(0)==1.0f);online=0;
 active=0; assert(Port_ZenkaiGiantEffectScale(0)==1.0f);active=1;
 testOpponent=0; assert(Port_ZenkaiGiantEffectScale(0)==1.0f);
 assert(Port_ZenkaiGiantScale(1,&param)==1); assert(Port_ZenkaiGiantScale(0,0)==1);
 session=1; assert(Port_ZenkaiGiantScale(0,&param)==1);session=0;
 online=1; assert(Port_ZenkaiGiantScale(0,&param)==1);online=0;
 for(i=0;i<4;i++)model.size[i]=10+i;
 param.radius=20;
 BtlObjXf_Reset(&o); assert(o.xf.scale==2.5f);
 BtlObjBody_Init(&o); assert(o.body.radius==50);
 for(i=0;i<4;i++)assert(o.body.size[i]==model.size[i]*2.5f);
 BtlObjBody_Init(&o); assert(o.body.radius==50); /* no accumulated scaling */
 param.radius=0; BtlObjBody_Init(&o); assert(o.body.radius==65);
 o.xf.pos.x=12; o.xf.pos.y=-6; o.xf.pos.z=3; BtlObjXf_Update(&o);
 for(i=0;i<3;i++)assert(fabsf(o.xf.mtx.m[i][i]*o.xf.inv.m[i][i]-1)<0.00001f);
 assert(fabsf(o.xf.inv.m[3][0]+4.8f)<0.00001f);
 BtlObjXf_SetMtx(&o,&o.xf.mtx);
 assert(fabsf(o.xf.inv.m[3][0]+4.8f)<0.00001f);
 memset(&box,0,sizeof box);
 for(i=0;i<3;i++) { ((f32 *)&box.axis[i])[i]=4.0f; ((f32 *)&box.local[0])[i]=2+i; }
 Port_BtlObjObbScale(&box);
 for(i=0;i<3;i++) { assert(((f32 *)&box.axis[i])[i]==1); assert(((f32 *)&box.unk10)[i]==(2+i)*4.0f); }
 memset(&hurt,0,sizeof hurt);hurt.box=box;
 Port_BtlObjBodyPartSphere(&hurt);
 assert(hurt.radiusSq==(8*8+12*12+16*16));
 assert(fabsf(hurt.radius-sqrtf(464))<0.00001f);
 Port_BtlObjBodyPartSphere(&hurt);assert(hurt.radiusSq==464);
 /* A projectile at x=15 misses the old sphere (sqrt(29)), but hits the enlarged sphere. */
 assert(15>sqrtf(29)+0.5f);assert(15<hurt.radius+0.5f);
 memset(&shot,0,sizeof shot);shot.pos.x=15;shot.radius=0.5f;
 assert(ColSphere_TestSphere(&shot,(CpSphere *)&hurt.center));
 memset(&bone,0,sizeof bone);memset(&node,0,sizeof node);
 hurt.flags=1;hurt.bone=&bone;hurt.part=&node;o.body.parts=&hurt;
 for(i=0;i<4;i++)node.mtx.m[i][i]=i==3?1:4;
 for(i=0;i<8;i++) { hurt.box.local[i].x=(i&1)?-2:2;hurt.box.local[i].y=(i&2)?-3:3;hurt.box.local[i].z=(i&4)?-4:4;hurt.box.local[i].w=1; }
 BtlObjBody_UpdateParts(&o.body);
 assert(hurt.radiusSq==464);assert(o.body.box.max[0]==8);
 assert(o.body.centerBox.max[0]>21); /* projectile broad phase grows too */
 assert(ColSphere_TestSphere(&shot,(CpSphere *)&hurt.center));
 BtlObjBody_UpdateParts(&o.body);assert(hurt.radiusSq==464);
 o.xf.pos.x=o.xf.pos.y=o.xf.pos.z=0;
 o.xf.rootPos.x=o.xf.rootPos.y=o.xf.rootPos.z=0;
 target.x=100;target.y=target.z=0;
 assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==8);
 /* The model matrix origin may be offset by animation root compensation. */
 o.xf.pos.x=25;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==8);o.xf.pos.x=0;
 target.x=0;target.z=100;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==16);
 target.y=13;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==5);
 target.y=0;target.z=0;target.x=100;
 node.mtx.m[0][0]=0;node.mtx.m[0][2]=4;node.mtx.m[2][0]=-4;node.mtx.m[2][2]=0;
 BtlObjBody_UpdateParts(&o.body);assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==16);
 node.mtx.m[3][0]=10;BtlObjBody_UpdateParts(&o.body);
 assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==26);
 hurt.box.axis[0].x=hurt.box.axis[0].y=hurt.box.axis[0].z=0;
 assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==5);
 BtlObjBody_UpdateParts(&o.body);
 /* The same surface distance is returned even after a fighter has penetrated. */
 target.x=12;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==26);
 online=1;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==5);online=0;
 active=0;assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==5);active=1;
 node.mtx.m[3][0]=0;node.mtx.m[0][2]=node.mtx.m[2][0]=0;
 for(i=0;i<3;i++)node.mtx.m[i][i]=1;
 BtlObjBody_UpdateParts(&o.body);assert(hurt.radiusSq==29);
 assert(!ColSphere_TestSphere(&shot,(CpSphere *)&hurt.center));
 Port_BtlObjObbScale(&box); /* restore original extents after a normal-size rebind */
 for(i=0;i<3;i++)assert(((f32 *)&box.unk10)[i]==2+i);
 hurt.box=box;Port_BtlObjBodyPartSphere(&hurt);assert(hurt.radiusSq==29);
 testHead=&node;node.mtx.m[3][0]=12;node.mtx.m[3][1]=-150;node.mtx.m[3][2]=24;
 assert(Port_ZenkaiGiantFocus(&o,&target));assert(target.x==6&&target.y==-81.25f&&target.z==12&&target.w==1);
 testFocusObj=&o;
 for(mode=0;mode<3;mode++) {
  active=0;normalPitch=ChrCam_GetOffset(&viewer,&normalOffset,mode);
  active=1;giantPitch=ChrCam_GetOffset(&viewer,&giantOffset,mode);
  assert(giantPitch==normalPitch);assert(giantOffset.w==1);
  assert(fabsf(giantOffset.z-normalOffset.z*3)<0.00001f);
  assert(fabsf((giantOffset.y+7)-(normalOffset.y+7)*3)<0.00001f);
  testOwnCameraObj=&o;
  ChrCam_GetOffset(&viewer,&giantOffset,mode);
  assert(giantOffset.z==normalOffset.z);
  assert(giantOffset.y==normalOffset.y); /* giant vs giant never stacks 3x */
  ChrCam_GetOffset(&viewer,&giantOffset,mode);
  assert(giantOffset.z==normalOffset.z); /* repeated updates remain stable */
  testFocusObj=0;ChrCam_GetOffset(&viewer,&giantOffset,mode);assert(giantOffset.z==normalOffset.z);
  testFocusObj=&o;
  online=1;ChrCam_GetOffset(&viewer,&giantOffset,mode);assert(giantOffset.z==normalOffset.z);online=0;
  active=0;ChrCam_GetOffset(&viewer,&giantOffset,mode);assert(giantOffset.z==normalOffset.z);active=1;
  testOwnCameraObj=0;
 }
 cameraPos.x=cameraPos.z=0;cameraPos.y=-20;
 Port_ChrCamGiantFocus(0,&cameraPos,&cameraDelta,10);
 assert(cameraDelta.x==6&&cameraDelta.y==-56.25f&&cameraDelta.z==12);
 active=0;cameraDelta.y=123;Port_ChrCamGiantFocus(0,&cameraPos,&cameraDelta,10);assert(cameraDelta.y==123);active=1;
 node.mtx.m[3][1]=-180;assert(Port_ZenkaiGiantFocus(&o,&target));assert(target.y==-96.25f);
 online=1;assert(!Port_ZenkaiGiantFocus(&o,&target));online=0;
 active=0;assert(!Port_ZenkaiGiantFocus(&o,&target));active=1;
 testHead=0;assert(!Port_ZenkaiGiantFocus(&o,&target));
 memset(&small,0,sizeof small);memset(&smallParam,0,sizeof smallParam);small.mdl.param=&smallParam;
 testOpponent=&small;assert(Port_ZenkaiGiantVsSmall(&o));
 testOwnCameraObj=&o;assert(BtlMove_GetApproachHeightDiff(&viewer)==7.5f);testOwnCameraObj=0;
 assert(Port_ZenkaiGiantApproachOffset(&o)==-7.5f); /* height 25 -> animation height 10 */
 assert(12.5f-5+Port_ZenkaiGiantApproachOffset(&o)==0); /* same-height roots can engage a height-10 fighter */
 small.mdl.param=&param;assert(!Port_ZenkaiGiantVsSmall(&o));assert(Port_ZenkaiGiantApproachOffset(&o)==0);small.mdl.param=&smallParam;
 online=1;assert(!Port_ZenkaiGiantVsSmall(&o));assert(Port_ZenkaiGiantApproachOffset(&o)==0);online=0;
 active=0;assert(!Port_ZenkaiGiantVsSmall(&o));assert(Port_ZenkaiGiantApproachOffset(&o)==0);active=1;
 testOpponent=0;assert(!Port_ZenkaiGiantVsSmall(&o));
 strike.min[0]=10;strike.max[0]=20;strike.min[1]=-60;strike.max[1]=-40;strike.min[2]=30;strike.max[2]=40;
 o.xf.rootPos.y=0;o.xf.scale=4;
 Port_BtlObjMeleeReachBounds(&o,&strike);
 assert(strike.min[1]==-60&&strike.max[1]==-10); /* reaches a small fighter at -12 */
 assert(strike.min[0]==2.5f&&strike.max[0]==20&&strike.min[2]==7.5f&&strike.max[2]==40);
 Port_BtlObjMeleeWorldBox(&attackBox,&strike);
 targetBounds.min[0]=3;targetBounds.max[0]=4;targetBounds.min[1]=-14;targetBounds.max[1]=-11;targetBounds.min[2]=8;targetBounds.max[2]=9;
 Port_BtlObjMeleeWorldBox(&targetBox,&targetBounds);
 assert(ColObb_Overlaps(&attackBox,&targetBox)); /* real narrow phase: close small target */
 targetBounds.min[0]=21;targetBounds.max[0]=22;Port_BtlObjMeleeWorldBox(&targetBox,&targetBounds);
 assert(!ColObb_Overlaps(&attackBox,&targetBox)); /* beyond the strike still misses */
 targetBounds.min[0]=-4;targetBounds.max[0]=-3;Port_BtlObjMeleeWorldBox(&targetBox,&targetBounds);
 assert(!ColObb_Overlaps(&attackBox,&targetBox)); /* behind the strike still misses */
 /* Translation must not change coverage relative to the fighter. */
 o.xf.rootPos.x=100;strike.min[0]=110;strike.max[0]=120;
 Port_BtlObjMeleeReachBounds(&o,&strike);assert(strike.min[0]==102.5f&&strike.max[0]==120);
 o.xf.scale=1;strike.max[1]=-40;Port_BtlObjMeleeReachBounds(&o,&strike);assert(strike.max[1]==-40);
 ((u8 *)&param)[2]=0; BtlObjXf_Reset(&o); BtlObjBody_Init(&o);
 assert(o.xf.scale==1); assert(o.body.size[0]==10); assert(o.body.radius==26);
 ((u8 *)&param)[2]=4; active=0; BtlObjXf_Reset(&o); assert(o.xf.scale==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='zenkai-giants-') as tmp:
    tmp = Path(tmp)
    (tmp / 'test.c').write_text(harness + body + tests)
    collision_source = (ROOT / 'src/battle/col_a.c').read_text()
    declarations = '\n'.join(line for line in collision_source.splitlines() if line.startswith('extern '))
    (tmp / 'collision.c').write_text('#include "common.h"\n#include "battle/col_a.h"\n' + declarations + '\n' +
        '\n'.join(function(collision_source, n) for n in ('ColObb_Init', 'ColObb_Update', 'ColObb_Overlaps')))
    subprocess.run(['gcc', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-std=gnu99', '-g', '-fno-builtin', '-Wno-incompatible-pointer-types', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'test.c'), str(tmp / 'collision.c'), 'src/battle/port_zenkai_giants.c', '-lm', '-o', str(tmp / 'test')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'test')], check=True)
    # Disabled build: exercise real sizing and collision entry points as well.
    (tmp / 'disabled.c').write_text(harness + body + r'''
int main(void) {
 BobjObj o={0};BobjParam param={0};BobjModelHdr model={0};Vec4 target={0};int i;
 active=1;((u8 *)&param)[2]=4;o.mdl.param=&param;o.mdl.model=&model;
 param.radius=20;for(i=0;i<4;i++)model.size[i]=10+i;
 BtlObjXf_Reset(&o);assert(o.xf.scale==1);
 BtlObjBody_Init(&o);assert(o.body.radius==20);
 for(i=0;i<4;i++)assert(o.body.size[i]==model.size[i]);
 testOpponent=&o;
 for(cinematic=0;cinematic<2;cinematic++) {
  assert(Port_ZenkaiGiantScale(0,&param)==1);
  assert(Port_ZenkaiGiantAnimationScale(0,&param)==1);
  assert(Port_ZenkaiGiantEffectScale(0)==1);
  assert(!Port_ZenkaiGiantFocus(&o,&target));
  assert(!Port_ZenkaiGiantVsSmall(&o));
  assert(Port_ZenkaiGiantApproachOffset(&o)==0);
  assert(Port_ZenkaiGiantContactRadius(&o,&target,5)==5);
 }
 return 0;
}
''')
    subprocess.run(['gcc', '-DPORT_ZENKAI_GIANTS=0', '-ffunction-sections', '-fdata-sections',
                    '-Wl,--gc-sections', '-std=gnu99', '-fno-builtin', '-Wno-incompatible-pointer-types',
                    '-fsanitize=undefined', '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'disabled.c'), str(tmp / 'collision.c'), 'src/battle/port_zenkai_giants.c',
                    '-lm', '-o', str(tmp / 'disabled')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'disabled')], check=True)
    print('Disabled giant flag restores original sizing and disables giant helpers, including cinematics.')
    radius_source = function((ROOT / 'src/battle/btl_capi_a.c').read_text(), 'BtlCharApi_GetRadius')
    (tmp / 'radius.c').write_text(r'''
#include "common.h"
#include "battle/btl_capi_a.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BtlCapiObj object;
static int active;
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return 0; }
int Port_NetActive(void) { return 0; }
static int cinematic;
s32 EftCam_IsActive(void) { return cinematic; }
BtlCapiObj *BtlObj_Get(s32 id) { return id==0 ? &object : 0; }
BtlCapiChr *BtlChar_FindByObjId(s32 id) { (void)id;return 0; }
s32 BtlOpp_GetObjId(BtlCapiChr *c) { (void)c;return -1; }
''' + radius_source + r'''
int main(void) {
 BtlCapiParam param = {0}; object.param=&param;
 ((u8 *)&param)[2]=4; param.radius=20; active=1;
 assert(BtlCharApi_GetRadius(0)==20); /* close combat retains original approach distance */
 active=0; assert(BtlCharApi_GetRadius(0)==20);
 active=1;((u8 *)&param)[2]=0;assert(BtlCharApi_GetRadius(0)==20);
 ((u8 *)&param)[2]=4;param.radius=0;object.unkFF0=15;
 assert(BtlCharApi_GetRadius(0)==15*1.8f/2.5f); /* undo body-size scale for separation */
 active=0;assert(BtlCharApi_GetRadius(0)==15*1.8f);
 assert(BtlCharApi_GetRadius(1)==0);return 0;
}
''')
    subprocess.run(['gcc', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'radius.c'), 'src/battle/port_zenkai_giants.c', '-lm', '-o', str(tmp / 'radius')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'radius')], check=True)
    pose_source = function((ROOT / 'src/battle/btl_char_ctl_d.c').read_text(), 'BtlChar_PoseToObj')
    (tmp / 'pose.c').write_text(r'''
#include "common.h"
#include "battle/btl_char_ctl.h"
#include "sys/port_zenkai.h"
#include <assert.h>
#include <math.h>
static BtlCtlObj object;
static int active=1, online;
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return online; }
int Port_NetActive(void) { return 0; }
static int cinematic;
s32 EftCam_IsActive(void) { return cinematic; }
BtlCtlObj *BtlChar_GetObj(BtlCtlChr *c) { return &object; }
BtlCtlPose *BtlChar_GetPos(BtlCtlChr *c) { return &c->pose; }
s32 BtlChar_TestFlag(BtlCtlChr *c,s32 f) { return 0; }
void Vec4_Copy(Vec4 *d,Vec4 *s) { *d=*s; }
void Vec4_Add(Vec4 *d,Vec4 *a,Vec4 *b) { Vec4 v={a->x+b->x,a->y+b->y,a->z+b->z,a->w+b->w};*d=v; }
void Vec4_Sub(Vec4 *d,Vec4 *a,Vec4 *b) { Vec4 v={a->x-b->x,a->y-b->y,a->z-b->z,a->w-b->w};*d=v; }
void Vec4_Scale(Vec4 *d,Vec4 *a,f32 s) { Vec4 v={a->x*s,a->y*s,a->z*s,a->w*s};*d=v; }
f32 Vec3_Length(Vec4 *v) { return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z); }
void Mtx_StoreIdentity(Mtx44 *m) { (void)m; }
void Mtx_RotateY(Mtx44 *d,Mtx44 *s,f32 a) { (void)d;(void)s;(void)a; }
void Mtx_RotateX(Mtx44 *d,Mtx44 *s,f32 a) { (void)d;(void)s;(void)a; }
void Mtx_RotateZ(Mtx44 *d,Mtx44 *s,f32 a) { (void)d;(void)s;(void)a; }
/* A pre-rotated root offset; these checks isolate scale application and order. */
void Mtx_MulVec4(Vec4 *d,Mtx44 *m,Vec4 *v) { *d=*v; }
void BtlObjXf_Update(BtlCtlObj *o) { assert(o->scale>0); }
''' + pose_source + r'''
int main(void) {
 BtlCtlChr c={0}; u8 param[16]={0}; int i;
 object.fighterParam=param;param[2]=4;c.scale=1;
 c.pose.pos.x=100;c.pose.rootPos.x=2;
 for(i=0;i<3;i++) { BtlChar_PoseToObj(&c,0);assert(object.scale==2.5f);assert(object.pos.x==95); }
 cinematic=1;BtlChar_PoseToObj(&c,0);assert(object.scale==1);assert(object.pos.x==98);
 BtlChar_PoseToObj(&c,0);assert(object.scale==1);assert(object.pos.x==98);
 cinematic=0;BtlChar_PoseToObj(&c,0);assert(object.scale==2.5f);assert(object.pos.x==95);

 c.scale=0.5f;BtlChar_PoseToObj(&c,0);assert(object.scale==1.25f);assert(object.pos.x==97.5f);
 active=0;BtlChar_PoseToObj(&c,0);assert(object.scale==0.5f);
 active=1;online=1;BtlChar_PoseToObj(&c,0);assert(object.scale==0.5f);
 online=0;param[2]=0;BtlChar_PoseToObj(&c,0);assert(object.scale==0.5f);
 param[2]=4;object.type=1;BtlChar_PoseToObj(&c,0);assert(object.scale==0.5f);
 return 0;
}
''')
    subprocess.run(['gcc', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'pose.c'), 'src/battle/port_zenkai_giants.c', '-lm', '-o', str(tmp / 'pose')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'pose')], check=True)
    targeting = (ROOT / 'src/battle/btl_char_flag_opp.c').read_text()
    target_functions = '\n'.join(function(targeting, n) for n in ('BtlOpp_GetTargetPos', 'BtlOpp_GetHalfHeightDiff'))
    (tmp / 'focus.c').write_text(r'''
#include "common.h"
#include "battle/btl_char_flag.h"
#include "battle/bobj_a.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BtlFlagChr chars[2];
static BobjObj objects[2];
static BobjPart head;
static u8 giantParam[16];
static int active=1,online;
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return online; }
int Port_NetActive(void) { return 0; }
static int cinematic;
s32 EftCam_IsActive(void) { return cinematic; }
BtlFlagChr *BtlChar_Get(s32 i) { return &chars[i]; }
void *BtlChar_GetObj(BtlFlagChr *c) { return &objects[c->player]; }
void BtlChar_GetSnapPos(BtlFlagChr *c,Vec4 *v,s32 slot) { v->x=1000+c->player;v->y=v->z=0;v->w=1; }
f32 BtlCharApi_GetHeight(s32 id) { return id==1?160:10; }
BobjPart *BtlObj_GetNode(BobjObj *o,s32 node) { return node==0x30?&head:0; }
#define OPPONENT(chr) ((chr)->player==0?BtlChar_Get(1):BtlChar_Get(0))
''' + target_functions + r'''
int main(void) {
 Vec4 v;chars[0].player=0;chars[1].player=1;chars[0].objId=0;chars[1].objId=1;
 giantParam[2]=4;objects[1].mdl.param=(BobjParam *)giantParam;objects[1].body.size[0]=160;
 head.mtx.m[3][0]=12;head.mtx.m[3][1]=-150;head.mtx.m[3][2]=24;
 BtlOpp_GetTargetPos(&chars[0],&v);assert(v.x==6&&v.y==-115&&v.z==12);
 assert(BtlOpp_GetHalfHeightDiff(&chars[0])==5); /* no second giant half-height shift */
 BtlOpp_GetTargetPos(&chars[1],&v);assert(v.x==1000); /* giant targeting a normal opponent */
 active=0;BtlOpp_GetTargetPos(&chars[0],&v);assert(v.x==1001);
 assert(BtlOpp_GetHalfHeightDiff(&chars[0])==-75);
 active=1;online=1;BtlOpp_GetTargetPos(&chars[0],&v);assert(v.x==1001);
 return 0;
}
''')
    subprocess.run(['gcc', '-std=gnu99', '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                    '-fno-builtin', '-fsanitize=undefined', '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'focus.c'), 'src/battle/port_zenkai_giants.c', '-lm', '-o', str(tmp / 'focus')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'focus')], check=True)
    aim_wrapper = function((ROOT / 'src/battle/btl_capi_a.c').read_text(), 'BtlCharApi_CalcAimDir45')
    aim_keep = function((ROOT / 'src/battle/eft_a.c').read_text(), 'EftAim_GetDirKeep')
    (tmp / 'blast.c').write_text(r'''
#include "common.h"
#include "battle/eft_a.h"
#include <math.h>
#include <assert.h>
#define Vec4 EftVec
static int assist=1,locked=1,backwards;
static f32 desiredPitch=1.4f,lastLimit;
static EftVec actualTarget={0,-100,-10,1};
s32 BtlCharApi_GetOpponentObjId(s32 id) { return 1; }
void BtlCharApi_GetNodePos(s32 id,s32 node,EftVec *out) { *out=actualTarget; }
void Vec4_Sub(EftVec *out,EftVec *a,EftVec *b) { out->x=a->x-b->x;out->y=a->y-b->y;out->z=a->z-b->z;out->w=0; }
f32 Vec3_Length(EftVec *v) { return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z); }
void Vec4_Scale(EftVec *out,EftVec *v,f32 s) { *out=*v;out->x*=s;out->y*=s;out->z*=s; }

void *BtlObj_Get(s32 id) { return 0; }
int Port_ZenkaiGiantVsSmall(void *o) { return assist; }
s32 BtlCharApi_IsLockedOn(s32 id) { return locked; }
void BtlCharApi_GetRot(s32 id,EftVec *v) { v->x=v->y=v->z=0; }
void Vec4_Copy(EftVec *d,EftVec *s) { *d=*s; }
f32 Vec3_Dot(EftVec *a,EftVec *b) { return a->x*b->x+a->y*b->y+a->z*b->z; }
void Vec3_Normalize(EftVec *d,EftVec *s) { f32 n=sqrtf(Vec3_Dot(s,s));*d=*s;if(n>0){d->x/=n;d->y/=n;d->z/=n;} }
void BtlCharApi_CalcAimDir(s32 id,s32 node,EftVec *from,EftVec *out,f32 lo,f32 hi) {
 f32 pitch=desiredPitch;lastLimit=hi;if(pitch<lo)pitch=lo;if(pitch>hi)pitch=hi;
 out->x=0;out->y=sinf(pitch);out->z=cosf(pitch)*(backwards?-1:1);out->w=0;
}
''' + aim_wrapper + aim_keep + r'''
int main(void) {
 EftHitSrc src={0};EftHitDef def={0};EftVec out;src.def=&def;
 EftAim_GetDirKeep(&src,&out,0,0);
 assert(lastLimit>1.57f&&out.y>0.98f&&out.z>0.16f&&out.z<0.18f);
 assist=0;EftAim_GetDirKeep(&src,&out,0,0);assert(lastLimit<0.79f&&out.y<0.71f);
 assist=1;backwards=1;EftAim_GetDirKeep(&src,&out,0,0);assert(out.y>0.98f&&out.z<0);
 backwards=0;desiredPitch=1.5707963f;EftAim_GetDirKeep(&src,&out,0,0);assert(out.y>0.999f);
 desiredPitch=1.4f;def.aimMode=1;src.aimFlags=0;EftAim_GetDirKeep(&src,&out,0,0);
 desiredPitch=0;EftAim_GetDirKeep(&src,&out,0,0);assert(out.y==0); /* giant refreshes launch pose */
 assist=0;desiredPitch=0.5f;src.aimFlags=0;EftAim_GetDirKeep(&src,&out,0,0);
 desiredPitch=0;EftAim_GetDirKeep(&src,&out,0,0);assert(out.y>0.47f); /* normal aim-once retained */
 assist=1;
 { EftVec from={0,40,20,1};
   BtlCharApi_CalcAimDir45(0,&from,&out);
   assert(out.y<-0.97f&&out.z<0); /* enlarged muzzle beyond target: never flip yaw */
   assert(fabsf(out.z/out.y-30.0f/140.0f)<0.0001f);
 }
 locked=0;EftAim_GetDirKeep(&src,&out,0,0);assert(out.y==0&&out.z==1);
 return 0;
}
''')
    subprocess.run(['gcc', '-std=gnu99', '-fno-builtin', '-Wno-int-conversion', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h', str(tmp / 'blast.c'),
                    '-lm', '-o', str(tmp / 'blast')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'blast')], check=True)
print('Giant sizing checks passed (save/online gates, per-frame pose/root motion, three camera presets (3x against giants), midpoint camera/attack targeting, steep super-blast aiming, dimensions, inverse, giant-versus-small melee coverage, melee boxes, projectile hurt spheres/bounds).')
