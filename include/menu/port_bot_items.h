#ifndef MENU_PORT_BOT_ITEMS_H
#define MENU_PORT_BOT_ITEMS_H
#include "types.h"
/* Uses the simulation RNG; caller must run this on both peers. */
void Port_BotItemsGenerate(s32 chara, u16 *items);
#endif
