#ifndef MENU_PORT_STORY_ENDINGS_H
#define MENU_PORT_STORY_ENDINGS_H
#ifdef PORT
#include "sys/port_zenkai.h"

/* Unused save tail: 0x3800..0x3BF7, before difficulty badges at 0x3E00
 * and options at 0x3F00. Explicit bytes retain the original 0x4000 layout.
 * One bit per victorious action 10000..10051, for each of 9 sagas x 16 episodes.
 * Legacy clear bits cannot establish which ending was seen. */
#define PORT_STORY_ENDINGS_OFFSET 0x3800
#define PORT_STORY_ENDINGS_SAGAS 9
#define PORT_STORY_ENDINGS_EPISODES 16
#define PORT_STORY_ENDINGS_STRIDE 7
#define PORT_STORY_ENDINGS_BYTES (8 + PORT_STORY_ENDINGS_SAGAS * PORT_STORY_ENDINGS_EPISODES * PORT_STORY_ENDINGS_STRIDE)
#if PORT_STORY_ENDINGS_OFFSET + PORT_STORY_ENDINGS_BYTES > 0x3E00
#error Story ending records overlap difficulty badges
#endif

static inline int Port_StoryEndingsValid(const void *save, int saga, int episode, int action) {
    return save && Port_ZenkaiEnabled() && saga >= 0 && saga < 9 && episode >= 0 && episode < 16 &&
           action >= 10000 && action < 10052;
}

static inline int Port_StoryEndingsHeader(const unsigned char *p) {
    return p[0]=='Z' && p[1]=='E' && p[2]=='N' && p[3]=='1' &&
           p[4]==9 && p[5]==16 && p[6]==7 && p[7]==0;
}

static inline int Port_StoryHasEnding(const void *save, int saga, int episode, int action) {
    const unsigned char *p;
    int bit;
    if (!Port_StoryEndingsValid(save, saga, episode, action)) return 0;
    p = (const unsigned char *)save + PORT_STORY_ENDINGS_OFFSET;
    if (!Port_StoryEndingsHeader(p)) return 0;
    bit = action - 10000;
    return (p[8 + (saga * 16 + episode) * 7 + bit / 8] >> (bit % 8)) & 1;
}

/* Storage primitive: caller must have validated the victorious result.
 * Returns 1 only for a newly collected ending. */
static inline int Port_StoryRecordEnding(void *save, int saga, int episode, int action) {
    unsigned char *p;
    int i, bit;
    if (!Port_StoryEndingsValid(save, saga, episode, action)) return 0;
    if (Port_StoryHasEnding(save, saga, episode, action)) return 0;
    p = (unsigned char *)save + PORT_STORY_ENDINGS_OFFSET;
    if (!Port_StoryEndingsHeader(p)) {
        for (i = 0; i < PORT_STORY_ENDINGS_BYTES; i++) p[i] = 0;
        p[0]='Z'; p[1]='E'; p[2]='N'; p[3]='1'; p[4]=9; p[5]=16; p[6]=7;
    }
    bit = action - 10000;
    p[8 + (saga * 16 + episode) * 7 + bit / 8] |= 1u << (bit % 8);
    return 1;
}

static inline int Port_StoryEndingCount(const void *save, int saga, int episode) {
    int action, count = 0;
    for (action = 10000; action < 10052; action++) count += Port_StoryHasEnding(save, saga, episode, action);
    return count;
}

void Port_StoryEndingsReset(void);
void Port_StoryEndingsStage(int story, int action, int result);
int Port_StoryEndingsCommit(void *save, int saga, int episode, int story, int flags, int reason);
#endif
#endif
