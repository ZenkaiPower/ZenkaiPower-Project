#include "common.h"
#include "menu/menu_l.h"
#include "menu/port_tour_bracket.h"
#include <assert.h>
#include <stdio.h>

static SaveData save;
SaveData *gSaveData = &save;
static LTourProgress progress;
MenuProgress *gProgress = (MenuProgress *)&progress;
static unsigned randomState;
static int forcedRoll=-1, randomBound, randomCalls;
static int zenkai = 1;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_ZenkaiPackEnabled(void) { return Port_ZenkaiEnabled(); }
u32 Rand_Range(u32 n) {
    assert(n > 0);randomBound=n;randomCalls++;
    if(forcedRoll>=0) {assert((u32)forcedRoll<n);return forcedRoll;}
    randomState = randomState * 1664525u + 1013904223u;return (randomState >> 8) % n;
}
int ChrTbl_GetCost(int id) { return id % 10; }
int ChrTbl_WrapCostume(int id, int *costume) { return id == 98 ? 3 : id == 99 ? 4 : 2; }
extern void Bracket_FillEntrants(LBracket *);
extern void Bracket_SetupBattle(LBracket *);
extern void Bracket_PlayCpuMatch(LBracket *,s32);
static int battleChara[2],battleCostume[2],battleFinished,battleStage;
void Battle_ClearWork(void) { battleFinished=0; }
void BattleSetup_SetRule(int screen,int mode,int bgm,int time,int announcer,int stage,int extra) {battleStage=stage;}
void BattleSetup_SetSide(int side,int control,int pad,int count,int form,int change,int lead,void *bits) {}
void BattleSetup_SetMember(int side,int index,int chara,int costume,int variant,int cpu,float health,u16 *items) {
    battleChara[side]=chara;battleCostume[side]=costume;
}
void BattleSetup_Finish(void) { battleFinished++; }

