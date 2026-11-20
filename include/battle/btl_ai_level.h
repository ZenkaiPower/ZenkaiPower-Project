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
#define BTLAI_ZENKAI_MAX 29
/* Tagged levels preserve exact story and Duel effective levels. */
#define BTLAI_STORY_LOW_BASE 0x100

static inline int BtlAiLevel_ZenkaiEnabled(void) {
#ifdef PORT
    return PORT_ZENKAI_RAISED_DIFFICULTY && Port_ZenkaiPackEnabled() && !Port_NetSession() && !Port_NetActive();
#else
    return 0;
#endif
}

static inline int BtlAiLevel_FromBase(int level) {
#ifdef PORT
    if (level >= 0 && BtlAiLevel_ZenkaiEnabled()) {
        if (level >= BTLAI_STORY_LOW_BASE && level <= BTLAI_STORY_LOW_BASE + BTLAI_ZENKAI_MAX)
            return level - BTLAI_STORY_LOW_BASE;
        if (level < 10) level += 7;
        else if (level < 15) level += 6;
        else if (level < 20) level += 5;
        else if (level < 25) level += 4;
        else if (level < 29) level += 3;
        return level > BTLAI_ZENKAI_MAX ? BTLAI_ZENKAI_MAX : level;
    }
#endif
    return level;
}

/* Story difficulty is applied after tapering the scripted base. */
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

/* Taper first, then apply difficulty so band crossings do not alter its spacing. */
static inline int BtlAiLevel_StoryBase(int level, int difficulty) {
    int adjusted = level + BtlAiLevel_StoryOffset(difficulty);
    if (BtlAiLevel_ZenkaiEnabled()) {
        int effective;
        if (level < 0) return level;
        effective = BtlAiLevel_FromBase(level) + BtlAiLevel_StoryOffset(difficulty);
        if (effective < 0) effective = 0;
        if (effective > BTLAI_ZENKAI_MAX) effective = BTLAI_ZENKAI_MAX;
        return BTLAI_STORY_LOW_BASE + effective;
    }
    return adjusted;
}

/* Select the first base reaching this effective level for the pause-menu label.
 * Tapering has overlapping outputs, so this inverse is intentionally canonical. */
static inline int BtlAiLevel_ToBase(int level) {
#ifdef PORT
    if (level >= 0 && BtlAiLevel_ZenkaiEnabled()) {
        int base;
        for (base = 0; base <= BTLAI_ORIGINAL_MAX; base++)
            if (BtlAiLevel_FromBase(base) >= level) return base;
        return BTLAI_ORIGINAL_MAX;
    }
#endif
    return level;
}

/* Duel bypasses tapering and preserves every original menu level exactly. */
static inline int BtlAiLevel_DuelBase(int original) {
    return original >= 0 && BtlAiLevel_ZenkaiEnabled()
        ? BTLAI_STORY_LOW_BASE + original : original;
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
