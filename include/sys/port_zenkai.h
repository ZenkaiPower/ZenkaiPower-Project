#ifndef PORT_ZENKAI_H
#define PORT_ZENKAI_H
#include "port_zenkai_features.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Selection is transactional: cancelling a load/new-game flow retains the
 * current game's rules and save destination. UI settings only select a future game. */
int Port_ZenkaiEnabled(void);
/* Per-save Classic Mode. Disabling the pack does not change save selection. */
int Port_ZenkaiPackEnabled(void);
int Port_ZenkaiGreatApeMoonBind(void *object, int giant);
int Port_ZenkaiCellTransformBind(void *object, int character, int target);
void Port_ZenkaiCellTransformPresent(void *object, int on);
int Port_ZenkaiCellTransformCinematic(void *object);
unsigned long long Port_ZenkaiCellTransformVictimAttrs(void *object, int motion, float previous, float current);
unsigned long long Port_ZenkaiCellTransformVictimEvents(int objectId);
int Port_ZenkaiCellTransformTemporary(void *object);
void Port_ZenkaiCellTransformEnd(void *object);
int Port_ZenkaiCellTransformPartner(int costume);
int Port_ZenkaiCellTransformOpeningFrames(void);
void Port_ZenkaiCellTransformFrontCamera(void *fighter);
void Port_ZenkaiCellTransformKeepPose(void *fighter, int restore);
void Port_ZenkaiCellTransformDialogue(void *fighter);
void Port_ZenkaiCellTransformReveal(void *fighter);
void Port_ZenkaiCellTransformGlow(int objectId);
void Port_ZenkaiCellTransformPartnerVisible(void *fighter, int visible);
int Port_ZenkaiPackCanDisable(void);
int Port_ZenkaiPackDisable(void);
void Port_ZenkaiPackInit(void *save);
void Port_ZenkaiPackEarnedZeni(void *save, int amount);
int Port_ZenkaiFeatureEnabled(const char *id);
int Port_ZenkaiFeatureInGroup(const char *id, const char *group);
/* Offline Zenkai giant model, collision, camera and effect helpers. */
int Port_GinyuSelfDamageActive(void *fighter);
int Port_GinyuSideSelfDamageActive(int side);
int Port_GinyuChooseOpponentBody(void *fighter, int *chara, int *costume, int *variant);
void Port_GinyuClearSide(int side);
int Port_GinyuIncomingBorrowed(int side);
int Port_GinyuOpponentReload(int side);
int Port_GinyuReplacedMember(int side);
int Port_GinyuMoveOwner(int side);
int Port_GinyuReloadMember(int side);
void Port_GinyuFinishOpponentLoad(int side);
void Port_GinyuCaptureMoves(int side, void *object, int modelOnly);
void Port_GinyuRestoreMoves(int side, void *object);
float Port_ZenkaiGiantScale(int type, const void *param);
float Port_ZenkaiGiantAnimationScale(int type, const void *param);
float Port_ZenkaiGiantEffectScale(int objId);
float Port_ZenkaiGiantContactRadius(void *object, const void *target, float fallback);
int Port_ZenkaiGiantFocus(void *object, void *position);
int Port_ZenkaiGiantVsSmall(void *object);
float Port_ZenkaiGiantApproachOffset(void *object);
/* Startup-only check: reads Autosave from .options before the card load. */
int Port_BootAutoSaveEnabled(void);
int Port_ZenkaiSelected(void);
void Port_ZenkaiSelect(void);
void Port_ZenkaiCommit(void);
void Port_ZenkaiFinish(void);
/* Native and port Options companion, persisted with the ZENKAI save flow. */
void Port_OptionsSaveLoad(void *save);
int Port_OptionsSaveSync(void *save);
int Port_OptionsSaveCommit(void);
int Port_OptionsFileRead(void *payload);
int Port_OptionsFileWrite(const void *payload);
void Port_SettingsGameCommit(void);
/* A tagged character/line pair travels through the normal positional voice queue. */
#define PORT_ZENKAI_VOICE_TAG 0x40000000
int Port_ZenkaiMoveVoice(void *object, int slot, int line);
void *Port_ZenkaiMoveLip(void *object, int voice);
#ifdef __cplusplus
}
#endif
#endif
