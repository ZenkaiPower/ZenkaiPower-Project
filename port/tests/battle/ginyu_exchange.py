"""Exercise production Ginyu exchange planning, HP transfers and reload completion."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'src/battle/btl_char_mgr.c').read_text()
a=source.index('typedef struct PortGinyuExchange')
b=source.index('\n#endif',a)
production=source[a:b]
harness=r'''
#include "common.h"
#include "battle/btl_char_mgr.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BtlMgrChr fighters[2];
static int pack=1,boost,requests,neutral;
s32 BtlAct_GetCurrent(BtlMgrChr *c) {return 0;}
s32 BtlChar_TestFlag(BtlMgrChr *c,s32 flag) {return 0;}
BtlMgrChr *BtlChar_Get(s32 side) {return &fighters[side];}
BtlMgrMember *BtlMember_Get(BtlMgrChr *c,s32 i) {return &c->members[i];}
BtlMgrMember *BtlMember_GetActive(BtlMgrChr *c) {return &c->members[c->curMember];}
BtlMgrGauge *BtlMember_GetActiveGauge(BtlMgrChr *c) {return &BtlMember_GetActive(c)->gauge;}
void BtlMember_LoadParams(BtlMgrChr *c,s32 i,s32 init,f32 health,s32 variant) {c->members[i].gauge.healthMax=30000;}
int Port_ZenkaiPackEnabled(void) {return pack;}
int Port_GinyuSelfDamageActive(void *c) {return boost;}
s32 BtlUtil_Clamp(s32 v,s32 low,s32 high) {return v<low?low:v>high?high:v;}
void BtlChange_RequestChara(s32 side,s32 chara,s32 costume,s32 variant,s32 anim,s32 data,s32 voice) {
 assert(side==1 && chara==86 && anim==-1 && data==-1 && voice==-1);requests++;
}
void BtlAnim_Play(BtlMgrChr *c,s32 anim,f32 t) {assert(anim==0);}
void BtlAct_Request(BtlMgrChr *c,s32 action) {assert(action==0xB);neutral++;}
'''
tests=r'''
static void reset(void) {
 memset(fighters,0,sizeof(fighters));memset(sGinyuExchange,0,sizeof(sGinyuExchange));
 memset(sGinyuReplaced,0,sizeof(sGinyuReplaced));memset(sGinyuOpponentLoad,0,sizeof(sGinyuOpponentLoad));
 requests=neutral=boost=0;pack=1;
 fighters[0].side=0;fighters[0].memberCount=1;fighters[0].nextMember=-1;
 fighters[1].side=1;fighters[1].memberCount=3;fighters[1].nextMember=1;
 fighters[0].members[0].chara=86;fighters[0].members[0].present=1;fighters[0].members[0].gauge.health=10000;
 fighters[1].members[0].chara=1;fighters[1].members[0].present=1;fighters[1].members[0].gauge.health=24000;fighters[1].members[0].gauge.healthMax=30000;
 fighters[1].members[1].chara=25;fighters[1].members[1].present=1;fighters[1].members[1].gauge.health=17000;
 fighters[1].members[2].chara=13;fighters[1].members[2].present=1;
}
int main(void) {
 int chara,costume,variant;
 reset();
 if(!PORT_ZENKAI_GINYU_BODY_CHANGE) {
  assert(!Port_GinyuChooseOpponentBody(&fighters[0],&chara,&costume,&variant));return 0;
 }
 assert(!Port_GinyuChooseOpponentBody(&fighters[0],&chara,&costume,&variant));
 assert(!sGinyuExchange[0].pending);
 Port_GinyuCommitExchange(&fighters[0]);
 assert(fighters[0].members[0].chara==86 && fighters[0].members[0].gauge.health==10000);
 assert(fighters[1].members[0].chara==1 && fighters[1].members[0].gauge.health==24000);
 assert(fighters[1].members[1].chara==25 && fighters[1].members[1].gauge.health==17000);
 assert(!requests && !Port_GinyuReplacedMember(1));
 reset();fighters[0].members[0].bonus[0]=3;fighters[1].members[0].bonus[0]=9;boost=1;fighters[0].members[0].chara=29;fighters[0].members[0].gauge.unk30=1;fighters[0].members[0].gauge.health=5000;
 assert(Port_GinyuChooseOpponentBody(&fighters[0],&chara,&costume,&variant));
 assert(chara==1 && sGinyuExchange[0].member==0);
 fighters[0].members[0].chara=chara;fighters[0].members[0].gauge.healthMax=30000;
 Port_GinyuCommitExchange(&fighters[0]);
 assert(fighters[0].members[0].gauge.health==24000);
 assert(fighters[1].members[0].chara==86 && fighters[1].members[0].gauge.health==5000);
 assert(requests==1 && Port_GinyuOpponentReload(1));
 assert(fighters[0].members[0].bonus[0]==9 && fighters[1].members[0].bonus[0]==3);
 assert(Port_GinyuMoveOwner(1)==1);
 assert(Port_GinyuReplacedMember(1) && Port_GinyuReloadMember(1)==0);
 assert(fighters[1].members[1].chara==25 && fighters[1].members[1].gauge.health==17000);
 Port_GinyuFinishOpponentLoad(1);
 assert(!Port_GinyuOpponentReload(1) && fighters[1].members[0].gauge.health==5000);
 assert(fighters[1].members[0].gauge.ki==0 && neutral==1);
 reset();fighters[1].members[1].gauge.health=0;
 assert(!Port_GinyuChooseOpponentBody(&fighters[0],&chara,&costume,&variant));
 reset();pack=0;
 assert(!Port_GinyuChooseOpponentBody(&fighters[0],&chara,&costume,&variant));
 assert(fighters[1].members[1].chara==25);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+production+tests)
 for flag in (0,1):
  subprocess.run(['gcc','-std=gnu99','-fno-builtin','-w','-no-pie','-fsanitize=address,undefined',
                  f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}', '-Iinclude','-Iport/include','-include','port_compat.h',
                  str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
  subprocess.run([str(path/'test')],check=True)
print('Ginyu exchange checks passed: boosted active target, unchanged reserves, HP transfer, queued reload, preset fallback and feature gates.')

source=(ROOT/'src/battle/btl_char_status.c').read_text()
a=source.index('int Port_GinyuSelfDamageActive(');b=source.index('\n}',a)+2
harness=r'''
#include "common.h"
#include "battle/btl_char_status.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static int pack=1,skill=0x17;
int Port_ZenkaiPackEnabled(void) {return pack;}
s32 BtlSkill_GetId(BtlStatChr *c,s32 slot) {return skill;}
'''
tests=r'''
int main(void) {
 BtlStatChr chr={0};
 assert(!Port_GinyuSelfDamageActive(&chr));
 chr.slotOn[1]=1;chr.stat[0].untilA[1]=2;
 assert(Port_GinyuSelfDamageActive(&chr));
 chr.stat[0].untilA[1]=0;assert(!Port_GinyuSelfDamageActive(&chr));
 chr.stat[1].untilA[1]=7;assert(Port_GinyuSelfDamageActive(&chr));
 chr.slotOn[1]=0;assert(!Port_GinyuSelfDamageActive(&chr));
 chr.slotOn[1]=1;skill=0x18;assert(!Port_GinyuSelfDamageActive(&chr));
 skill=0x17;pack=0;assert(!Port_GinyuSelfDamageActive(&chr));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+source[a:b]+tests)
 subprocess.run(['gcc','-std=gnu99','-fno-builtin','-w','-no-pie','-fsanitize=address,undefined',
                 '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Ginyu power-up eligibility checks passed: actual effect, cleared modifiers, expired slot, unrelated skill and Classic gate.')

# Exercise the actual reload freeze accessor and model-loaded animation setup.
source=(ROOT/'src/battle/btl_char_get.c').read_text()
a=source.index('s32 BtlChar_IsFrozen(');b=source.index('\n}',a)+2
frozen=source[a:b]
source=(ROOT/'src/battle/btl_char_mgr.c').read_text()
a=source.index('void BtlChar_OnModelLoaded(')
b=source.index('    switch (BtlAct_GetCurrent(chr))',a)
loaded=source[a:b]+'}\n'
harness=r'''
#include "common.h"
#include "battle/btl_char_mgr.h"
#define BtlChar_GetObj BtlCharGet_UnusedGetObjDeclaration
#include "battle/btl_char_get.h"
#undef BtlChar_GetObj
#include "sys/port_zenkai.h"
#include <assert.h>
static int reload[2],played,flushed,sampled;
static BtlMgrMember member;
static BtlMgrObj object;
int Port_GinyuOpponentReload(int side) {return reload[side];}
BtlMgrMember *BtlMember_GetActive(BtlMgrChr *c) {return &member;}
BtlMgrObj *BtlChar_GetObj(BtlMgrChr *c) {return &object;}
void BtlChar_BindObject(BtlMgrChr *c) {}
void BtlChar_SetFlag(BtlMgrChr *c,s32 f) {}
void BtlAnim_Play(BtlMgrChr *c,s32 anim,f32 blend) {assert(anim==0);played++;}
void BtlAnim_FlushRequest(BtlMgrChr *c) {assert(!reload[c->side]);flushed++;}
void BtlObjAnim_SamplePose(BtlMgrObj *o) {assert(played || flushed);sampled++;}
void BtlChar_PoseToObj(BtlMgrChr *c,s32 arg) {}
void BtlObjPose_CalcMatrices(BtlMgrObj *o) {}
'''
tests=r'''
int main(void) {
 BtlCharGetChr getter={0};BtlMgrChr chr={0};
 assert(!BtlChar_IsFrozen(&getter));
 reload[1]=1;assert(!BtlChar_IsFrozen(&getter));
 getter.side=1;assert(BtlChar_IsFrozen(&getter));
 chr.side=1;BtlChar_OnModelLoaded(&chr);
 assert(played==1 && !flushed && sampled==1);
 reload[1]=0;assert(!BtlChar_IsFrozen(&getter));
 BtlChar_OnModelLoaded(&chr);assert(flushed==1 && sampled==2);
 getter.freeze=1;assert(BtlChar_IsFrozen(&getter));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+frozen+loaded+tests)
 subprocess.run(['gcc','-std=gnu99','-DPORT','-fno-builtin','-w','-no-pie','-fsanitize=address,undefined',
                 '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Ginyu reload safety checks passed: frozen replacement, side isolation, neutral motion before pose sampling, normal animation requests and hit-stop.')

source=(ROOT/'src/battle/btl_act_f.c').read_text()
a=source.index('static s32 Port_GinyuRushNextStep(');b=source.index('\n}',a)+2
harness=r'''
#include "common.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static int pack,boost;
int Port_ZenkaiPackEnabled(void) {return pack;}
int Port_GinyuSideSelfDamageActive(int side) {return side==1 && boost;}
'''
tests=r'''
int main(void) {
 int steps;
 for(pack=0;pack<2;pack++) for(boost=0;boost<2;boost++) for(steps=0;steps<=5;steps++) {
  int expected=PORT_ZENKAI_GINYU_BODY_CHANGE && pack && boost && steps>2 ? steps-1 : 1;
  assert(Port_GinyuRushNextStep(0x280,boost,steps,0)==expected);
  assert(Port_GinyuRushNextStep(0x280,0,steps,0)==1);
  assert(Port_GinyuRushNextStep(0x281,1,steps,0)==1);
  assert(Port_GinyuRushNextStep(0x280,1,steps,1)==2);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+source[a:b]+tests)
 for flag in (0,1):
  subprocess.run(['gcc','-std=gnu99','-w',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}',
                  '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
  subprocess.run([str(path/'test')],check=True)
print('Ginyu shortened sequence checks passed: opening-to-firing transition, latched boost, step bounds and Classic/build gates.')

source=(ROOT/'src/battle/btl_char_flag_snd.c').read_text()
a=source.index('        chara = *(s32 *)((u8 *)BtlChar_GetObj(chr) + 0xC);')
b=source.index('        switch (req->p.kind)',a)
harness=r'''
#include "common.h"
#include <assert.h>
static int object[4],borrowed,replaced,original;
void *BtlChar_GetObj(void *chr) {return object;}
int BtlChar_TestMemberUnk70(void *chr) {return borrowed;}
int Port_GinyuReplacedMember(int side) {return replaced;}
int Port_GinyuMoveOwner(int side) {return original;}
static int selectVoice(void *chr,int owner) {int chara;
'''
tests=r'''
int main(void) {
 object[3]=25;assert(selectVoice(0,0)==25);
 borrowed=1;assert(selectVoice(0,0)==86);
 borrowed=0;object[3]=86;replaced=1;original=25;
 assert(selectVoice(0,0)==25);
 borrowed=1;assert(selectVoice(0,0)==25);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+source[a:b]+'return chara;}\n'+tests)
 subprocess.run(['gcc','-std=gnu99','-DPORT','-w','-Iinclude','-Iport/include','-include','port_compat.h',
                 str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Ginyu voice checks passed: normal identity, borrowed Ginyu, opponent original voice and model-independent selection.')

