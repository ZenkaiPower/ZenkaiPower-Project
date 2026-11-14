/* cc -std=gnu99 -fsanitize=undefined -ffunction-sections -fdata-sections \
 * $(pkg-config --cflags sdl3) -Iport/include port/tests/ui/game_mouse.c \
 * -Wl,--gc-sections -pthread -lm $(pkg-config --libs sdl3) -o /tmp/game_mouse_test
 * /tmp/game_mouse_test */
#include <assert.h>
#include "../../../port/src/gs/gs_input.c"
volatile int gUiPresentX=0, gUiPresentY=0, gUiPresentW=1000, gUiPresentH=700;
static int setting=1, focused=1;
static bool padButtons[SDL_GAMEPAD_BUTTON_COUNT];
static Sint16 padAxes[SDL_GAMEPAD_AXIS_COUNT];
bool SDL_GetGamepadButton(SDL_Gamepad *g,SDL_GamepadButton b) { return padButtons[b]; }
Sint16 SDL_GetGamepadAxis(SDL_Gamepad *g,SDL_GamepadAxis a) { return padAxes[a]; }
static SDL_MouseButtonFlags physicalMouse;
SDL_MouseButtonFlags SDL_GetMouseState(float *x,float *y) { return physicalMouse; }
static Uint64 now=1000;
static bool keyboard[SDL_SCANCODE_COUNT];
const bool *SDL_GetKeyboardState(int *count) {
    if (count) *count=SDL_SCANCODE_COUNT;
    return keyboard;
}
Uint64 SDL_GetTicks(void) { return now; }
int Port_Setting(const char *name,int fallback) {
    return strcmp(name,"mouse_support")==0 ? setting : fallback;
}
const char *Port_GetEnv(const char *name) { return NULL; }
static int testClash;
void *BtlChar_FindByObjId(int id) { return testClash ? (void *)1 : NULL; }
int BtlAct_GetCurrent(void *chr) { return 0x130; }
unsigned char gPad[0x380];
unsigned gGsFrame;
int gPortBootSkipMouse;
static int context=PORT_MOUSE_GAME_FIGHT;
int Port_GameMouseMode(void) { return context; }
static int zenkai;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_NetOptionsControlsEnabled(void) { return 0; }
unsigned Port_PcActionMask(int action) {
    static const unsigned masks[]={B_L2,B_L1,B_TRIANGLE,B_SQUARE,B_R2,B_R1,B_CIRCLE,B_CROSS};
    return masks[action];
}
int GsGpu_Enabled(void) { return 1; }
int Port_NetActive(void) { return 0; }
int Port_NetMe(void) { return 0; }
void Port_LagNote(int what,unsigned long long value) {}
void Port_UiNotice(const char *message) {}
SDL_Window *GsDraw_Window(void) { return (SDL_Window *)1; }
SDL_WindowFlags SDL_GetWindowFlags(SDL_Window *w) { return focused ? SDL_WINDOW_INPUT_FOCUS : 0; }
bool SDL_GetWindowSize(SDL_Window *w,int *x,int *y) { *x=1000; *y=700; return true; }
bool SDL_GetWindowSizeInPixels(SDL_Window *w,int *x,int *y) { return SDL_GetWindowSize(w,x,y); }
static void button(Uint32 type, Uint8 which, int consumed) {
    SDL_Event ev={0}; ev.type=type; ev.button.button=which;
    ev.button.x=500; ev.button.y=350;
    PortInput_MenuMouseEvent(&ev,consumed);
}
int main(void) {
    /* No menu is enabled. Held attacks and fast click/release still reach play. */
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_SQUARE);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_SQUARE);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==(B_SQUARE|B_CIRCLE));
    now += 249; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==(B_SQUARE|B_CIRCLE));
    now++; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==(B_SQUARE|B_CIRCLE));
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_CIRCLE);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0); /* release ends Circle */
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0);
    now += 100;
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_CIRCLE);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_SQUARE);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_LOADING)==0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_LOADING)==B_CROSS);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_LOADING)==0); /* mash, not auto-repeat */
    /* A held loading/menu button cannot attack or skip the next screen. */
    assert(game_mouse_buttons(PORT_MOUSE_GAME_SCENE)==0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_SCENE)==(B_CROSS|B_START));
    assert(game_mouse_buttons(PORT_MOUSE_GAME_SCENE)==0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==B_SQUARE);
    gPortOverlayOpen=1;
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    gPortOverlayOpen=0;
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0); /* release required */
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    gPortOverlayOpen=1;
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,1);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    gPortOverlayOpen=0;
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0);
    setting=0; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    setting=1; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT,0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0);
    gPortChatTyping=1; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    gPortChatTyping=0; assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT,0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0);
    { SDL_Event ev={0}; ev.type=SDL_EVENT_WINDOW_FOCUS_LOST; PortInput_MenuMouseEvent(&ev,0); }
    assert(game_mouse_buttons(PORT_MOUSE_GAME_FIGHT)==0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_DIALOG)==0);
    button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
    assert(game_mouse_buttons(PORT_MOUSE_GAME_DIALOG)==B_CROSS); /* no pause */
    assert(game_mouse_buttons(PORT_MOUSE_GAME_DIALOG)==0);
    button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
    { unsigned char data[18];
      context=PORT_MOUSE_GAME_OFF; Port_PadRead(0,data);
      button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
      context=PORT_MOUSE_GAME_FIGHT; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_SQUARE));
      button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,0); now+=250; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ (B_SQUARE|B_CIRCLE)));
      button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
      button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT,0); Port_PadRead(0,data);
      button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
      context=PORT_MOUSE_GAME_SCENE; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ (B_CROSS|B_START)));
      Port_PadRead(0,data); assert(data[0]==255 && data[1]==255);
      button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
      button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
      context=PORT_MOUSE_GAME_DIALOG; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_CROSS));
    }
    assert(kActions[4].key==SDL_SCANCODE_SPACE && kActions[8].key==SDL_SCANCODE_Q);
    assert(kActions[10].key==SDL_SCANCODE_LSHIFT && kActions[12].key==SDL_SCANCODE_LCTRL);
    assert(kActions[13].key==SDL_SCANCODE_LALT && kActions[15].key==0);
    { bool keys[SDL_SCANCODE_COUNT]={0};
      keys[SDL_SCANCODE_E]=true; assert(keyboard_secondary_buttons(keys)==B_L2);
      keys[SDL_SCANCODE_RCTRL]=true; assert(keyboard_secondary_buttons(keys)==(B_L2|B_L3));
      keys[SDL_SCANCODE_RALT]=true; assert(keyboard_secondary_buttons(keys)==(B_L2|B_L3|B_R3));
    }
    { unsigned char data[18];
      button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);
      context=PORT_MOUSE_GAME_OFF; Port_PadRead(0,data);
      keyboard[SDL_SCANCODE_SPACE]=true;
      keyboard[SDL_SCANCODE_LSHIFT]=true;
      keyboard[SDL_SCANCODE_Q]=true;
      keyboard[SDL_SCANCODE_LCTRL]=true;
      keyboard[SDL_SCANCODE_LALT]=true;
      Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ (B_L1|B_L2|B_L3|B_R3)));
      memset(keyboard,0,sizeof(keyboard)); keyboard[SDL_SCANCODE_E]=true;
      Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_L2));
      memset(keyboard,0,sizeof(keyboard)); keyboard[SDL_SCANCODE_RETURN]=true;
      Port_PadRead(0,data); /* Enter confirms in menus/pause context. */
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_CROSS));
      gPortBootSkipMouse=1;
      Port_PadRead(0,data); /* Enter skips boot logos and video. */
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_START));
      memset(keyboard,0,sizeof(keyboard)); keyboard[SDL_SCANCODE_ESCAPE]=true;
      Port_PadRead(0,data); /* Escape also skips boot logos and video. */
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_START));
      gPortBootSkipMouse=0;
      memset(keyboard,0,sizeof(keyboard)); keyboard[SDL_SCANCODE_RETURN]=true;
      Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_CROSS));
      memset(keyboard,0,sizeof(keyboard)); keyboard[SDL_SCANCODE_BACKSPACE]=true;
      Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0]) == (0xFFFFu ^ B_TRIANGLE));
      memset(keyboard,0,sizeof(keyboard));
    }
    /* Physical SDL state reaches battle without a menu event callback. */
    context=PORT_MOUSE_GAME_FIGHT; game_mouse_buttons(context);
    physicalMouse=SDL_BUTTON_LMASK; game_mouse_poll();
    assert(game_mouse_buttons(context)==B_SQUARE);
    { SDL_Event ev={0}; ev.type=SDL_EVENT_MOUSE_MOTION;
      PortInput_MenuMouseEvent(&ev,1); /* unrelated consumed UI event */
      assert(game_mouse_buttons(context)==B_SQUARE);
    }
    physicalMouse|=SDL_BUTTON_RMASK; game_mouse_poll(); now+=250;
    assert(game_mouse_buttons(context)==(B_SQUARE|B_CIRCLE));
    physicalMouse=0; game_mouse_poll(); game_mouse_buttons(context);
    physicalMouse=SDL_BUTTON_RMASK; game_mouse_poll(); now+=100;
    assert(game_mouse_buttons(context)==B_CIRCLE);
    physicalMouse=0; game_mouse_poll();
    assert(game_mouse_buttons(context)==0);
    physicalMouse=SDL_BUTTON_LMASK; game_mouse_poll();
    focused=0; game_mouse_poll(); assert(game_mouse_buttons(context)==0);
    focused=1; physicalMouse=0; game_mouse_poll();
    { unsigned char data[18];
      context=PORT_MOUSE_GAME_SCENE; Port_PadRead(0,data);
      physicalMouse=SDL_BUTTON_LMASK; game_mouse_poll(); Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ (B_CROSS|B_START)));
      physicalMouse=0; game_mouse_poll(); Port_PadRead(0,data);
      context=PORT_MOUSE_GAME_DIALOG; Port_PadRead(0,data);
      physicalMouse=SDL_BUTTON_LMASK; game_mouse_poll(); Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ B_CROSS));
      physicalMouse=0; game_mouse_poll();
    }
    { unsigned char data[18];
      physicalMouse=0; game_mouse_poll();
      memset(keyboard,0,sizeof(keyboard));
      keyboard[SDL_SCANCODE_ESCAPE]=true;
      context=PORT_MOUSE_GAME_OFF; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ B_TRIANGLE));
      context=PORT_MOUSE_GAME_FIGHT; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ B_START));
      context=PORT_MOUSE_GAME_PAUSED; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ B_START));
      gPortOverlayOpen=1; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
      gPortOverlayOpen=0; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
      keyboard[SDL_SCANCODE_ESCAPE]=false; Port_PadRead(0,data);
      keyboard[SDL_SCANCODE_ESCAPE]=true; Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu ^ B_START));
      keyboard[SDL_SCANCODE_ESCAPE]=false;
    }
    /* Native capture is transactional and cannot leak its key into menus. */
    { SDL_Event event={0};int before[PORT_PC_BINDINGS],after[PORT_PC_BINDINGS];unsigned char data[18];
      PortInput_PcGet(before);PortInput_PcCaptureStart(3);assert(!PortInput_PcCapturing());
      zenkai=1;PortInput_PcCaptureStart(3);
      assert(gPortInputCapture && PortInput_PcCapturing());
      event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_ESCAPE;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==-1 && !gPortInputCapture);
      PortInput_PcGet(after);assert(!memcmp(before,after,sizeof(before)));
      { const SDL_Scancode arrows[]={SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT};
        for(int i=0;i<4;i++) {
          PortInput_PcCaptureStart(3);event.key.scancode=arrows[i];
          assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
          PortInput_PcGet(after);assert(after[3]==arrows[i]);
        }
      }
      PortInput_PcCaptureStart(3);event.key.scancode=SDL_SCANCODE_N;keyboard[SDL_SCANCODE_N]=true;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
      PortInput_PcGet(after);assert(after[3]==SDL_SCANCODE_N && after[PORT_PC_STORAGE_ACTIONS+3]==0);
      context=PORT_MOUSE_GAME_FIGHT;zenkai=1;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
      keyboard[SDL_SCANCODE_N]=false;Port_PadRead(0,data);
      keyboard[SDL_SCANCODE_N]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_SQUARE));
      keyboard[SDL_SCANCODE_N]=false;keyboard[SDL_SCANCODE_J]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu); /* replaced key no longer attacks */
      keyboard[SDL_SCANCODE_J]=false;
      PortInput_PcCaptureStart(0);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_MIDDLE;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
      Port_PadRead(0,data);physicalMouse=SDL_BUTTON_MMASK;game_mouse_poll();Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_L2));
      physicalMouse=0;game_mouse_poll();
      PortInput_PcGet(after);after[12]=SDL_SCANCODE_LEFT;PortInput_PcSet(after);
      keyboard[SDL_SCANCODE_W]=keyboard[SDL_SCANCODE_LEFT]=true;Port_PadRead(0,data);
      assert(data[5]==0 && data[2]==0 && data[3]==128); /* horizontal camera only */
      memset(keyboard,0,sizeof(keyboard));keyboard[SDL_SCANCODE_UP]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu); /* no battle D-pad side effect */
      context=PORT_MOUSE_GAME_OFF;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_UP)); /* menus retain arrow navigation */
      memset(keyboard,0,sizeof(keyboard));PortInput_PcCaptureStart(PORT_PC_ACTIONS);
      assert(!PortInput_PcCapturing()); /* no Pause entry */
      context=PORT_MOUSE_GAME_FIGHT;
      PortInput_PcCaptureStart(3);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
      PortInput_PcGet(after);assert(after[3]==0 && after[PORT_PC_STORAGE_ACTIONS+3]==SDL_BUTTON_LEFT);
      Port_PadRead(0,data);
      button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,0);
      button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,0);Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_SQUARE));
      Port_PadRead(0,data);assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
      keyboard[SDL_SCANCODE_LALT]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^(B_L3|B_R3))); /* Switch Character is one input */
      memset(keyboard,0,sizeof(keyboard));keyboard[SDL_SCANCODE_LCTRL]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_R3)); /* Transform/Progress remains R3 */
      memset(keyboard,0,sizeof(keyboard));
      keyboard[SDL_SCANCODE_C]=keyboard[SDL_SCANCODE_LCTRL]=keyboard[SDL_SCANCODE_LALT]=true;Port_PadRead(0,data);
      assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^(B_SELECT|B_L3|B_R3)));
      memset(keyboard,0,sizeof(keyboard));
      PortInput_PcCaptureStart(0);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
      PortInput_PcGet(after);assert(after[PORT_PC_STORAGE_ACTIONS]==SDL_BUTTON_LEFT && after[PORT_PC_STORAGE_ACTIONS+3]==0);
      PortInput_PcCaptureStart(0);event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_BACKSPACE;
      assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
      PortInput_PcGet(after);assert(after[PORT_PC_STORAGE_ACTIONS]==0 && after[0]==0);
      PortInput_PcSet(before);zenkai=0;
    }
    {
        SDL_Event event={0};int before[PORT_GAMEPAD_BINDINGS],after[PORT_GAMEPAD_BINDINGS];
        unsigned char lx=128,ly=128,rx=128;
        PortInput_GamepadGet(before);zenkai=1;
        PortInput_GamepadCaptureStart(3);
        event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_N;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=1;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.button=SDL_GAMEPAD_BUTTON_EAST;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;
        assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
        PortInput_GamepadGet(after);assert(after[3]==SDL_GAMEPAD_BUTTON_EAST+1 && after[6]==0);
        padButtons[SDL_GAMEPAD_BUTTON_EAST]=true;
        assert(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)==B_SQUARE);
        padButtons[SDL_GAMEPAD_BUTTON_EAST]=false;
        PortInput_GamepadCaptureStart(0);event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;
        event.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;event.gaxis.value=30000;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gaxis.value=0;assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
        PortInput_GamepadGet(after);assert(after[0]==111 && after[4]==0);
        padAxes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER]=30000;
        assert(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)==B_L2);
        memset(padAxes,0,sizeof(padAxes));
        PortInput_GamepadSet(before);sDeadzone[0][0]=-1;
        padAxes[SDL_GAMEPAD_AXIS_LEFTX]=16384;
        assert(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)==0 && lx>128 && lx<255 && ly==128 && rx==128);
        padAxes[SDL_GAMEPAD_AXIS_LEFTX]=0;
        padButtons[SDL_GAMEPAD_BUTTON_LEFT_STICK]=true;
        assert(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)==0);
        padButtons[SDL_GAMEPAD_BUTTON_RIGHT_STICK]=true;
        assert(gamepad_battle_buttons((SDL_Gamepad *)1,0,&lx,&ly,&rx)==(B_L3|B_R3));
        padButtons[SDL_GAMEPAD_BUTTON_LEFT_STICK]=padButtons[SDL_GAMEPAD_BUTTON_RIGHT_STICK]=false;
        { char name[128];PortInput_GamepadBindingName(15,name,sizeof(name));assert(!strcmp(name,"L3 + R3"));
          PortInput_GamepadBindingName(14,name,sizeof(name));assert(!strcmp(name,"Select"));
          PortInput_GamepadBindingName(3,name,sizeof(name));assert(!strcmp(name,"Square"));
          PortInput_GamepadBindingName(0,name,sizeof(name));assert(!strcmp(name,"L2"));
          PortInput_GamepadBindingName(5,name,sizeof(name));assert(!strcmp(name,"R1")); }
        PortInput_GamepadCaptureStart(15);
        event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.button=SDL_GAMEPAD_BUTTON_RIGHT_STICK;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gbutton.button=SDL_GAMEPAD_BUTTON_LEFT_STICK;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gbutton.button=SDL_GAMEPAD_BUTTON_RIGHT_STICK;
        assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
        PortInput_GamepadGet(after);assert(!memcmp(before,after,sizeof(before)));
        PortInput_GamepadCaptureStart(0);
        event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.button=SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gbutton.button=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;event.gaxis.axis=SDL_GAMEPAD_AXIS_LEFT_TRIGGER;event.gaxis.value=30000;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gaxis.value=0;assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_GAMEPAD_BUTTON_UP;event.gbutton.button=SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.gbutton.button=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
        PortInput_GamepadGet(after);
        assert(after[0]==(PORT_GAMEPAD_CHORD | 10 | (11<<8) | (109<<16)));
        padButtons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=padButtons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=true;
        assert(!gamepad_held((SDL_Gamepad *)1,after[0]));
        padAxes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=30000;assert(gamepad_held((SDL_Gamepad *)1,after[0]));
        padAxes[SDL_GAMEPAD_AXIS_LEFT_TRIGGER]=0;
        padButtons[SDL_GAMEPAD_BUTTON_LEFT_SHOULDER]=padButtons[SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER]=false;
        PortInput_GamepadSet(before);
        PortInput_GamepadCaptureStart(0);event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;event.gbutton.button=SDL_GAMEPAD_BUTTON_NORTH;
        assert(PortInput_PcCaptureEvent(&event) && !PortInput_PcCapturePoll());
        event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_ESCAPE;
        assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==-1);
        PortInput_GamepadGet(after);assert(!memcmp(before,after,sizeof(before)));
        zenkai=0;PortInput_GamepadCaptureStart(0);assert(!PortInput_PcCapturing());
    }
    {
        int pc[PORT_PC_BINDINGS]={0},pad[PORT_GAMEPAD_BINDINGS]={0};
        PortInput_PcSet(pc);PortInput_GamepadSet(pad);zenkai=1;
        PortInput_PcDefaults();PortInput_PcGet(pc);PortInput_GamepadGet(pad);
        assert(pc[0]==SDL_SCANCODE_LSHIFT && pc[PORT_PC_STORAGE_ACTIONS+3]==SDL_BUTTON_LEFT);
        for(int i=0;i<PORT_GAMEPAD_BINDINGS;i++) assert(!pad[i]);
        pc[0]=SDL_SCANCODE_N;PortInput_PcSet(pc);
        PortInput_GamepadDefaults();PortInput_GamepadGet(pad);PortInput_PcGet(pc);
        assert(!memcmp(pad,sGamepadDefaults,sizeof(pad)) && pc[0]==SDL_SCANCODE_N);
        zenkai=0;PortInput_PcDefaults();PortInput_PcGet(pc);assert(pc[0]==SDL_SCANCODE_N);
    }
    {
        unsigned char data[18];zenkai=1;context=PORT_MOUSE_GAME_OFF;
        memset(keyboard,0,sizeof(keyboard));sPcRelease=sGamepadRelease=sKeySwallow=0;
        keyboard[SDL_SCANCODE_RETURN]=true;Port_PadRead(0,data);
        assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_CROSS));
        memset(keyboard,0,sizeof(keyboard));keyboard[SDL_SCANCODE_BACKSPACE]=true;Port_PadRead(0,data);
        assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_TRIANGLE));
        memset(keyboard,0,sizeof(keyboard));keyboard[SDL_SCANCODE_K]=keyboard[SDL_SCANCODE_I]=true;Port_PadRead(0,data);
        assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
        memset(keyboard,0,sizeof(keyboard));keyboard[SDL_SCANCODE_KP_ENTER]=true;Port_PadRead(0,data);
        assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_CROSS));
    }
    {
        SDL_Event event={0};int bindings[PORT_PC_BINDINGS];unsigned char data[18];char name[64];
        zenkai=1;context=PORT_MOUSE_GAME_FIGHT;memset(keyboard,0,sizeof(keyboard));
        sPcRelease=sGamepadRelease=sPcMouseHeld=sPcMousePressed=sPcMouseBlocked=0;
        PortInput_PcDefaults();PortInput_PcGet(bindings);
        assert(bindings[1]==SDL_SCANCODE_R && bindings[4]==SDL_SCANCODE_Q && bindings[5]==SDL_SCANCODE_E);
        assert(bindings[12]==SDL_SCANCODE_LEFT && bindings[13]==SDL_SCANCODE_RIGHT);
        assert(bindings[15]==SDL_SCANCODE_LALT && bindings[16]==SDL_SCANCODE_LCTRL);
        for(int direction=6;direction<=9;direction++) {
            PortInput_PcCaptureStart(3);event.type=SDL_EVENT_MOUSE_WHEEL;
            event.wheel.x=direction==8 ? -1:direction==9 ? 1:0;
            event.wheel.y=direction==6 ? 1:direction==7 ? -1:0;
            event.wheel.direction=SDL_MOUSEWHEEL_NORMAL;
            assert(PortInput_PcCaptureEvent(&event) && PortInput_PcCapturePoll()==1);
            PortInput_PcGet(bindings);assert(bindings[3]==0 && bindings[PORT_PC_STORAGE_ACTIONS+3]==direction);
            PortInput_PcBindingName(3,name,sizeof(name));assert(!strncmp(name,"Scroll ",7));
            Port_PadRead(0,data); /* leave the capture release guard */
            PortInput_MenuMouseEvent(&event,0);Port_PadRead(0,data);
            assert((((unsigned)data[1]<<8)|data[0])==(0xFFFFu^B_SQUARE));
            Port_PadRead(0,data);assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
            PortInput_MenuMouseEvent(&event,1);Port_PadRead(0,data);
            assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
        }
        event.wheel.direction=SDL_MOUSEWHEEL_FLIPPED;event.wheel.x=0;event.wheel.y=-1;
        assert(pc_wheel_binding(&event)==6);
    }
    {
        unsigned char data[18], lx, ly;
        unsigned buttons;
        zenkai=1;context=PORT_MOUSE_GAME_FIGHT;
        PortInput_PcDefaults();sPcKeys[3]=SDL_SCANCODE_N;
        sPcRelease=sGamepadRelease=sKeySwallow=sPcMouseHeld=sPcMousePressed=sPcMouseBlocked=0;
        memset(keyboard,0,sizeof(keyboard));physicalMouse=0;
        Port_ClashSquareApply(0);testClash=1;
        Port_PadRead(0,data); /* release any held Strike from preceding checks */
        keyboard[SDL_SCANCODE_N]=true;Port_PadRead(0,data);
        assert(data[4]!=0x80 || data[5]!=0x80); /* keyboard Strike becomes a stick direction */
        assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
        keyboard[SDL_SCANCODE_N]=false;Port_PadRead(0,data);
        assert(data[4]==0x80 && data[5]==0x80);
        physicalMouse=SDL_BUTTON_LMASK;game_mouse_poll();Port_PadRead(0,data);
        assert(data[4]!=0x80 || data[5]!=0x80); /* mouse Strike works in Rotate Stick mode too */
        assert((((unsigned)data[1]<<8)|data[0])==0xFFFFu);
        physicalMouse=0;game_mouse_poll();Port_PadRead(0,data);
        buttons=B_SQUARE;lx=0xFF;ly=0x80;
        clash_square(0,&buttons,&lx,&ly,0,B_SQUARE);
        assert(buttons==B_SQUARE && lx==0xFF && ly==0x80); /* gamepad still rotates */
        Port_ClashSquareApply(1);
        unsigned directions[5];
        for(int i=0;i<5;i++) {
            buttons=B_SQUARE;lx=ly=0x80;
            clash_square(0,&buttons,&lx,&ly,0,B_SQUARE);
            assert(!(buttons&B_SQUARE));
            directions[i]=((unsigned)lx<<8)|ly;
            assert(directions[i]!=0x8080u);
            buttons=B_SQUARE;lx=ly=0x80;
            clash_square(0,&buttons,&lx,&ly,0,B_SQUARE);
            assert(directions[i]==(((unsigned)lx<<8)|ly)); /* holding adds no direction */
            buttons=0;lx=ly=0x80;
            clash_square(0,&buttons,&lx,&ly,0,B_SQUARE);
            assert(lx==0x80 && ly==0x80); /* release adds no extra credit */
        }
        for(int i=1;i<4;i++) for(int j=0;j<i;j++) assert(directions[i]!=directions[j]);
        assert(directions[4]==directions[0]); /* four taps complete one turn */
        testClash=0;
    }
    puts("Game mouse and gamepad input checks passed");
    return 0;
}
