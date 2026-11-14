/* cc -std=gnu99 -fno-builtin -ffunction-sections -fdata-sections -Iinclude -Iport/include \
 * -include port_compat.h port/tests/ui/game_mouse_context.c -Wl,--gc-sections -o /tmp/game_mouse_context
 * /tmp/game_mouse_context */
#include "../../../src/sys/pad.c"
extern void abort(void);
#define CHECK(x) do { if (!(x)) abort(); } while (0)
static SaveData save;
SaveData *gSaveData=&save;
static BattleWork battle;
static Progress progress;
Progress *gProgress=&progress;
static int text, fighting, mode, event;
s32 Battle_GetMode(void) { return mode; }
s32 BtlScript_IsEventRunning(void) { return event; }
BattleWork *Battle_GetWork(void) { return &battle; }
s32 BtlScript_IsTextShown(void) { return text; }
s32 BtlSeq_IsFighting(void) { return fighting; }
int main(void) {
    { const int keys[]={2,1,0,3,4,5,6,7};
      for(int i=0;i<8;i++) save.key[0][i]=keys[i];
      CHECK(Port_PcActionMask(0)==0x100 && Port_PcActionMask(3)==0x8000);
      save.key[0][2]=3;save.key[0][3]=0;
      CHECK(Port_PcActionMask(3)==0x1000 && Port_PcActionMask(2)==0x8000);
    }
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_OFF);
    gPortBootSkipMouse=1;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_SCENE);
    CHECK(Port_GameMouseMap(Port_GameMouseMode(), PORT_MOUSE_LEFT, PORT_MOUSE_LEFT)==0x4008u);
    CHECK(Port_GameMouseMap(Port_GameMouseMode(), PORT_MOUSE_LEFT, 0)==0);
    gPortBootSkipMouse=0;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_OFF);
    progress.flags=PROGRESS_FLAG_LOADING;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_LOADING);
    progress.flags=0; battle.running=1;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT);
    battle.flags=BATTLE_FLAG_DEMO;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_SCENE);
    battle.flags=0; text=1;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_DIALOG);
    fighting=1;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT); /* speech/subtitles during combat */
    mode=7;CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_SCENE); /* title attract match, even during combat */
    mode=1; event=1;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT); /* event task can persist in combat */
    battle.flags=BATTLE_FLAG_DEMO;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_SCENE);
    battle.flags=0;
    mode=0;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT);
    mode=1; event=0;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT); /* ordinary Story speech */
    text=0;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_FIGHT);
    battle.flags=BATTLE_FLAG_PAUSE | BATTLE_FLAG_PAUSE_MENU;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_PAUSED);
    battle.flags=BATTLE_FLAG_LOADING;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_OFF);
    progress.flags=PROGRESS_FLAG_LOADING;
    CHECK(Port_GameMouseMode()==PORT_MOUSE_GAME_LOADING);
    return 0;
}
