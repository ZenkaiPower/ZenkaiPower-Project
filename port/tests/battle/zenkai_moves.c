/* Invoked by zenkai_moves.py with extracted, user-owned character resources. */
#include "../../../src/battle/port_zenkai_moves.c"
#include "sys/bpe.h"
#undef malloc
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern s32 memcmp(const void *, const void *, u32);
static const char *directory;
static int active, online, allocations, moon, classic;
s32 BtlStage_HasMoon(void) {return moon;}
static SaveData save;
SaveData *gSaveData = &save;
int Port_ZenkaiEnabled(void) { return active; }
int Port_ZenkaiPackEnabled(void) { return Port_ZenkaiEnabled() && !classic; }
int Port_NetSession(void) { return online; }
int Port_NetActive(void) { return online; }
void *Heap_Alloc(s32 size, u32 align, s32 tail, s32 heap) {
    void *p; (void)align; (void)tail; (void)heap;
    p = malloc(size); assert(p); allocations++; return p;
}
void Heap_Free(void *p) { if (p) { allocations--; free(p); } }
void *File_LoadSync(s32 id, void *buf, s32 unused) {
    char path[1024]; FILE *f; long size; void *p;
    (void)buf; (void)unused;
    snprintf(path, sizeof(path), "%s/%d.bin", directory, id);
    f = fopen(path, "rb"); assert(f);
    fseek(f, 0, SEEK_END); size = ftell(f); rewind(f);
    p = Heap_Alloc(size, 0x40, 0, HEAP_ANY);
    assert(fread(p, 1, size, f) == (size_t)size); fclose(f); return p;
}
void *BObjFile_GetEntry(BObjFile *file, s32 entry) {
    u32 *p = file->buf;
    return p[entry] == p[entry + 1] ? NULL : (u8 *)p + (p[entry] & ~3u);
}
static unsigned hash(const u8 *p, size_t size) {
    unsigned h = 2166136261u;
    while (size--) h = (h ^ *p++) * 16777619u;
    return h;
}
static size_t pack_size(u32 *p) { return p[p[0] + 1] & ~3u; }
static void setup(BObj *obj, BObjSlot *slot, int language) {
    int i;
    memset(obj, 0, sizeof(*obj)); obj->slot = slot;
    obj->mdl.unk914 = BObjFile_GetEntry(&slot->file[0], 0x18);
    obj->mdl.unkA4 = BObjFile_GetEntry(&slot->file[0], 0x2C + language);
    for (i = 0; i < 0x19E; i++) obj->mdl.anims[i] = BObjFile_GetEntry(&slot->file[1], i + 1);
    for (i = 0; i < 5; i++) obj->mdl.unk84[i] = BObjFile_GetEntry(&slot->file[2], i + 1);
    for (i = 0; i < 100; i++) obj->mdl.lip[i] = BObjFile_GetEntry(&slot->file[0], 0x35 + ((save.flags & 1) ? 100 : 0) + i);
}
static void equal_span(const u16 *text, const u16 *source, int slot) {
    const u16 *a, *b, *c, *d;
    assert(move_span(text, slot, &a, &b) && move_span(source, slot, &c, &d));
    assert(b - a == d - c);
    while (a < b) assert(*a++ == *c++);
}
static void test_cell_transform(void) {
    BObj obj;
    BObjSlot donor = {0}, original = {0};
    int mode, i;
    unsigned sourceHash;
    donor.file[1].buf = File_LoadSync(0x598 + 106 * 10, NULL, 0);
    original.file[1].buf = File_LoadSync(0x598 + 107 * 10, NULL, 0);
    sourceHash = hash((u8 *)donor.file[1].buf, pack_size(donor.file[1].buf));
    for (mode = 0; mode < 4; mode++) {
        memset(&obj, 0, sizeof(obj));
        active = mode != 0; online = mode == 1; classic = mode == 2;
        obj.mdl.anims[1] = BObjFile_GetEntry(&original.file[1], 2);
        for (i = 0; i < 3; i++) obj.mdl.anims[0x160 + i] = BObjFile_GetEntry(&original.file[1], 0x161 + i);
        obj.mdl.anims[0x179] = BObjFile_GetEntry(&original.file[1], 0x17A);
        assert(!Port_ZenkaiCellTransformBind(&obj, 106, 107));
        assert(!Port_ZenkaiCellTransformBind(&obj, 107, 106));
        assert(Port_ZenkaiCellTransformBind(&obj, 107, 108) == (mode == 3 && PORT_ZENKAI_CELL_TRANSFORMATION));
        assert(Port_ZenkaiCellTransformTemporary(&obj) == (mode == 3 && PORT_ZENKAI_CELL_TRANSFORMATION));
        if (mode == 3 && PORT_ZENKAI_CELL_TRANSFORMATION) {
            int language, phase;
            BObjFile model = {0};
            int size, rawSize;
            u8 *raw = Bpe_Decode(obj.mdl.anims[1], NULL, &rawSize);
            u8 *expected = Bpe_Decode(BObjFile_GetEntry(&donor.file[1], 2), NULL, &size);
            assert(raw && expected && rawSize == size && !memcmp(raw, expected, size));
            Heap_Free(raw); Heap_Free(expected);
            /* Cancelling before the swap restores Perfect Form's own idle. */
            Port_ZenkaiCellTransformEnd(&obj);
            assert(obj.mdl.anims[1] == BObjFile_GetEntry(&original.file[1], 2));
            assert(Port_ZenkaiCellTransformBind(&obj, 107, 108));
            /* A full form reload must retain its new table on cleanup. */
            obj.mdl.anims[1] = BObjFile_GetEntry(&donor.file[1], 2);
            model.buf = File_LoadSync(0x590 + 106 * 10, NULL, 0);
            for (language = 0; language < 2; language++) {
                save.flags = language ? SAVE_FLAG_VOICE : 0;
                assert(Port_ZenkaiCellTransformPartner(language) == 281 + language);
                for (phase = 0; phase < 2; phase++) {
                    int voice = PORT_ZENKAI_VOICE_TAG | (106 << 16) | (0x5B + phase);
                    int entry = 0x35 + language * 100 + 0x5B + phase;
                    int size = (model.buf[entry + 1] & ~3u) - (model.buf[entry] & ~3u);
                    void *lip = Port_ZenkaiMoveLip(&obj, voice);
                    assert(lip && !memcmp(lip, BObjFile_GetEntry(&model, entry), size));
                }
                {
                    BObjFile prop = {0};
                    u8 *animation;
                    prop.buf = File_LoadSync(0xC1C + Port_ZenkaiCellTransformPartner(language), NULL, 0);
                    animation = BObjFile_GetEntry(&prop, 0x1D);
                    assert(animation && *(u16 *)(animation + 2) == 279);
                    Heap_Free(prop.buf);
                }
            }
            Heap_Free(model.buf);
            obj.mdl.anims[0x160] = obj.mdl.anims[0x161] = obj.mdl.anims[0x162] = NULL;
            Port_ZenkaiMovesBind(&obj, 0, 106, 0);
            assert(obj.mdl.anims[1] == sCellWeakened);
            for (i = 0; i < 3; i++) assert(obj.mdl.anims[0x160 + i] == sCellTransform[i]);
            assert(Port_ZenkaiCellTransformOpeningFrames() == 45);
            Port_ZenkaiCellTransformPresent(&obj, 1);
            assert(Port_ZenkaiCellTransformCinematic(&obj));
            assert(Port_ZenkaiMoveVoice(&obj, 4, 0x5B) == (PORT_ZENKAI_VOICE_TAG | (106 << 16) | 0x5B));
            assert(Port_ZenkaiCellTransformVictimAttrs(&obj, 0x161, 334, 336) & 0x1000);
            assert(!(Port_ZenkaiCellTransformVictimAttrs(&obj, 0x161, 336, 338) & 0x1000));
            assert(Port_ZenkaiCellTransformVictimAttrs(&obj, 0x162, 248, 250) & 0x4000);
        }
        for (i = 0; i < 3; i++) {
            void *expected = BObjFile_GetEntry(&donor.file[1], 0x161 + i);
            if (mode == 3 && PORT_ZENKAI_CELL_TRANSFORMATION) {
                int size, rawSize;
                u8 *raw = Bpe_Decode(obj.mdl.anims[0x160 + i], NULL, &rawSize);
                u8 *comparison = Bpe_Decode(expected, NULL, &size);
                assert(raw[1] > 0 && rawSize == size && rawSize <= 0xC000);
                assert(!memcmp(raw, comparison, size)); /* All pose tracks preserved. */
                Heap_Free(raw); Heap_Free(comparison);
            } else assert(obj.mdl.anims[0x160 + i] == BObjFile_GetEntry(&original.file[1], 0x161 + i));
        }
        assert(obj.mdl.anims[0x179] == BObjFile_GetEntry(&original.file[1], 0x17A));
        assert(sourceHash == hash((u8 *)donor.file[1].buf, pack_size(donor.file[1].buf)));
        Port_ZenkaiCellTransformEnd(&obj);
        assert(obj.mdl.anims[1] == BObjFile_GetEntry(
            mode == 3 && PORT_ZENKAI_CELL_TRANSFORMATION ? &donor.file[1] : &original.file[1], 2));
        assert(!Port_ZenkaiCellTransformTemporary(&obj));
        assert(!Port_ZenkaiMoveLip(&obj, PORT_ZENKAI_VOICE_TAG | (106 << 16) | 0x5B));
        Port_ZenkaiMovesClear();
        assert(!sCellTransform[0] && !sCellTransform[1] && !sCellTransform[2]);
        assert(!sCellWeakened);
        assert(!sCellVictim[0] && !sCellVictim[1] && !sCellVictim[2]);
        assert(!Port_ZenkaiCellTransformCinematic(&obj));
    }
    classic = online = active = 0;
    Heap_Free(donor.file[1].buf); Heap_Free(original.file[1].buf);
    assert(!allocations);
    puts("Cell transformation asset checks passed (gates, complete motions, paired effects and cleanup)");
}

