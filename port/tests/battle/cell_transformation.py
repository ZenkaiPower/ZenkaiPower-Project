"""Check the production transformation handler without a game/Android build."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/battle/btl_act_h_b.c').read_text(encoding='utf-8')
start = source.index('s32 BtlAct_TransformA(')
handler = source[start:source.index('\n/*', start)]

fixture = r'''
#include "common.h"
#include "battle/btl_act_h.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static BtlActHPose pose;
static BtlActHMember member;
static int active, classic, online, missing, anim, ended, loaded, newFrame;
static int requested, ready, done, spent, finished, sub, loops, flashes, cleared;
static int requestCharacter[4], requestVariant[4], modelOnly[4];
static int propRequested, propPlayed, propSteps, propVisible, propActive, propReleased;
static int cinematic, starts, camStops, placements, frontCamera, trimmed, transformStarts, bodyHidden, glowCount;
static float entryBlend, dialogueBlend;
static int arrivals, teleportSounds, arrivalHidden;
static int detransformVoices;
void DemoCam_ClearFixed(void) { frontCamera = 0; }
void Port_ZenkaiCellTransformFrontCamera(void *chr) { frontCamera = 1; }
void Port_ZenkaiCellTransformKeepPose(void *chr, int restore) { assert(!restore && anim == 1); }
void Port_ZenkaiCellTransformDialogue(void *chr) { trimmed++; }
void Port_ZenkaiCellTransformReveal(void *chr) { bodyHidden = 0; }
void Port_ZenkaiCellTransformGlow(int objectId) { glowCount++; }
void Port_ZenkaiCellTransformPresent(void *obj, int on) { cinematic = on; }
void EftTransform_End(void) {}
void EftCam_Stop(void) { camStops++; }
void BtlSuper_SetRushFlags(BtlActHChr *chr) {}
s32 BtlSuper_GetUnk223(BtlActHChr *chr, s32 slot) { assert(slot == 4); return 999; }
f32 BtlAnim_GetFrame(BtlActHChr *chr) { return ended ? 100.0f : 2.0f; }
void BtlChar_RequestPlaceRelative(BtlActHChr *chr, s32 slot, s32 step, s32 self) {
    assert(slot == 4 && step > 0 && step < 3 && self == 1); placements++;
}
void Port_ZenkaiCellTransformEnd(void *obj) { cleared++; cinematic = 0; }
int Port_ZenkaiCellTransformOpeningFrames(void) { return 45; }
int Port_ZenkaiCellTransformPartner(int costume) { return costume == 1 ? 282 : 281; }
void Port_ZenkaiCellTransformPartnerVisible(void *chr, int on) { propVisible = on; }
void BtlChange_RequestObject(s32 player, s32 id, s32 costume, s32 variant, s32 slot) {
    assert(id == 281 && slot == 1); propRequested++;
}
void BtlPartner_PlayAnim(BtlActHChr *chr, s32 animation) { assert(animation == 0x19E && propActive); propPlayed++; }
s32 BtlPartner_StepAnim(BtlActHChr *chr) { assert(propActive); propSteps++; return 0; }
void BtlPartner_Release(BtlActHChr *chr) { if (propActive) { propReleased++; propActive = 0; } }
int Port_ZenkaiCellTransformBind(void *obj, int character, int target) {
    return PORT_ZENKAI_CELL_TRANSFORMATION && active && !classic && !online &&
           !missing && character == 107 && target == 108;
}
BtlActHMember *BtlMember_GetActive(BtlActHChr *c) { return &member; }
BtlActHObj *BtlChar_GetObj(BtlActHChr *c) { return 0; }
BtlActHPose *BtlChar_GetPos(BtlActHChr *c) { return &pose; }
void BtlAnim_Play(BtlActHChr *c, s32 id, f32 blend) {
    anim = id;
    if (id == 1) entryBlend = blend;
    if (id == 0x161) dialogueBlend = blend;
}
void BtlAnim_PlaySub(BtlActHChr *c, s32 id) { sub++; }
void BtlAnim_RequestSubToMain(BtlActHChr *c) { anim = 0x178; }
void BtlAnim_Request(BtlActHChr *c, s32 id, f32 blend) { anim = id; }
s32 BtlAnim_GetId(BtlActHChr *c) { return anim; }
s32 BtlAnim_Advance(BtlActHChr *c, s32 flags) { return ended; }
void BtlAnim_AdvanceLoop(BtlActHChr *c, s32 flags) { loops++; }
s32 BtlAnim_IsNew(BtlActHChr *c) { return newFrame; }
s32 BtlAnim_WasNew(BtlActHChr *c) { return 0; }
void BtlMove_SetHeading(BtlActHChr *c, f32 a, f32 b) {}
void BtlChar_SavePlacement(BtlActHChr *c) {}
void BtlChar_SetFlag(BtlActHChr *c, s32 id) { if (id == 0xB) arrivalHidden++; }
void BtlChar_SetHeldFlag(BtlActHChr *c, s32 id) { if (id == 0x30) bodyHidden = 1; }
void BtlChar_ResetLook(BtlActHChr *c) {}
void BtlEvent_Raise(s32 side, s32 event) {}
void BtlCharSnd_PlayCommon(BtlActHChr *c, s32 id) { if (id == 0x20) teleportSounds++; }
void BtlChar_PlayVoice(BtlActHChr *c, s32 id) { if (id == 0xE) detransformVoices++; }
void BtlChar_SetFxBit(BtlActHChr *c, s32 id) { if (id == 1) flashes++; if (id == 0x40) starts++; if (id == 0) transformStarts++; if (id == 0xD) arrivals++; }
void BtlChar_Vibrate(BtlActHChr *c, f32 a, f32 b) {}
void BtlChange_RequestChara(s32 a, s32 b, s32 d, s32 e, s32 f, s32 g, s32 h) {
    requestCharacter[requested] = b;
    requestVariant[requested] = e;
    modelOnly[requested++] = f == -1 && g == -1 && h == -1;
}
s32 BtlChange_IsLoadedFor(s32 side) { return loaded; }
void BtlChange_SetReady(s32 side) { ready++; }
void BtlChange_SetDone(s32 side) { done++; }
void BtlMember_SubBlast(BtlActHChr *c, s32 cost) { assert(cost == 200000); spent++; }
void ChrCam_RequestCut(BtlActHChr *c, s32 a, s32 b) {}
s32 ChrCam_IsCutActive(BtlActHChr *c) { return 0; }
void ChrCam_AddShake(BtlActHChr *c, f32 a, f32 b) {}
u32 BtlParam_GetFlags(BtlActHChr *c) { return 0; }
void BtlMove_Step(BtlActHChr *c, s32 a, s32 b, s32 d, f32 e, f32 f) {}
void BtlMove_BrakeVertical(BtlActHChr *c) {}
void BtlActChange_SetFlags(BtlActHChr *c) {}
void BtlActChange_PushBack(BtlActHChr *c) {}
void BtlActChange_Finish(BtlActHChr *c) { finished++; }
void BtlChar_AddStageTimer(f32 time) {}
'''
checks = r'''
int main(void) {
    int test;
    for (test = 0; test < 9; test++) {
        BtlActHChr c = {0};
        int custom;
        active = test != 0; classic = test == 1; online = test == 2; missing = test == 3;
        member.chara = test == 4 ? 106 : 107;
        c.form.chara = test == 5 ? 107 : 108;
        c.form.cost = 200000; c.form.variant = 1;
        member.variant = test >= 6 ? test - 6 : 0;
        if (test >= 6) c.form.variant = 0; /* Final form's clean outfit request. */
        custom = test >= 6 && PORT_ZENKAI_CELL_TRANSFORMATION;
        requested = ready = done = spent = finished = sub = loops = flashes = cleared = 0;
        propRequested = propPlayed = propSteps = propVisible = propActive = propReleased = 0;
        cinematic = starts = camStops = placements = frontCamera = trimmed = transformStarts = bodyHidden = glowCount = 0;
        arrivals = teleportSounds = arrivalHidden = 0;
        detransformVoices = 0;
        loaded = ended = newFrame = 0;
        BtlAct_TransformA(&c, 0);
        assert(c.work[18] == custom && sub == !custom && anim == (custom ? 1 : 0x177));
        if (custom) assert(entryBlend == 0.25f);
        c.actionFrame = 1;
        BtlAct_TransformA(&c, 1);
        if (custom) {
            assert(anim == 1 && c.work[19] == 11 && loops == 1);
            assert(!requested && !propRequested && !transformStarts && !flashes);
            for (int frame = 2; frame < 30; frame++) {
                c.actionFrame = frame;
                BtlAct_TransformA(&c, 1);
                assert(anim == 1 && !requested && !transformStarts);
            }
            c.actionFrame = 30;
            BtlAct_TransformA(&c, 1);
            assert(loops == 31 && transformStarts == 1 && c.work[19] == 0);
        }
        assert(requested == 1 && !ready && !spent && propRequested == custom);
        assert(detransformVoices == !custom);
        c.actionFrame = 2;
        if (custom) {
            assert(requestCharacter[0] == 106 && modelOnly[0]);
            assert(requestVariant[0] == member.variant);
            BtlAct_TransformA(&c, 1);
            assert(anim == 1 && !ready && !spent);
            loaded = 1;
            BtlAct_TransformA(&c, 1);
            assert(ready == 1 && c.work[19] == 1);
            loaded = 0;
            BtlAct_TransformA(&c, 1);
            assert(done == 1 && c.work[19] == 2);
            BtlAct_TransformA(&c, 1);
            assert(ready == 1 && !cinematic);
            loaded = 1;
            BtlAct_TransformA(&c, 1);
            assert(ready == 2 && c.work[19] == 3);
            loaded = 0; propActive = 1;
            BtlAct_TransformA(&c, 1);
            assert(c.work[19] == 6 && requested == 2 && done == 2 && !cinematic);
            assert(requestCharacter[1] == 106 && !modelOnly[1]);
            assert(requestVariant[1] == member.variant);
            BtlAct_TransformA(&c, 1);
            assert(ready == 2 && !cinematic);
            loaded = 1;
            BtlAct_TransformA(&c, 1);
            assert(ready == 3 && c.work[19] == 7 && !cinematic);
            loaded = 0;
            BtlAct_TransformA(&c, 1);
            assert(c.work[19] == 10 && done == 3 && !cinematic && anim == 1);
            for (int frame = 0; frame < 29; frame++) BtlAct_TransformA(&c, 1);
            assert(!cinematic && ready == 3 && done == 3 && frontCamera);
            BtlAct_TransformA(&c, 1);
            assert(anim == 0x161 && done == 3 && cinematic && starts == 1 && trimmed == 1 && !frontCamera);
            assert(dialogueBlend == 0.2f);
            assert(c.stepFrame[0] == 45.0f); /* Skip punch, retain camera timeline offset. */
            ended = 0; newFrame = 1;
            BtlAct_TransformA(&c, 1);
            assert(placements == 1 && requested == 2 && !propPlayed);
            ended = 1; newFrame = 0;
            BtlAct_TransformA(&c, 1);
            assert(anim == 0x162 && c.stepFrame[1] == 100.0f);
            ended = 0; newFrame = 1;
            BtlAct_TransformA(&c, 1);
            assert(placements == 2 && propVisible && propPlayed == 1 && propSteps == 1);
            assert(requested == 2 && !spent); /* Final load must not free active Ultimate effects. */
            ended = 1; newFrame = 0;
            BtlAct_TransformA(&c, 1);
            assert(!cinematic && camStops == 1 && requested == 3 && !spent && bodyHidden && transformStarts == 2);
            assert(requestCharacter[2] == 108 && !modelOnly[2] && c.form.variant == 0);
            assert(requestVariant[2] == 0);
            assert(!propActive && !propVisible && propReleased == 1);
            BtlAct_TransformA(&c, 1);
            assert(requested == 3 && !spent && anim == 0x162);
            loaded = 1;
            BtlAct_TransformA(&c, 1);
            assert(ready == 4 && spent == 1 && anim == 0 && cleared == 1 && !c.work[1]);
        } else {
            ended = 1;
            BtlAct_TransformA(&c, 1);
            assert(anim == 0x178);
            loaded = 1;
            BtlAct_TransformA(&c, 1);
            assert(ready == 1 && spent == 1 && anim == 0x179);
        }
        ended = 0; newFrame = 1;
        BtlAct_TransformA(&c, 1);
        assert(done == (custom ? 4 : 1) && !c.work[1]);
        if (custom) {
            assert(frontCamera && c.work[19] == 9 && !bodyHidden && flashes == 1 && glowCount == 1);
            assert(arrivals == 1 && teleportSounds == 1 && arrivalHidden == 1);
            for (int frame = 0; frame < 18; frame++) BtlAct_TransformA(&c, 1);
            assert(!c.work[1] && frontCamera);
            BtlAct_TransformA(&c, 1);
            assert(c.work[1] && !frontCamera && anim == 0);
            assert(arrivals == 1 && teleportSounds == 1 && arrivalHidden == 3);
        }
        if (!custom) {
            ended = 1; newFrame = 0;
            BtlAct_TransformA(&c, 1);
        }
        assert(c.work[1] && spent == 1);
        BtlAct_TransformA(&c, 2);
        assert(finished == 1);
    }
    return 0;
}
'''

manager = (ROOT / 'src/battle/btl_char_mgr.c').read_text(encoding='utf-8')
begin = manager.index('    case 0xEC:', manager.index('void BtlChar_OnModelLoaded('))
transform_case = manager[begin:manager.index('    case 0xF3:', begin)]
begin = manager.index('void BtlChar_BindObject(')
bind_object = manager[begin:manager.index('\n}', begin) + 2]
stats_fixture = r'''
#include "common.h"
#include "battle/btl_char_mgr.h"
#include "sys/port_zenkai.h"
#include <assert.h>
#include <string.h>
static BtlMgrMember member;
static BtlMgrObj object;
static int temporary, loads, statResets, unusedObjectWord;
#define FILE_TABLE(file, word) ((u32 *)(file) + ((u32 *)(file))[word] / 4)
#define OBJ_WORD_18028(obj) unusedObjectWord
int Port_ZenkaiCellTransformTemporary(void *object) { return temporary; }
s32 BattleSide_GetObjId(s32 side) { return 0; }
BtlMgrObj *BtlChar_GetObj(BtlMgrChr *c) { return &object; }
void BtlChar_ClearFlag(BtlMgrChr *c, s32 flag) {}
void BtlChar_ClearFlagRange(BtlMgrChr *c, s32 a, s32 b) {}
void BtlObj_InitChains(BtlMgrObj *o) {}
void BtlStat_Reset(BtlMgrChr *c) { statResets++; }
void Port_SpiritBombRestore(void *fighter) {}
BtlMgrMember *BtlMember_GetActive(BtlMgrChr *c) { return &member; }
s32 BtlMember_GetActiveIndex(BtlMgrChr *c) { return 0; }
f32 BtlMember_GetHealthRatio(BtlMgrChr *c) { return (float)member.gauge.health / member.gauge.healthMax; }
void BtlMember_LoadParams(BtlMgrChr *c, s32 i, s32 init, f32 health, s32 variant) {
    loads++; member.gauge.healthMax = 40000; member.gauge.kiMax = 50000; member.gauge.blastMax = 300000;
}
s32 BtlParam_GetUnkAD(BtlMgrChr *c) { return 7; }
s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi) { return v < lo ? lo : v > hi ? hi : v; }
''' + bind_object + r'''
static void apply(BtlMgrChr *chr) {
    BtlMgrMember *m;
    BtlMgrGauge *g;
    void *obj = &member;
    float ratio;
    BtlChar_BindObject(chr);
    switch (0xEC) {
''' + transform_case + r'''
    }
}
int main(void) {
    BtlMgrChr c = {0};
    BtlMgrMember saved;
    member.chara = 107; member.costume = 1; member.aiType = 4;
    member.gauge.health = 10000; member.gauge.healthMax = 20000;
    member.gauge.ki = 3456; member.gauge.blast = 250000;
    saved = member;
    c.newChara = 106; c.newCostume = 0;
    temporary = 1;
    apply(&c);
    assert(!loads && !statResets && !memcmp(&member, &saved, sizeof(member)));
    temporary = 0; c.newChara = 108;
    apply(&c);
    assert(loads == 1 && statResets == 1 && member.chara == 108);
    assert(member.gauge.health == 35000 && member.gauge.ki == 50000);
    assert(member.gauge.blast == 250000);
    return 0;
}
'''

effects = (ROOT / 'src/battle/btl_char_fx_c.c').read_text(encoding='utf-8')
def effect_function(signature):
    begin = effects.index(signature)
    return effects[begin:effects.index('\n}', begin) + 2]

effects_fixture = r'''
#include "common.h"
#include "battle/btl_char_fx.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static FxObj owner, partner;
static int temporary, hidden, attackEvents;
static int frontCuts, hideMask, stageVisible, poseMotion = 1, poseStarts;
s32 BtlAnim_GetId(FxChr *chr) { return poseMotion; }
void BtlAnim_Play(FxChr *chr, s32 motion, f32 blend) {
    assert(motion == 1 && blend == 0); poseStarts++; owner.anim.frame = 0;
}
void EftStage_SetDrawOn(s32 on) { stageVisible = on; }
void StgTint_Start(s32 slot, s32 direction, f32 seconds) { assert(slot == 1 && direction == 1); }
void ChrCam_RequestCut(FxChr *chr, s32 table, s32 cut) {
    assert(table == 0 && cut == 6); frontCuts++;
}
void BtlObj_SetColorMode(FxObj *obj, s32 bit, s32 on) {
    assert(obj == &owner && bit == 3); hideMask = on;
}
FxObj *BtlChar_GetObj(FxChr *chr) { return &owner; }
FxObj *BtlObj_Get(s32 id) { return &partner; }
int Port_ZenkaiCellTransformTemporary(void *object) { assert(object == &owner); return temporary; }
void BtlChar_SetHeldFlag(FxChr *chr, s32 flag) { assert(flag == 0x30); hidden = 1; }
void BtlChar_ClearFlag(FxChr *chr, s32 flag) { assert(flag == 0x30); hidden = 0; }
void BtlPartner_HandleEvents(FxObj *obj, FxChr *chr) { attackEvents++; }
''' + effect_function('void Port_ZenkaiCellTransformKeepPose(') + '\n' + effect_function('void Port_ZenkaiCellTransformFrontCamera(') + '\n' + effect_function('void Port_ZenkaiCellTransformDialogue(') + '\n' + effect_function('void Port_ZenkaiCellTransformReveal(') + '\n' + effect_function('void BtlPartner_UpdateEvents(') + '\n' + effect_function(
    'void Port_ZenkaiCellTransformPartnerVisible(') + r'''
int main(void) {
    FxChr chr = {0};
    owner.pos.x = 10; owner.pos.y = 20; owner.pos.z = 30;
    Port_ZenkaiCellTransformFrontCamera(&chr);
    assert(frontCuts == 1);
    Port_ZenkaiCellTransformDialogue(&chr);
    assert(owner.anim.frame == 152 && owner.anim.prevFrame == 152);
    hidden = hideMask = 1; owner.flags = 0;
    Port_ZenkaiCellTransformReveal(&chr);
    assert(!hidden && !hideMask && stageVisible && (owner.flags & 2));
    temporary = 1;
    owner.anim.frame = 23.5f;
    Port_ZenkaiCellTransformKeepPose(&chr, 0);
    owner.anim.frame = owner.anim.prevFrame = 0;
    Port_ZenkaiCellTransformKeepPose(&chr, 1);
    assert(owner.anim.frame == 23.5f && owner.anim.prevFrame == 23.5f && poseStarts == 1);
    poseMotion = 0x162;
    Port_ZenkaiCellTransformKeepPose(&chr, 1);
    assert(poseStarts == 1); /* Never restart the explosion or final reveal. */
    poseMotion = 1;
    Port_ZenkaiCellTransformPartnerVisible(&chr, 1);
    assert(!hidden); /* A missing prop must not hide the real fighter. */
    chr.partner.active = 1;
    Port_ZenkaiCellTransformPartnerVisible(&chr, 1);
    assert(hidden && (partner.flags & 2));
    BtlPartner_UpdateEvents(&chr);
    assert(attackEvents == 1); /* Authored prop events use the original handler. */
    Port_ZenkaiCellTransformPartnerVisible(&chr, 0);
    assert(!hidden && !(partner.flags & 2));
    temporary = 0;
    Port_ZenkaiCellTransformPartnerVisible(&chr, 1);
    assert(!hidden && !(partner.flags & 2));
    BtlPartner_UpdateEvents(&chr);
    assert(attackEvents == 2); /* Other partner choreography is unchanged. */
    return 0;
}
'''


def production_function(path, signature):
    text = (ROOT / path).read_text(encoding='utf-8')
    begin = text.index(signature)
    return text[begin:text.index('\n}', begin) + 2] + '\n'

camera_fixture = r'''
#include "common.h"
#include "battle/btl_char_api.h"
#include "battle/eft_a.h"
#include "battle/battle.h"
#include "sys/port_zenkai.h"
#include <assert.h>
typedef BtlCharApiChr BtlCapiChr;
static BtlCharApiChr fighter;
static BtlCharApiObj object;
static int cinematic, temporary, motion, stopped, hidden, frozen, priority;
static float shownTime;
static EftCam camera;
EftCam *gEftCam = &camera;
static EftHitList hitList;
EftHitList *gEftHitList = &hitList;
static BattleWork battle;
BattleWork *Battle_GetWork(void) { return &battle; }
BtlCharApiChr *BtlChar_FindByObjId(s32 id) { return id == 0 ? &fighter : 0; }
BtlCharApiObj *BtlChar_GetObj(BtlCharApiChr *chr) { return &object; }
void *BtlObj_Get(s32 id) { return id == 0 ? &object : 0; }
int Port_ZenkaiCellTransformCinematic(void *obj) { return cinematic && obj == &object; }
int Port_ZenkaiCellTransformTemporary(void *obj) { return temporary && obj == &object; }
s32 BtlAct_GetCurrent(BtlCharApiChr *chr) { return chr->action; }
s32 BtlAct_IsTechniqueId(s32 action) { return action >= 0x105 && action <= 0x132; }
s32 BtlChar_TestFlag(BtlCharApiChr *chr, s32 flag) { return 0; }
s32 BtlAct_GetMotionLevel(BtlCharApiChr *chr, s32 id) { return id - 0x160; }
s32 BtlAnim_GetId(BtlCharApiChr *chr) { return motion; }
f32 BtlAnim_GetFrame(BtlCharApiChr *chr) { return object.unkC78; }
void DemoCam_SetAnim(void *anim) { assert(anim == &object); }
void DemoCam_SetChr(void *obj) { assert(obj == &object); }
void DemoCam_SetTime(f32 time) { shownTime = time; }
void DemoCam_Start(void) {}
void DemoCam_Stop(void) { stopped++; }
f32 DemoCam_GetTime(void) { return shownTime; }
f32 DemoCam_GetLength(void) { return 1000.0f; }
s32 BtlScene_IsTimeStopped(void) { return 0; }
f32 EftHit_GetScale(EftHitRec *rec) { return 1.0f; }
void EftHit_InitMultiHit(EftHitRec *rec) {}
void Port_EftHitScaleGiant(EftHitRec *rec) {}
s32 BtlCharApi_IsHidden(s32 id) { return hidden; }
s32 BtlCharApi_AnyFrozen(void) { return frozen; }
s32 BtlCharApi_IsFrozen(s32 id) { return frozen; }
s32 BtlStage_IsReady(void) { return 1; }
s32 BtlCharApi_AnyCamPriority(void) { return priority; }
'''
for name in ('BtlCharApi_IsInTechnique', 'BtlCharApi_IsRushConnected', 'BtlCharApi_IsInRushSequence', 'BtlCharApi_IsChanging'):
    camera_fixture += production_function('src/battle/btl_capi_a.c', 's32 ' + name + '(')
camera_fixture += production_function('src/battle/btl_scene.c', 's32 BtlScene_IsEffectHidden(')
camera_fixture += production_function('src/battle/btl_char_api.c', 'f32 BtlCharApi_GetChargedUnkC78(')
for name in ('EftCam_Start', 'EftCam_Stop', 'EftCam_Update', 'EftHit_Add'):
    camera_fixture += production_function('src/battle/eft_a.c', 'void ' + name + '(')
camera_fixture += r'''
int main(void) {
    EftCamArg arg = { &object, 0, 726 };
    EftHitRec hit = {0};
    fighter.action = 0xEC;
    /* Transformation filtering must not hide the Ultimate's explosion meshes
     * and particles, including while the swelling partner hides the fighter. */
    assert(BtlScene_IsEffectHidden(0, 1));
    cinematic = 1;
    assert(BtlCharApi_IsChanging(0));
    assert(!BtlScene_IsEffectHidden(0, 1));
    hidden = 1;
    assert(!BtlScene_IsEffectHidden(0, 1));
    assert(BtlScene_IsEffectHidden(0, 3));
    assert(BtlScene_IsEffectHidden(0, 4));
    assert(BtlScene_IsEffectHidden(1, 1));
    hidden = cinematic = 0;
    assert(BtlScene_IsEffectHidden(0, 1));
    assert(!BtlCharApi_IsInTechnique(0) && !BtlCharApi_IsInRushSequence(0));
    temporary = cinematic = 1; motion = 0x160; object.unkC78 = 10.0f;
    assert(BtlCharApi_IsInTechnique(0) && BtlCharApi_IsInRushSequence(0) && BtlCharApi_IsRushConnected(0));
    EftCam_Start(&arg);
    assert(camera.active && shownTime == 10.0f);
    fighter.unkEFC[0] = 45; fighter.unkEFC[1] = 654;
    motion = 0x161; object.unkC78 = 100;
    EftCam_Update(); assert(shownTime == 146.0f);
    motion = 0x162; object.unkC78 = 200;
    EftCam_Update(); assert(shownTime == 901.0f);
    battle.flags = BATTLE_FLAG_PAUSE; object.unkC78 = 220;
    EftCam_Update(); assert(shownTime == 901.0f);
    EftHit_Add(&hit); assert(hitList.count == 0);
    hit.objId = 1; EftHit_Add(&hit); assert(hitList.count == 1);
    EftCam_Stop(); assert(!camera.active && stopped == 1);
    cinematic = 0; hit.objId = 0; EftHit_Add(&hit); assert(hitList.count == 1);
    temporary = 0; EftHit_Add(&hit); assert(hitList.count == 2);
    assert(!BtlCharApi_IsInTechnique(0) && !BtlCharApi_IsInRushSequence(0));
    return 0;
}
'''

events_fixture = r'''
#include "common.h"
#include "battle/eft_k.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static EftTechEvtEntry entries[2];
static EftTechEvt events = {0, entries, 2};
EftTechEvt *gEftTechEvt = &events;
static int cinematic = 1, alive = 1, deaths;
static u64 nativeAttrs[2], pairedAttrs;
void *BtlObj_Get(s32 id) { return (void *)(unsigned long)(id + 1); }
int Port_ZenkaiCellTransformCinematic(void *obj) { return cinematic && obj == BtlObj_Get(0); }
u64 Port_ZenkaiCellTransformVictimEvents(int id) { assert(id == 0); return pairedAttrs; }
s32 BtlCharApi_GetOpponentObjId(s32 id) { return 1 - id; }
s32 BtlCharApi_IsRushConnected(s32 id) { return alive; }
s32 BtlCharApi_IsInTechnique(s32 id) { return alive; }
s32 BtlCharApi_IsInRushSequence(s32 id) { return alive; }
s32 BtlCharApi_ObjTestAttr(s32 id, u64 mask) { return (nativeAttrs[id] & mask) != 0; }
void BtlTask_SetDead(EftKTask *task) { deaths++; }
''' + production_function('src/battle/eft_k.c', 'void EftTechEvtTask_UpdateRush(') + r'''
int main(void) {
    EftKSrc src = {0};
    EftTechEvtTask work = {0};
    EftKTask task = {0};
    work.arg.src = &src; task.work = &work;
    nativeAttrs[0] = 0x200; nativeAttrs[1] = 0x400;
    pairedAttrs = 0x1000 | 0x4000 | 0x200000 | 0x400000;
    EftTechEvtTask_UpdateRush(&task);
    assert(!deaths && entries[0].events == EFT_TECH_EVT_START);
    assert(entries[1].events == (EFT_TECH_EVT_10 | EFT_TECH_EVT_40 | EFT_TECH_EVT_80 | EFT_TECH_EVT_100));
    entries[0].events = entries[1].events = 0; cinematic = 0;
    EftTechEvtTask_UpdateRush(&task);
    assert(entries[1].events == EFT_TECH_EVT_FIRE); /* Original rush event routing. */
    alive = 0;
    EftTechEvtTask_UpdateRush(&task);
    assert(deaths == 1 && (entries[0].events & EFT_TECH_EVT_ABORT));
    return 0;
}
'''

glow_fixture = r'''
#include "common.h"
#include "battle/eft_ab.h"
#include "sys/port_zenkai.h"
#include <assert.h>
static EftTransformMgr manager;
EftTransformMgr *gEftTransform = &manager;
static int hiddenMask, stageOn = 1, particles, shockWaves;
s32 BtlScene_IsEffectStopped(s32 id, s32 kind) { return 0; }
void EftTransform_UpdateNodes(EftAbTask *task, s32 kind) {}
void EftTransform_OnFlash(EftAbTask *task, s32 kind) {}
void EftStage_SetDrawOn(s32 on) { stageOn = on; }
void StgTint_Start(u32 slot, s32 direction, f32 seconds) {}
void BtlCharApi_ObjSetMaskBit3(s32 id) { hiddenMask = 1; }
void BtlCharApi_ObjClearMaskBit3(s32 id) { hiddenMask = 0; }
void BtlCharApi_GetNodePos(s32 id, s32 node, EftAbVec *pos) { pos->x = pos->y = pos->z = 0; }
s32 ScrWarp_Spawn(s32 id, EftAbVec *pos, f32 a, f32 b, f32 c, f32 d, f32 e) { shockWaves++; return 1; }
void EftEmit_SetNode(void *nodes, s32 slot, s32 node, EftAbVec *pos) {}
void EftEmit_RefreshFixedNodes(s32 id, void *nodes) {}
void EftTransform_SpawnParts(s32 id, EftAbTask *task, EftAbSet *set) { particles++; }
void EftStage_UpdateSprite(void) {}
void BtlTask_SetDead(EftAbTask *task) {}
'''
for signature in ('void Port_ZenkaiCellTransformGlow(', 's32 EftTransform_Flash(', 'void EftTransform_Update('):
    glow_fixture += production_function('src/battle/eft_ab_b.c', signature)
glow_fixture += r'''
int main(void) {
    EftTransform work = {0};
    EftAbTask task = {0};
    manager.task = &task; manager.busy = 1; task.work = &work; task.step = 1;
    work.arg.objId = 42; work.arg.kind = 7; work.delay = 10;
    Port_ZenkaiCellTransformGlow(43);
    assert(task.step == 1 && !work.flags); /* Never change another effect owner. */
    Port_ZenkaiCellTransformGlow(42);
    assert(task.step == 3 && (work.flags & EFT_TF_AFTER));
    for (int frame = 0; frame < 25; frame++) EftTransform_Update(&task);
    assert(!hiddenMask && stageOn && particles == 25 && (work.groups & 2));
    assert(!(work.flags & (EFT_TF_FLASH | EFT_TF_TINT)));
    work.flags = 0; task.step = 1;
    EftTransform_Flash(); EftTransform_Update(&task);
    assert(hiddenMask && !stageOn); /* Original flash behavior remains available. */
    for (int frame = 0; frame < 11; frame++) EftTransform_Update(&task);
    assert(!hiddenMask && shockWaves == 2);
    Port_ZenkaiCellTransformGlow(42);
    assert(stageOn && !hiddenMask);
    return 0;
}
'''

close_camera_fixture = r'''
#include "common.h"
#include "battle/btl_char_cam.h"
#include <assert.h>
static int temporary, motion;
void *BtlChar_GetObj(ChrCamChr *chr) { return chr; }
int Port_ZenkaiCellTransformTemporary(void *obj) { return temporary; }
s32 BtlAnim_GetId(ChrCamChr *chr) { return motion; }
f32 BtlCharApi_GetHeight(s32 id) { return 10.0f; }
''' + production_function('src/battle/btl_char_cam_modes.c',
                         'static void Port_ChrCamCellDetransform(') + r'''
int main(void) {
    ChrCamChr chr = {0};
    chr.cut.unk4 = 6;
    temporary = 1; motion = 1;
    chr.cut.valC = 80; chr.cut.valCDelta = 120;
    Port_ChrCamCellDetransform(&chr);
    assert(chr.cut.valC == 22 && chr.cut.valCDelta == 0);
    /* The standing Super Perfect reveal and regular transformations keep
     * their native camera distance and interpolation. */
    for (int scenario = 0; scenario < 3; scenario++) {
        temporary = scenario != 0;
        motion = scenario == 1 ? 0 : 1;
        chr.cut.unk4 = scenario == 2 ? 4 : 6;
        chr.cut.valC = 80; chr.cut.valCDelta = 120;
        Port_ChrCamCellDetransform(&chr);
        assert(chr.cut.valC == 80 && chr.cut.valCDelta == 120);
    }
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='cell-transformation-') as directory:
    tmp = Path(directory)
    (tmp / 'test.c').write_text(fixture + handler + checks, encoding='utf-8')
    (tmp / 'stats.c').write_text(stats_fixture, encoding='utf-8')
    for flag in (0, 1):
        subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                        '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                        f'-DPORT_ZENKAI_CELL_TRANSFORMATION={flag}', str(tmp / 'test.c'),
                        '-o', str(tmp / 'test')], cwd=ROOT, check=True)
        subprocess.run([str(tmp / 'test')], check=True)
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'stats.c'), '-o', str(tmp / 'stats')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'stats')], check=True)
    (tmp / 'events.c').write_text(events_fixture, encoding='utf-8')
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'events.c'), '-o', str(tmp / 'events')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'events')], check=True)
    (tmp / 'camera.c').write_text(camera_fixture, encoding='utf-8')
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'camera.c'), '-o', str(tmp / 'camera')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'camera')], check=True)
    (tmp / 'glow.c').write_text(glow_fixture, encoding='utf-8')
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'glow.c'), '-o', str(tmp / 'glow')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'glow')], check=True)
    (tmp / 'effects.c').write_text(effects_fixture, encoding='utf-8')
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'effects.c'), '-o', str(tmp / 'effects')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'effects')], check=True)
    (tmp / 'close_camera.c').write_text(close_camera_fixture, encoding='utf-8')
    subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin', '-fsanitize=undefined',
                    '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                    str(tmp / 'close_camera.c'), '-o', str(tmp / 'close_camera')], cwd=ROOT, check=True)
    subprocess.run([str(tmp / 'close_camera')], check=True)
print('Cell transformation checks passed: cinematic timing, model wait, cost and fallback')
