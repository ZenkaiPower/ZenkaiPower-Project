/* Runtime movesets for the active ZENKAI save. Disc files and saved character
 * customization stay untouched. Donor assets live only for this battle. */
#include "common.h"
#ifdef PORT
#include "battle/bobj_b.h"
#include "sys/file.h"
#include "sys/heap.h"
#include "sys/port_zenkai.h"
#include "sys/save.h"

/* Runtime guards below restrict the replacement moveset to offline ZENKAI. */
#ifndef PORT_ZENKAI_SSJ2_MOVESET
#define PORT_ZENKAI_SSJ2_MOVESET 1
#endif

extern void *memcpy(void *, const void *, u32);
extern void *memset(void *, s32, u32);
extern int Port_NetSession(void);
extern int Port_NetActive(void);

#define ZENKAI_OBJECTS 12
#define SUPER_BYTES 0x244
#define SUPER_MOTIONS 0x22
#define TEXT_LANGUAGES 9 /* model entries 0x2C..0x34 */
typedef struct MoveText { u16 *text; s32 length; } MoveText;
typedef struct ZenkaiMove {
    u8 super[SUPER_BYTES];
    void *motion[SUPER_MOTIONS];
    void *events;
    void *voiceLip[2][2]; /* Japanese / English, charge / fire */
    MoveText text[TEXT_LANGUAGES];
} ZenkaiMove;
static ZenkaiMove sMoves[2];
static s32 sMovesReady;
static u8 sSuper[ZENKAI_OBJECTS][SUPER_BYTES] __attribute__((aligned(16)));
static u16 *sText[ZENKAI_OBJECTS];

/* First page has the two Blast 1 entries, two Blast 2 entries and Ultimate.
 * Include the complete entry: cost icons, controls, conditions and notes. */
static s32 move_span(const u16 *text, s32 slot, const u16 **first, const u16 **last) {
    const u16 *p;
    s32 entry = -1;
    *first = *last = NULL;
    if (text == NULL) return 0;
    for (p = text; *p; p++) {
        if ((p == text || p[-1] == '\n' || p[-1] == 0xFEFF) && p[0] == '!' && p[1] == 'F' &&
            (p[2] == '*' || p[2] == '$' || p[2] == '@')) {
            if (*first != NULL) { *last = p; return 1; }
            if (p[2] == '*' && ++entry == slot) *first = p;
        }
    }
    if (*first != NULL) { *last = p; return 1; }
    return 0;
}

static void *clone_entry(u32 *pack, s32 entry) {
    s32 size = (pack[entry + 1] & ~3u) - (pack[entry] & ~3u);
    void *copy;
    if (size <= 0) return NULL;
    copy = Heap_Alloc(size, 0x40, 0, HEAP_ANY);
    if (copy != NULL) memcpy(copy, (u8 *)pack + (pack[entry] & ~3u), size);
    return copy;
}

void Port_ZenkaiMovesClear(void) {
    s32 i, j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < SUPER_MOTIONS; j++) if (sMoves[i].motion[j]) Heap_Free(sMoves[i].motion[j]);
        for (j = 0; j < TEXT_LANGUAGES; j++) if (sMoves[i].text[j].text) Heap_Free(sMoves[i].text[j].text);
        if (sMoves[i].events) Heap_Free(sMoves[i].events);
        for (j = 0; j < 4; j++) if (sMoves[i].voiceLip[j / 2][j % 2]) Heap_Free(sMoves[i].voiceLip[j / 2][j % 2]);
    }
    for (i = 0; i < ZENKAI_OBJECTS; i++) if (sText[i]) Heap_Free(sText[i]);
    memset(sMoves, 0, sizeof(sMoves));
    memset(sText, 0, sizeof(sText));
    sMovesReady = 0;
}