static void test_great_ape_moon(void) {
    BObj obj;
    BObjSlot source = {0};
    int form, i, mode;
    for (form = 0; form < 2; form++) {
        source.file[0].buf = File_LoadSync(0x590 + (80 + form) * 10, NULL, 0);
        source.file[1].buf = File_LoadSync(0x598 + (80 + form) * 10, NULL, 0);
        for (mode = 0; mode < 4; mode++) {
            memset(&obj, 0, sizeof(obj));
            active = mode != 0; online = mode == 1; moon = mode != 2;
            assert(Port_ZenkaiGreatApeMoonBind(&obj, form) == (mode == 3 && PORT_ZENKAI_GREAT_APE_MOON));
            {
                int entry=0x25+form*2;
                u32 *pack=source.file[0].buf;
                void *original=BObjFile_GetEntry(&source.file[0],entry);
                int size=(pack[entry+1]&~3u)-(pack[entry]&~3u);
                if (mode==3 && PORT_ZENKAI_GREAT_APE_MOON) {
                    assert(obj.mdl.unk8F0[form*2]!=original);
                    assert(!memcmp(obj.mdl.unk8F0[form*2],original,size));
                } else assert(!obj.mdl.unk8F0[form*2]);
                assert(!obj.mdl.unk8F0[1]);
                assert(!obj.mdl.unk8F0[2-form*2]);
            }
            for (i = 0; i < 3; i++) {
                u32 *pack = source.file[1].buf;
                void *original = BObjFile_GetEntry(&source.file[1], 0x178 + i);
                int size = (pack[0x179 + i] & ~3u) - (pack[0x178 + i] & ~3u);
                if (mode == 3 && PORT_ZENKAI_GREAT_APE_MOON) {
                    assert(obj.mdl.anims[0x177 + i] != original);
                    assert(!memcmp(obj.mdl.anims[0x177 + i], original, size));
                } else assert(!obj.mdl.anims[0x177 + i]);
            }
        }
        Heap_Free(source.file[0].buf);
        Heap_Free(source.file[1].buf);
    }
    {
        static const int ids[] = {30,72,122,12,74,76,78,81};
        active=moon=1;online=0;
        for (i=0;i<8;i++) {
            memset(&obj,0,sizeof(obj));
            Port_ZenkaiMovesBind(&obj,0,ids[i],0);
            assert((obj.mdl.anims[0x179]!=NULL)==(i<3 && PORT_ZENKAI_GREAT_APE_MOON));
        }
    }
    Port_ZenkaiMovesClear();assert(allocations == 0);
    active=online=moon=0;
    puts("PASS: Nappa Great Ape motions and moon-framing cameras, mode gates and cleanup.");
}

