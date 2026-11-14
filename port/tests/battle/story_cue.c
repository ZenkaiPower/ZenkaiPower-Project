/* Link btl_script.c with function sections and --gc-sections. */
#include "battle/btl_script.h"
#include <assert.h>
#include <stdio.h>
extern void Port_BtlScriptOfferNextCue(void);
BtlScriptWork gBtlScript;
static int mode=1, enabled=1;
int Battle_GetMode(void) {return mode;}
int Port_StoryNextCueActive(void) {return enabled;}
int main(void) {
 BtlScriptRequest *requests=gBtlScript.talk.requests;
 BtlScriptEvent *event=&gBtlScript.events[0];
 gBtlScript.talk.requestCount=2;
 requests[0].kind=BTL_SCRIPT_REQ_LINE;
 requests[1].kind=BTL_SCRIPT_REQ_EVENT;requests[1].trigger=0x16;
 requests[1].u.start.event=event;event->kind=1;
 enabled=0;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 enabled=1;mode=0;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 mode=1;event->side=1;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 event->side=0;event->group=3;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 event->group=0;requests[1].trigger=BTL_SCRIPT_TRIGGER_WAIT_OFF;
 Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 requests[1].trigger=0x22;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 requests[1].trigger=0x49;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 requests[1].trigger=0x16;event->kind=0;Port_BtlScriptOfferNextCue();assert(!gBtlScript.pending);
 event->kind=1;Port_BtlScriptOfferNextCue();assert(gBtlScript.pending==&requests[1]);
 gBtlScript.pending=&requests[0];Port_BtlScriptOfferNextCue();assert(gBtlScript.pending==&requests[0]);
 puts("Story cue checks passed: mode/toggle gates, event filtering, next prompt, and pending-event priority.");
 return 0;
}
