"""Exercise production per-player capture, battle routing and defaults with SDL."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
s = (root / 'port/src/gs/gs_input.c').read_text()
code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <SDL3/SDL.h>
#include "menu/port_pc_bindings.h"
#define PORT_TRIGGER_AT 16000
#define PORT_MOUSE_GAME_FIGHT 1
#define PORT_MOUSE_GAME_PAUSED 2
#define B_SELECT 1
#define B_L3 2
#define B_R3 4
static pthread_mutex_t sPcLock=PTHREAD_MUTEX_INITIALIZER;
static unsigned sPcMouseHeld,sPcMousePressed,sPcMouseBlocked;
static int gPortInputCapture,sDeadzone[2][2],zenkai=1;
#define MAX_PADS 8
static int sPadSlot[2]={0,1};
static SDL_JoystickID sIds[MAX_PADS]={7};
static int sIdCount=1;
static struct { SDL_JoystickID id;SDL_Gamepad *pad; } sOpen[MAX_PADS]={{7,(SDL_Gamepad *)1}};
static pthread_once_t sLoadOnce=PTHREAD_ONCE_INIT;
static void load(void) {}
static void pads_refresh(void) {}
int Port_ZenkaiEnabled(void) {return zenkai;}
int Port_NetOptionsControlsEnabled(void) {return 0;}
unsigned Port_PcActionMask(int action) {return 1u<<(action+8);}
static int pc_wheel_binding(const SDL_Event *ev) {return ev->wheel.y>0 ? 6:7;}
static void stick_bytes(int dz,int x,int y,unsigned char *bx,unsigned char *by) {*bx=128;*by=128;}
static int heldButton=-1;
bool SDL_GetGamepadButton(SDL_Gamepad *g,SDL_GamepadButton b) {return b==heldButton;}
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *g,SDL_GamepadAxis a) {return 0;}
''' + s[s.index('#define SPCDEFAULTKEYS_INIT'):s.index('static unsigned keyboard_secondary_buttons')] + s[s.index('/* Run on the game thread after a completed capture'):s.index('/* ZENKAIPOWER_PAD=')] + r'''
int main(void) {
    int pc[2][PORT_PC_BINDINGS],pad[2][PORT_GAMEPAD_BINDINGS],out[PORT_PC_BINDINGS],padout[PORT_GAMEPAD_BINDINGS];
    PortInput_PlayerGet(0,pc[0],pad[0]);PortInput_PlayerGet(1,pc[1],pad[1]);
    assert(pc[0][0] && !pc[1][0] && pad[0][3]==pad[1][3]);
    SDL_Event ev={0};ev.type=SDL_EVENT_KEY_DOWN;ev.key.scancode=SDL_SCANCODE_Z;
    PortInput_PcCaptureStart(PORT_PC_STORAGE_ACTIONS+3);
    assert(PortInput_PcCaptureEvent(&ev) && PortInput_PcCapturePoll()==1);
    PortInput_PlayerGet(0,out,padout);assert(!memcmp(out,pc[0],sizeof(out)));
    PortInput_PlayerGet(1,out,padout);assert(out[3]==SDL_SCANCODE_Z);
    bool keys[SDL_SCANCODE_COUNT]={0};keys[SDL_SCANCODE_Z]=true;
    unsigned char lx=128,ly=128,rx=128,ry=128;int strike;
    assert(pc_battle_buttons(0,keys,&lx,&ly,&rx,&ry,&strike)==0 && !strike);
    assert(pc_battle_buttons(1,keys,&lx,&ly,&rx,&ry,&strike)==Port_PcActionMask(3) && strike);
    PortInput_GamepadCaptureStart(PORT_PC_STORAGE_ACTIONS+3);
    ev.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;ev.gbutton.button=SDL_GAMEPAD_BUTTON_NORTH;ev.gbutton.which=7;
    assert(PortInput_PcCaptureEvent(&ev));
    ev.type=SDL_EVENT_GAMEPAD_BUTTON_UP;assert(PortInput_PcCaptureEvent(&ev));
    assert(PortInput_PcCapturePoll()==1);
    assert(pad_of(1)==(SDL_Gamepad *)1 && pad_of(0)==NULL); /* first physical pad assigned to 2P */
    assert(PortInput_GamepadSlotsGet()==0x12);
    PortInput_GamepadSlotsSet(0);assert(sPadSlot[0]==0 && sPadSlot[1]==1); /* old save */
    PortInput_GamepadSlotsSet(0x12);assert(pad_of(1)==(SDL_Gamepad *)1 && pad_of(0)==NULL);
    PortInput_PlayerGet(0,out,padout);assert(!memcmp(padout,pad[0],sizeof(padout)));
    heldButton=SDL_GAMEPAD_BUTTON_NORTH;
    assert(!(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)&Port_PcActionMask(3)));
    assert(gamepad_battle_buttons((SDL_Gamepad *)1,1,&lx,&ly,&rx)&Port_PcActionMask(3));
    char name[128];PortInput_PcBindingName(PORT_PC_STORAGE_ACTIONS+3,name,sizeof(name));assert(!strcmp(name,"Z"));
    PortInput_GamepadBindingName(PORT_PC_STORAGE_ACTIONS+3,name,sizeof(name));assert(!strcmp(name,"Triangle"));
    ev.type=SDL_EVENT_KEY_DOWN;ev.key.scancode=SDL_SCANCODE_ESCAPE;
    PortInput_PcCaptureStart(PORT_PC_STORAGE_ACTIONS+3);PortInput_PcCaptureEvent(&ev);assert(PortInput_PcCapturePoll()==-1);
    PortInput_PlayerGet(1,out,padout);assert(out[3]==SDL_SCANCODE_Z);
    PortInput_PlayerDefaults(1,0);PortInput_PlayerGet(1,out,padout);assert(!out[3]);
    assert(padout[3]==SDL_GAMEPAD_BUTTON_NORTH+1); /* keyboard reset preserves pad */
    PortInput_PlayerDefaults(1,1);PortInput_PlayerGet(1,out,padout);assert(!memcmp(padout,pad[1],sizeof(padout)));
    PortInput_PlayerGet(0,out,padout);assert(!memcmp(out,pc[0],sizeof(out)) && !memcmp(padout,pad[0],sizeof(padout)));
    zenkai=0;PortInput_PcCaptureStart(PORT_PC_STORAGE_ACTIONS);assert(!PortInput_PcCapturing());
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='player-bindings-') as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(code)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl3'], text=True).split()
    subprocess.run(['cc', '-std=gnu11', '-Iinclude', '-fsanitize=undefined', str(path / 'test.c'),
                    '-o', str(path / 'test'), '-pthread', *flags], cwd=root, check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Per-player capture, keyboard/gamepad battle routing and defaults passed')
