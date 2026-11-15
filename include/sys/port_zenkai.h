#ifndef PORT_ZENKAI_H
#define PORT_ZENKAI_H
#include "sys/port_zenkai_features.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Selection is transactional: cancelling a load/new-game flow retains the
 * current game's rules and save destination. UI settings only select a future game. */
int Port_ZenkaiEnabled(void);
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
