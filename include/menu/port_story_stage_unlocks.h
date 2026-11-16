#ifndef MENU_PORT_STORY_STAGE_UNLOCKS_H
#define MENU_PORT_STORY_STAGE_UNLOCKS_H
#include "types.h"

/* Visible saga/episode indices (zero-based), not the rotated Special script IDs.
 * Stage IDs follow the original stage selector; include each time-of-day form. */
typedef struct PortStoryStageReward {
    int saga, episode;
    u64 stages;
} PortStoryStageReward;
static const PortStoryStageReward sPortStoryStageRewards[] = {
    {0,2,(u64)1<<2},                      /* Ultimate Decisive Battle: Namek */
    {1,3,(u64)1<<3},                      /* Legendary Super Saiyan: Dying Namek */
    {2,0,((u64)1<<10)|((u64)1<<33)},       /* Enraged Android: Mountain Road noon/evening */
    {2,1,(u64)1<<11},                     /* Vegeta's Curiosity: Islands */
    {2,2,((u64)1<<6)|((u64)1<<28)},        /* Perfect Body: Cell Games evening/noon */
    {4,0,(u64)1<<16},                     /* Alone for the Final Battle: Space */
    {4,1,(u64)1<<14},                     /* Strongest in the World: Glacier */
    {3,3,(u64)1<<7},                      /* Battle for the Universe: Supreme Kai */
    {7,2,((u64)1<<13)|((u64)1<<34)},       /* Galaxy Battle: Planet night/evening */
};
static u64 Port_StoryStageManagedMask(void) {
    u64 mask=0;
    int i;
    for(i=0;i<(int)(sizeof(sPortStoryStageRewards)/sizeof(sPortStoryStageRewards[0]));i++)
        mask|=sPortStoryStageRewards[i].stages;
    return mask;
}
static u64 Port_StoryStageRewardMask(int saga,int episode) {
    int i;
    for(i=0;i<(int)(sizeof(sPortStoryStageRewards)/sizeof(sPortStoryStageRewards[0]));i++)
        if(sPortStoryStageRewards[i].saga==saga && sPortStoryStageRewards[i].episode==episode)
            return sPortStoryStageRewards[i].stages;
    return 0;
}
#endif
