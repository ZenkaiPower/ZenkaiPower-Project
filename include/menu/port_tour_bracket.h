#ifndef MENU_PORT_TOUR_BRACKET_H
#define MENU_PORT_TOUR_BRACKET_H
#include "menu/port_tour_rules.h"

/* A bounded scan replaces rejection sampling, including when cost preferences
 * cannot be met. Never admit a character outside the progression pool. */
static int PortTour_Pick(const u8 *allowed, const LTourEntrant *entries, int count,
                         int boss, int level, int *costume) {
    int chara, slot, slots, i, copies, identities, families, score, tier, cost;
    int best = 0x7FFFFFFF, choices = 0, picked = -1;
    for (chara = 0; chara < SAVE_CHARA_COUNT; chara++) {
        if (!allowed[chara] || chara == boss) continue;
        copies = identities = families = 0;
        for (i = 0; i < count; i++) {
            int other = entries[i].chara;
            if (other < 0 || other >= SAVE_CHARA_COUNT) continue;
            copies += other == chara;
            identities += sPortTourCharacters[other].identity == sPortTourCharacters[chara].identity;
            families += sPortTourCharacters[other].family == sPortTourCharacters[chara].family;
        }
        if (boss >= 0 && boss < SAVE_CHARA_COUNT) {
            identities += sPortTourCharacters[boss].identity == sPortTourCharacters[chara].identity;
            families += sPortTourCharacters[boss].family == sPortTourCharacters[chara].family;
        }
        if (!PortTour_IsIndividual(chara) && copies) continue;
        slots = 1;
        if (chara == 79 || chara == 109) slots = 5;
        if (chara == 98 || chara == 99) slots = ChrTbl_WrapCostume(chara, NULL);
        cost = ChrTbl_GetCost(chara);
        tier = !identities ? 0 : !families ? 1 : 2;
        score = tier * 10000 + identities * 100 + families * 10;
        score += level == 0 ? cost >= 4 : level == 1 ? cost >= 8 : cost < 7;
        for (slot = 0; slot < slots; slot++) {
            if ((chara == 79 || chara == 109) && slot < copies) continue;
            if (chara == 98 || chara == 99) {
                for (i = 0; i < count; i++)
                    if (entries[i].chara == chara && entries[i].costume == slot) break;
                if (i != count) continue;
            }
            /* Each unused individual is an independent first-priority chance. */
            if (PortTour_IsIndividual(chara))
                score = level == 0 ? cost >= 4 : level == 1 ? cost >= 8 : cost < 7;
            if (score < best) { best = score; choices = 0; }
            if (score == best && Rand_Range(++choices) == 0) {
                picked = chara;
                *costume = (chara == 98 || chara == 99) ? slot : 0;
            }
        }
    }
    return picked;
}

static int PortTour_Boss(const u8 *allowed, int tour) {
    static const s16 candidates[5][2] = {{66,56},{55,-1},{107,108},{61,6},{26,-1}};
    int i, boss = -1, choices = 0;
    if (tour < 0 || tour >= 5) return -1;
    for (i = 0; i < 2; i++) {
        int chara = candidates[tour][i];
        if (chara >= 0 && allowed[chara] && Rand_Range(++choices) == 0) boss = chara;
    }
    return boss;
}

static int PortTour_FillEntrants(LBracket *b) {
    u8 allowed[SAVE_CHARA_COUNT];
    LTourEntrant entries[LTOUR_ENTRANT_MAX];
    int boss, i, costume, chara;
    PortTour_Eligible(gSaveData, allowed);
    boss = PortTour_Boss(allowed, LTOUR_PROG->tour);
    if (boss < 0) return 0;
    memset(entries, 0, sizeof(entries));
    for (i = 0; i < 16; i++) {
        if (i < LTOUR_PROG->entryNum) entries[i] = LTOUR_PROG->entrant[i];
        else {
            chara = PortTour_Pick(allowed, entries, i, boss, LTOUR_PROG->level, &costume);
            if (chara < 0) return 0;
            entries[i].chara = chara;
            entries[i].costume = costume;
        }
    }
    entries[16].chara = boss;
    entries[16].pos = 30;
    entries[16].flags = LTOUR_ENT_BOSS;
    memcpy(b->entrant, entries, sizeof(entries));
    return 1;
}
#endif
