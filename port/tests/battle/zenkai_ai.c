/* Compiled against the actual AI objects by zenkai_ai.py, under ASan/UBSan. */
#include "common.h"
#include "battle/btl_ai_level.h"
#include <assert.h>
#include <stdio.h>

static int active, session, online;
static u32 roll;
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return session; }
int Port_NetActive(void) { return online; }
u32 Rand_Range(u32 n) { assert(n > 0); return roll % n; }

#ifdef TEST_CHARGE
#include "battle/btl_ai_act.h"
static AiActMgr state;
AiActMgr *gBtlAi = &state;
extern f32 BtlAiAtk_RollCharge(AiActSide *, s8, s8);
int main(void) {
    int level, r, count, previous = 0;
    for (level = -3; level <= 100; level++) {
        state.side[0].cpuLevel = level;
        count = 0;
        for (r = 0; r < 100; r++) {
            roll = r;
            count += BtlAiAtk_RollCharge(&state.side[0], 10, 0) > 0;
        }
        assert(count >= previous);
        previous = count;
        if (level == 29) assert(count == 50);
        if (level > 29) assert(count == 50);
    }
    puts("Charge selection: levels above 29 use the original maximum threshold");
    return 0;
}
#else
#include "battle/btl_ai.h"
static BtlAi state;
BtlAi *gBtlAi = &state;

#ifdef TEST_REACT
static int action;
extern s32 BtlAiCond_React(BtlAiWork *, u8);
s32 BtlCharApi_GetUnk974(s32 side) { (void)side; return action; }
s32 BtlCharApi_GetPromptButtons(s32 side) { (void)side; return 0; }
int main(void) {
    static const int expected[5][5] = {
        {20,40,60,80,95}, {10,20,30,40,50},
        {0,4,8,12,15}, {0,3,5,8,10}, {0,2,3,4,5}
    };
    static BtlAiData data;
    static struct { BtlAiActTable act; s8 tail[256]; } actions;
    int level, kind, r, count, previous;
    state.data = &data;
    data.act = &actions.act;
    for (kind = 0; kind < 5; kind++) {
        previous = 0;
        for (level = -3; level <= 100; level++) {
            state.work[0].level = level;
            action = kind < 2 ? 0 : 0x37 + kind - 2;
            count = 0;
            for (r = 0; r < 100; r++) {
                roll = r;
                count += BtlAiCond_React(&state.work[0], kind < 2 ? kind + 1 : 3);
            }
            if (level < 0) assert(count == 0);
            else if (level <= 29) assert(count == expected[kind][level / 6]);
            else assert(count == expected[kind][4]);
            assert(count >= previous && count <= 100);
            previous = count;
        }
    }
    puts("Reaction conditions: original probabilities preserved; levels above 29 are capped");
    return 0;
}
#else
#include "battle/btl_ai_mgr.h"
#include "battle/btl_ai_seq_a.h"
/* Isolate the live controller setter; kit-specific rate construction is not
 * needed to verify rebasing, resets, sentinels or the actual delay/scale code. */
s32 AiThink_FindBasicColumn(s32 button, s32 arg) { (void)button; (void)arg; return -1; }
s32 BtlAi_GetPairRate(BtlAiWork *s, u32 k, s32 off, s8 *lo, s8 *hi, s32 gauge) {
    (void)s; (void)k; (void)off; (void)lo; (void)hi; (void)gauge; return 0;
}
void AiThink_BuildTotals(BtlAiWork *s) { (void)s; }
void BtlAiSeq_Reset(BtlAiSeq *seq) { seq->depth = 0; }

