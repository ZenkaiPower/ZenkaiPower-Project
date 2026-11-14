#include "common.h"
#ifdef PORT
#include "menu/port_story_endings.h"

extern int Port_NetSession(void);
extern int Port_NetActive(void);

/* Survives BtlScript_Free's teardown; consumed by the episode result screen.
 * These game-source globals are included in the port's simulation snapshots. */
static int sStory = -1, sAction = -1, sResult = -1;

static int enabled(void) {
    return Port_ZenkaiEnabled() && !Port_NetSession() && !Port_NetActive();
}

void Port_StoryEndingsReset(void) {
    sStory = sAction = sResult = -1;
}

void Port_StoryEndingsStage(int story, int action, int result) {
    Port_StoryEndingsReset();
    if (!enabled() || story < 0 || action < 10000 || action >= 10052 || (result != 1 && result != 3)) return;
    sStory = story;
    sAction = action;
    sResult = result;
}

int Port_StoryEndingsCommit(void *save, int saga, int episode, int story, int flags, int reason) {
    int action = sAction, result = sResult, expectedReason;
    int matches = story == sStory;
    Port_StoryEndingsReset();
    if (!enabled() || !matches || action < 10000 || (result != 1 && result != 3)) return 0;
    expectedReason = result < 2 ? 1 : 4;
    /* Aborts, restarts, draws and unrelated/overridden results are not endings. */
    if ((flags & 0x1F) != 1 || (reason & 7) != expectedReason || (reason & 0x18000)) return 0;
    return Port_StoryRecordEnding(save, saga, episode, action);
}
#endif
