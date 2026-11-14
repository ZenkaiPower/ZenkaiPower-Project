#ifndef PORT_STORY_BADGE_H
#define PORT_STORY_BADGE_H
#ifdef PORT
#include "sys/port_zenkai.h"
/* Port-only extension in the unused tail of the original 0x4000-byte save.
   Magic + 9 sagas * 16 episodes; values 1..3 are highest clear difficulty. */
/* Migrate original clear bits to Easy without inferring a higher difficulty. */
static inline void Port_StoryInitRecords(void *save) {
    u8 *data = (u8 *)save + 0x3E00;
    s32 saga, episode, i;
    if (!Port_ZenkaiEnabled()) return;
    if (data[0] != 'Z' || data[1] != 'H' || data[2] != 'D' || data[3] != '1') {
        for (i = 0; i < 4 + 9 * 16; i++) data[i] = 0;
        data[0] = 'Z'; data[1] = 'H'; data[2] = 'D'; data[3] = '1';
    }
    for (saga = 0; saga < 9; saga++) {
        u32 cleared = *(u32 *)((u8 *)save + 0x18 + saga * 0x10);
        for (episode = 0; episode < 16; episode++) {
            u8 *record = &data[4 + saga * 16 + episode];
            if ((cleared & (1u << episode)) && (*record == 0 || *record > 3)) *record = 1;
        }
    }
}
static inline s32 Port_StoryClearLevel(void *save, s32 saga, s32 episode) {
    u8 *data = (u8 *)save + 0x3E00;
    if (!Port_ZenkaiEnabled() || saga < 0 || saga >= 9 || episode < 0 || episode >= 16 ||
        (data[0] != 'Z' || data[1] != 'H' || data[2] != 'D' || data[3] != '1')) return 0;
    return data[4 + saga * 16 + episode] <= 3 ? data[4 + saga * 16 + episode] : 0;
}
static inline void Port_StoryRecordClear(void *save, s32 saga, s32 episode, s32 difficulty) {
    u8 *data = (u8 *)save + 0x3E00;
    s32 old;
    if (!Port_ZenkaiEnabled() || saga < 0 || saga >= 9 || episode < 0 || episode >= 16 || difficulty < 0 || difficulty > 2) return;
    Port_StoryInitRecords(save);
    old = Port_StoryClearLevel(save, saga, episode);
    if (difficulty + 1 > old) data[4 + saga * 16 + episode] = difficulty + 1;
}
extern void Port_StoryBadge(void *entry, s32 level, s32 reset);
#endif
#endif
