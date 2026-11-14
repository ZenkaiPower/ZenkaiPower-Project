#include "../../include/sys/port_zenkai.h"
#include <stdlib.h>
extern int Port_Setting(const char *, int);
/* Simulation state: kept with the game across rollback/snapshots. */
static int sActive, sSelected, sSelecting;
int Port_ZenkaiEnabled(void) { return !getenv("ZENKAIPOWER_SAVES") && sActive; }
int Port_ZenkaiSelected(void) {
    return !getenv("ZENKAIPOWER_SAVES") && (sSelecting ? sSelected : sActive);
}
void Port_ZenkaiSelect(void) {
    sSelected = !getenv("ZENKAIPOWER_SAVES") && Port_Setting("zenkai_power", 1) != 0;
    sSelecting = 1;
}
void Port_ZenkaiCommit(void) {
    if (sSelecting) sActive = sSelected;
}
void Port_ZenkaiFinish(void) { sSelecting = 0; }
