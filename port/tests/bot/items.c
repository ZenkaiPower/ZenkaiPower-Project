/* cc -std=gnu99 -fno-builtin -ffunction-sections -fdata-sections -Iinclude
 * -Iport/include -include port_compat.h port/tests/bot/items.c
 * -Wl,--gc-sections -o /tmp/bot-items && /tmp/bot-items */
#include "../../src/bot_items.c"
#include "../../../src/battle/view_b_b.c"
#include <assert.h>
#include <stdio.h>

CommonRes *gCommonRes;
SaveData *gSaveData;
static unsigned rng;
s32 Rand_Range(s32 n) { assert(n > 0);rng = rng * 1664525u + 1013904223u;return (rng >> 8) % n; }
static struct {
    u32 header[4];
    ChrTblEntry charas[161];
    ItemTblEntry items[SAVE_ITEM_COUNT];
} data;

int main(void) {
    CommonRes resources = {0};
    u16 items[8], repeated[8];
    int withType = 0, withoutType = 0;
    gCommonRes = &resources;resources.data[2] = &data;
    data.header[1] = (u8 *)data.charas - (u8 *)&data;
    data.header[2] = (u8 *)data.items - (u8 *)&data;
    data.charas[1].flags = CHRTBL_FLAG_0;
    for (int i = 0; i < SAVE_ITEM_COUNT; i++) {
        data.items[i].type = i % 3;
        data.items[i].unk1[0] = i % 17;
        data.items[i].slots = data.items[i].type == ITEMTBL_TYPE_AI ? 0 : 1+i%4;
        data.items[i].flags = ITEMTBL_FLAG_8 | ITEMTBL_FLAG_10;
    }
    data.items[4].flags |= ITEMTBL_FLAG_NONE_A;
    data.items[5].flags = ITEMTBL_FLAG_8;
    data.items[6].slots = 9;
    data.items[7].type = 3;
    for (unsigned seed = 1; seed <= 2000; seed++) {
        int chara = seed % 2, cost = 0;
        rng = seed;Port_BotItemsGenerate(chara, items);
        rng = seed;Port_BotItemsGenerate(chara, repeated);
        for (int i = 0; i < 8; i++) {
            assert(items[i] == repeated[i]);
            if (!items[i]) continue;
            int id = items[i]-1;
            assert(id >= 0 && id < SAVE_ITEM_COUNT && ItemTbl_CanEquip(id, chara, data.items));
            assert((i == 7) == (data.items[id].type == ITEMTBL_TYPE_AI));
            assert(data.items[id].type < 3);
            cost += data.items[id].slots;
            for (int j = 0; j < i; j++) if (items[j]) {
                int other = items[j]-1;
                assert(id != other);
                assert(data.items[id].type != data.items[other].type ||
                       data.items[id].unk1[0] != data.items[other].unk1[0]);
            }
        }
        assert(cost <= 7);
        if (items[7]) withType++;else withoutType++;
    }
    assert(withType > 800 && withoutType > 800);
    for (int i = 0; i < SAVE_ITEM_COUNT; i++) data.items[i].flags = ITEMTBL_FLAG_NONE_A;
    Port_BotItemsGenerate(0, items);
    for (int i = 0; i < 8; i++) assert(!items[i]);
    Port_BotItemsGenerate(-1, items);
    for (int i = 0; i < 8; i++) assert(!items[i]);
    puts("PASS: bot item capacity, type chance, compatibility, conflicts and deterministic RNG");
}
