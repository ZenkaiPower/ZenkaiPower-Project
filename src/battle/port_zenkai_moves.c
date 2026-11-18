/* Runtime movesets for the active ZENKAI save. Disc files and saved character
 * customization stay untouched. Donor assets live only for this battle. */
#include "common.h"
#ifdef PORT
#include "battle/bobj_b.h"
#include "sys/bpe.h"
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
/* Normal grab: attacker, paired victim and recovery, animation IDs 0x96..0x98. */
static void *sFutureGohanGrab[3];
static void *sGreatApeMoon[2][3];
static void *sGreatApeMoonCamera[2];
/* Semi-Perfect Cell's Ultimate cinematic: opening, preparation and detonation poses.
 * Keep the incoming Super Perfect Cell's own recovery animation. */
static void *sCellTransform[3];
static void *sCellWeakened;
static void *sCellOriginalIdle[ZENKAI_OBJECTS];
typedef struct CellCinematicEvent {
    u32 attrLo, attrHi;
    u16 frame, shape;
    u32 arg;
} CellCinematicEvent;
static u8 *sCellVictim[3];
static void *sCellTransformLip[2][2]; /* Japanese / English, preparation / detonation. */
static s32 sCellTransformPartner[2];
static void *sCellTransformActors[ZENKAI_OBJECTS];
static u8 sCellCinematic[ZENKAI_OBJECTS];
extern s32 BtlStage_HasMoon(void);
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

static void clear_cell_transform(void) {
    s32 i;
    if (sCellWeakened) Heap_Free(sCellWeakened);
    sCellWeakened = NULL;
    memset(sCellOriginalIdle, 0, sizeof(sCellOriginalIdle));
    for (i = 0; i < 3; i++) {
        if (sCellTransform[i]) Heap_Free(sCellTransform[i]);
        sCellTransform[i] = NULL;
        if (sCellVictim[i]) Heap_Free(sCellVictim[i]);
        sCellVictim[i] = NULL;
    }
    for (i = 0; i < 4; i++) {
        if (sCellTransformLip[i / 2][i % 2]) Heap_Free(sCellTransformLip[i / 2][i % 2]);
        sCellTransformLip[i / 2][i % 2] = NULL;
    }
    memset(sCellTransformActors, 0, sizeof(sCellTransformActors));
    memset(sCellCinematic, 0, sizeof(sCellCinematic));
    memset(sCellTransformPartner, 0, sizeof(sCellTransformPartner));
}

static void clear_ssj2_moves(void) {
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

void Port_ZenkaiMovesClear(void) {
    s32 i, j;
    clear_ssj2_moves();
    clear_cell_transform();
    for (i = 0; i < 3; i++) {
        if (sFutureGohanGrab[i]) Heap_Free(sFutureGohanGrab[i]);
        sFutureGohanGrab[i] = NULL;
    }
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            if (sGreatApeMoon[i][j]) Heap_Free(sGreatApeMoon[i][j]);
            sGreatApeMoon[i][j] = NULL;
        }
        if (sGreatApeMoonCamera[i]) Heap_Free(sGreatApeMoonCamera[i]);
        sGreatApeMoonCamera[i] = NULL;
    }
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
    if (Port_ZenkaiCellTransformCinematic(object) && slot == 4 && (line == 0x5B || line == 0x5C))
        return PORT_ZENKAI_VOICE_TAG | (106 << 16) | line;
    if (!has_moves(object)) return line;
    /* SSJ2's former Ultimate dialogue becomes the new regular Super beam. */
    if (slot == 2 && (line == 0x57 || line == 0x58)) return line + 4;
    /* Ultimate dialogue comes from SSJ1, not SSJ2's voice bank. */
    if (slot == 4 && (line == 0x5B || line == 0x5C)) return PORT_ZENKAI_VOICE_TAG | (4 << 16) | line;
    return line;
}

