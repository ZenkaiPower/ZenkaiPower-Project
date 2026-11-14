#include "common.h"
#include "sys/gsc.h"
#include "menu/port_story_endings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int active=1, session, online, mode=1, story, result;
static GscTask task;
static unsigned char save[0x4000], before[0x4000];
int Port_ZenkaiEnabled(void) { return active; }
int Port_NetSession(void) { return session; }
int Port_NetActive(void) { return online; }
s32 Battle_GetMode(void) { return mode; }
s32 Battle_GetScript(void) { return story; }
s32 Gsc_GetInt(void) { return result; }
GscTask *Gsc_GetCurrentTask(void) { return &task; }
void BtlFacade_SetResult(s32 value) { assert(value==result); }
s32 BtlFacade_IsNotFinished(void) { return 1; }
extern s32 BtlScriptCmd_SetResult(u32 phase, void *work);

static void begin(int action, int value, int phase) {
    int work[8]={0};
    task.action=action;result=value;
    BtlScriptCmd_SetResult(phase,work);
}
static int commit(int flags, int reason) {
    return Port_StoryEndingsCommit(save,0,0,story,flags,reason);
}

int main(void) {
    int i, action;
    memset(save,0xa5,sizeof(save));
    memcpy(before,save,sizeof(save));
    /* Migration reads are non-mutating and infer nothing from old clear bits. */
    assert(Port_StoryEndingCount(save,0,0)==0 && !memcmp(save,before,sizeof(save)));
    Port_StoryEndingsReset();
    begin(10000,-1,GSC_PHASE_BEGIN);assert(!commit(1,1));
    begin(10030,1,GSC_PHASE_BEGIN);assert(commit(1,1));
    assert(Port_StoryHasEnding(save,0,0,10030));
    assert(!commit(1,1));
    begin(10032,1,GSC_PHASE_BEGIN);assert(commit(1,1));
    begin(10040,0,GSC_PHASE_BEGIN);assert(!commit(2,1) && !Port_StoryHasEnding(save,0,0,10040));
    begin(10050,3,GSC_PHASE_BEGIN);assert(commit(1,4));
    begin(10051,2,GSC_PHASE_BEGIN);assert(!commit(2,4) && !Port_StoryHasEnding(save,0,0,10051));
    assert(Port_StoryEndingCount(save,0,0)==3);
    begin(10030,1,GSC_PHASE_BEGIN);assert(!commit(1,1));
    /* Restart/abort, task abort, mismatched outcome and unrelated episode. */
    begin(10031,1,GSC_PHASE_BEGIN);Port_StoryEndingsReset();assert(!commit(1,1));
    begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(8,1));
    begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(1,0x8001));
    begin(10031,1,GSC_PHASE_BEGIN_ABORT);assert(!commit(1,1));
    task.abort=1;begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(1,1));task.abort=0;
    begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(2,1));
    begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(1,4));
    begin(10031,1,GSC_PHASE_BEGIN);assert(!Port_StoryEndingsCommit(save,0,0,story+1,1,1));
    for(i=0;i<4;i++) {
        active=i!=0;session=i==1;online=i==2;mode=i==3?0:1;
        Port_StoryEndingsReset();begin(10031,1,GSC_PHASE_BEGIN);assert(!commit(1,1));
    }
    active=1;session=online=0;mode=1;
    memcpy(before,save,sizeof(save));
    assert(!Port_StoryRecordEnding(save,-1,0,10030));
    assert(!Port_StoryRecordEnding(save,9,0,10030));
    assert(!Port_StoryRecordEnding(save,0,16,10030));
    assert(!Port_StoryRecordEnding(save,0,0,9999));
    assert(!Port_StoryRecordEnding(save,0,0,10052));
    assert(!Port_StoryRecordEnding(NULL,0,0,10030));
    assert(!memcmp(before,save,sizeof(save)));
    /* Every bit, both episode boundaries, and no writes into adjacent sections. */
    for(action=10000;action<10052;action++) {
        Port_StoryRecordEnding(save,8,15,action);
        assert(Port_StoryHasEnding(save,8,15,action));
    }
    assert(Port_StoryEndingCount(save,8,15)==52 && Port_StoryEndingCount(save,8,14)==0);
    assert(!memcmp(before,save,PORT_STORY_ENDINGS_OFFSET));
    assert(!memcmp(before+PORT_STORY_ENDINGS_OFFSET+PORT_STORY_ENDINGS_BYTES,
                   save+PORT_STORY_ENDINGS_OFFSET+PORT_STORY_ENDINGS_BYTES,
                   sizeof(save)-PORT_STORY_ENDINGS_OFFSET-PORT_STORY_ENDINGS_BYTES));
    puts("Story endings: real result handler capture, victory/ring-out wins only, defeat rejection, duplicate accumulation, abort/restart/mode isolation and bounds passed.");
    return 0;
}
