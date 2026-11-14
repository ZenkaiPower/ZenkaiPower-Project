/* Build src/sys/save.c with the port compatibility header and function sections,
 * then link this harness with --gc-sections and port/src/plat_zenkai.c. */
#include "sys/save.h"
#include "sys/common.h"
#include "sys/port_zenkai.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
CommonRes *gCommonRes;
SaveData *gSaveData;
static int setting;
int Port_Setting(const char *name,int fallback) { return !strcmp(name,"zenkai_power") ? setting : fallback; }
void Progress_ClearSession(void) {}
int Port_OptionsSaveSync(void *save) { (void)save;return 0; }
static int unlocked(int i) { return (SAVE_CHARA_WORD(gSaveData,i) & SAVE_CHARA_MASK(i)) != 0; }
int main(void) {
 CommonRes common={0};
 struct { ItemFile header; ItemInfo info[SAVE_ITEM_COUNT]; } items={0};
 SaveData save;
 int count=0, i;
 unsetenv("ZENKAIPOWER_SAVES");gCommonRes=&common;gSaveData=&save;
 items.header.itemOffset=(u32)((u8 *)items.info-(u8 *)&items);common.data[2]=&items;
 setting=1;Port_ZenkaiSelect();Save_SetDefaults(&save);Port_ZenkaiFinish();
 for(i=0;i<SAVE_CHARA_COUNT;i++) count+=unlocked(i);
 assert(count==10 && unlocked(0) && unlocked(13) && unlocked(22) && unlocked(25) && unlocked(26) && unlocked(27) && unlocked(28) && unlocked(66) && unlocked(67) && unlocked(60));
 assert(!unlocked(77) && !unlocked(79) && !unlocked(80) && !unlocked(29) && !unlocked(30));
 setting=0;Port_ZenkaiSelect();Save_SetDefaults(&save);Port_ZenkaiFinish();count=0;
 for(i=0;i<SAVE_CHARA_COUNT;i++)count+=unlocked(i);
 assert(count>9 && unlocked(77) && unlocked(79) && unlocked(80) && unlocked(29) && unlocked(30));
 assert(!unlocked(7));
 puts("New-game roster checks passed: ten ZENKAI character entries and original common-save defaults.");
 return 0;
}
