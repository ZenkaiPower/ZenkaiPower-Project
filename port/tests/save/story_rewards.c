/* Link with menu_c_c.c compiled using port_compat.h and function sections,
 * --gc-sections, and port/src/plat_zenkai.c. */
#include "menu/menu_c.h"
#include "sys/port_zenkai.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern int strcmp(const char *, const char *);
extern void HistResult_BuildRewards(void);
MenuProgress *gProgress;
SaveData *gSaveData;
static MBattleResult battle;
static int setting;
int Port_Setting(const char *name, int fallback) { return !strcmp(name,"zenkai_power") ? setting : fallback; }
MBattleResult *BattleResult_GetPtr(void) { return &battle; }
void Save_AddItem(s32 id) { MSAVE->item[id] |= 1; }
static int has(s32 id) { return (SAVE_CHARA_WORD(gSaveData,id) & SAVE_CHARA_MASK(id)) != 0; }
int main(void) {
 HistProgress progress;
 HistResult result;
 SaveData save;
 const int extra[3][3]={{77,78,-1},{80,81,79},{29,30,-1}};
 int mode, level, episode, i;
 unsetenv("ZENKAIPOWER_SAVES");gSaveData=&save;gHistResult=&result;gProgress=(MenuProgress *)&progress;
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<3;episode++) {
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.level=level;progress.subMenuItem=episode;
  progress.reward.points[level]=100;
  progress.reward.chara[0]=24; /* an original script reward */
  progress.reward.item[0]=1;progress.reward.stage[0]=2;
  HistResult_BuildRewards();
  assert(has(24));
  for(i=0;i<3;i++) if(extra[episode][i]>=0) assert(has(extra[episode][i])==mode);
  assert((MSAVE->item[1]&1) && (MSAVE->stageBits&(1LL<<2)) && result.pointsTotal==100);
  /* Already earned characters survive an Easy replay; no duplicate popups. */
  progress.level=0;HistResult_BuildRewards();assert(has(24));
  for(i=0;i<result.rewardCount;i++) assert(result.reward[i].kind!=0);
 }
 /* Super Saiyan?! replaces Nail with the Frieza force in ZENKAI mode. */
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) {
  const int expected[]={84,82,83,90,89,88,87,86};
  int characterCount=0;
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.subMenu=1;progress.subMenuItem=0;progress.level=level;
  progress.reward.points[level]=100;progress.reward.chara[0]=24;
  HistResult_BuildRewards();
  for(i=0;i<8;i++) assert(has(expected[i])==mode);
  assert(!has(85)); /* Cui moved to Alone for the Final Battle. */
  assert(has(24)==!mode);
  for(i=0;i<result.rewardCount;i++) if(result.reward[i].kind==0) {
   assert(result.reward[i].id==(mode ? expected[characterCount] : 24));characterCount++;
  }
  assert(characterCount==(mode ? 8 : 1));
  if(mode) {
   HistResult_BuildRewards();
   assert(result.rewardCount==0); /* no duplicate character popups */
   /* Missing Frieza-force rewards can still be earned on replay. */
   SAVE_CHARA_WORD(gSaveData,84)&=~SAVE_CHARA_MASK(84);
   HistResult_BuildRewards();
   assert(result.rewardCount==1 && result.reward[0].id==84 && has(84));
  }
 }
 /* Frieza Saga rewards progress independently through all four episodes. */
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<4;episode++) {
  int eligible=mode;
  int earned=eligible && episode==1;
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.subMenu=1;progress.subMenuItem=episode;progress.level=level;
  progress.reward.points[level]=100;
  HistResult_BuildRewards();
  assert(has(24)==earned);
  assert(has(1)==(eligible && episode==2));
  assert(has(2)==(eligible && episode==3));
  assert(result.rewardCount==(!eligible ? 0 : episode==1 ? 4 : episode==2 ? 2 : episode==3 ? 2 : 0));
  for(i=91;i<=93;i++) assert(has(i)==earned);
  assert(has(94)==(eligible && episode==2));
  assert(has(95)==(eligible && episode==3));
  if(eligible && episode>=2) {
   assert(result.reward[0].kind==0 && result.reward[0].id==(episode==2 ? 1 : 2));
   assert(result.reward[1].kind==0 && result.reward[1].id==(episode==2 ? 94 : 95));
   HistResult_BuildRewards();assert(result.rewardCount==0);
  }
  if(earned) {
   for(i=0;i<3;i++) assert(result.reward[i].kind==0 && result.reward[i].id==91+i);
   assert(result.reward[3].kind==0 && result.reward[3].id==24);
   HistResult_BuildRewards();assert(result.rewardCount==0);
   /* Existing Nail does not block a missing Frieza form on replay. */
   SAVE_CHARA_WORD(gSaveData,93)&=~SAVE_CHARA_MASK(93);
   HistResult_BuildRewards();assert(result.rewardCount==1 && result.reward[0].id==93 && has(93));
   progress.level=0;HistResult_BuildRewards();assert(has(24) && has(93) && !has(94) && !has(95));
  }
 }
 /* Android/Cell Saga: every episode's ordered rewards, without other forms. */
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<8;episode++) {
  const int counts[]={7,7,3,5,2,2,10};
  const int expected[7][11]={
   {96,39,40,31,32,103,104}, {23,101,102,105,106,41,42}, {33,43,107},
   {55,3,4,14,15}, {100,109}, {16,108}, {17,18,46,47,44,45,34,35,56,20}
  };
  int earned=mode && episode<7;
  int count=earned ? counts[episode] : 0;
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.subMenu=2;progress.subMenuItem=episode;progress.level=level;
  progress.reward.points[level]=100;
  HistResult_BuildRewards();
  assert(result.rewardCount==count);
  for(i=0;i<SAVE_CHARA_COUNT;i++) {
   int j, wanted=0;
   for(j=0;j<count;j++) if(expected[episode][j]==i) wanted=1;
   assert(has(i)==wanted);
  }
  if(earned) {
   int last=expected[episode][count-1];
   for(i=0;i<count;i++) assert(result.reward[i].kind==0 && result.reward[i].id==expected[episode][i]);
   HistResult_BuildRewards();assert(result.rewardCount==0);
   SAVE_CHARA_WORD(gSaveData,last)&=~SAVE_CHARA_MASK(last);
   HistResult_BuildRewards();assert(result.rewardCount==1 && result.reward[0].id==last && has(last));
   progress.level=0;HistResult_BuildRewards();assert(has(last));
  }
 }
 /* Majin Buu Saga: requested forms, ordered popups and difficulty gates. */
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<6;episode++) {
  const int counts[]={6,2,9,1,3};
  const int expected[5][11]={
   {158,111,19,58,5,36}, {37,112},
   {59,48,49,50,21,113,114,115,116}, {6}, {117,51,52}
  };
  int earned=mode && episode<5;
  int count=earned ? counts[episode] : 0;
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.subMenu=3;progress.subMenuItem=episode;progress.level=level;
  progress.reward.points[level]=100;
  HistResult_BuildRewards();
  assert(result.rewardCount==count);
  for(i=0;i<SAVE_CHARA_COUNT;i++) {
   int j, wanted=0;
   for(j=0;j<count;j++) if(expected[episode][j]==i) wanted=1;
   assert(has(i)==wanted);
  }
  if(earned) {
   int last=expected[episode][count-1];
   for(i=0;i<count;i++) assert(result.reward[i].kind==0 && result.reward[i].id==expected[episode][i]);
   HistResult_BuildRewards();assert(result.rewardCount==0);
   SAVE_CHARA_WORD(gSaveData,last)&=~SAVE_CHARA_MASK(last);
   HistResult_BuildRewards();assert(result.rewardCount==1 && result.reward[0].id==last && has(last));
   progress.level=0;HistResult_BuildRewards();assert(has(last));
  }
 }
 /* GT and Dragon Ball: additions are scoped to the matching episodes. */
 {
  int saga;
  for(saga=5;saga<=6;saga++) for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<6;episode++) {
   const int gtCounts[]={7,1,0,1,1};
   const int gt[5][7]={{63,64,65,140,141,142,143},{144},{0},{146},{147}};
   const int dbCounts[]={3,1};
   const int db[2][3]={{11,12,68},{148}};
   int earned=mode && episode<(saga==5 ? 5 : 2);
   int count=earned ? (saga==5 ? gtCounts[episode] : dbCounts[episode]) : 0;
   const int *expected=count ? (saga==5 ? gt[episode] : db[episode]) : NULL;
   setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
   memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
   memset(&progress.reward,0xff,sizeof progress.reward);
   progress.subMenu=saga;progress.subMenuItem=episode;progress.level=level;
   progress.reward.points[level]=100;
   HistResult_BuildRewards();assert(result.rewardCount==count);
   for(i=0;i<SAVE_CHARA_COUNT;i++) {
    int j,wanted=0;
    for(j=0;j<count;j++) if(expected[j]==i) wanted=1;
    assert(has(i)==wanted);
   }
   if(count) {
    int last=expected[count-1];
    for(i=0;i<count;i++) assert(result.reward[i].kind==0 && result.reward[i].id==expected[i]);
    HistResult_BuildRewards();assert(result.rewardCount==0);
    SAVE_CHARA_WORD(gSaveData,last)&=~SAVE_CHARA_MASK(last);
    HistResult_BuildRewards();assert(result.rewardCount==1 && result.reward[0].id==last && has(last));
    progress.level=0;HistResult_BuildRewards();assert(has(last));
   }
  }
 }
 /* Special Saga: exact forms, episode boundaries, popup order and replays. */
 for(mode=0;mode<=1;mode++) for(level=0;level<3;level++) for(episode=0;episode<17;episode++) {
  const int counts[]={5,0,2,2,2,3,1,2,1,1,2,2,1,1,2,3};
  const int expected[16][5]={
   {99,98,85,73,74}, {0}, {121,122}, {123,124}, {118,119}, {125,126,127},
   {128}, {129,130}, {131}, {132}, {134,135}, {136,61}, {133}, {137}, {138,53}, {57,62,139}
  };
  int earned=mode && episode<16;
  int count=earned ? counts[episode] : 0;
  setting=mode;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
  memset(&save,0,sizeof save);memset(&result,0,sizeof result);memset(&progress,0,sizeof progress);
  memset(&progress.reward,0xff,sizeof progress.reward);
  progress.subMenu=4;progress.subMenuItem=episode;progress.level=level;
  progress.reward.points[level]=100;
  HistResult_BuildRewards();
  assert(result.rewardCount==count);
  for(i=0;i<SAVE_CHARA_COUNT;i++) {
   int j, wanted=0;
   for(j=0;j<count;j++) if(expected[episode][j]==i) wanted=1;
   assert(has(i)==wanted);
  }
  if(count) {
   int last=expected[episode][count-1];
   for(i=0;i<count;i++) assert(result.reward[i].kind==0 && result.reward[i].id==expected[episode][i]);
   HistResult_BuildRewards();assert(result.rewardCount==0);
   SAVE_CHARA_WORD(gSaveData,last)&=~SAVE_CHARA_MASK(last);
   HistResult_BuildRewards();assert(result.rewardCount==1 && result.reward[0].id==last && has(last));
   progress.level=0;HistResult_BuildRewards();assert(has(last));
  }
 }
 puts("Story reward checks passed: all difficulties, both modes, original/additional characters, other rewards and replays.");
 return 0;
}
