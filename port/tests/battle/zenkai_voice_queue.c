/* Test the real animation-event -> positional voice request path.
 * gcc -std=gnu99 -g -no-pie -fno-builtin -ffunction-sections -fdata-sections \
 *   -Iinclude -Iport/include -include port_compat.h \
 *   port/tests/battle/zenkai_voice_queue.c -Wl,--gc-sections -o /tmp/zenkai_voice_queue
 * /tmp/zenkai_voice_queue */
#include "../../../src/battle/btl_char_flag_snd.c"
#include <assert.h>
static BtlFlagRoster roster;
BtlFlagRoster *gBtlChars = &roster;
static int slot, mapped = 1, mouthMode, mouthArg;
static u64 event;
static s32 object[8], lip[8];
static int cellTransform;
int Port_ZenkaiCellTransformCinematic(void *obj) { assert(obj == object); return cellTransform; }
void Vec4_Copy(Vec4 *dst, Vec4 *src) { *dst = *src; }
void BtlCharApi_GetNodePos(s32 id, s32 node, Vec4 *out) { Vec4 zero = {0}; (void)id; (void)node; *out = zero; }
void *BtlChar_GetObj(BtlFlagChr *chr) { (void)chr; return object; }
s32 BtlAct_GetCurrentClass(BtlFlagChr *chr) { (void)chr; return slot; }
s32 BtlAnim_TestAttr(BtlFlagChr *chr, u64 mask) { (void)chr; return (event & mask) != 0; }
s32 BtlOpp_GetParamWord0(BtlFlagChr *chr) { (void)chr; return 0; }
s32 BtlSuper_GetFlags(BtlFlagChr *chr, s32 which) { (void)chr; (void)which; return 0; }
void BtlObj_SetSubState(void *obj, s32 mode, s32 arg) { assert(obj == object); mouthMode = mode; mouthArg = arg; }
/* Asset-backed routing/lip selection is tested separately in zenkai_moves.c. */
int Port_ZenkaiMoveVoice(void *obj, int which, int line) {
    assert(obj == object);
    if (cellTransform && which == 4) return PORT_ZENKAI_VOICE_TAG | (106 << 16) | line;
    if (!mapped) return line;
    return which == 2 ? line + 4 : which == 4 ? PORT_ZENKAI_VOICE_TAG | (4 << 16) | line : line;
}
void *Port_ZenkaiMoveLip(void *obj, int voice) { assert(obj == object); return (voice & PORT_ZENKAI_VOICE_TAG) ? lip : NULL; }
int main(void) {
    BtlFlagChr chr = {0};
    int side, phase;
    for (side = 0; side < 2; side++) for (phase = 0; phase < 2; phase++) {
        BtlFlagSndParam *request = &roster.snd.req[0].p;
        chr.player = side; chr.objId = side;
        event = phase ? 0x8000000000ull : 0x8000;
        slot = 2; roster.snd.count = 0;
        BtlCharSnd_PlayTechSounds(&chr);
        assert(roster.snd.count == 1 && request->id == 0x5B + phase);
        assert(request->owner == side && request->kind == side + 5);
        assert(request->near == 200.0f && request->far == 1500.0f);
        assert(mouthMode == 2 && mouthArg == 0x5B + phase);
        slot = 4; roster.snd.count = 0;
        BtlCharSnd_PlayTechSounds(&chr);
        assert(roster.snd.count == 1 && (request->id & PORT_ZENKAI_VOICE_TAG));
        assert(((request->id >> 16) & 255) == 4 && (request->id & 0xFFFF) == 0x5B + phase);
        assert(request->owner == side && request->kind == side + 5);
        assert(mouthMode == 14 && mouthArg == (s32)(unsigned long)lip);
        mapped = 0; slot = 2; roster.snd.count = 0;
        BtlCharSnd_PlayTechSounds(&chr);
        assert(request->id == 0x57 + phase && mouthMode == 2 && mouthArg == 0x57 + phase);
        mapped = 1;
        mapped = 0; cellTransform = 1; roster.snd.count = 0;
        slot = -1; BtlCharSnd_PlayTechSounds(&chr);
        assert(roster.snd.count == 1 && ((request->id >> 16) & 255) == 106);
        assert((request->id & PORT_ZENKAI_VOICE_TAG) && (request->id & 0xFFFF) == 0x5B + phase);
        assert(request->owner == side && request->kind == side + 5);
        assert(mouthMode == 14 && mouthArg == (s32)(unsigned long)lip);
        roster.snd.count = 0; cellTransform = 0;
        slot = -1; BtlCharSnd_PlayTechSounds(&chr);
        assert(!roster.snd.count);
        cellTransform = 1; event = 0;
        BtlCharSnd_PlayTechSounds(&chr);
        assert(!roster.snd.count);
        mapped = 1; cellTransform = 0;
    }
    return 0;
}
