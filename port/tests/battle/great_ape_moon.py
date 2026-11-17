"""Exercise the production Great Ape handler with and without its Zenkai flag."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
text=(ROOT/'src/battle/btl_act_h_b.c').read_text()
a=text.index('s32 BtlAct_TransformC(')
b=text.index('\n/*',a)
fixture=r'''
#include "common.h"
#include "battle/btl_act_h.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BtlActHPose pose;
static int enabled,moon,binds,donor,anim,loaded,requested,ready,done,spent,ballVoice,finished;
int Port_ZenkaiPackEnabled(void) {return enabled;}
s32 BtlStage_HasMoon(void) {return moon;}
int Port_ZenkaiGreatApeMoonBind(void *obj,int giant) {binds++;return donor;}
BtlActHObj *BtlChar_GetObj(BtlActHChr *c) {return 0;}
void BtlAnim_Play(BtlActHChr *c,s32 id,f32 blend) {anim=id;}
BtlActHPose *BtlChar_GetPos(BtlActHChr *c) {return &pose;}
void BtlMove_SetHeading(BtlActHChr *c,f32 a,f32 b) {}
void BtlChar_SavePlacement(BtlActHChr *c) {}
void BtlChar_SetFlag(BtlActHChr *c,s32 id) {if(id==0x132) ballVoice++;}
void BtlChar_ResetLook(BtlActHChr *c) {}
void BtlEvent_Raise(s32 a,s32 b) {}
void BtlCharSnd_PlayCommon(BtlActHChr *c,s32 id) {}
void BtlChar_SetFxBit(BtlActHChr *c,s32 id) {}
void BtlChange_RequestChara(s32 a,s32 b,s32 d,s32 e,s32 f,s32 g,s32 h) {requested++;}
s32 BtlAnim_GetId(BtlActHChr *c) {return anim;}
s32 BtlAnim_Advance(BtlActHChr *c,s32 flags) {return 1;}
s32 BtlChange_IsLoadedFor(s32 side) {return loaded;}
void BtlAnim_Request(BtlActHChr *c,s32 id,f32 blend) {anim=id;}
void BtlMember_SubBlast(BtlActHChr *c,s32 cost) {spent++;}
void BtlChange_SetReady(s32 side) {ready++;}
s32 BtlAnim_IsNew(BtlActHChr *c) {return 1;}
void BtlChange_SetDone(s32 side) {done++;}
void BtlChar_PlayVoice(BtlActHChr *c,s32 id) {}
void BtlChar_Vibrate(BtlActHChr *c,f32 a,f32 b) {}
void BtlCharSnd_PlayStream(BtlActHChr *c,s32 id) {}
void BtlMove_Step(BtlActHChr *c,s32 a,s32 b,s32 d,f32 e,f32 f) {}
void BtlMove_ApplyGravity(BtlActHChr *c) {}
void BtlActChange_SetFlags(BtlActHChr *c) {}
void BtlActChange_Finish(BtlActHChr *c) {finished++;}
s32 BtlChar_TestFlag(BtlActHChr *c,s32 id) {return 0;}
void BtlChar_AddStageTimer(f32 time) {}
'''
checks=r'''
int main(void) {
    int kind;
    for(enabled=0;enabled<2;enabled++) for(moon=0;moon<2;moon++) for(kind=1;kind<=4;kind++) for(donor=0;donor<2;donor++) {
        BtlActHChr chr={0};
        int natural=PORT_ZENKAI_GREAT_APE_MOON && enabled && moon && kind==4;
        chr.form.kind=kind;chr.form.cost=200000;
        binds=requested=ready=done=spent=ballVoice=finished=loaded=0;
        BtlAct_TransformC(&chr,0);assert(binds==natural && anim==0x177);
        assert(chr.form.kind==(natural && donor ? 3:kind));
        chr.actionFrame=1;
        BtlAct_TransformC(&chr,1);
        assert(requested==1 && !ready && !spent && anim==0x177);
        assert(ballVoice==(kind==4 && !(natural && donor)));
        chr.actionFrame=2;loaded=1;
        BtlAct_TransformC(&chr,1);assert(ready==1 && spent==1 && anim==0x179);
        BtlAct_TransformC(&chr,1);assert(done==1 && chr.work[1]==1);
        BtlAct_TransformC(&chr,2);assert(finished==1);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory)
    (tmp/'test.c').write_text(fixture+text[a:b]+checks)
    for flag in (0,1):
        subprocess.run(['gcc','-std=gnu99','-fno-builtin','-fsanitize=undefined','-Iinclude','-Iport/include',
            '-include','port_compat.h',f'-DPORT_ZENKAI_GREAT_APE_MOON={flag}',str(tmp/'test.c'),'-o',str(tmp/'test')],cwd=ROOT,check=True)
        subprocess.run([str(tmp/'test')],check=True)
print('Great Ape moon checks passed: mode/flag/moon/kind gates, pending load, gauge and completion.')