static s32 load_move(s32 move, s32 chara, s32 slot, s32 expected) {
    ZenkaiMove *dst = &sMoves[move];
    u32 *pack;
    u8 *params;
    const u16 *first, *last;
    s32 i, entry;
    pack = File_LoadSync(0x590 + chara * 10, NULL, 0);
    if (pack == NULL) return 0;
    if (pack[0] < 0x34) { Heap_Free(pack); return 0; }
    params = (u8 *)pack + (pack[0x18] & ~3u);
    if ((pack[0x19] & ~3u) - (pack[0x18] & ~3u) < SUPER_BYTES ||
        ((s16 *)(params + 0x18))[slot - 2] != expected) { Heap_Free(pack); return 0; }
    memcpy(dst->super, params, SUPER_BYTES);
    if (move == 1) {
        /* SSJ1's Instant Kamehameha uses voice lines 0x5B / 0x5C.
           Keep both voice languages independently of the selected text. */
        for (i = 0; i < 4; i++) {
            entry = 0x35 + (i / 2) * 100 + 0x5B + i % 2;
            if ((u32)entry > pack[0]) { Heap_Free(pack); return 0; }
            dst->voiceLip[i / 2][i % 2] = clone_entry(pack, entry);
            if (pack[entry] != pack[entry + 1] && !dst->voiceLip[i / 2][i % 2]) { Heap_Free(pack); return 0; }
        }
    }
    for (i = 0; i < TEXT_LANGUAGES; i++) {
        entry = 0x2C + i;
        if (pack[entry] == pack[entry + 1]) continue;
        if (move_span((u16 *)((u8 *)pack + (pack[entry] & ~3u)), slot, &first, &last)) {
            dst->text[i].length = last - first;
            dst->text[i].text = Heap_Alloc((last - first) * 2, 0x40, 0, HEAP_ANY);
            if (!dst->text[i].text) { Heap_Free(pack); return 0; }
            memcpy(dst->text[i].text, first, (last - first) * 2);
        }
    }
    Heap_Free(pack);
    pack = File_LoadSync(0x598 + chara * 10, NULL, 0);
    if (pack == NULL) return 0;
    for (i = 0; i < SUPER_MOTIONS; i++) {
        entry = 0x105 + (slot - 2) * SUPER_MOTIONS + i + 1;
        if ((u32)entry > pack[0]) { Heap_Free(pack); return 0; }
        dst->motion[i] = clone_entry(pack, entry);
        if (pack[entry] != pack[entry + 1] && !dst->motion[i]) { Heap_Free(pack); return 0; }
    }
    Heap_Free(pack);
    pack = File_LoadSync(0x599 + chara * 10, NULL, 0);
    if (pack == NULL) return 0;
    if (pack[0] >= (u32)(slot + 1)) dst->events = clone_entry(pack, slot + 1);
    Heap_Free(pack);
    return dst->events != NULL;
}

/* The parameter file is a structure of three-column arrays, not three records.
 * Copy all fields of one column, including fields not yet named by accessors. */
static void copy_super(u8 *dst, const u8 *src, s32 slot) {
    static const s32 spans[][3] = {
        {0x000, 0x018, 4}, {0x018, 0x024, 2}, {0x024, 0x090, 4},
        {0x090, 0x16B, 1}, {0x16C, 0x208, 4}, {0x208, 0x220, 2},
        {0x220, 0x22C, 1}, {0x22C, 0x244, 4}
    };
    s32 i, offset, width;
    for (i = 0; i < 8; i++) {
        width = spans[i][2];
        for (offset = spans[i][0]; offset < spans[i][1]; offset += width * 3)
            memcpy(dst + offset + slot * width, src + offset + slot * width, width);
    }
}

