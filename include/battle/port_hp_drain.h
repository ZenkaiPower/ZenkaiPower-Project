#ifndef BATTLE_PORT_HP_DRAIN_H
#define BATTLE_PORT_HP_DRAIN_H
#include "../sys/port_zenkai_features.h"
#ifndef PORT_ZENKAI_KAIOKEN
#define PORT_ZENKAI_KAIOKEN 1
#endif
/* Zenkai-only HP-drain ability bits. */
#define PORT_ABILITY_HP_DRAIN_50 0x7D
#define PORT_ABILITY_HP_DRAIN_100 0x7E
#define PORT_ABILITY_HP_DRAIN_150 0x7F
/* Two reserved member ability bits keep the tier inside rollback state. */
#define PORT_KAIOKEN_TIER_MASK (3u << 26)
/* Highest drain tier wins. */
static int Port_HpDrainRate(int tier50,int tier100,int tier150,int kaioken) {
    int rate = tier150 ? 150:tier100 ? 100:tier50 ? 50:0;
    return kaioken * 50 > rate ? kaioken * 50:rate;
}
static int Port_HpDrainAmount(int hp,int rate,int lethal) {
    int room=hp-(lethal ? 0:1);
    return room<=0 ? 0:rate<room ? rate:room;
}
#ifdef PORT
extern int Port_ZenkaiEnabled(void);
extern int Port_ZenkaiPackEnabled(void);
int Port_BtlKaiokenTier(void *fighter);
void Port_BtlKaiokenClear(void *fighter);
int Port_BtlKaiokenUpgrade(void *fighter,int cost);
void Port_BtlHpDrainTick(void *fighter,int kaioken);
#endif
#endif