static void test_future_gohan_grab(void) {
    BObjSlot base = {0}, ssj = {0};
    BObj obj, other;
    int i, mode;
    unsigned originalHash;
    for (i=0;i<3;i++) {
        int kind = i==0 ? 0x590 : (i==1 ? 0x598 : 0x599);
        base.file[i].buf=File_LoadSync(kind+159*10,NULL,0);
        ssj.file[i].buf=File_LoadSync(kind+160*10,NULL,0);
    }
    originalHash=hash((u8 *)base.file[1].buf,pack_size(base.file[1].buf));
    for (mode=0;mode<5;mode++) {
        setup(&obj,&base,1);
        active=mode!=0; online=mode==1;
        Port_ZenkaiMovesBind(&obj,0,mode==2 ? 160 : 159,mode==3 ? 1 : 0);
        for (i=0;i<0x19E;i++) {
            void *original=BObjFile_GetEntry(&base.file[1],i+1);
            if (mode==4 && PORT_ZENKAI_FUTURE_GOHAN_GRAB && i>=0x96 && i<=0x98) {
                u32 *pack=ssj.file[1].buf;
                int size=(pack[i+2]&~3u)-(pack[i+1]&~3u);
                assert(obj.mdl.anims[i]!=original);
                assert(!memcmp(obj.mdl.anims[i],BObjFile_GetEntry(&ssj.file[1],i+1),size));
            } else assert(obj.mdl.anims[i]==original);
        }
        assert(obj.mdl.unk914==BObjFile_GetEntry(&base.file[0],0x18));
    }
    setup(&other,&base,1);
    Port_ZenkaiMovesBind(&other,1,159,0);
    if (PORT_ZENKAI_FUTURE_GOHAN_GRAB) {
        for (i=0x96;i<=0x98;i++) assert(other.mdl.anims[i]==obj.mdl.anims[i]);
    }
    assert(hash((u8 *)base.file[1].buf,pack_size(base.file[1].buf))==originalHash);
    Port_ZenkaiMovesClear();
    for (i=0;i<3;i++) {Heap_Free(base.file[i].buf);Heap_Free(ssj.file[i].buf);}
    assert(allocations==0);
    puts("PASS: Future Gohan grab donor, paired victim/recovery, mode gates and cleanup.");
}

