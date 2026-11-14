#ifndef MENU_PORT_MENU_ART_POLICY_H
#define MENU_PORT_MENU_ART_POLICY_H

/* Options shares mode 62 across supported pages and the original screen
 * calibration preview. Keep calibration and its reset prompt on the fallback. */
static inline int Port_MenuArtOptionsStateCovered(int state) {
    return state != 9 && state != 11;
}

/* Add a mode only after its complete foreground has proportion rules.
   Shared headings/dialogs alone do not make a screen covered. */
static inline int Port_MenuArtModeCovered(int mode) {
    switch (mode) {
    case 1: /* title */
    case 4: /* main menu */
    case 6: case 7: case 8: case 9: /* Dragon History selection, outcomes and saga outro */
    case 10: /* Dragon History save: blank page; McFlow dialogs already preserve proportions */
    case 13: case 14: case 15: case 16: /* Ultimate Battle / Mission 100 */
    case 17: case 18: case 19: /* Survival */
    case 20: case 21: case 22: case 23: /* Sim Dragon */
    case 24: case 25: case 26: case 27: case 28: case 29: case 30: /* Disc Fusion, selectors, ladder/courses and results */
    case 33: case 34: case 35: /* Dragon World Tour */
    case 38: case 39: case 40: /* Versus Duel, single, team and DP character/stage selection */
    case 44: /* Ultimate Training menus and lesson explanations */
    case 45: /* Training uses the shared character/stage selector (scopes 10/11/12). */
    case 48: case 49: case 50: /* Evolution Z main, character customization and Item Shop */
    case 53: case 54: case 55: case 56: /* Data Center, password, character list and replays */
    case 60: /* Character Reference list, details and model viewer */
    case 62: /* Options and screen/sound/controller pages */
    case 70: /* Shenron: wide 3D scene, centered proportional wish UI */
    case 71: /* Shenron save: shared memory-card dialogs */
    case 80: /* Dragon Net Battle */
        return 1;
    default:
        return 0;
    }
}

#endif
