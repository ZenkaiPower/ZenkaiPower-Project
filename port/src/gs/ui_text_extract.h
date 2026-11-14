#ifndef ZENKAIPOWER_UI_TEXT_EXTRACT_H
#define ZENKAIPOWER_UI_TEXT_EXTRACT_H

/* Generate the English UI, mode and story dialogue catalog from the extracted disc files.
   Work runs in a detached thread on legacy startup, or synchronously when
   ZENKAIPOWER_PREPARE_TEXT is set by setup. */
void UiText_ExtractOnStartup(const char *gameTextDir);

/* The Spanish, German, French and Italian catalogs (ui/<locale>.json) from the European disc's archives the setup
   took (gamedata/pzs3eu1), when one is missing or was made by another version of this. At once, in the caller's
   thread (only when catalogs need to be generated). Returns how many were made, 0 when none had to be, -1 when they could
   not be (`why` says why). */
#include <stddef.h>
int UiText_EuropeanOnStartup(const char *gameTextDir, char *why, size_t whyLen);

/* Generate game-skills.pak (virtual translations/skills/<locale>.json) from scripts embedded
   in USA character packs. Reads only script sections; preserves edited catalogs. */
void UiText_ExtractSkills(const char *gameTextDir);

#endif
