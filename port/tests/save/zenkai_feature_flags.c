#include "common.h"
#include "menu/menu_c.h"
#include "sys/port_zenkai.h"
#include "sys/common.h"
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
    /* Frieza episode 1 substitutes its force for Nail only with story progression. */
    memset(&save,0,sizeof(save));memset(&progress,0,sizeof(progress));
    memset(&progress.reward,0xFF,sizeof(progress.reward));
    progress.subMenu=1;progress.reward.chara[0]=24;progress.reward.points[0]=100;
    HistResult_BuildRewards();
    assert(!!(save.charaBits[0]&((u64)1<<24))==!PORT_ZENKAI_STORY_UNLOCKS);
    assert(!!(save.charaBits[1]&((u64)1<<(84%64)))==PORT_ZENKAI_STORY_UNLOCKS);
}
