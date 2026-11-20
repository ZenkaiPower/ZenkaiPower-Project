#include "common.h"
#include "menu/menu_l.h"
#include "menu/port_tour_bracket.h"
#include "battle/btl_ai_level.h"
#include <assert.h>

MenuProgress *gProgress;
SaveData *gSaveData;
int Port_ZenkaiEnabled(void) {return 1;}
int Port_NetSession(void) {return 0;}
int Port_NetActive(void) {return 0;}
static unsigned state=17;
u32 Rand_Range(u32 n) {assert(n);state=state*1664525u+1013904223u;return (state>>8)%n;}
int ChrTbl_GetCost(int chara) {return 3;}
int ChrTbl_WrapCostume(int chara,int *color) {return 2;}
extern void TestStoryFeatures(void);
extern int Port_StoryIsFirstEpisode(void);
extern int Port_StoryIsSaiyanEpisode(void);
extern void Bracket_FillEntrants(LBracket *);
extern void Bracket_SetupBattle(LBracket *);
static int stage;
void Battle_ClearWork(void) {}
void BattleSetup_SetRule(int screen,int mode,int bgm,int time,int announcer,int map,int extra) {stage=map;}
void BattleSetup_SetSide(int side,int control,int pad,int count,int form,int change,int lead,void *bits) {}
void BattleSetup_SetMember(int side,int index,int chara,int costume,int variant,int cpu,float health,u16 *items) {}
void BattleSetup_Finish(void) {}

int main(void) {
    static LTourProgress progress;
    static SaveData save;
    static LBracket bracket;
    static LTourCpu cpu[15];
    static struct {u32 header[64],gridHeader[4];LChrCell grid[20];} pack;
    int i;
    TestStoryFeatures();
    gProgress=(MenuProgress *)&progress;gSaveData=&save;
    gProgress->subMenu=0;gProgress->subMenuItem=0;
    assert(Port_StoryIsFirstEpisode()==PORT_ZENKAI_STORY_RESTRICTIONS);
    assert(Port_StoryIsSaiyanEpisode()==PORT_ZENKAI_STORY_RESTRICTIONS);
    gProgress->subMenuItem=1;assert(!Port_StoryIsFirstEpisode());
    assert(Port_StoryIsSaiyanEpisode()==PORT_ZENKAI_STORY_RESTRICTIONS);
    gProgress->subMenu=1;assert(!Port_StoryIsSaiyanEpisode());
    assert(BtlAiLevel_FromBase(6)==(PORT_ZENKAI_RAISED_DIFFICULTY?13:6));
    assert(BtlAiLevel_StoryOffset(0)==(PORT_ZENKAI_RAISED_DIFFICULTY?-5:0));
    assert(BtlAiLevel_StoryOffset(2)==(PORT_ZENKAI_RAISED_DIFFICULTY?5:9));
    assert(PortTour_Available(&save,2)==!PORT_ZENKAI_TOURNAMENT);
    save.slot[0].flags=1;
    {const int initial[]={0,13,22,25,26,27,28,60,66,67};
     for(i=0;i<10;i++) SAVE_CHARA_WORD(&save,initial[i])|=(u64)1<<(initial[i]%64);}
    pack.header[24]=256;pack.gridHeader[0]=20;
    for(i=0;i<20;i++) pack.grid[i].id=i+100;
    bracket.res=(u32 *)&pack;bracket.grid=pack.grid;bracket.gridCount=20;
    progress.entryNum=1;progress.entrant[0].chara=0;progress.entrant[0].flags=LTOUR_ENT_PLAYER;
    Bracket_FillEntrants(&bracket);
    for(i=1;i<16;i++) {
        if(PORT_ZENKAI_TOURNAMENT) {
            u8 pool[161];PortTour_Eligible(&save,pool);assert(pool[bracket.entrant[i].chara]);
        } else assert(bracket.entrant[i].chara>=100 && bracket.entrant[i].chara<120);
    }
    progress.tour=LTOUR_YAMCHA;bracket.cpu=cpu;bracket.round=4;bracket.match=0;
    bracket.match_[0].ent[0]=0;bracket.match_[0].ent[1]=1;
    bracket.entrant[0].flags=LTOUR_ENT_PLAYER;bracket.entrant[1].flags=0;
    save.stageBits=(u64)1<<2;
    Bracket_SetupBattle(&bracket);
    assert(stage==(PORT_ZENKAI_TOURNAMENT?32:2));
    /* A saved Off pack wins over every enabled build switch. */
    Port_ZenkaiPackInit(&save);
    assert(Port_ZenkaiPackDisable());
    gProgress->subMenu=0;gProgress->subMenuItem=0;
    assert(!Port_StoryIsFirstEpisode() && !Port_StoryIsSaiyanEpisode());
    assert(!PortTour_Enabled() && PortTour_Available(&save,2));
    assert(BtlAiLevel_FromBase(6)==6 && BtlAiLevel_StoryOffset(2)==9);
    return 0;
}
