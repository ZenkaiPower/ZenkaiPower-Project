#!/usr/bin/env python3
"""Exercise production sky-preview lifecycle and original charge-state gating."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
api = (root/'src/battle/btl_char_api.c').read_text()
a = api.index('s32 Port_BtlSpiritBombEnabled(')
b = api.index('\n#endif', a)
gate = api[a:b]
fx = (root/'src/battle/eft_j.c').read_text()
a = fx.index('static void Port_SpiritBombHide(')
b = fx.index('/* Sky effects own their resources', a)
helpers = fx[a:b]
emitter = (root/'src/battle/eft_h.c').read_text()
a = emitter.index('void Port_EftEmitHoldScale(')
b = emitter.index('\n}', a)+2
hold = emitter[a:b].replace('Port_EftEmitHoldScale(', 'Test_HoldScale(')
a=emitter.index('f32 Port_EftSpiritBombBottom(');b=emitter.index('\n}',a)+2
bottom=emitter[a:b].replace('Port_EftSpiritBombBottom(', 'Test_Bottom(')
a = fx.index('#ifdef PORT\n    if (src->def->id == 0x268)',fx.index('void EftBlast_Init('))
b = fx.index('\n#endif',a)+len('\n#endif')
fired = 'void Test_Fired(EftBlast *w,EftJSrc *src) { EftJDef *def=src->def;\n'+fx[a:b]+'\n}\n'
z=(root/'src/battle/eft_z.c').read_text()
a=z.index('s32 Port_EftBillWorldDepth(');b=z.index('\n}',a)+2
depth=z[a:b]
nodes=(root/'src/battle/eft_i.c').read_text()
a=nodes.index('void Port_EftEmitOffsetNodes(');b=nodes.index('\n}',a)+2
offset=nodes[a:b].replace("void Port_EftEmitOffsetNodes(", "void Test_OffsetNodes(")+"\nvoid Port_EftEmitOffsetNodes(void *n,float y) { Test_OffsetNodes(n,y); }\n"
source = r'''
#include <assert.h>
#include <math.h>
#include "battle/eft_z.h"
#include <string.h>
#define PORT 1
#ifndef PORT_ZENKAI_SPIRIT_BOMB
#define PORT_ZENKAI_SPIRIT_BOMB 1
#endif
#include "battle/eft_j.h"
#include "battle/eft_h.h"
#define EftEmit_KillAll Decl_KillAll
#define EftEmit_UpdateAlive Decl_UpdateAlive
#define EftEmit_Spawn Decl_Spawn
#include "battle/eft_i.h"
#undef EftEmit_KillAll
#undef EftEmit_UpdateAlive
#undef EftEmit_Spawn
typedef struct { int unkE5C; } BtlCharApiChr;
static BtlCharApiChr fighter;
static int pack=1, character=1, dead, technique, stopped;
int Port_ZenkaiPackEnabled(void) { return pack; }
BtlCharApiChr *BtlChar_FindByObjId(int id) { return id==1 ? &fighter:0; }
int *BtlMember_GetActive(BtlCharApiChr *p) { return &character; }
int BtlChar_IsDead(BtlCharApiChr *p) { return dead; }
int BtlCharApi_IsInTechnique(int id) { return technique; }
int BtlAct_GetCurrent(BtlCharApiChr *p) { return technique; }
int BtlAct_IsTechniqueId(int id) { return id != 0; }
int BtlAct_GetCurrentClass(BtlCharApiChr *p) { return technique == 1 ? 4:2; }
int BtlSuper_GetId(BtlCharApiChr *p,int cls) { return 0x268; }
''' + gate + r'''
static int starts, kills, updates, spawns, holds;
static void Port_EftEmitHoldScale(EftJSet *s,void *e,int group,int part) { holds++; }
static void Port_EftEmitWorldDepth(EftJSet *s,void *e,int group,int part) {}
static float Port_EftSpiritBombBottom(EftJSet *s,float scale,int phase,float dirY) { return 100.0f*scale; }
/* The real stage cache survives technique/cinematic pauses. */
static int Port_SpiritBombStageCharge(int side) {
    return Port_BtlSpiritBombEnabled(side) && !dead ? fighter.unkE5C:0;
}
static int fire;
static float handY=200,lowerHandY=260;
static void BtlCharApi_GetNodePos(int id,int node,EftJVec *out) {
    assert(id==1 && (node==10 || node==14));
    out->x=out->z=0;out->y=node==10 ? handY:lowerHandY;out->w=1;
}
static int EftShot_TestBits(int id,int bits) { return fire && bits==4; }
static float drawn;
static int BtlScene_IsCharStopped(int id) { return stopped; }
static float BtlStage_GetTop(void) { return -1200; }
static void EftEmit_InitState(EftJSet *s,void *e) { starts++; }
static void EftEmit_TermState(EftJSet *s,void *e) {}
static void EftEmit_KillAll(EftJSet *s,void *e) { kills++; }
static void EftEmit_UpdateAlive(EftJSet *s,void *e) { updates++; }
static int EftEmit_GetFlags(EftJSet *s,void *e,int id,int g,int j,int end,int kill,int req) {
    assert(id==1 && !end && !kill && (req==0 || req==2));
    return req ? 1:4;
}
static void EftEmit_Spawn(EftJSet *s,void *e,void *nodes,void *pos,void *dir,
    int id,int node,int own,int g,int j,int flags,float scale) {
    EftJVec *p=pos;
    assert(id==1 && p->x==0 && fabsf(p->y+100.0f*scale+1260.0f)<0.001f && p->z==0);
    assert(*(unsigned *)nodes==0x555);
    drawn=scale;spawns++;
}
''' + helpers + hold + bottom + fired + offset + r'''
typedef struct { int objId,unkE5C; } TestChr;
void EftBill_Update(EftZTask *t) {}
'''+depth+r'''
int main(void) {
    EftBlastMgr m={0}; EftJSrc src={0}; EftJDef def={0};
    unsigned mask=1; unsigned char info[]={0,1};
    assert(sizeof(m)<0x2000);
    { EftEmitNodes n={0};n.flags=0x555;n.flagsC=0x3F;
      for(int i=0;i<6;i++) { n.n[i][0].pos.y=100;n.n[i][1].pos.y=100;n.c[i].pos.y=100; }
      Port_EftEmitOffsetNodes(&n,-25);
      assert(n.n[0][0].pos.y==75 && n.c[5].pos.y==75);
      n.flags|=0x1000;Port_EftEmitOffsetNodes(&n,-25);
      assert(n.n[0][0].pos.y==75); /* frozen nodes do not accumulate offsets */
    }
    { EftSet set={0};EftSetHead head={0};EftSetGroupDef group={0};EftSetDef part={0};
      float shape[32]={0};head.mask=1<<16;set.head=&head;set.parts=&part;
      set.group[16].entry=&group;group.nParts=1;set.pair[0].a=(s32 *)shape;
      shape[0x34/4]=32;shape[0x34/4+1]=16;shape[0x34/4+2]=24;
      part.unk1C=5;part.unk10=2;
      assert(fabsf(Test_Bottom(&set,1,0,-1)-8)<0.001f);
      for(int tier=0;tier<4;tier++) {
        float scale=0.25f*(tier+1),full=Test_Bottom(&set,1,0,-1);
        float edge=Test_Bottom(&set,scale,0,-1);
        float center=-1200-full+(full-edge);
        assert(fabsf(center+edge+1200)<0.001f);
        assert(fabsf((full-edge)+edge-full)<0.001f);
      }
      assert(Test_Bottom(&set,1,1,-1)==0); /* other phase is excluded */
      assert(shape[0x34/4]==32); /* shared definitions stay intact */
    }
    { EftZTask t={0};EftBill bill={0};void *cls[]={EftBill_Update};
      t.cls=cls;t.work=&bill;bill.flags=EFT_BILL_ALIVE|EFT_BILL_FRONT|EFT_BILL_VISIBLE;
      assert(Port_EftBillWorldDepth(&t));
      assert(!(bill.flags&EFT_BILL_FRONT) && (bill.flags&EFT_BILL_VISIBLE));
      assert(!Port_EftBillWorldDepth(0));
    }
    if (!PORT_ZENKAI_SPIRIT_BOMB) { fighter.unkE5C=3;assert(!Port_BtlSpiritBombCharge(1));return 0; }
    { EftSet set={0};EftSetState state={0};EftSetDef parts[3]={{0}};
      set.parts=parts;set.group[5].firstPart=1;parts[2].unk1C=5.0f;
      state.unk168[2]=0.0f;state.unk208[2]=90.0f;state.unk208[1]=77.0f;
      Test_HoldScale(&set,&state,5,1);
      assert(state.unk168[2]==5.0f && state.unk208[2]==0.0f && state.unk208[1]==77.0f);
      assert(parts[2].unk1C==5.0f);
    }
    src.objId=0;src.objId20=1;src.def=&def;def.scale=1;
    m.spiritSrc=&src;m.set.mask=&mask;m.set.grp[0].info=info;
    { EftBlast w={0};src.objId=1;def.id=0x268;
      const float sizes[]={0.5f,0.7f,0.85f,1.0f};
      for(int charge=0;charge<=3;charge++) {
        fighter.unkE5C=charge;w.flags=0x1800;Test_Fired(&w,&src);
        assert(fabsf(w.scale-sizes[charge])<0.0001f && !(w.flags&0x1000));
      }
      src.objId=0;
    }
    fighter.unkE5C=0;Port_SpiritBombUpdate(&m);assert(!starts && !spawns);
    fighter.unkE5C=1;Port_SpiritBombUpdate(&m);assert(starts==1 && spawns==1 && holds==1 && m.spiritOn);
    float small=drawn;
    fighter.unkE5C=2;for(int i=0;i<60;i++) Port_SpiritBombUpdate(&m);
    float medium=drawn;assert(medium>small && fabsf(medium-0.75f)<0.001f);
    fighter.unkE5C=3;for(int i=0;i<60;i++) Port_SpiritBombUpdate(&m);
    assert(drawn>medium && fabsf(drawn-1.0f)<0.001f && starts==1);
    stopped=1;int before=updates;Port_SpiritBombUpdate(&m);assert(updates==before);stopped=0;
    technique=2;Port_SpiritBombUpdate(&m);assert(m.spiritOn && kills==0);technique=0;
    technique=1;stopped=1;Port_SpiritBombUpdate(&m);assert(!m.spiritOn && kills==1);technique=0;stopped=0;
    Port_SpiritBombUpdate(&m);assert(m.spiritOn && starts==2);
    fighter.unkE5C=0;Port_SpiritBombUpdate(&m);assert(!m.spiritOn && kills==2);
    fighter.unkE5C=3;character=0;assert(!Port_BtlSpiritBombCharge(1));
    character=2;assert(!Port_BtlSpiritBombCharge(1));
    character=1;pack=0;assert(!Port_BtlSpiritBombCharge(1));pack=1;
    dead=1;assert(!Port_BtlSpiritBombCharge(1));dead=0;
    assert(!Port_BtlSpiritBombCharge(99));
    fighter.unkE5C=99;assert(Port_BtlSpiritBombCharge(1)==3);
    fighter.unkE5C=-1;assert(!Port_BtlSpiritBombCharge(1));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'spirit.c';path.write_text(source)
    exe=Path(tmp)/'spirit'
    for flags in [[], ['-DPORT_ZENKAI_SPIRIT_BOMB=0']]:
        subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include')]+flags+[str(path),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('Spirit Bomb charge gating, growth and lifecycle checks passed')