void *Port_ZenkaiMoveLip(void *object, int voice) {
    s32 phase = (voice & 0xFFFF) - 0x5B;
    if ((voice & PORT_ZENKAI_VOICE_TAG) && ((voice >> 16) & 0xFF) == 106 &&
        phase >= 0 && phase < 2 && Port_ZenkaiCellTransformTemporary(object)) {
        return sCellTransformLip[(gSaveData->flags & SAVE_FLAG_VOICE) != 0][phase];
    }
    if (!has_moves(object) || !(voice & PORT_ZENKAI_VOICE_TAG) ||
        ((voice >> 16) & 0xFF) != 4 || phase < 0 || phase > 1) return NULL;
    return sMoves[1].voiceLip[(gSaveData->flags & SAVE_FLAG_VOICE) != 0][phase];
}

/* Copy Nappa's complete transformation motion set. The incoming Great Ape
 * needs its own closing animation. Camera tracks are separate model-pack entries:
 * the human intro must frame the moon while motion events 0x4000 show its sprite. */
int Port_ZenkaiGreatApeMoonBind(void *object, int giant) {
    BObj *obj = object;
    u32 *pack;
    s32 form, i, j;
    if (!PORT_ZENKAI_GREAT_APE_MOON || !obj || giant < 0 || giant > 1 ||
        !Port_ZenkaiPackEnabled() || Port_NetSession() || Port_NetActive() || !BtlStage_HasMoon()) return 0;
    if (!sGreatApeMoon[0][0]) {
        for (form = 0; form < 2; form++) {
            pack = File_LoadSync(0x598 + (80 + form) * 10, NULL, 0);
            if (!pack) break;
            if (pack[0] < 0x17A) { Heap_Free(pack); break; }
            for (i = 0; i < 3; i++) {
                sGreatApeMoon[form][i] = clone_entry(pack, 0x178 + i);
                if (!sGreatApeMoon[form][i]) break;
            }
            Heap_Free(pack);
            if (i != 3) break;
            pack = File_LoadSync(0x590 + (80 + form) * 10, NULL, 0);
            if (!pack) break;
            if (pack[0] >= (u32)(0x25 + form * 2))
                sGreatApeMoonCamera[form] = clone_entry(pack, 0x25 + form * 2);
            Heap_Free(pack);
            if (!sGreatApeMoonCamera[form]) break;
        }
        if (form != 2) {
            for (i = 0; i < 2; i++) for (j = 0; j < 3; j++) {
                if (sGreatApeMoon[i][j]) Heap_Free(sGreatApeMoon[i][j]);
                sGreatApeMoon[i][j] = NULL;
            }
            for (i = 0; i < 2; i++) {
                if (sGreatApeMoonCamera[i]) Heap_Free(sGreatApeMoonCamera[i]);
                sGreatApeMoonCamera[i] = NULL;
            }
            return 0;
        }
    }
    for (i = 0; i < 3; i++) obj->mdl.anims[0x177 + i] = sGreatApeMoon[giant][i];
    obj->mdl.unk8F0[giant * 2] = sGreatApeMoonCamera[giant];
    return 1;
}

