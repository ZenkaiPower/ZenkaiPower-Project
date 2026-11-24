/* cc -std=gnu99 -O1 -ffunction-sections -fdata-sections -Iport/include -Iport/src \
 * port/tests/ui/soundtrack_audio.c port/src/plat_songs.c -Wl,--gc-sections -lSDL3 -lm -pthread -o /tmp/soundtrack-audio-test
 * /tmp/soundtrack-audio-test gamedata
 * Uses SDL's silent dummy device and the player's own extracted discs. */
#include <assert.h>
#include "../../src/gs/snd_adx.c"
#undef getenv
const char *Port_GetEnv(const char *name) { return getenv(name); }

int gPortAdxCount, gPortResim;
unsigned gGsFrame;
PortAdxView gPortAdxView[MAX_PLAYERS];
static int japanese;
int Port_ZenkaiEnabled(void) { return 1; }
int GsGpu_Enabled(void) { return 0; }
int Port_Setting(const char *name, int fallback) { return !strcmp(name, "game_version") ? !japanese : fallback; }
int Port_NetActive(void) { return 1; }
int Port_NetSession(void) { return 1; }
int Port_NetVoiceAlt(void) { return 0; }
int Port_SpeedMilli(void) { return 1000; }
float PortA11y_StreamGain(const void *player) { (void)player; return 1; }
int PortMa_Format(const char *path) { (void)path; return 0; }
int PortMa_Blanks(const char *path) { (void)path; return 0; }
void *PortMa_Start(const char *path, int loop) { (void)path; (void)loop; return NULL; }
void PortMa_Stop(void *p) { (void)p; }
void PortMa_SetVolume(void *p, float gain) { (void)p; (void)gain; }
void PortMa_SetPan(void *p, int a, int b) { (void)p; (void)a; (void)b; }
void PortMa_Pause(void *p, int paused) { (void)p; (void)paused; }
int PortMa_AtEnd(void *p) { (void)p; return 0; }

int main(int argc, char **argv) {
    Player *p;
    PortAdxView before;
    char original[512];
    unsigned usSamples;
    assert(argc == 2);
    setenv("ZENKAIPOWER_DATA", argv[1], 1);
    setenv("ZENKAIPOWER_SOUND", "1", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
    assert(PortSongs_SoundtrackAvailable());
    snprintf(original, sizeof(original), "%s/pzs3us2/64982.bin", argv[1]);
    p = ADXT_Create(2, NULL, 0);
    p->vol = -180; p->pan[0] = -7; p->pan[1] = 8; p->paused = 1;
    start(p, original);
    assert(p->stream && p->loop);
    usSamples = p->total;
    view_start(p, original, 1);
    gPortAdxView[0].vol = p->vol;
    gPortAdxView[0].pan[0] = p->pan[0]; gPortAdxView[0].pan[1] = p->pan[1];
    gPortAdxView[0].paused = p->paused;
    before = gPortAdxView[0];
    /* Also cover a switch before the first status query. */
    japanese = 1;
    gPortResim = 1;
    ADXT_GetStat(p);
    assert(!strcmp(p->path, original)); /* No side effects during replay. */
    gPortResim = 0;
    ADXT_GetStat(p);
    assert(strstr(p->path, "/soundtrack/meteor/08.adx"));
    assert(p->stream && p->loop && p->total != usSamples);
    assert(p->vol == -180 && p->pan[0] == -7 && p->pan[1] == 8 && p->paused);
    assert(!memcmp(&before, &gPortAdxView[0], sizeof(before)));
    japanese = 0;
    ADXT_GetStat(p);
    assert(!strcmp(p->path, original) && p->total == usSamples);
    assert(!memcmp(&before, &gPortAdxView[0], sizeof(before)));
    /* Fixed mission/course slots use the US movie's audio as looping battle music. */
    for (int slot = 20; slot <= 21; slot++) {
        snprintf(original, sizeof(original), "%s/pzs3us2/%05d.bin", argv[1], 64974 + slot);
        start(p, original);
        assert(p->stream && p->loop);
        assert(strstr(p->path, "/disc/DATA/ZS3USOP.ADX"));
    }
    /* Playing the opening normally keeps its original one-shot behavior. */
    snprintf(original, sizeof(original), "%s/disc/DATA/ZS3USOP.ADX", argv[1]);
    start(p, original);
    assert(p->stream && !p->loop);
    stop(p);
    SDL_CloseAudioDevice(sDevice);
    SDL_Quit();
    puts("soundtrack audio: live switch both ways, loops, pause/volume/pan, replay guard, and unchanged online state passed");
    return 0;
}
