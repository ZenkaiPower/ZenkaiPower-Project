#include "common.h"
#include "menu/menu_c.h"
#include "sys/port_zenkai.h"
#include "sys/common.h"
#include "menu/port_story_stage_unlocks.h"
#include <assert.h>

static CommonRes resources;
CommonRes *gCommonRes=&resources;
static struct {ItemFile file;ItemInfo items[SAVE_ITEM_COUNT];} itemData;
static MBattleResult battle;
MBattleResult *BattleResult_GetPtr(void) {return &battle;}
void Progress_ClearSession(void) {}
void Port_ZenkaiCommit(void) {}
void Port_ZenkaiNewGame(void) {}
int Port_OptionsSaveSync(void *save) {return 0;}
extern void HistResult_BuildRewards(void);

void TestStoryFeatures(void) {
    static HistProgress progress;
    static HistResult result;
    static SaveData save;
    int i,count;
    gProgress=(MenuProgress *)&progress;gSaveData=&save;gHistResult=&result;
    itemData.file.itemOffset=sizeof(ItemFile);resources.data[2]=&itemData;
    Save_SetDefaults(&save);
    count=0;for(i=0;i<161;i++) count+=!!(SAVE_CHARA_WORD(&save,i)&((u64)1<<(i%64)));
    assert(count==(PORT_ZENKAI_STORY_UNLOCKS?10:132));
    assert((save.stageBits & Port_StoryStageManagedMask()) ==
        (PORT_ZENKAI_STORY_UNLOCKS ? 0 : Port_StoryStageManagedMask()));
    /* Frieza episode 1 substitutes its force for Nail only with story progression. */
    memset(&save,0,sizeof(save));memset(&progress,0,sizeof(progress));
    memset(&progress.reward,0xFF,sizeof(progress.reward));
    progress.subMenu=1;progress.reward.chara[0]=24;progress.reward.points[0]=100;
    HistResult_BuildRewards();
    assert(!!(save.charaBits[0]&((u64)1<<24))==!PORT_ZENKAI_STORY_UNLOCKS);
    assert(!!(save.charaBits[1]&((u64)1<<(84%64)))==PORT_ZENKAI_STORY_UNLOCKS);
    /* Every visible episode, including both Legendary Super Saiyan titles. */
    {
        static const int counts[]={3,4,7,5,16,5,4,4};
        static const struct {int saga,episode;u64 mask;} expected[]={
            {0,2,(u64)1<<2},{1,3,(u64)1<<3},{2,1,(u64)1<<11},
            {2,0,((u64)1<<10)|((u64)1<<33)},
            {2,2,((u64)1<<6)|((u64)1<<28)},{4,0,(u64)1<<16},
            {4,1,(u64)1<<14},{3,3,(u64)1<<7},{7,2,((u64)1<<13)|((u64)1<<34)}
        };
        int saga,episode,j,n;
        for(saga=0;saga<8;saga++) for(episode=0;episode<counts[saga];episode++) {
            u64 mask=0,seen=0;
            memset(&save,0,sizeof(save));memset(&progress,0,sizeof(progress));
            Port_ZenkaiPackInit(&save);
            memset(&progress.reward,0xFF,sizeof(progress.reward));
            progress.subMenu=saga;progress.subMenuItem=episode;
            for(j=0;j<(int)(sizeof(expected)/sizeof(expected[0]));j++) if(expected[j].saga==saga && expected[j].episode==episode) mask=expected[j].mask;
            if(!PORT_ZENKAI_STORY_UNLOCKS) mask=0;
            HistResult_BuildRewards();assert(save.stageBits==mask);
            assert(result.rewardCount<=HISTRESULT_REWARD_MAX);
            for(n=0;n<result.rewardCount;n++) if(result.reward[n].kind==1) {
                u64 bit=(u64)1<<result.reward[n].id;
                assert(!(seen & bit));seen|=bit;
            }
            assert(seen==mask);
            HistResult_BuildRewards();
            for(n=0;n<result.rewardCount;n++) assert(result.reward[n].kind!=1);
            assert(save.stageBits==mask); /* replay never duplicates stage popups */
        }
        /* Classic Mode still accepts original stage rewards, without additions. */
        memset(&save,0,sizeof(save));Port_ZenkaiPackInit(&save);assert(Port_ZenkaiPackDisable());
        assert((save.stageBits & Port_StoryStageManagedMask())==Port_StoryStageManagedMask());
        save.stageBits=0;memset(&progress.reward,0xFF,sizeof(progress.reward));
        progress.subMenu=2;progress.subMenuItem=2;progress.reward.stage[0]=0;
        HistResult_BuildRewards();assert(save.stageBits==1);
    }
}
