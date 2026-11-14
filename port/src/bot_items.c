/* Bot loadouts are battle-local: never edit either player's saved custom sets. */
#include "types.h"
#include <stddef.h>
#include "battle/view_b.h"
#include "sys/common.h"
#include "sys/save.h"
#include "menu/port_bot_items.h"

extern s32 Rand_Range(s32 count);
extern void *memset(void *dst, s32 value, u32 size);

void Port_BotItemsGenerate(s32 chara, u16 *items) {
    ItemTblEntry *table;
    u16 pool[SAVE_ITEM_COUNT];
    s32 count = 0, used = 0, remaining = 7, i, j;
    memset(items, 0, ITEMSET_COUNT * sizeof(*items));
    if (chara < 0 || chara >= 161 || gCommonRes == NULL || gCommonRes->data[2] == NULL) return;
    table = (ItemTblEntry *)((u8 *)gCommonRes->data[2] +
                            ((((u32 *)gCommonRes->data[2])[2] >> 2) << 2));
    /* AI/type items have their own eighth position. Half of custom picks omit it. */
    if (Rand_Range(2)) {
        for (i = 0; i < SAVE_ITEM_COUNT; i++)
            if (table[i].type == ITEMTBL_TYPE_AI && table[i].slots <= remaining &&
                ItemTbl_CanEquip(i, chara, table)) pool[count++] = i + 1;
        if (count) {
            items[7] = pool[Rand_Range(count)];
            remaining -= table[items[7] - 1].slots;
        }
    }
    count = 0;
    for (i = 0; i < SAVE_ITEM_COUNT; i++)
        if (table[i].type < ITEMTBL_TYPE_AI && ItemTbl_CanEquip(i, chara, table)) pool[count++] = i + 1;
    /* Shuffle once: no duplicates, bounded work even with very few legal items. */
    for (i = count - 1; i > 0; i--) {
        s32 at = Rand_Range(i + 1);
        u16 swap = pool[i]; pool[i] = pool[at]; pool[at] = swap;
    }
    for (i = 0; i < count && used < ITEMSET_USABLE; i++) {
        ItemTblEntry *entry = &table[pool[i] - 1];
        if (entry->slots > remaining) continue;
        for (j = 0; j < used; j++) {
            ItemTblEntry *other = &table[items[j] - 1];
            if (other->type == entry->type && other->unk1[0] == entry->unk1[0]) break;
        }
        if (j != used) continue;
        items[used++] = pool[i];
        remaining -= entry->slots;
    }
}