int main(void) {
    static BtlAiData data;
    static const int duel[5] = {0, 6, 13, 21, 29};
    static const int duelEffective[5] = {0, 6, 13, 21, 29};
    int mode, level, side, lo, hi, value, old, r;
    state.data = &data;
    for (mode = 0; mode < 4; mode++) {
        active = mode != 0;
        session = mode == 2;
        online = mode == 3;
        for (level = 0; level < 3; level++) {
            const int original[3] = {0, 6, 9};
            const int zenkai[3] = {-5, 0, 5};
            int offset = BtlAiLevel_StoryOffset(level);
            assert(offset == (mode == 1 ? zenkai[level] : original[level]));
            if (mode == 1) {
                int adjusted = 13 + offset;
                assert(BtlAiLevel_FromBase(adjusted) == (adjusted > 19 ? 29 : adjusted + 10));
                assert(BtlAiLevel_FromBase(BtlAiLevel_StoryBase(29, level)) == 29);
                assert(BtlAiLevel_FromBase(BtlAiLevel_StoryBase(0, level)) == 10 + offset);
                assert(BtlAiLevel_FromBase(BtlAiLevel_StoryBase(5, 0)) == 10);
                for (old = 0; old < 5; old++) {
                    int base = BtlAiLevel_StoryBase(old, 0);
                    BtlAiMgr_SetLevel(1, base);
                    assert(BtlAiMgr_GetLevel(1) == old + 5);
                    assert(BtlAiLevel_FromBase(BtlAiLevel_ToBase(old + 5)) == old + 5);
                }
            }
        }
        assert(BtlAiLevel_StoryOffset(-1) == 0);
        assert(BtlAiLevel_StoryOffset(3) == 0);
        for (side = 0; side < 2; side++) {
            for (level = -999; level <= 29; level++) {
                int expected = level < 0 || mode != 1 ? level : (level > 19 ? 29 : level + 10);
                BtlAiMgr_SetLevel(side, level);
                assert(BtlAiMgr_GetLevel(side) == expected);
                if (level <= 19) assert(BtlAiLevel_ToBase(expected) == level);
                /* A new member/reset reads the unchanged base, never +40. */
                BtlAiMgr_SetLevel(side, level);
                assert(BtlAiMgr_GetLevel(side) == expected);
            }
        }
    }
    active = 1; session = online = 0;
    {
        for (mode=0;mode<4;mode++) {
            active=mode!=0;session=mode==2;online=mode==3;
            for (level=0;level<5;level++) {
                int base=BtlAiLevel_DuelBase(duel[level]);
                int want=duelEffective[level];
                BtlAiMgr_SetLevel(1,base);
                assert(BtlAiMgr_GetLevel(1)==want);
                assert(BtlAiLevel_DuelSettingBase(want)==duel[level]);
                BtlAiMgr_SetLevel(1,base);assert(BtlAiMgr_GetLevel(1)==want);
            }
            assert(BtlAiLevel_DuelBase(-1)==-1);
            assert(BtlAiLevel_DuelSettingBase(-1)==-1);
        }
        active=1;session=online=0;
    }
    assert(BtlAiLevel_FromBase(100) == 29);
    for (lo = 0; lo <= 100; lo += 5) {
        for (hi = 0; hi <= 100; hi += 5) {
            old = hi;
            for (level = 0; level <= 100; level++) {
                value = BtlAi_ScaleByLevel(level, lo, hi);
                assert(value >= 0 && value <= 100);
                if (level <= 29) {
                    int original = lo + (int)((float)(hi-lo) / 29.0f * level + 0.01f);
                    assert(value == original);
                } else {
                    assert(value == BtlAi_ScaleByLevel(29, lo, hi));
                }
                old = value;
            }
        }
    }
    assert(BtlAi_ScaleByLevel(100, 20, 0) == BtlAi_ScaleByLevel(29, 20, 0));
    assert(BtlAi_ScaleByLevel(100, -1, -1) == BtlAi_ScaleByLevel(29, -1, -1));
    for (level = -3; level <= 100; level++) {
        state.work[0].level = level;
        assert(BtlAiLevel_Index(level) >= 0 && BtlAiLevel_Index(level) < 5);
        assert(BtlAiLevel_Index(level) >= 0 && BtlAiLevel_Index(level) < 5);
        for (r = 0; r < 100; r++) {
            roll = r;
            BtlAiPick_LevelDelay(&state.work[0], 1, 1);
            assert(state.work[0].seq.timer >= 0);
            BtlAiPick_LevelDelay(&state.work[0], 1, 2);
            assert(state.work[0].seq.timer >= 0);
            if (level == 29) assert(state.work[0].seq.timer == 10);
            if (level >= 29) assert(state.work[0].seq.timer == 10);
        }
    }
    puts("Controller: +10 with cap 29, original Duel levels, offsets, sentinels, scaling and delays verified");
    return 0;
}
#endif
#endif
