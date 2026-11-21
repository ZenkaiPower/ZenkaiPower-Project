#include "common.h"
#include "sys/save.h"
#include "sys/port_zenkai.h"
#include "sys/port_zenkai_pack.h"
#include <assert.h>
#include <string.h>
static SaveData save, disk;
SaveData *gSaveData=&save;
static int active=1,online;
int Port_ZenkaiEnabled(void) {return active;}
int Port_NetSession(void) {return online;}
int Port_NetActive(void) {return online;}
int gPortUbPointsPending;
void Snd_PlaySe(int a,int b) {}
extern int TestPackTransfer(int *,int *,int,float);
int main(void) {
    static const char *const ids[]={"giant_scaling","dragon_history_unlocking","world_tour_change",
        "raise_base_level","story_restrictions","goku_ssj2","tiered_kaioken","ginyu_body_change","extra_outfits","future_gohan_grab","great_ape_moon","cell_transformation","spirit_bomb","manual_lock","free_camera"};
    static const int compiled[]={PORT_ZENKAI_GIANTS,PORT_ZENKAI_STORY_UNLOCKS,PORT_ZENKAI_TOURNAMENT,
        PORT_ZENKAI_RAISED_DIFFICULTY,PORT_ZENKAI_STORY_RESTRICTIONS,PORT_ZENKAI_SSJ2_MOVESET,PORT_ZENKAI_KAIOKEN,PORT_ZENKAI_GINYU_BODY_CHANGE,PORT_ZENKAI_EXTRA_OUTFITS,PORT_ZENKAI_FUTURE_GOHAN_GRAB,PORT_ZENKAI_GREAT_APE_MOON,PORT_ZENKAI_CELL_TRANSFORMATION,PORT_ZENKAI_SPIRIT_BOMB,PORT_ZENKAI_MANUAL_LOCK,PORT_ZENKAI_FREE_CAMERA};
    int i,n,points;
    /* Legacy zero-balance saves remain locked: spending cannot establish no earnings. */
    assert(Port_ZenkaiPackEnabled() && !Port_ZenkaiPackCanDisable());
    Port_ZenkaiPackInit(&save);
    assert(Port_ZenkaiPackCanDisable());
    for(i=0;i<(int)(sizeof(ids)/sizeof(ids[0]));i++) {
        assert(Port_ZenkaiFeatureEnabled(ids[i])==compiled[i]);
        assert(Port_ZenkaiFeatureInGroup(ids[i],"zenkai_pack"));
        assert(Port_ZenkaiFeatureInGroup(ids[i],"battle_mechanic_change")== (i==0 || i==5 || i==6 || i==9 || i==10 || i==11 || i==12 || i==13 || i==14));
    }
    assert(!Port_ZenkaiFeatureEnabled("unknown") && !Port_ZenkaiFeatureInGroup(0,"zenkai_pack"));
    Save_AddMoney(0);Save_AddMoney(-5);assert(Port_ZenkaiPackCanDisable());
    Save_AddMoney(1);assert(!Port_ZenkaiPackCanDisable() && !Port_ZenkaiPackDisable());
    Save_AddMoney(-1);assert(save.money==0 && !Port_ZenkaiPackCanDisable());
    memcpy(&disk,&save,sizeof(save));memset(&save,0,sizeof(save));memcpy(&save,&disk,sizeof(save));
    assert(Port_ZenkaiPackFlags(&save)&PORT_ZENKAI_PACK_EARNED_ZENI);
    assert(!Port_ZenkaiPackCanDisable());
    /* Direct Ultimate Battle/Mission/Survival money transfers also lock it. */
    Port_ZenkaiPackInit(&save);points=100;
    TestPackTransfer(&points,&save.money,10,1);assert(save.money==10 && !Port_ZenkaiPackCanDisable());
    Save_AddMoney(-10);assert(!Port_ZenkaiPackCanDisable());
    Port_ZenkaiPackInit(&save);points=1;
    TestPackTransfer(&points,&save.money,10,0.1f);assert(save.money==0 && Port_ZenkaiPackCanDisable());
    save.money=SAVE_MONEY_MAX;Save_AddMoney(1);
    assert(Port_ZenkaiPackFlags(&save)&PORT_ZENKAI_PACK_EARNED_ZENI);
    memset(&save,0,sizeof(save));Port_ZenkaiPackInit(&save);
    active=0;assert(!Port_ZenkaiPackEnabled() && !Port_ZenkaiPackDisable());active=1;
    online=1;assert(!Port_ZenkaiPackEnabled() && !Port_ZenkaiPackDisable());online=0;
    assert(Port_ZenkaiPackDisable());
    assert(!Port_ZenkaiPackEnabled() && !Port_ZenkaiPackCanDisable() && !Port_ZenkaiPackDisable());
    for(i=n=0;i<SAVE_CHARA_COUNT;i++) if(SAVE_CHARA_WORD(&save,i)&((u64)1<<(i%64))) n++;
    assert(n==132);
    memcpy(&disk,&save,sizeof(save));memset(&save,0,sizeof(save));memcpy(&save,&disk,sizeof(save));
    for(i=0;i<(int)(sizeof(ids)/sizeof(ids[0]));i++) assert(!Port_ZenkaiFeatureEnabled(ids[i]));
    assert(!Port_ZenkaiPackCanDisable());
    return 0;
}
