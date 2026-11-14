#ifndef BATTLE_PORT_TRAINING_OPTIONS_H
#define BATTLE_PORT_TRAINING_OPTIONS_H
#define PORT_TRAINING_TITLE_PART 8
#define PORT_TRAINING_REGEN_TEXT 0x7000
#ifdef PORT
extern int Port_ZenkaiEnabled(void);
extern int gPortTrainingHealthRegeneration;
extern int gPortTrainingKiRegeneration;
extern int gPortTrainingBlastRegeneration;
#endif
#endif
