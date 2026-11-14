/* Invoked by zenkai_moves.py with extracted, user-owned character resources. */
#include "../../../src/battle/port_zenkai_moves.c"
#undef malloc
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static const char *directory;
static int active, online, allocations;
static SaveData save;
SaveData *gSaveData = &save;
int Port_ZenkaiEnabled(void) { return active; }
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
int main(int argc, char **argv) {
    BObjSlot target = {0}, ssj = {0}, ssj3 = {0};
    BObj obj, other;
    unsigned hashes[3];
    int i, language, base[3] = {0x590, 0x598, 0x599};
    u8 *original;
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
        memcpy(expected, original, SUPER_BYTES);
        copy_super(expected, sMoves[0].super, 0); copy_super(expected, sMoves[1].super, 2);
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