static void own(int id) { SAVE_CHARA_WORD(&save,id) |= ((u64)1 << ((id) % 64)); }
static void start(void) {
    static const int initial[] = {0,13,22,25,26,27,28,60,66,67};
    int i;
    memset(&save,0,sizeof(save));
    save.slot[0].flags = 1;
    for (i=0;i<10;i++) own(initial[i]);
}
static int poolCount(u8 *pool) { int i,n=0; for(i=0;i<161;i++) n+=!!pool[i]; return n; }
static void original(void) {
    int i;
    /* Independent representation of Save_SetDefaults' original starting set. */
    for(i=0;i<161;i++) own(i);
    { const int excluded[]={7,8,9,10,24,38,54,69,70,71,72,75,76,97,110,120,
        145,149,150,151,152,153,154,155,156,157,158,159,160};
      for(i=0;i<29;i++) SAVE_CHARA_WORD(&save,excluded[i]) &= ~((u64)1 << ((excluded[i]) % 64)); }
}
static void bracketCheck(int stage, int tour, int level, int players) {
    LBracket b;
    u8 allowed[161];
    int i,j,n;
    memset(&b,0,sizeof(b));memset(&progress,0,sizeof(progress));
    progress.tour=tour;progress.level=level;progress.entryNum=players;
    for(i=0;i<players;i++) {progress.entrant[i].chara=i?25:0;progress.entrant[i].flags=LTOUR_ENT_PLAYER;}
    PortTour_Eligible(&save,allowed);
    Bracket_FillEntrants(&b);
    assert(b.entrant[16].flags & LTOUR_ENT_BOSS);
    assert(allowed[b.entrant[16].chara] && b.entrant[16].pos==30);
    for(i=0;i<17;i++) {
        int id=b.entrant[i].chara;
        assert(id>=0 && id<161 && allowed[id]);
        assert(b.entrant[i].costume>=0 && b.entrant[i].costume<ChrTbl_WrapCostume(id,NULL));
        if(i<players) assert(b.entrant[i].flags & LTOUR_ENT_PLAYER);
        n=0;
        for(j=0;j<i;j++) if(b.entrant[j].chara==id) {
            n++;
            if(id==98 || id==99) assert(b.entrant[j].costume!=b.entrant[i].costume);
        }
        if(id==79 || id==109) assert(n<5);
        else if(id!=98 && id!=99 && i>=players) assert(n==0);
    }
}
int main(void) {
    const int sagas[]={0,1,2,3,5,6,7}, totals[]={20,48,93,122,139,147,152};
    u8 pool[161];int i,j,tour,level,seed,costume,picked;
    LTourEntrant entries[17];
    start();PortTour_Eligible(&save,pool);assert(poolCount(pool)==20);
    assert(pool[120] && pool[121] && !pool[73] && !pool[151]);
    own(151);PortTour_Eligible(&save,pool);assert(pool[151] && poolCount(pool)==21);
    for(i=0;i<128;i++) {
        save.slot[2].val[1]=i;
        for(tour=0;tour<5;tour++) assert(PortTour_Available(&save,tour)==(tour==0 || tour==4 || i==127));
    }
    original();assert(PortTour_OriginalRoster(&save));
    for(tour=0;tour<5;tour++) assert(PortTour_Available(&save,tour));
    for(i=0;i<8;i++) save.slot[i].flags=1;
    PortTour_Eligible(&save,pool);assert(poolCount(pool)==132 && !pool[151]);
    own(151);PortTour_Eligible(&save,pool);assert(poolCount(pool)==133 && pool[151]);
    SAVE_CHARA_WORD(&save,0)&=~((u64)1 << ((0) % 64));
    assert(!PortTour_OriginalRoster(&save)); /* 132 owned, but wrong set. */
    start();
    for(i=0;i<7;i++) {
        save.slot[sagas[i]].flags=1;
        PortTour_Eligible(&save,pool);assert(poolCount(pool)==totals[i]);
        if(i>=3) save.slot[2].val[1]=127;
        for(tour=0;tour<5;tour++) if(PortTour_Available(&save,tour))
        for(level=0;level<3;level++) for(seed=0;seed<80;seed++) {
            randomState=seed;bracketCheck(i,tour,level,1+seed%2);
        }
    }
    /* Identity beats cost preference, then incarnation beats form. */
    memset(pool,0,sizeof(pool));memset(entries,0,sizeof(entries));entries[0].chara=1;
    pool[2]=pool[3]=pool[25]=1;
    for(i=0;i<3;i++) assert(PortTour_Pick(pool,entries,1,-1,i,&costume)==25);
    pool[25]=0;assert(PortTour_Pick(pool,entries,1,-1,0,&costume)==3);
    pool[3]=0;assert(PortTour_Pick(pool,entries,1,-1,0,&costume)==2);
    /* Requested independent identities. */
    assert(sPortTourCharacters[13].identity!=sPortTourCharacters[159].identity);
    assert(sPortTourCharacters[39].identity!=sPortTourCharacters[44].identity);
    assert(sPortTourCharacters[112].identity!=sPortTourCharacters[113].identity);
    assert(sPortTourCharacters[113].identity==sPortTourCharacters[117].identity);
    /* Every individual slot is consumed exactly once. */
    {const int chars[]={79,109,98,99}, limits[]={5,5,3,4};
     for(j=0;j<4;j++) {
        memset(pool,0,sizeof(pool));pool[chars[j]]=1;memset(entries,0,sizeof(entries));
        for(i=0;i<limits[j];i++) {
            picked=PortTour_Pick(pool,entries,i,-1,0,&costume);assert(picked==chars[j]);
            entries[i].chara=picked;entries[i].costume=costume;
        }
        assert(PortTour_Pick(pool,entries,limits[j],-1,0,&costume)==-1);
     }}
    /* An unused individual is a new identity, not a third-tier duplicate. */
    memset(pool,0,sizeof(pool));pool[79]=pool[2]=1;entries[0].chara=79;entries[1].chara=1;
    assert(PortTour_Pick(pool,entries,2,-1,0,&costume)==79);
    /* Reserve boss identity without excluding its other forms entirely. */
    memset(pool,0,sizeof(pool));pool[67]=pool[25]=1;
    assert(PortTour_Pick(pool,entries,0,66,0,&costume)==25);
    pool[25]=0;assert(PortTour_Pick(pool,entries,0,66,0,&costume)==67);
    start();zenkai=0;
    for(tour=0;tour<5;tour++) assert(PortTour_Available(&save,tour));
    /* Production Original-mode generation still draws from its supplied grid. */
    {
        LBracket b; LChrCell grid[20];
        memset(&b,0,sizeof(b));memset(grid,0,sizeof(grid));memset(&progress,0,sizeof(progress));
        for(i=0;i<20;i++) grid[i].id=i;
        b.grid=grid;b.gridCount=20;progress.entryNum=1;progress.entrant[0].chara=0;
        progress.entrant[0].flags=LTOUR_ENT_PLAYER;progress.tour=0;
        Bracket_FillEntrants(&b);
        for(i=1;i<16;i++) {
            assert(b.entrant[i].chara>=0 && b.entrant[i].chara<20);
            for(j=0;j<i;j++) assert(b.entrant[j].chara!=b.entrant[i].chara);
        }
        assert(b.entrant[16].chara==66 || b.entrant[16].chara==56);
    }
    /* Five player-selected individuals exhaust the CPU's Saibaman slots. */
    zenkai=1;start();own(79);
    {
        LBracket b;
        memset(&b,0,sizeof(b));memset(&progress,0,sizeof(progress));
        progress.entryNum=5;
        for(i=0;i<5;i++) {progress.entrant[i].chara=79;progress.entrant[i].flags=LTOUR_ENT_PLAYER;}
        Bracket_FillEntrants(&b);
        for(i=5;i<17;i++) assert(b.entrant[i].chara!=79);
    }
    /* Outfit-specific slots survive the actual battle hand-off in either orientation. */
    {
        LBracket b;LTourCpu cpu[15];
        memset(&b,0,sizeof(b));memset(cpu,0,sizeof(cpu));memset(&progress,0,sizeof(progress));
        b.cpu=cpu;b.match_[0].ent[0]=0;b.match_[0].ent[1]=1;
        for(i=0;i<2;i++) {
            b.entrant[i].chara=0;b.entrant[i].costume=0;b.entrant[i].flags=LTOUR_ENT_PLAYER;
            b.entrant[i^1].chara=99;b.entrant[i^1].costume=3;b.entrant[i^1].flags=0;
            Bracket_SetupBattle(&b);
            assert(battleFinished==1 && battleChara[0]==0 && battleChara[1]==99 && battleCostume[1]==3);
        }
    }
    puts("Tournament rules: eligibility, gates, boss reservation, diversity, individual slots and 132-entry exception passed");
    /* Enumerate every possible random draw: exact probabilities, not flaky sampling. */
    {
        LBracket b;int leftWins;
        const int pairs[][2]={{2,8},{8,2},{3,3},{0,8},{0,0}};
        for(zenkai=0;zenkai<=1;zenkai++) for(int pair=0;pair<5;pair++) {
            int left=pairs[pair][0],right=pairs[pair][1];
            int total=zenkai && left+right>0?left+right:2;
            leftWins=0;
            for(forcedRoll=0;forcedRoll<total;forcedRoll++) {
                memset(&b,0,sizeof(b));b.match_[0].ent[0]=0;b.match_[0].ent[1]=1;
                b.match_[0].flags=LTOUR_MATCH_TO_LEFT;b.match_[0].next=1;b.match_[0].pos=17;
                b.entrant[0].chara=left;b.entrant[1].chara=right;randomCalls=0;
                Bracket_PlayCpuMatch(&b,0);
                int winner=!(b.entrant[0].flags & LTOUR_ENT_LOST)?0:1;
                leftWins+=winner==0;
                assert(randomCalls==1 && randomBound==total);
                assert(b.entrant[winner].pos==17 && b.match_[1].ent[0]==winner);
                assert(b.match_[0].flags & LTOUR_MATCH_DONE);
                Bracket_PlayCpuMatch(&b,0);assert(randomCalls==1);
            }
            assert(leftWins==(zenkai && left+right>0?left:1));
        }
        forcedRoll=0;zenkai=1;
        for(i=0;i<2;i++) {
            memset(&b,0,sizeof(b));b.match_[0].ent[1]=1;b.entrant[i].flags=LTOUR_ENT_PLAYER;
            randomCalls=0;Bracket_PlayCpuMatch(&b,0);
            assert(randomCalls==0 && !(b.match_[0].flags & LTOUR_MATCH_DONE));
        }
        forcedRoll=-1;
    }
    puts("CPU results: exact DP-weighted odds, advancement, player guards and Original 50/50 passed");
    {
        LBracket b;LTourCpu cpu[15];
        const int stages[]={19,19,19,31,32};
        memset(&b,0,sizeof(b));memset(cpu,0,sizeof(cpu));memset(&progress,0,sizeof(progress));
        b.cpu=cpu;b.match_[0].ent[1]=1;b.entrant[0].flags=LTOUR_ENT_PLAYER;
        b.entrant[0].chara=0;b.entrant[1].chara=26;progress.tour=LTOUR_YAMCHA;
        zenkai=1;
        for(i=0;i<5;i++) {b.round=i;Bracket_SetupBattle(&b);assert(battleStage==stages[i]);}
        /* Original still selects its maps from unlocked Yamcha stages. */
        zenkai=0;save.stageBits=(u64)1<<2;
        for(i=0;i<5;i++) {b.round=i;Bracket_SetupBattle(&b);assert(battleStage==2);}
    }
    puts("Yamcha stages: Noon rounds, Evening final, Night boss, and Original random pool passed");
    return 0;
}
