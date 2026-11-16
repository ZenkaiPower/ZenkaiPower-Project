"""Check production Body Change selection with pack and compile gates."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/battle/btl_char_action.c').read_text()
start = source.index('void BtlAct_SetFormRandom(')
opening = source.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
body = source[start:end]
harness = r'''
#include <assert.h>
#include "sys/port_zenkai_features.h"
typedef int s32;
typedef struct { struct { int chara,costume,unk0,unkC,unk10,unk24,unk2C,unk28,unk14,unk18,unk1C,unk20; } form; } BtlActChr;
static int frame, pack, injured;
int Port_GinyuSelfDamageActive(void *chr) {return injured;}
static struct { int chara; } member;
#define BtlMember_GetActive(chr) (&member)
int Port_GinyuChooseOpponentBody(void *chr,int *chara,int *costume,int *variant) {assert(0 && "Opponent exchange must remain disabled");return 0;}
int BtlChar_FrameMod(int n) { return frame % n; }
int Port_ZenkaiPackEnabled(void) { return pack; }
'''
tests = r'''
int main(void) {
    static const int original[6][3]={{1,0,0},{13,2,0},{25,1,0},{29,1,0},{24,0,0},{89,0,0}};
    static const int changed[4][3]={{1,0,0},{13,0,0},{25,1,0},{29,2,0}};
    static const int boosted[3][3]={{2,2,0},{22,0,1},{94,0,1}};
    static const int currents[]={86,1,13,25,29,2,22,94};
    BtlActChr chr;
    int c;
    for(pack=0;pack<2;pack++) for(injured=0;injured<2;injured++) for(c=0;c<8;c++) {
        const int (*pool)[3]=pack && PORT_ZENKAI_GINYU_BODY_CHANGE ? (injured ? boosted : changed) : original;
        int total=pack && PORT_ZENKAI_GINYU_BODY_CHANGE ? (injured ? 3 : 4) : 6;
        int candidates[6],count=0,seen=0,k;
        member.chara=currents[c];
        for(k=0;k<total;k++) {
            if(pack && PORT_ZENKAI_GINYU_BODY_CHANGE && pool[k][0]==member.chara) continue;
            candidates[count++]=k;
        }
        for(frame=0;frame<120;frame++) {
            const int *expected=pool[candidates[frame%count]];
            BtlAct_SetFormRandom(&chr);
            assert(chr.form.chara==expected[0] && chr.form.costume==expected[1]);
            assert(chr.form.unk14==expected[2] && chr.form.unk18==-1 && chr.form.unk20==-1);
            seen |= 1<<candidates[frame%count];
        }
        for(k=0;k<count;k++) assert(seen & (1<<candidates[k]));
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(harness + body + tests)
    for enabled in (0,1):
        subprocess.run(['gcc','-std=c99','-DPORT',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={enabled}',
                        '-Iinclude',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
        subprocess.run([str(path/'test')],check=True)
print('Body Change checks passed: normal/boosted rosters, damaged outfits, no repeats, original Classic roster and disabled flag.')

# Exercise real move-cache helpers and simulate the source model buffer being reused.
source = (ROOT / 'src/battle/bobj_a.c').read_text()
start = source.index('typedef struct PortGinyuMoves')
end = source.index('\n#endif', start)
cache = source[start:end]
harness = r'''
#include <assert.h>
#include <string.h>
#include "sys/port_zenkai_features.h"
typedef unsigned int u32;
typedef unsigned char u8;typedef signed char s8;
typedef struct { struct { int id; u32 *buf; } file[3]; } Slot;
typedef struct { Slot *slot; struct { void *unk914,*unk918,*unk90C,*unk98[1],*common9C,*unkA0,*unkA4,*unk910,*unk924,*unk91C,*unk920,*unk904,*unk908; } mdl; } BObj;
static int incomingBorrowed=1,replaced,reloading,retargeted;
static void Port_GinyuEnableRetarget(void *object) {retargeted=1;}
static void Port_GinyuCaptureSkeleton(int side,void *object) {}
static void Port_GinyuClearSkeleton(int side) {}
static void Port_GinyuPrepareFriezaTail(void *object) {}
int Port_GinyuReplacedMember(int side) {return replaced;}
int Port_GinyuOpponentReload(int side) {return reloading;}
int Port_GinyuIncomingBorrowed(int side) {return incomingBorrowed;}
static int pack=1;
static struct { int unk00; } progress;
#define gProgress (&progress)
void *BObjFile_GetEntry(void *file, int n) {
    u32 *buf=((Slot *)file)->file[0].buf;
    return buf[n+1]!=buf[n] ? (u8 *)buf+(buf[n]&~3u) : 0;
}
int Port_ZenkaiPackEnabled(void) { return pack; }
'''
tests = r'''
int main(void) {
    Slot slots[2]={0};
    u32 presentation[1024]={0};
    u8 bodyParam[0xCC]={0},bodyAttack[0x30]={0};
    int i;
    u32 super[0x244/4],skill[0xC8/4],ki[13*0x34/4];
    BObj obj={&slots[0],{super,skill,ki}}, other={&slots[1],{0,0,0}};
    memset(super,0x12,sizeof(super)); memset(skill,0x34,sizeof(skill)); memset(ki,0x56,sizeof(ki));
    slots[0].file[0].id=0x590+86*10;
    slots[0].file[0].buf=presentation;
    for(i=0x29;i<=0x2E;i++) presentation[i]=0x100+(i-0x29)*4;
    presentation[0x100/4]=0x11111111;
    presentation[0x104/4]=0x22222222;
    presentation[0x108/4]=0x33333333;
    presentation[0x110/4]=0x44444444;
    Port_GinyuCaptureMoves(0,&obj,1);
    memset(super,0,sizeof(super));memset(skill,0,sizeof(skill));memset(ki,0,sizeof(ki));
    memset(presentation+0x100/4,0,0x20);
    Port_GinyuRestoreMoves(0,&obj);
    if(PORT_ZENKAI_GINYU_BODY_CHANGE) {
        assert(((u32 *)obj.mdl.unk914)[0]==0x12121212);
        assert(((u32 *)obj.mdl.unk918)[0]==0x34343434);
        assert(((u32 *)obj.mdl.unk90C)[0]==0x56565656);
        assert(*(u32 *)obj.mdl.unk98[0]==0x11111111);
        assert(*(u32 *)obj.mdl.common9C==0x22222222);
        assert(*(u32 *)obj.mdl.unkA0==0x33333333);
        assert(*(u32 *)obj.mdl.unkA4==0x44444444);
        obj.mdl.common9C=0; /* common-table binding runs in OnModelLoaded */
        obj.mdl.unkA4=0;
        Port_GinyuRestoreMoves(0,&obj);
        assert(*(u32 *)obj.mdl.common9C==0x22222222);
        assert(*(u32 *)obj.mdl.unkA4==0x44444444);
        slots[0].file[0].id=0x590+13*10;
        Port_GinyuCaptureMoves(0,&obj,1); /* repeat Body Change */
        assert(sPortGinyuMoves[0].valid);
        Port_GinyuCaptureMoves(0,&obj,0); /* teammate loads */
        assert(sPortGinyuMoves[0].valid);
        incomingBorrowed=0;
        Port_GinyuRestoreMoves(0,&other);
        assert(other.mdl.unk914==0); /* never lend Ginyu moves to a teammate */
        incomingBorrowed=1;
        Port_GinyuRestoreMoves(0,&obj);
        assert(((u32 *)obj.mdl.unk914)[0]==0x12121212);
    } else assert(obj.mdl.unk914==super);
    Port_GinyuCaptureMoves(1,&other,0);
    assert(!sPortGinyuMoves[1].valid);
    if(PORT_ZENKAI_GINYU_BODY_CHANGE) {
        /* Capture a non-Ginyu opponent before a model-only exchange. */
        Port_GinyuClearSide(1);reloading=1;replaced=1;incomingBorrowed=0;
        other.slot=&slots[0];slots[0].file[0].id=0x590+25*10;
        other.mdl.unk914=super;other.mdl.unk918=skill;other.mdl.unk90C=ki;
        memset(super,0x78,sizeof(super));
        presentation[0x12]=0x200;presentation[0x13]=0x2CC;presentation[0x14]=0x2FC;
        ((u8 *)presentation)[0x200]=7;((u8 *)presentation)[0x2CC]=7;
        bodyParam[0]=9;bodyParam[4]=88;bodyParam[8]=77;bodyParam[0x7C]=99;bodyAttack[0]=9;bodyAttack[4]=99;
        presentation[0x1B]=0x120;presentation[0x1C]=0x124;presentation[0x1D]=0x128;
        presentation[0x120/4]=0x12345678;
        Port_GinyuCaptureMoves(1,&other,1);
        memset(super,0,sizeof(super));other.mdl.unk914=0;
        other.mdl.unk914=super;other.mdl.unk90C=ki;
        other.mdl.unk904=bodyParam;other.mdl.unk908=bodyAttack;
        ((u8 *)super)[0x184]=99;((u8 *)ki)[4]=99;
        reloading=0;Port_GinyuRestoreMoves(1,&other);
        assert(((u32 *)other.mdl.unk914)[0]==0x78787878);
        assert(*(u32 *)other.mdl.unk91C==0x12345678);
        assert(((u8 *)other.mdl.unk914)[0x184]==99);
        assert(((u8 *)other.mdl.unk90C)[4]==99);
        assert(retargeted && ((u8 *)other.mdl.unk904)[4]==88 && ((u8 *)other.mdl.unk904)[8]==77);
        assert(((u8 *)other.mdl.unk904)[0]==7 && ((u8 *)other.mdl.unk904)[0x7C]==99);
        assert(((u8 *)other.mdl.unk908)[0]==7 && ((u8 *)other.mdl.unk908)[4]==99);
        Port_GinyuRestoreMoves(1,&other);
        assert(((u8 *)other.mdl.unk908)[4]==99); /* repeated restore does not compound */
        Port_GinyuCaptureMoves(1,&other,0); /* survives team reload */
        other.mdl.unk914=0;Port_GinyuRestoreMoves(1,&other);
        assert(((u32 *)other.mdl.unk914)[0]==0x78787878);
        replaced=0;other.mdl.unk914=0;Port_GinyuRestoreMoves(1,&other);
        assert(!other.mdl.unk914);
    }
    pack=0;Port_GinyuCaptureMoves(0,&obj,1);
    assert(!sPortGinyuMoves[0].valid);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)
    (path/'test.c').write_text(harness+cache+tests)
    for flag in (0,1):
        subprocess.run(['gcc','-std=c99',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}',
                        '-Iinclude',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
        subprocess.run([str(path/'test')],check=True)
print('Ginyu move retention checks passed: reload buffer reuse, repeated changes, side isolation and Classic gate.')

source=(ROOT/'src/battle/btl_seq.c').read_text()
start=source.index('s32 BtlText_CheckUnlock(')
end=source.index('\n}',start)+2
body=source[start:end]
harness=r'''
#include <assert.h>
#include "sys/port_zenkai_features.h"
typedef int s32;typedef unsigned int u32;typedef unsigned short u16;
static int pack,changed;
static unsigned long long gSaveData[512];
static struct { int side; } menu;
#define BtlMenu_GetWork() (&menu)
int Port_ZenkaiPackEnabled(void) { return pack; }
int BtlCtrl_TestMemberUnk70(int side) { return changed; }
'''
tests=r'''
int main(void) {
 u16 line[]={'&','0','8','6',0}, *cursor;
 for(pack=0;pack<2;pack++) for(changed=0;changed<2;changed++) {
  cursor=line;
  assert(BtlText_CheckUnlock(&cursor)==(!changed || (pack && PORT_ZENKAI_GINYU_BODY_CHANGE)));
  assert(cursor==line+4);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)
 (path/'test.c').write_text(harness+body+tests)
 for flag in (0,1):
  subprocess.run(['gcc','-std=c99','-DPORT',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}',
                  '-Iinclude',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
  subprocess.run([str(path/'test')],check=True)
print('Ginyu View Skill unlock checks passed, including Classic Mode and disabled build flag.')

harness=r'''
#include <assert.h>
#include "battle/port_ginyu_health.h"
int main(void) {
 Port_GinyuHealthReset(0);Port_GinyuHealthReset(1);
 assert(Port_GinyuHealthChange(0,0,86,1,9000,30000)==30000);
 assert(Port_GinyuHealthChange(0,0,1,25,12000,30000)==30000);
 assert(Port_GinyuHealthChange(0,0,25,1,21000,30000)==12000);
 assert(Port_GinyuHealthChange(0,0,1,25,10000,30000)==21000);
 assert(Port_GinyuHealthChange(0,0,25,1,20000,8000)==8000);
 assert(Port_GinyuHealthChange(0,1,86,1,1000,30000)==30000);
 assert(Port_GinyuHealthChange(1,0,86,1,1000,30000)==30000);
 Port_GinyuHealthReset(0);
 assert(Port_GinyuHealthChange(0,0,86,1,1000,30000)==30000);
 assert(Port_GinyuHealthChange(0,1,86,1,1000,30000)==30000);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)
 (path/'test.c').write_text(harness)
 subprocess.run(['gcc','-std=c99','-fsanitize=address,undefined','-no-pie',
                 '-Iinclude',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Body Change HP checks passed: first visits, return visits, damage updates, maximum caps, member/side isolation and resets.')

def extract(source,name):
 start=source.index(name+'(');start=source.rfind('\n',0,start)+1
 end=source.index('\n}',start)+2
 return source[start:end]
tech=extract((ROOT/'src/battle/btl_act_j.c').read_text(),'BtlAct_CanUseTechnique')
skill=extract((ROOT/'src/battle/btl_act_j.c').read_text(),'BtlAct_CanUseSkill')
dim=extract((ROOT/'src/battle/btl_seq.c').read_text(),'BtlText_IsLineDimmed')
skills=(ROOT/'src/battle/btl_act_h_b.c').read_text()
a=skills.index('    case 0x17:',skills.index('void BtlSkill_Apply('))
b=skills.index('    case 0x18:',a)
damage='int damage(int slot) {int health=0;switch(0x17) { '+skills[a:b]+' }return health;}'
harness=r'''
#include <assert.h>
#include "sys/port_zenkai_features.h"
typedef int s32;typedef unsigned int u32;typedef struct {int dodges,dodgesB,unkE5C;} BtlActJChr;
static int pack,changed,boost;
int Port_GinyuSelfDamageActive(void *chr) {return boost;}
int Port_GinyuSideSelfDamageActive(int side) {return boost;}
static struct {int techLock[5],skillLock[2];} gauge;
static int skillId=0x17;
int BtlSkill_GetId(void *chr,int slot) {return skillId;}
int BtlSkill_GetFlags(void *chr,int slot) {return 0;}
int BtlSkill_GetBlastCost(void *chr,int slot) {return 0;}
int BtlMember_HasBlast(void *chr,int amount) {return 1;}
int Port_BtlKaiokenTier(void *chr) {return 0;}
int Port_ZenkaiPackEnabled(void) {return pack;}
int BtlChar_TestMemberUnk70(void *chr) {return changed;}
int BtlCtrl_TestMemberUnk70(int side) {return changed;}
int Port_GokuFirstEpisodeRestricted(void *chr) {return 0;}
int BtlChar_TestFlag(void *chr,int flag) {return 1;}
int BtlSuper_GetKiCost(void *chr,int slot) {return 0;}
int BtlMember_HasKi(void *chr,int cost) {return 1;}
int BtlSuper_GetFlags(void *chr,int slot) {return 0;}
int Battle_GetMode(void) {return 0;}
#define BtlMember_GetActiveGauge(chr) (&gauge)
'''
tests=r'''
int main(void) {
 int slot;BtlActJChr chr={0};
 for(pack=0;pack<2;pack++) for(changed=0;changed<2;changed++) for(boost=0;boost<2;boost++) {
  for(slot=2;slot<=4;slot++) {
   int enabled=!changed || (pack && PORT_ZENKAI_GINYU_BODY_CHANGE && (slot!=4));
   assert(BtlAct_CanUseTechnique(0,slot)==enabled);
  }
  assert(BtlText_IsLineDimmed(0,4,0)==(pack && PORT_ZENKAI_GINYU_BODY_CHANGE && changed));
  assert(BtlText_IsLineDimmed(0,1,0)==(pack && PORT_ZENKAI_GINYU_BODY_CHANGE && changed));
  assert(BtlAct_CanUseSkill(&chr,1)==(!changed));
  if(pack && PORT_ZENKAI_GINYU_BODY_CHANGE) assert(BtlAct_CanUseSkill(&chr,0));
  skillId=0x10;
  assert(BtlAct_CanUseSkill(&chr,1)==(!changed || (pack && PORT_ZENKAI_GINYU_BODY_CHANGE)));
  skillId=0x17;
  assert(BtlText_IsLineDimmed(0,3,0)==0);
  assert(BtlText_IsLineDimmed(1,4,0)==0);
  assert(damage(1)==(pack && PORT_ZENKAI_GINYU_BODY_CHANGE ? -10000:-3000));
  assert(damage(0)==-3000);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)
 (path/'test.c').write_text(harness+tech+skill+dim+damage+tests)
 for flag in (0,1):
  subprocess.run(['gcc','-std=c99','-DPORT',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}',
                  '-Iinclude',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
  subprocess.run([str(path/'test')],check=True)
print('Ginyu checks passed: changed-body Ultimate lock, View Skill dimming, 10,000 HP self-damage and Classic/build gates.')

# Run the real skeleton capture and translation retargeter in both directions.
source=(ROOT/'src/battle/bobj_a.c').read_text()
a=source.index('typedef struct PortGinyuSkeleton');b=source.index('\n#endif',a)
helpers=source[a:b]
a=source.index('void BtlObjAnim_Retarget(');b=source.index('\n}',a)+2
retarget=source[a:b]
harness=r'''
#include "common.h"
#include "battle/bobj_a.h"
#include <assert.h>
#include <string.h>
#define TRACK_AT(anim, off) ((BobjTrack *)((u16 *)(anim) + (off) * 2))
static int borrowed=1,replaced;
int Port_GinyuIncomingBorrowed(int side) {return side==0 && borrowed;}
int Port_GinyuReplacedMember(int side) {return side==0 && replaced;}
f32 D_002F2420[BOBJ_NODE_MAX*3];
BobjBone *BtlObj_FindBound(BobjObj *obj,s32 id) {return obj->mdl.nodes;}
'''
tests=r'''
static void check(float sourceHeight,float sourceRest,float targetHeight,float targetRest,float delta,float expected) {
 static BobjWork work;
 BobjObj obj={0};BobjParam param={0};BobjBone oldBone={0},newBone={0};
 union {u64 align;u8 bytes[1024];} buffer={0};
 BobjAnim *anim=(BobjAnim *)buffer.bytes;
 BobjTrack *track=(BobjTrack *)(buffer.bytes+256);
 oldBone.id=newBone.id=0;oldBone.last=newBone.last=1;
 oldBone.rest.y=sourceRest;newBone.rest.y=targetRest;
 param.height=sourceHeight;obj.mdl.param=&param;obj.mdl.nodes=&oldBone;
 obj.work=&work;work.retarget=0;
 Port_GinyuCaptureSkeleton(0,&obj);
 param.height=targetHeight;obj.mdl.nodes=&newBone;
 Port_GinyuEnableRetarget(&obj);assert(work.retarget);
 anim->track[0]=64;track->flags=0;track->count=1;track->u.key[0].y=sourceRest+delta;
 BtlObjAnim_Retarget(anim,&obj,&obj,0,1);
 assert(track->u.key[0].y>expected-0.001f && track->u.key[0].y<expected+0.001f);
 Port_GinyuClearSkeleton(0);assert(!Port_GinyuAnimationSkeleton(&obj));
}
int main(void) {
 check(2,10,1,3,0,3); /* Ginyu into a small body: preserve ground baseline */
 check(2,10,1,3,2,4); /* scale vertical motion instead of translating the whole body */
 borrowed=0;replaced=1;
 check(1,3,2,10,0,10); /* small character into Ginyu: no sinking */
 check(1,3,2,10,2,14);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+helpers+retarget+tests)
 subprocess.run(['gcc','-std=gnu99','-DPORT','-w','-no-pie','-fsanitize=address,undefined',
                 '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Ginyu skeleton checks passed: small/large body baselines, vertical movement, both exchange directions and reset.')

source=(ROOT/'src/battle/bobj_a.c').read_text()
a=source.index('static BobjTrack *Port_GinyuFriezaTailTrack(');b=source.index('\n}',a)+2
harness=r'''
#include "common.h"
#include "battle/bobj_a.h"
#include "sys/port_zenkai_features.h"
#include <assert.h>
#define TRACK_AT(anim, off) ((BobjTrack *)((u16 *)(anim) + (off) * 2))
static u32 sGinyuFriezaIdle[7][4096];
static int sGinyuFriezaIdleReady=127,borrowed=1,pack=1;
int Port_ZenkaiPackEnabled(void) {return pack;}
static void *Port_GinyuAnimationSkeleton(void *obj) {return borrowed ? obj : 0;}
'''
tests=r'''
int main(void) {
 BobjObj obj={0};BobjAnim *idle=(BobjAnim *)sGinyuFriezaIdle[0];float frame=-1;int i;
 *(s32 *)((u8 *)&obj+0xC)=94;obj.anim.layer[0].id=0;obj.anim.layer[0].length=59;
 obj.anim.frame=29.5f;idle->length=78;
 for(i=4;i<=7;i++) idle->track[i]=64;
 for(i=0;i<BOBJ_NODE_MAX;i++) assert((Port_GinyuFriezaTailTrack(&obj,i,&frame)!=0)==(i>=4 && i<=7));
 assert(frame==39);
 {
  static const int motions[7]={0,1,0x26,0x27,0x34,0x35,0x36};int slot;
  for(slot=0;slot<7;slot++) {
   BobjAnim *clip=(BobjAnim *)sGinyuFriezaIdle[slot];clip->length=78;clip->track[4]=64;
   obj.anim.layer[0].id=motions[slot];
   assert(Port_GinyuFriezaTailTrack(&obj,4,&frame)==TRACK_AT(clip,64));assert(frame==39);
   sGinyuFriezaIdleReady &= ~(1<<slot);assert(!Port_GinyuFriezaTailTrack(&obj,4,&frame));
   sGinyuFriezaIdleReady |= 1<<slot;
  }
 }
 obj.anim.layer[0].id=2;assert(!Port_GinyuFriezaTailTrack(&obj,4,&frame));
 obj.anim.layer[0].id=0;borrowed=0;assert(!Port_GinyuFriezaTailTrack(&obj,4,&frame));
 borrowed=1;pack=0;assert(!Port_GinyuFriezaTailTrack(&obj,4,&frame));
 pack=1;*(s32 *)((u8 *)&obj+0xC)=13;assert(!Port_GinyuFriezaTailTrack(&obj,4,&frame));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+source[a:b]+tests)
 subprocess.run(['gcc','-std=gnu99','-w','-no-pie','-fsanitize=address,undefined',
                 '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
 subprocess.run([str(path/'test')],check=True)
print('Frieza tail checks passed: only tail nodes, ground/air idle, charge start/loop/end, loop synchronization, missing clips and Classic isolation.')

source=(ROOT/'src/battle/battle_load.c').read_text()
a=source.index('static int Port_GinyuPortraitOnly(');b=source.index('\n#endif',a)
harness=r'''
#include "common.h"
#include "battle/battle.h"
#include "battle/battle_work.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BattleMember member;
static int pack=1,borrowed=1,fileId,relocations;
static u32 buffer[1024],original[1024];
int Port_ZenkaiPackEnabled(void) {return pack;}
int Port_GinyuIncomingBorrowed(int side) {return borrowed;}
s32 BtlSide_GetActiveMember(s32 side) {return 0;}
BattleMember *BattleSide_GetMember(s32 side,s32 idx) {assert(idx==0);return &member;}
void *File_Request3(s32 id,void *buf,s32 size) {
 assert(buf==buffer && size==0x1000);fileId=id;return buf;
}
void Res_RelocateOffsets(void *out,void *base,void *hdr) {
 assert(out==&member.buf[1] && base==buffer && hdr==buffer);relocations++;
}
'''
tests=r'''
int main(void) {
 BtlJob job={0};int side;
 job.modelOnly=1;job.chara=94;
 assert(Port_GinyuPortraitOnly(&job)==PORT_ZENKAI_GINYU_BODY_CHANGE);
 pack=0;assert(!Port_GinyuPortraitOnly(&job));pack=1;
 borrowed=0;assert(!Port_GinyuPortraitOnly(&job));borrowed=1;
 job.modelOnly=0;assert(!Port_GinyuPortraitOnly(&job));job.modelOnly=1;
 for(side=0;side<2;side++) {
  job.side=side;member.buf[1]=buffer;member.data=original;
  Port_GinyuRequestPortrait(&job);
  assert(fileId==94*2+side+8 && member.data==original);
  Port_GinyuBindPortrait(&job);assert(member.data==buffer);
 }
 assert(relocations==2);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(harness+source[a:b]+tests)
 for flag in (0,1):
  subprocess.run(['gcc','-std=gnu99','-w','-no-pie','-fsanitize=address,undefined',f'-DPORT_ZENKAI_GINYU_BODY_CHANGE={flag}',
                  '-Iinclude','-Iport/include','-include','port_compat.h',str(path/'test.c'),'-o',str(path/'test')],cwd=ROOT,check=True)
  subprocess.run([str(path/'test')],check=True)
print('Ginyu portrait checks passed: body/side file selection, delayed binding, relocation and Classic/build gates.')
