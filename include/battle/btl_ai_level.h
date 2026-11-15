#ifndef BATTLE_BTL_AI_LEVEL_H
#define BATTLE_BTL_AI_LEVEL_H

/* Mode data, saves and scripts keep their original levels. Only the live AI
 * controller uses the ZENKAI range, so resets never apply the offset twice. */
#ifdef PORT
#include "sys/port_zenkai.h"
extern int Port_NetSession(void);
extern int Port_NetActive(void);
#endif

#define BTLAI_ORIGINAL_MAX 29
#define BTLAI_ZENKAI_OFFSET 10
#define BTLAI_ZENKAI_MAX 29
/* Tagged story bases represent effective levels below 10 without colliding
 * with the negative training-dummy settings. */
#define BTLAI_STORY_LOW_BASE 0x100

static inline int BtlAiLevel_ZenkaiEnabled(void) {
#ifdef PORT
    return PORT_ZENKAI_RAISED_DIFFICULTY && Port_ZenkaiEnabled() && !Port_NetSession() && !Port_NetActive();
#else
    return 0;
#endif
}

static inline int BtlAiLevel_FromBase(int level) {
#ifdef PORT
    if (level >= 0 && BtlAiLevel_ZenkaiEnabled()) {
        if (level >= BTLAI_STORY_LOW_BASE && level < BTLAI_STORY_LOW_BASE + BTLAI_ZENKAI_OFFSET)
            return level - BTLAI_STORY_LOW_BASE;
        if (level > BTLAI_ZENKAI_MAX - BTLAI_ZENKAI_OFFSET)
            level = BTLAI_ZENKAI_MAX - BTLAI_ZENKAI_OFFSET;
        return level + BTLAI_ZENKAI_OFFSET;
    }
#endif
    return level;
}

/* Dragon History's script-level adjustment, before the controller's +10. */
static inline int BtlAiLevel_StoryOffset(int difficulty) {
    if (BtlAiLevel_ZenkaiEnabled()) {
        switch (difficulty) {
        case 0: return -5;
        case 1: return 0;
        case 2: return 5;
        }
    } else {
        switch (difficulty) {
        case 1: return 6;
        case 2: return 9;
        }
    }
    return 0;
}

/* Easy may fall below effective level 10. Encode that case separately from
 * negative dummy settings; the controller still applies the rebase once. */
static inline int BtlAiLevel_StoryBase(int level, int difficulty) {
    int adjusted = level + BtlAiLevel_StoryOffset(difficulty);
    if (BtlAiLevel_ZenkaiEnabled() && adjusted < 0) {
        int effective = adjusted + BTLAI_ZENKAI_OFFSET;
        if (effective < 0) effective = 0;
        return BTLAI_STORY_LOW_BASE + effective;
    }
    return adjusted;
}

static inline int BtlAiLevel_ToBase(int level) {
#ifdef PORT
    if (level >= 0 && level < BTLAI_ZENKAI_OFFSET && BtlAiLevel_ZenkaiEnabled())
        return BTLAI_STORY_LOW_BASE + level;
    if (level >= BTLAI_ZENKAI_OFFSET && BtlAiLevel_ZenkaiEnabled())
        return level - BTLAI_ZENKAI_OFFSET;
#endif
    return level;
}

/* Preserve the five original Duel levels while other modes use the +10 rebase. */
static inline int BtlAiLevel_DuelBase(int original) {
    return BtlAiLevel_ZenkaiEnabled() ? BtlAiLevel_ToBase(original) : original;
}

/* Duel's effective levels are the original menu values. */
static inline int BtlAiLevel_DuelSettingBase(int effective) {
    return effective;
}

/* Existing five-entry tables retain their original meanings and safe bounds. */
static inline int BtlAiLevel_Index(int level) {
    return level < 0 ? 0 : (level > BTLAI_ORIGINAL_MAX ? 4 : level / 6);
}

#endif
