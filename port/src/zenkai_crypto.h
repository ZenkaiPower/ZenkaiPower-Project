#ifndef ZENKAI_CRYPTO_H
#define ZENKAI_CRYPTO_H
#include <stdio.h>
/* Unencrypted binary v3 saves use checksums and atomic replacement. Legacy encrypted
 * v2 saves remain readable and migrate to v3 on their next successful write.
 * Handles expose just the payload through an anonymous temporary stream. */
FILE *Port_ZenkaiOpen(const char *path, int mode);
int Port_ZenkaiFlush(const char *path, FILE *plain);
/* Returns 1 for a validated v2/v3 main ZENKAI save. Outputs are
 * lowercase hex, NUL terminated (32 chars for the ID, 64 for the digest).
 * Missing, corrupt and unsupported-version files return 0 with empty outputs. */
int Port_ZenkaiMetadata(const char *path, char save_id[33], char save_hash[65]);
/* Save_SetDefaults begins a new logical game; opening an existing main save for read
 * cancels it. Neither action changes an on-disk file before a successful flush. */
void Port_ZenkaiNewGame(void);
void Port_ZenkaiExistingSaveLoaded(void);
#endif
