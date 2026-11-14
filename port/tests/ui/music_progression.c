/* cc -std=gnu99 -O1 -w -DPORT -ffunction-sections -fdata-sections -Iinclude -Iport/include \
 * -include port_compat.h port/tests/ui/music_progression.c src/sys/save.c src/menu/menu_k_f.c \
 * -Wl,--gc-sections -o /tmp/music-progression-test
 * /tmp/music-progression-test <decoded-US-bracket-section-41.bin>
 * The fixture is extracted from the player's disc, never embedded in this test. */
#include "common.h"
#include "sys/save.h"
#include "menu/menu_l.h"
#include <assert.h>
#include <stdio.h>
extern int strcmp(const char *, const char *);

SaveData *gSaveData;
MenuProgress *gProgress;
LCommonRes *gCommonRes;
void Progress_ClearSession(void) {}
static int version, online;
int Port_Setting(const char *name, int fallback) { return !strcmp(name, "game_version") ? version : fallback; }
int Port_NetSession(void) { return online; }
u32 Rand_Range(u32 n) { return n ? n - 1 : 0; }

int main(int argc, char **argv) {
    SaveData save;
    LCommonRes common;
    struct { ItemFile header; ItemInfo items[SAVE_ITEM_COUNT]; } itemFile;
    LTourProgress progress;
    LBracket bracket;
    LTourPrize prizes[5];
    unsigned char raw[1024];
    s32 ids[25];
    int tour, level, i;
    FILE *fp;
    assert(argc == 2);
    fp = fopen(argv[1], "rb"); assert(fp && fread(raw, 1, sizeof(raw), fp) == sizeof(raw)); fclose(fp);
    memset(&save, 0, sizeof(save)); memset(&progress, 0, sizeof(progress)); memset(&bracket, 0, sizeof(bracket));
    memset(prizes, 0xFF, sizeof(prizes));
    for (tour = 0; tour < 5; tour++) for (level = 0; level < 3; level++) {
        memcpy(&prizes[tour].prize[6][level], raw + tour * 100 + 16 + 6 * 12 + level * 4, 4);
    }
    assert(prizes[0].prize[6][1] == 23 && prizes[1].prize[6][2] == 22);
    assert(prizes[2].prize[6][1] == 21 && prizes[3].prize[6][2] == 20);
    bracket.prize = prizes; gSaveData = &save; gProgress = (MenuProgress *)&progress;
    memset(&common, 0, sizeof(common)); memset(&itemFile, 0, sizeof(itemFile));
    itemFile.header.itemOffset = sizeof(ItemFile);
    common.data[2] = &itemFile; gCommonRes = &common;
    save.bgmBits = SAVE_METEOR_MUSIC_PROGRESS;
    Save_SetDefaults(&save);
    assert(!(save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS));
    assert((save.bgmBits & 0xFFFFF) == 0xFFFFF);
    save.bgmBits = 0xFFFFF; /* Includes the legacy USA Romance bit: it must not unlock METEOR. */
    save.unlockFlags = 127; /* Disable incidental dragon-ball rewards. */
    assert(Save_BuildMusicList(&save, ids, 0) == 24);
    assert(ids[7] == 25);
    for (i = 20; i < 24; i++) assert(ids[i] == 25);
    assert(Save_BuildMusicList(&save, ids, 1) == 25 && ids[24] == 24);
    version = 1;
    assert(Save_BuildMusicList(&save, ids, 0) == 20);
    for (i = 0; i < 20; i++) assert(ids[i] == i);
    Save_AwardMeteorMusic(&save, 7); assert(!(save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS));
    version = 0;
    Save_AwardMeteorMusic(&save, 7);
    assert(Save_BuildMusicList(&save, ids, 0) == 24 && ids[7] == 7);
    for (tour = 0; tour < 5; tour++) for (level = 0; level < 3; level++) {
        int slot = prizes[tour].prize[6][level];
        save.bgmBits = 0xFFFFF; progress.tour = tour; progress.level = level;
        Bracket_GivePrizes(&bracket, 1);
        assert(!(save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS)); /* Runner-up gets no song. */
        Bracket_GivePrizes(&bracket, 0);
        assert((save.bgmBits & 0xFFFFF) == 0xFFFFF && bracket.rewardCount == 0);
        Save_BuildMusicList(&save, ids, 0);
        for (i = 20; i < 24; i++) assert(ids[i] == (i == slot ? i : 25));
        assert(ids[7] == 25);
    }
    save.bgmBits = 0xFFFFF;
    Save_AwardMeteorMusic(&save, 7);
    for (i = 20; i < 24; i++) Save_AwardMeteorMusic(&save, i);
    fp = tmpfile(); assert(fp && fwrite(&save, 1, SAVE_SIZE, fp) == SAVE_SIZE);
    memset(&save, 0, sizeof(save)); rewind(fp); assert(fread(&save, 1, SAVE_SIZE, fp) == SAVE_SIZE); fclose(fp);
    for (i = 0; i < 12; i++) {
        version = i & 1;
        assert(Save_BuildMusicList(&save, ids, 0) == (version ? 20 : 24));
        assert((save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS) == SAVE_METEOR_MUSIC_PROGRESS);
    }
    version = 0;
    assert(Save_BuildMusicList(&save, ids, 0) == 24);
    for (i = 0; i < 24; i++) assert(ids[i] == i);
    online = 1;
    assert(Save_BuildMusicList(&save, ids, 1) == 21 && ids[20] == 24);
    save.bgmBits = 0xFFFFF; Save_AwardMeteorMusic(&save, 20);
    assert(!(save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS));
    online = 0;
    Save_UnlockAll(&save);
    assert((save.bgmBits & SAVE_METEOR_MUSIC_PROGRESS) == SAVE_METEOR_MUSIC_PROGRESS);
    puts("music progression: defaults, all tournament/difficulty rewards, runner-up rejection, ending award, mode isolation, save roundtrip, online catalog and cheat unlock passed");
    return 0;
}