int main(int argc, char **argv) {
    BObjSlot target = {0}, ssj = {0}, ssj3 = {0};
    BObj obj, other;
    unsigned hashes[3];
    int i, language, base[3] = {0x590, 0x598, 0x599};
    u8 *original;
    assert(argc == 2); directory = argv[1];
    test_cell_transform();
    test_great_ape_moon();
    test_future_gohan_grab();
    active=online=0;
    if (!PORT_ZENKAI_SSJ2_MOVESET) {
        active = 1;
        Port_ZenkaiMovesBind(NULL, 0, 5, 0);
        assert(!sMovesReady && allocations == 0);
        assert(Port_ZenkaiMoveVoice(NULL, 2, 0x57) == 0x57);
        assert(Port_ZenkaiMoveVoice(NULL, 4, 0x5B) == 0x5B);
        assert(Port_ZenkaiMoveLip(NULL, PORT_ZENKAI_VOICE_TAG | (4 << 16) | 0x5B) == NULL);
        puts("Goku SSJ2 override disabled: original moves/voices retained, no donor allocations");
        return 0;
    }
    assert(argc == 2); directory = argv[1];
    for (i = 0; i < 3; i++) {
        target.file[i].buf = File_LoadSync(base[i] + 50, NULL, 0);
        ssj.file[i].buf = File_LoadSync(base[i] + 40, NULL, 0);
        ssj3.file[i].buf = File_LoadSync(base[i] + 60, NULL, 0);
        hashes[i] = hash((u8 *)target.file[i].buf, pack_size(target.file[i].buf));
    }
    setup(&obj, &target, 1); original = obj.mdl.unk914;
    Port_ZenkaiMovesBind(&obj, 0, 5, 0);
    assert(!sMovesReady && obj.mdl.unk914 == original); /* ordinary save */
    active = 1; online = 1; Port_ZenkaiMovesBind(&obj, 0, 5, 0);
    assert(!sMovesReady && obj.mdl.unk914 == original);
    online = 0; Port_ZenkaiMovesBind(&obj, 0, 6, 0);
    assert(!sMovesReady && obj.mdl.unk914 == original); /* other character */
    Port_ZenkaiMovesBind(&obj, 0, 5, 1);
    assert(!sMovesReady && obj.mdl.unk914 == original); /* non-fighter model */
    for (language = 0; language < TEXT_LANGUAGES; language++) {
        u8 *params, expected[SUPER_BYTES];
        const u16 *oldText;
        setup(&obj, &target, language); oldText = obj.mdl.unkA4;
        if (!oldText) continue;
        Port_ZenkaiMovesBind(&obj, 0, 5, 0);
        assert(sMovesReady && obj.mdl.unk914 == sSuper[0]);
        params = obj.mdl.unk914;
        assert(((s16 *)(params + 0x18))[0] == 301);
        assert(((s16 *)(params + 0x18))[1] == 485); /* Meteor Blast retained */
        assert(((s16 *)(params + 0x18))[2] == 603);
        assert(params[0x140] == 9); /* Instant Kamehameha's attack handler */
        assert(((s32 *)(params + 0x19C))[2] == 100000); /* donor's ultimate cost */
        assert(((s32 *)(params + 0x184))[0] == 7460); /* SSJ2's first Blast 2 damage */
        assert(((s32 *)(params + 0x184))[1] == 7270); /* Meteor Blast unchanged */
        assert(((s32 *)(params + 0x184))[2] == 13640); /* SSJ2's Ultimate damage */
        memcpy(expected, original, SUPER_BYTES);
        copy_super(expected, sMoves[0].super, 0); copy_super(expected, sMoves[1].super, 2);
        memcpy(expected + 0x184, original + 0x184, 3 * sizeof(s32));
        for (i = 0; i < SUPER_BYTES; i++) assert(params[i] == expected[i]);
        for (i = 0; i < 5; i++) {
            BObjSlot *donor = i == 2 ? &ssj3 : i == 4 ? &ssj : &target;
            equal_span(obj.mdl.unkA4, BObjFile_GetEntry(&donor->file[0], 0x2C + language), i);
        }
        for (i = 0; i < 5; i++) {
            BObjSlot *donor = i == 2 ? &ssj3 : i == 4 ? &ssj : &target;
            u32 *pack = donor->file[2].buf;
            void *source = BObjFile_GetEntry(&donor->file[2], i + 1);
            if (!source) assert(!obj.mdl.unk84[i]);
            else assert(hash(source, (pack[i + 2] & ~3u) - (pack[i + 1] & ~3u)) ==
                        hash(obj.mdl.unk84[i], (pack[i + 2] & ~3u) - (pack[i + 1] & ~3u)));
        }
        for (i = 0; i < 0x19E; i++) {
            BObjSlot *donor = i >= 0x105 && i < 0x127 ? &ssj3 : i >= 0x149 && i < 0x16B ? &ssj : &target;
            u32 *pack = donor->file[1].buf;
            void *source = BObjFile_GetEntry(&donor->file[1], i + 1);
            if (!source) assert(!obj.mdl.anims[i]);
            else assert(hash(source, (pack[i + 2] & ~3u) - (pack[i + 1] & ~3u)) ==
                        hash(obj.mdl.anims[i], (pack[i + 2] & ~3u) - (pack[i + 1] & ~3u)));
        }
    }
    /* Both charge/fire cues, independently of text and voice language. */
    for (language = 0; language < 2; language++) {
        int phase;
        save.flags = language ? SAVE_FLAG_VOICE : 0;
        setup(&obj, &target, 1); Port_ZenkaiMovesBind(&obj, 0, 5, 0);
        for (phase = 0; phase < 2; phase++) {
            int voice = Port_ZenkaiMoveVoice(&obj, 2, 0x57 + phase);
            int entry = 0x35 + language * 100 + 0x5B + phase;
            u32 *pack = ssj.file[0].buf;
            void *lip;
            assert(voice == 0x5B + phase); /* SSJ2's original Ultimate dialogue */
            assert(Port_ZenkaiMoveLip(&obj, voice) == NULL); /* keeps SSJ2's own lip track */
            assert(obj.mdl.lip[voice] == BObjFile_GetEntry(&target.file[0], entry));
            voice = Port_ZenkaiMoveVoice(&obj, 4, 0x5B + phase);
            assert((voice & PORT_ZENKAI_VOICE_TAG) && ((voice >> 16) & 255) == 4);
            assert((voice & 0xFFFF) == 0x5B + phase); /* SSJ1 Instant Kamehameha */
            lip = Port_ZenkaiMoveLip(&obj, voice);
            assert(lip && hash(lip, (pack[entry + 1] & ~3u) - (pack[entry] & ~3u)) ==
                   hash(BObjFile_GetEntry(&ssj.file[0], entry), (pack[entry + 1] & ~3u) - (pack[entry] & ~3u)));
        }
        assert(Port_ZenkaiMoveVoice(&obj, 3, 0x59) == 0x59); /* Meteor Blast */
        assert(Port_ZenkaiMoveVoice(&obj, 0, 0x53) == 0x53); /* Blast 1 */
        active = 0; assert(Port_ZenkaiMoveVoice(&obj, 2, 0x57) == 0x57); active = 1;
        online = 1; assert(Port_ZenkaiMoveVoice(&obj, 4, 0x5B) == 0x5B); online = 0;
    }
    setup(&other, &target, 1);
    assert(Port_ZenkaiMoveVoice(&other, 2, 0x57) == 0x57); /* only bound replacement moves */
    Port_ZenkaiMovesBind(&other, 1, 5, 0);
    assert(other.mdl.unk914 == sSuper[1] && other.mdl.unk914 != obj.mdl.unk914);
    for (i = 0; i < 3; i++) assert(hashes[i] == hash((u8 *)target.file[i].buf, pack_size(target.file[i].buf)));
    Port_ZenkaiMovesClear(); assert(!sMovesReady);
    for (i = 0; i < 3; i++) { Heap_Free(target.file[i].buf); Heap_Free(ssj.file[i].buf); Heap_Free(ssj3.file[i].buf); }
    assert(allocations == 0);
    puts("ZENKAI Goku SSJ2 checks passed (nine text languages, both voice languages)");
    return 0;
}
