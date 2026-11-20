#!/usr/bin/env python3
"""Run the production gauge update and Zenkai drain helpers with synthetic fighters."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
member=(root/'src/battle/btl_char_member.c').read_text()
a=member.index('int Port_BtlKaiokenTier(void *fighter) {')
b=member.index('\n#endif',a)
helpers=member[a:b]
action=(root/'src/battle/btl_char_action.c').read_text()
a=action.index('void BtlAct_EndFlag98(')
b=action.index('\n}',a)+2
end_kaioken=action[a:b]
a=action.index('void BtlAct_UpdateGauges(')
b=action.index('\n/*\n * Timers and bookkeeping',a)
update=action[a:b]
requests=(root/'src/battle/btl_act_j.c').read_text()
a=requests.index('s32 BtlAct_CanUseSkill(')
b=requests.index('/*\n * Skill inputs:',a)
can_use=requests[a:b]
skills=(root/'src/battle/btl_act_h_b.c').read_text()
a=skills.index('void BtlSkill_Apply(')
b=skills.index('\n}',a)+2
apply=skills[a:b]
a=skills.index('int Port_BtlKaiokenTextTier(')
b=skills.index('\n#endif',a)
text_tier=skills[a:b]
sequence=(root/'src/battle/btl_seq.c').read_text()
a=sequence.index('void BtlText_DrawEntryNameTier(')
b=sequence.index('/* Battle sequence state machine,',a)
text_draw=sequence[a:b]
a=skills.index('    if ((mask & 0x80) && !BtlChar_TestFlag(chr, 6) &&')
b=skills.index('    if ((mask & 0x1000000)',a)
charge=skills[a:b]
source=r'''
#include <assert.h>
#include <string.h>
#define PORT 1
typedef int s32;typedef unsigned u32;typedef unsigned short u16;typedef unsigned char u8;typedef float f32;
#include "battle/port_hp_drain.h"
typedef struct { union {int hp;int health;};union {int hpMax;int healthMax;};int skillUses[2],blast,ki,maxPower,kiMax,gaugeB,gaugeBMax,skillLock[2]; } BtlActVitals;
typedef struct { int present;u32 ability[4];union {BtlActVitals vitals;BtlActVitals gauge;}; } BtlActMember;
typedef struct { int skillTimerC,skillTimerD,kind2On,kind3On,stat[4],skillStackA,skillStackB,skillSlot,skillTimer,skillKiRate,skillCount3,skillValE,skillValF,skillTimerE,skillTimerF,slotOn[2],unk99C,dodges,dodgesB,unkE5C,active,memberCount,secTimer,unkE10,unkD80,unkE20,objId,player,ki,flags[400];BtlActMember members[2]; } BtlActChr;
typedef BtlActMember BtlMember;
typedef BtlActVitals BtlMemberGauge;
typedef BtlActChr BtlMemberChr;
typedef BtlActChr BtlActJChr;
typedef BtlActChr BtlActIChr;
static BtlActChr *testSide;
BtlActChr *BtlChar_Get(int side) {return testSide;}
BtlActChr *BtlChar_FindBySide(int side) {return testSide;}
int BtlMember_HasAbility(BtlActChr *,int);
static int queued;
int BtlParam_GetUnk0(BtlActChr *c) {return 0;}
int BtlMember_IsKiFull(BtlActChr *c) {return 0;}
void BtlAct_SetQueue(BtlActChr *c,unsigned slot,int action) {queued=action;}
int Port_GokuFirstEpisodeRestricted(BtlActChr *c) {return 0;}
int BtlSkill_GetId(BtlActChr *c,int slot) {return slot ? 0x18:0;}
int BtlSkill_GetFlags(BtlActChr *c,int slot) {return 0x180;}
int BtlSkill_GetBlastCost(BtlActChr *c,int slot) {return BtlMember_HasAbility(c,0x15) ? 200000:300000;}
int BtlChar_TestMemberUnk70(BtlActChr *c) {return 0;}
int BtlMember_HasBlast(BtlActChr *c,int amount) {return c->members[c->active].vitals.blast>=amount;}
static int classic,zenkai=1,stopped,damageCalls,ends,mode;
int gPortTrainingHealthRegeneration=1,gPortTrainingKiRegeneration=1,gPortTrainingBlastRegeneration=1,gPortTrainingSwitchRegeneration=1;
static u32 motionFlags;
int Port_ZenkaiEnabled(void) {return zenkai;}
int Port_ZenkaiPackEnabled(void) { return Port_ZenkaiEnabled() && !classic; }
BtlActMember *BtlMember_GetActive(BtlActChr *c) {return &c->members[c->active];}
BtlActMember *BtlMember_Get(BtlActChr *c,int i) {return &c->members[i];}
BtlActVitals *BtlMember_GetActiveGauge(BtlActChr *c) {return &BtlMember_GetActive(c)->vitals;}
int BtlMember_HasAbility(BtlActChr *c,int n) {return (BtlMember_GetActive(c)->ability[n>>5]>>(n&31))&1;}
int BtlMember_HasAbilityOf(BtlActChr *c,int i,int n) {return (c->members[i].ability[n>>5]>>(n&31))&1;}
void BtlMember_Damage(BtlActChr *c,int amount,int flags) {
 assert(flags==0x42B || flags==0x2B);
 assert(amount>0 && amount<=BtlMember_GetActiveGauge(c)->hp);
 if(flags==0x42B) assert(amount<BtlMember_GetActiveGauge(c)->hp);
 damageCalls++;BtlMember_GetActiveGauge(c)->hp-=amount;
}
void BtlMember_AddHealth(BtlActChr *c,int n) {BtlMember_GetActiveGauge(c)->hp+=n;}
int BtlChar_TestFlag(BtlActChr *c,int f) {return c->flags[f];}
void BtlChar_SetFlag(BtlActChr *c,int f) {c->flags[f]=1;}
void BtlChar_SetHeldFlag(BtlActChr *c,int f) {c->flags[f]=1;}
void BtlChar_ClearFlag(BtlActChr *c,int f) {c->flags[f]=0;}
void *BtlChar_GetObj(BtlActChr *c) {return c;}
void BtlObj_SetColorMode(void *obj,int mode,int value) {}
void BtlStat_EndKind4(BtlActChr *c) {memset(c->stat,0,sizeof(c->stat));ends++;}
int BtlCharApi_IsInTechnique(int id) {return 0;}
int BtlChars_IsTimeStopped(void) {return stopped;}
int BtlChar_IsDead(BtlActChr *c) {return BtlMember_GetActiveGauge(c)->hp<=0;}
int BtlAnim_GetId(BtlActChr *c) {return 0;}
u32 BtlAnim_GetFlags(int id) {return motionFlags;}
int BtlParam_GetMaxPowerDrain(BtlActChr *c) {return 0;}
int BtlParam_GetKiRecoverRate(BtlActChr *c) {return 0;}
int BtlParam_GetGaugeB(BtlActChr *c) {return 0;}
int BtlParam_GetKiRegenLimit(BtlActChr *c) {return 0;}
int BtlParam_GetKiRegenRate(BtlActChr *c) {return 0;}
int BtlParam_GetBlastGainRate(BtlActChr *c) {return 0;}
int BtlParam_GetGauge99CTime(BtlActChr *c) {return 1;}
int BtlInput_IsPressed(BtlActChr *c,int mask) {return 0;}
int BtlMember_HasKi(BtlActChr *c,int amount) {return c->ki>=amount;}
void BtlMember_SubMaxPower(BtlActChr *c,int amount) {}
int BtlMember_IsMaxPowerEmpty(BtlActChr *c) {return 0;}
void BtlMember_SpendKi(BtlActChr *c,int n,int flags) {c->ki-=n;BtlMember_GetActiveGauge(c)->ki=c->ki;BtlMember_GetActiveGauge(c)->maxPower=0;}
int BtlMember_IsKiEmpty(BtlActChr *c) {return c->ki<=0;}
void BtlMember_AddKi(BtlActChr *c,int n) {c->ki+=n;}
void BtlMember_AddBlast(BtlActChr *c,int n) {c->members[c->active].vitals.blast+=n;}
int Battle_GetMode(void) {return mode;}
static int switchEnabled;
int BattleSide_GetUnk200(int p) {return switchEnabled;}
int BtlAct_TestPoweredSkill(BtlActChr *c,int i) {return 0;}
void BtlStat_EndKind3(BtlActChr *c) {}
void BtlStat_Update(BtlActChr *c) {}
void BtlSkill_UpdateTimers(BtlActChr *c) {}
int BtlUtil_Min(int a,int b) {return a<b ? a:b;}
int BtlSkill_GetHealthChange(BtlActChr *c,int s) {return 0;}
int BtlSkill_GetKiChange(BtlActChr *c,int s) {return 0;}
int BtlSkill_GetFrames(BtlActChr *c,int s) {return 300;}
int BtlSkill_GetStatKind(BtlActChr *c,int s) {return 4;}
int BtlSkill_GetStatLevel0(BtlActChr *c,int s) {return 5;}
int BtlSkill_GetStatLevel1(BtlActChr *c,int s) {return 5;}
int BtlSkill_GetStatLevel2(BtlActChr *c,int s) {return 5;}
int BtlSkill_GetStatLevel3(BtlActChr *c,int s) {return 5;}
int BtlSkill_GetValE(BtlActChr *c,int s) {return 0;}
int BtlSkill_GetValF(BtlActChr *c,int s) {return 0;}
int BtlUtil_Clamp(int a,int lo,int hi) {return a<lo ? lo:a>hi ? hi:a;}
void BtlMember_AddMaxPower(BtlActChr *c,int n) {}
void BtlMember_SubBlast(BtlActChr *c,int n) {c->members[c->active].vitals.blast-=n;}
void BtlStat_SetPenalty(BtlActChr *c,int st,int sl,int v) {}
void BtlStat_SetKind4(BtlActChr *c,int st,int sl,int v,int add) {if(add)c->stat[st]+=v;else c->stat[st]=v;}
#define BtlStat_SetBase BtlStat_SetKind4
#define BtlStat_SetKind2 BtlStat_SetKind4
#define BtlStat_SetKind3 BtlStat_SetKind4
void BtlStat_SetTimed(BtlActChr *c,int st,int sl,int v,int add,int t) {BtlStat_SetKind4(c,st,sl,v,add);}
/* Native names point into a multiline script, rather than an isolated string. */
static u16 entryName[]={'K','a','i','o','-','k','e','n',13,10,'*','0','0','0','N','e','x','t',0},printed[256];
u16 *BtlText_FindEntry(int n) {return entryName;}
int Font_GetCmdCount(void) {return 0;}
void Font_PushStyle(void) {}
void Font_PopStyle(void) {}
void Font_SetFlags(int f) {}
void Font_SetScale(float f) {}
void Font_SetScaleXY(float x,float y) {}
void Font_SetColorRGBA(int r,int g,int b,int a) {}
void Font_SetShadowMode(int m) {}
void Font_SetShadowColorRGBA(int r,int g,int b,int a) {}
void Font_SetAlign(int a) {}
void Font_Flush(int f) {}
void Font_PrintAt(int x,int y,u16 *s) {int i=0;do {printed[i]=s[i];}while(s[i++]);}
''' +helpers+'\n'+end_kaioken+'\n'+update+'\n'+can_use+'\n'+apply+'\n'+text_tier+'\n'+text_draw+'\n#define TEST(n) 1\nstatic int charge_input(BtlActChr *chr) {unsigned mask=0x80;\n'+charge+'\nreturn 0;}\nstatic void activate(BtlActChr *chr) {BtlSkill_Apply(chr,1); }\n'+r'''
static void setup(BtlActChr *c) {
 memset(c,0,sizeof(*c));c->memberCount=1;c->flags[3]=1;c->ki=100000;c->unkE10=100;
 c->members[0].vitals.kiMax=100000;
 c->members[0].present=1;c->members[0].vitals.hp=c->members[0].vitals.hpMax=30000;
}
static void tier(BtlActChr *c,int bit) {c->members[c->active].ability[bit>>5]|=1u<<(bit&31);}
int main(void) {
 BtlActChr c;
 /* Native activation refills Ki in both modes, with its original drain rate. */
 for(zenkai=0;zenkai<=1;zenkai++) {
  setup(&c);c.members[0].vitals.blast=300000;c.ki=0;assert(BtlAct_CanUseSkill(&c,1));
  activate(&c);assert(c.ki>0 && c.skillKiRate==333);
  setup(&c);c.members[0].vitals.blast=300000;c.members[0].vitals.hp=5000;assert(BtlAct_CanUseSkill(&c,1));
  c.flags[0x98]=1;assert(!charge_input(&c));
  c.flags[0x98]=0;assert(charge_input(&c));
 }
 zenkai=1;
#if !PORT_ZENKAI_KAIOKEN
 setup(&c);c.flags[0x98]=1;c.secTimer=29;BtlAct_UpdateGauges(&c);
 assert(c.members[0].vitals.hp==30000 && Port_BtlKaiokenTier(&c)==0);
 assert(Port_BtlKaiokenTextTier(0,1)==0);
 setup(&c);c.members[0].vitals.blast=300000;
 assert(Port_BtlKaiokenUpgrade(&c,300000)==0 && Port_BtlKaiokenTier(&c)==0);
 setup(&c);tier(&c,PORT_ABILITY_HP_DRAIN_150);c.flags[0x98]=1;c.secTimer=29;
 BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==29850);
 return 0;
#else
 /* Normal and discounted costs, automatic selection and incremental upgrades. */
 for(int cost=200000;cost<=300000;cost+=100000) {
  for(int stocks=1;stocks<=12;stocks++) {
   setup(&c);c.members[0].vitals.blast=stocks*100000;
   int expected=stocks*100000/cost;if(expected>3) expected=3;
   if(cost==200000) c.members[0].ability[0]|=1u<<0x15;
   testSide=&c;
   assert(Port_BtlKaiokenTextTier(0,1)==expected);
   assert(Port_BtlKaiokenTextTier(0,0)==0 && Port_BtlKaiokenTextTier(0,2)==0);
   BtlText_DrawEntryNameTier(0,0,1,0,1.0f,expected);
   assert(printed[8]==(expected>1 ? ' ':13));
   if(expected>1) assert(printed[9]=='x' && printed[10]==(expected==2 ? '3':'4') && !printed[11]);
   if(expected) {
    activate(&c);
    assert(c.members[0].vitals.blast==stocks*100000-expected*cost);
    for(int st=0;st<4;st++) assert(c.stat[st]==expected*5);
   }
   assert(Port_BtlKaiokenTier(&c)==expected);
   c.flags[0x98]=1;c.secTimer=29;BtlAct_UpdateGauges(&c);
   assert(c.members[0].vitals.hp==30000-(expected ? expected:1)*50);
   if(expected==3) assert(!BtlAct_CanUseSkill(&c,1));
   Port_BtlKaiokenClear(&c);assert(Port_BtlKaiokenTier(&c)==0);
  }
 }
 setup(&c);c.members[0].vitals.blast=300000;
 activate(&c);assert(c.members[0].vitals.blast==0 && Port_BtlKaiokenTier(&c)==1);
 c.members[0].vitals.blast=600000;
 activate(&c);assert(c.members[0].vitals.blast==0 && Port_BtlKaiokenTier(&c)==3);
 for(int st=0;st<4;st++) assert(c.stat[st]==15);
 assert(Port_BtlKaiokenUpgrade(&c,300000)==0);
 c.flags[0x99]=0;c.ki=0;BtlAct_UpdateGauges(&c);
 assert(Port_BtlKaiokenTier(&c)==0 && !c.flags[0x98]);
 for(int st=0;st<4;st++) assert(c.stat[st]==0);
 zenkai=0;assert(Port_BtlKaiokenTier(&c)==0);
 zenkai=1;setup(&c);c.flags[0x98]=1;
 for(int i=0;i<29;i++) BtlAct_UpdateGauges(&c);
 assert(c.members[0].vitals.hp==30000 && c.ki==97100);
 BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==29950 && c.ki==97000);
 stopped=1;for(int i=0;i<60;i++) BtlAct_UpdateGauges(&c);
 assert(c.members[0].vitals.hp==29950 && c.secTimer==0);stopped=0;
 c.flags[0x125]=1;BtlAct_UpdateGauges(&c);assert(c.secTimer==0);c.flags[0x125]=0;
 c.ki=0;BtlAct_UpdateGauges(&c);assert(!c.flags[0x98]);
 /* No last-bar cutoff: Kaioken and its HP drain continue below 10,000 HP. */
 setup(&c);c.flags[0x98]=1;c.secTimer=29;c.members[0].vitals.hp=10010;
 BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==9960 && c.flags[0x98]);
 c.members[0].vitals.blast=300000;assert(BtlAct_CanUseSkill(&c,1));
 for(int bit=PORT_ABILITY_HP_DRAIN_50;bit<=PORT_ABILITY_HP_DRAIN_150;bit++) {
  setup(&c);tier(&c,bit);c.secTimer=29;BtlAct_UpdateGauges(&c);
  assert(c.members[0].vitals.hp==30000-(bit==PORT_ABILITY_HP_DRAIN_50 ? 50:bit==PORT_ABILITY_HP_DRAIN_100 ? 100:150));
 }
 setup(&c);tier(&c,PORT_ABILITY_HP_DRAIN_50);tier(&c,PORT_ABILITY_HP_DRAIN_100);tier(&c,PORT_ABILITY_HP_DRAIN_150);
 c.flags[0x98]=1;c.secTimer=29;BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==29850);
 c.members[0].vitals.hp=20;c.secTimer=29;BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==0);
 /* Standalone drain stays nonlethal. Kaioken is lethal at all three tiers. */
 setup(&c);tier(&c,PORT_ABILITY_HP_DRAIN_150);c.members[0].vitals.hp=20;c.secTimer=29;
 BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==1);
 for(int level=1;level<=3;level++) {
  setup(&c);c.members[0].vitals.blast=level*300000;activate(&c);
  c.flags[0x99]=0;c.members[0].vitals.hp=level*50;c.secTimer=29;
  BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==0);
  int calls=damageCalls;BtlAct_UpdateGauges(&c);assert(damageCalls==calls);
 }
 setup(&c);zenkai=0;c.flags[0x98]=1;tier(&c,PORT_ABILITY_HP_DRAIN_150);c.secTimer=29;
 BtlAct_UpdateGauges(&c);assert(c.ki==99900 && c.members[0].vitals.hp==30000);
 for(mode=5;mode<=6;mode++) {
  /* Classic ignores stale Off choices and keeps native Training refills. */
  setup(&c);zenkai=1;classic=1;c.members[0].vitals.hp=20000;
  gPortTrainingHealthRegeneration=0;BtlAct_UpdateGauges(&c);
  assert(c.members[0].vitals.hp==20200);
  setup(&c);zenkai=1;motionFlags=0x10000;gPortTrainingKiRegeneration=0;
  BtlAct_UpdateGauges(&c);assert(c.ki==101000);
  setup(&c);zenkai=1;motionFlags=0x20000;gPortTrainingBlastRegeneration=0;
  BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.blast==3333);
  classic=0;
  setup(&c);zenkai=1;c.memberCount=2;
  c.members[1].present=1;c.members[1].vitals.hp=19900;c.members[1].vitals.hpMax=30000;
  gPortTrainingHealthRegeneration=1;BtlAct_UpdateGauges(&c);
  assert(c.members[1].vitals.hp==20100);
  gPortTrainingHealthRegeneration=0;BtlAct_UpdateGauges(&c);assert(c.members[1].vitals.hp==20100);
  gPortTrainingHealthRegeneration=1;c.members[1].vitals.hp=29950;
  BtlAct_UpdateGauges(&c);assert(c.members[1].vitals.hp==30000);
  c.members[1].vitals.hp=0;BtlAct_UpdateGauges(&c);assert(c.members[1].vitals.hp==0);
  c.members[1].vitals.hp=10000;c.members[1].present=0;
  BtlAct_UpdateGauges(&c);assert(c.members[1].vitals.hp==10000);
  setup(&c);zenkai=1;switchEnabled=1;c.unk99C=50000;gPortTrainingSwitchRegeneration=0;
  int normalSwitchRefill=(s32)(100000.0f / (BtlParam_GetGauge99CTime(&c)*30.0f));
  BtlAct_UpdateGauges(&c);assert(c.unk99C==50000+normalSwitchRefill);
  c.unk99C=50000;gPortTrainingSwitchRegeneration=1;BtlAct_UpdateGauges(&c);
  assert(c.unk99C==50000+normalSwitchRefill+0x457);
  c.unk99C=99999;BtlAct_UpdateGauges(&c);assert(c.unk99C==100000);
  zenkai=0;gPortTrainingSwitchRegeneration=0;c.unk99C=50000;
  BtlAct_UpdateGauges(&c);assert(c.unk99C==50000+normalSwitchRefill+0x457);
  {int trainingMode=mode;mode=0;zenkai=1;c.unk99C=50000;
   BtlAct_UpdateGauges(&c);assert(c.unk99C==50000+normalSwitchRefill);mode=trainingMode;}
  switchEnabled=0;gPortTrainingSwitchRegeneration=1;
  setup(&c);c.members[0].vitals.hp=20000;zenkai=1;gPortTrainingHealthRegeneration=0;BtlAct_UpdateGauges(&c);
  assert(c.members[0].vitals.hp==20000);
  gPortTrainingHealthRegeneration=1;BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==20200);
  zenkai=0;gPortTrainingHealthRegeneration=0;BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.hp==20400);
  setup(&c);zenkai=1;motionFlags=0x10000;gPortTrainingKiRegeneration=0;
  BtlAct_UpdateGauges(&c);assert(c.ki==100000);
  gPortTrainingKiRegeneration=1;BtlAct_UpdateGauges(&c);assert(c.ki==101000);
  zenkai=0;gPortTrainingKiRegeneration=0;BtlAct_UpdateGauges(&c);assert(c.ki==102000);
  setup(&c);zenkai=1;motionFlags=0x20000;gPortTrainingBlastRegeneration=0;
  BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.blast==0);
  gPortTrainingBlastRegeneration=1;BtlAct_UpdateGauges(&c);assert(c.members[0].vitals.blast==0xD05);
  zenkai=0;gPortTrainingBlastRegeneration=0;BtlAct_UpdateGauges(&c);
  assert(c.members[0].vitals.blast==2*0xD05);
  motionFlags=0;
 }
#endif
}

'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'drain.c';e=Path(tmp)/'drain';p.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
 disabled=Path(tmp)/'drain-disabled'
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-DPORT_ZENKAI_KAIOKEN=0','-I'+str(root/'include'),str(p),'-o',str(disabled)],check=True)
 subprocess.run([str(disabled)],check=True)
print('Zenkai HP drain and native Kaioken isolation checks passed')