int Port_ZenkaiCellTransformBind(void *object, int character, int target) {
    BObj *obj = object;
    u32 *pack;
    s32 i, actor;
    if (!PORT_ZENKAI_CELL_TRANSFORMATION || !obj || character != 107 || target != 108 ||
        !Port_ZenkaiPackEnabled() || Port_NetSession() || Port_NetActive()) return 0;
    for (actor = 0; actor < ZENKAI_OBJECTS; actor++) {
        if (!sCellTransformActors[actor] || sCellTransformActors[actor] == obj) break;
    }
    if (actor == ZENKAI_OBJECTS) return 0;
    if (!sCellTransform[0]) {
        pack = File_LoadSync(0x598 + 106 * 10, NULL, 0);
        if (!pack) return 0;
        if (pack[0] < 0x168) { Heap_Free(pack); return 0; }
        sCellWeakened = clone_entry(pack, 2); /* Semi-Perfect low-health idle, motion 1. */
        for (i = 0; i < 3; i++) {
            s32 size = 0;
            void *compressed = clone_entry(pack, 0x166 + i);
            sCellTransform[i] = clone_entry(pack, 0x161 + i);
            if (compressed) {
                sCellVictim[i] = Bpe_Decode(compressed, NULL, &size);
                Heap_Free(compressed);
            }
            if (!sCellTransform[i] || !sCellVictim[i] || size < 0x94 ||
                size < 0x94 + sCellVictim[i][1] * sizeof(CellCinematicEvent)) break;
        }
        Heap_Free(pack);
        if (i != 3 || !sCellWeakened) {
            clear_cell_transform();
            return 0;
        }
        pack = File_LoadSync(0x590 + 106 * 10, NULL, 0);
        if (!pack) { clear_cell_transform(); return 0; }
        if (pack[0] >= 0x35 + 100 + 0x5C) {
            u8 *super = (u8 *)pack + (pack[0x18] & ~3u);
            s32 size = (pack[0x19] & ~3u) - (pack[0x18] & ~3u);
            if (size >= SUPER_BYTES) {
                sCellTransformPartner[0] = ((s16 *)(super + 0x208))[2];
                sCellTransformPartner[1] = ((s16 *)(super + 0x20E))[2];
            }
            for (i = 0; i < 4; i++) {
                sCellTransformLip[i / 2][i % 2] = clone_entry(pack, 0x35 + (i / 2) * 100 + 0x5B + i % 2);
                if (!sCellTransformLip[i / 2][i % 2]) break;
            }
        } else i = 0;
        Heap_Free(pack);
        if (i != 4 || sCellTransformPartner[0] < 0x100 || sCellTransformPartner[1] < 0x100) {
            clear_cell_transform();
            return 0;
        }
    }
    if (sCellTransformActors[actor] != obj) sCellOriginalIdle[actor] = obj->mdl.anims[1];
    /* Perfect Form's own motion 1 does not give the same exhausted stance.
     * Borrow the actual Semi-Perfect pose only for this temporary presentation. */
    obj->mdl.anims[1] = sCellWeakened;
    sCellTransformActors[actor] = obj;
    sCellCinematic[actor] = 0;
    return 1;
}

int Port_ZenkaiCellTransformOpeningFrames(void) {
    /* Paired opening motions share the same cinematic timeline. */
    return sCellVictim[0] ? ((u16 *)sCellVictim[0])[1] : 0;
}

int Port_ZenkaiCellTransformPartner(int costume) {
    return sCellTransformPartner[costume == 1];
}

int Port_ZenkaiCellTransformTemporary(void *object) {
    s32 i;
    if (!object) return 0;
    for (i = 0; i < ZENKAI_OBJECTS; i++) {
        if (sCellTransformActors[i] == object) return 1;
    }
    return 0;
}

void Port_ZenkaiCellTransformEnd(void *object) {
    s32 i;
    if (!object) return;
    for (i = 0; i < ZENKAI_OBJECTS; i++) {
        if (sCellTransformActors[i] == object) {
            BObj *obj = object;
            /* A subsequent full form load has its own table: never restore a
             * pointer from the old model into that new form. */
            if (obj->mdl.anims[1] == sCellWeakened) obj->mdl.anims[1] = sCellOriginalIdle[i];
            sCellOriginalIdle[i] = NULL;
            sCellTransformActors[i] = NULL;
            sCellCinematic[i] = 0;
        }
    }
}

/* Presentation state is separate from the battle action: effects may see the
 * Ultimate cinematic without activating its damage, costs or prone recovery. */
void Port_ZenkaiCellTransformPresent(void *object, int on) {
    s32 i;
    if (!object) return;
    for (i = 0; i < ZENKAI_OBJECTS; i++) {
        if (sCellTransformActors[i] == object) sCellCinematic[i] = on != 0;
    }
}

int Port_ZenkaiCellTransformCinematic(void *object) {
    s32 i;
    if (!object) return 0;
    for (i = 0; i < ZENKAI_OBJECTS; i++) {
        if (sCellTransformActors[i] == object) return sCellCinematic[i];
    }
    return 0;
}

