#include "common.h"
#ifdef PORT
#include "sys/save.h"
#include "sys/port_zenkai.h"
#include "sys/port_zenkai_pack.h"
extern int Port_NetSession(void);
extern int Port_NetActive(void);

static int pack_offline(void) {
    return Port_ZenkaiEnabled() && !Port_NetSession() && !Port_NetActive();
}
void Port_ZenkaiPackInit(void *save) {
    if (save && pack_offline()) Port_ZenkaiPackWriteFlags(save, 0);
}
int Port_ZenkaiPackEnabled(void) {
    return pack_offline() && !(Port_ZenkaiPackFlags(gSaveData) & PORT_ZENKAI_PACK_DISABLED);
}
int Port_ZenkaiPackCanDisable(void) {
    return pack_offline() && gSaveData && gSaveData->money == 0 &&
        Port_ZenkaiPackFlags(gSaveData) == 0;
}
void Port_ZenkaiPackEarnedZeni(void *save, int amount) {
    if (save && amount > 0 && pack_offline())
        Port_ZenkaiPackWriteFlags(save, Port_ZenkaiPackFlags(save) | PORT_ZENKAI_PACK_EARNED_ZENI);
}
int Port_ZenkaiPackDisable(void) {
    static const u8 excluded[] = {7,8,9,10,24,38,54,69,70,71,72,75,76,97,110,
        120,145,149,150,151,152,153,154,155,156,157,158,159,160};
    int i,j;
    if (!Port_ZenkaiPackCanDisable()) return 0;
    Port_ZenkaiPackWriteFlags(gSaveData, PORT_ZENKAI_PACK_DISABLED);
    /* Turning off story unlocking grants the original starting roster without
     * erasing any existing unlocks or other progress. */
    for (i=0;i<SAVE_CHARA_COUNT;i++) {
        for (j=0;j<(int)sizeof(excluded) && excluded[j]!=i;j++) {}
        if (j==(int)sizeof(excluded)) SAVE_CHARA_WORD(gSaveData,i) |= (u64)1 << (i%64);
    }
    for (i=0;i<SAVE_STAGE_COUNT;i++) {
        if ((i>=17 && i<=22) || i==31 || i==32) continue;
        gSaveData->stageBits |= (u64)1 << i;
    }
    return 1;
}
static int pack_equal(const char *a,const char *b) {
    if (!a || !b) return 0;
    while (*a && *a==*b) {a++;b++;}
    return *a==*b;
}
static int pack_feature(const char *id) {
    static const char *const ids[]={"giant_scaling","dragon_history_unlocking",
        "world_tour_change","raise_base_level","story_restrictions","goku_ssj2","tiered_kaioken"};
    int i;
    for (i=0;i<7;i++) if (pack_equal(id,ids[i])) return i;
    return -1;
}
int Port_ZenkaiFeatureEnabled(const char *id) {
    static const int compiled[]={PORT_ZENKAI_GIANTS,PORT_ZENKAI_STORY_UNLOCKS,
        PORT_ZENKAI_TOURNAMENT,PORT_ZENKAI_RAISED_DIFFICULTY,PORT_ZENKAI_STORY_RESTRICTIONS,
        PORT_ZENKAI_SSJ2_MOVESET,PORT_ZENKAI_KAIOKEN};
    int index=pack_feature(id);
    return index>=0 && compiled[index] && Port_ZenkaiPackEnabled();
}
int Port_ZenkaiFeatureInGroup(const char *id,const char *group) {
    int index=pack_feature(id);
    if (index<0) return 0;
    if (pack_equal(group,"zenkai_pack")) return 1;
    return pack_equal(group,"battle_mechanic_change") && (index==0 || index==5 || index==6);
}
#endif
