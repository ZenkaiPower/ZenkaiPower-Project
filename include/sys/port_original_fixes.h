#ifndef SYS_PORT_ORIGINAL_FIXES_H
#define SYS_PORT_ORIGINAL_FIXES_H
#include "port_zenkai.h"
/* Original-game corrections belong to Zenkai saves, including Classic Mode.
 * They are deliberately independent of Port_ZenkaiPackEnabled(). */
static inline int Port_OriginalFixesEnabled(void) {
    return PORT_ZENKAI_ORIGINAL_FIXES && Port_ZenkaiEnabled();
}
#endif