/* The Ultimate's paired event track drives its flashes, blur and sub-effects.
 * Read it without applying the victim's combat action or damage to the opponent. */
u64 Port_ZenkaiCellTransformVictimAttrs(void *object, int motion, float previous, float current) {
    u8 *data;
    u64 attrs = 0;
    s32 i;
    if (!Port_ZenkaiCellTransformCinematic(object) || motion < 0x160 || motion > 0x162) return 0;
    data = sCellVictim[motion - 0x160];
    if (!data) return 0;
    current += 1.0f / 60.0f;
    previous += 1.0f / 60.0f;
    if (current == previous) previous -= 1.0f;
    for (i = 0; i < data[1]; i++) {
        CellCinematicEvent *event = (CellCinematicEvent *)(data + 0x94) + i;
        if (current >= event->frame && (previous > 0 ? previous < event->frame : previous <= event->frame))
            attrs |= (u64)event->attrLo | ((u64)event->attrHi << 32);
    }
    return attrs;
}

static void bind_future_gohan_grab(BObj *obj, s32 chara, s32 type) {
    u32 *pack;
    s32 i;
    if (!PORT_ZENKAI_FUTURE_GOHAN_GRAB || !obj || type != 0 || chara != 159 ||
        !Port_ZenkaiPackEnabled() || Port_NetSession() || Port_NetActive()) return;
    if (!sFutureGohanGrab[0]) {
        pack = File_LoadSync(0x598 + 160 * 10, NULL, 0);
        if (!pack) return;
        if (pack[0] < 0x99) { Heap_Free(pack); return; }
        for (i = 0; i < 3; i++) {
            sFutureGohanGrab[i] = clone_entry(pack, 0x97 + i);
            if (!sFutureGohanGrab[i]) break;
        }
        Heap_Free(pack);
        if (i != 3) {
            for (i = 0; i < 3; i++) {
                if (sFutureGohanGrab[i]) Heap_Free(sFutureGohanGrab[i]);
                sFutureGohanGrab[i] = NULL;
            }
            return;
        }
    }
    for (i = 0; i < 3; i++) obj->mdl.anims[0x96 + i] = sFutureGohanGrab[i];
}

void Port_ZenkaiMovesBind(void *object, s32 id, s32 chara, s32 type) {
    BObj *obj = object;
    s32 i, language = -1;
    u16 *text;
    u8 *original;
    /* A model-only swap keeps the fighter object but rebuilds its animation
     * pointers. Reapply the cinematic motions to the temporary Semi-Perfect body. */
    if (type == 0 && chara == 106 && Port_ZenkaiCellTransformTemporary(obj)) {
        for (i = 0; i < ZENKAI_OBJECTS; i++)
            if (sCellTransformActors[i] == obj && obj->mdl.anims[1] != sCellWeakened)
                sCellOriginalIdle[i] = obj->mdl.anims[1];
        obj->mdl.anims[1] = sCellWeakened;
        for (i = 0; i < 3; i++) obj->mdl.anims[0x160 + i] = sCellTransform[i];
    }
    bind_future_gohan_grab(obj, chara, type);
    if (type == 0 && (chara == 30 || chara == 72 || chara == 122))
        Port_ZenkaiGreatApeMoonBind(obj, 1);
    if (!PORT_ZENKAI_SSJ2_MOVESET) return;
    original = obj->mdl.unk914;
    if (type != 0 || chara != 5 || id < 0 || id >= ZENKAI_OBJECTS || !Port_ZenkaiPackEnabled() ||
        Port_NetSession() || Port_NetActive() || original == NULL) return;
    if (((s16 *)(original + 0x18))[0] != 300 || ((s16 *)(original + 0x18))[2] != 602) return;
    if (!sMovesReady) {
        if (!load_move(0, 6, 2, 301) || !load_move(1, 4, 4, 603)) { clear_ssj2_moves(); return; }
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
