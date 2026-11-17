/* Integration check: raw controller bytes through Pad_Update and game buttons. */
#include "../../../src/sys/pad.c"
#include "../../../src/sys/game_pad.c"
#include <assert.h>
Pad gPad[PAD_COUNT];
static BattleWork battle;
Progress *gProgress;
SaveData *gSaveData;
static unsigned char raw[32];
BattleWork *Battle_GetWork(void) { return &battle; }
s32 Battle_GetMode(void) { return 0; }
s32 BtlSeq_IsFighting(void) { return 1; }
s32 BtlScript_IsTextShown(void) { return 0; }
s32 scePad2GetState(s32 socket) { return PAD_STATE_STABLE; }
s32 scePad2Read(s32 socket,u8 *data) { int i; for(i=0;i<32;i++) data[i]=raw[i]; return 32; }
s32 scePad2GetButtonProfile(s32 socket,void *p) { return 0; }
s32 sceVibGetProfile(s32 socket,u8 *p) { return 0; }
s32 sceVibSetActParam(s32 s,s32 a,u8 *p,s32 b,void *d) { return 0; }
static void neutral(void) {
    int i;
    for(i=0;i<32;i++) raw[i]=0;
    raw[PAD_DATA_BTN_LO]=raw[PAD_DATA_BTN_HI]=255;
    raw[PAD_DATA_LX]=raw[PAD_DATA_LY]=raw[PAD_DATA_RX]=raw[PAD_DATA_RY]=128;
    Pad_Update();
}
int main(void) {
    static const int axes[]={PAD_DATA_LY,PAD_DATA_LY,PAD_DATA_LX,PAD_DATA_LX};
    static const int values[]={0,255,0,255};
    static const u32 bits[]={PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT};
    static const u32 game[]={PADG_UP,PADG_DOWN,PADG_LEFT,PADG_RIGHT};
    int i,p;
    for(p=0;p<PAD_COUNT;p++) gPad[p].phase=PAD_PHASE_READ;
    battle.running=1;
    for(i=0;i<4;i++) {
        neutral(); raw[axes[i]]=values[i]; Pad_Update();
        for(p=0;p<PAD_COUNT;p++) {
            assert(gPad[p].held & bits[i]);
            assert(gPad[p].pressed & bits[i]);
            assert(gPad[p].gamePressed & game[i]);
        }
        Pad_Update(); assert(!(gPad[0].pressed & bits[i]));
    }
    neutral(); raw[PAD_DATA_LX]=140; Pad_Update(); assert(!(gPad[0].held & 0xf0));
    neutral(); raw[PAD_DATA_RX]=0; Pad_Update(); assert(!(gPad[0].held & 0xf0));
    battle.running=0; neutral(); raw[PAD_DATA_LX]=0; Pad_Update();
    assert(!(gPad[0].held & PAD_LEFT));
    battle.running=1; battle.flags=BATTLE_FLAG_PAUSE; Pad_Update();
    assert(!(gPad[0].held & PAD_LEFT));
    battle.flags=0; Pad_Update(); assert(gPad[0].pressed & PAD_LEFT);
    /* A held physical D-pad direction is not pressed again by the stick. */
    neutral(); raw[PAD_DATA_BTN_LO]&=~PAD_LEFT; Pad_Update();
    raw[PAD_DATA_LX]=0; Pad_Update(); assert(!(gPad[0].pressed & PAD_LEFT));
    return 0;
}