static u16 *make_text(const u16 *target, s32 language) {
    const u16 *a, *b, *c, *d, *end;
    MoveText *regular = &sMoves[0].text[language], *ultimate = &sMoves[1].text[language];
    u16 *out, *p;
    s32 length;
    if (!regular->text || !ultimate->text || !move_span(target, 2, &a, &b) || !move_span(target, 4, &c, &d)) return NULL;
    for (end = d; *end; end++) {}
    length = (a - target) + regular->length + (c - b) + ultimate->length + (end - d) + 1;
    out = Heap_Alloc(length * 2, 0x40, 0, HEAP_ANY);
    if (out == NULL) return NULL;
    p = out;
#define APPEND(src, count) do { s32 n = (count); memcpy(p, (src), n * 2); p += n; } while (0)
    APPEND(target, a - target);
    APPEND(regular->text, regular->length);
    APPEND(b, c - b);
    APPEND(ultimate->text, ultimate->length);
    APPEND(d, end - d + 1);
#undef APPEND
    return out;
}

static s32 has_moves(void *object) {
    BObj *obj = object;
    s32 i;
    if (!PORT_ZENKAI_SSJ2_MOVESET || obj == NULL || !sMovesReady || !Port_ZenkaiPackEnabled() || Port_NetSession() || Port_NetActive()) return 0;
    for (i = 0; i < ZENKAI_OBJECTS; i++) if (obj->mdl.unk914 == sSuper[i]) return 1;
    return 0;
}

int Port_ZenkaiMoveVoice(void *object, int slot, int line) {
    if (!has_moves(object)) return line;
    /* SSJ2's former Ultimate dialogue becomes the new regular Super beam. */
    if (slot == 2 && (line == 0x57 || line == 0x58)) return line + 4;
    /* Ultimate dialogue comes from SSJ1, not SSJ2's voice bank. */
    if (slot == 4 && (line == 0x5B || line == 0x5C)) return PORT_ZENKAI_VOICE_TAG | (4 << 16) | line;
    return line;
}

void *Port_ZenkaiMoveLip(void *object, int voice) {
    s32 phase = (voice & 0xFFFF) - 0x5B;
    if (!has_moves(object) || !(voice & PORT_ZENKAI_VOICE_TAG) ||
        ((voice >> 16) & 0xFF) != 4 || phase < 0 || phase > 1) return NULL;
    return sMoves[1].voiceLip[(gSaveData->flags & SAVE_FLAG_VOICE) != 0][phase];
}

void Port_ZenkaiMovesBind(void *object, s32 id, s32 chara, s32 type) {
    BObj *obj = object;
    s32 i, language = -1;
    u16 *text;
    u8 *original;
    if (!PORT_ZENKAI_SSJ2_MOVESET) return;
    original = obj->mdl.unk914;
    if (type != 0 || chara != 5 || id < 0 || id >= ZENKAI_OBJECTS || !Port_ZenkaiPackEnabled() ||
        Port_NetSession() || Port_NetActive() || original == NULL) return;
    if (((s16 *)(original + 0x18))[0] != 300 || ((s16 *)(original + 0x18))[2] != 602) return;
    if (!sMovesReady) {
        if (!load_move(0, 6, 2, 301) || !load_move(1, 4, 4, 603)) { Port_ZenkaiMovesClear(); return; }
        sMovesReady = 1;
    }
    for (i = 0; i < TEXT_LANGUAGES; i++) {
        if (BObjFile_GetEntry(&obj->slot->file[0], 0x2C + i) == obj->mdl.unkA4) language = i;
    }
    if (language < 0 || (text = make_text(obj->mdl.unkA4, language)) == NULL) return;
    if (sText[id]) Heap_Free(sText[id]);
    sText[id] = text;
    obj->mdl.unkA4 = text;
    memcpy(sSuper[id], original, SUPER_BYTES);
    copy_super(sSuper[id], sMoves[0].super, 0);
    copy_super(sSuper[id], sMoves[1].super, 2);
    obj->mdl.unk914 = sSuper[id];
    for (i = 0; i < SUPER_MOTIONS; i++) {
        obj->mdl.anims[0x105 + i] = sMoves[0].motion[i];
        obj->mdl.anims[0x149 + i] = sMoves[1].motion[i];
    }
    obj->mdl.unk84[2] = sMoves[0].events;
    obj->mdl.unk84[4] = sMoves[1].events;
}
#endif
