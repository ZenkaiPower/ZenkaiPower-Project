"""Exercise production Android Controller navigation, selector and save bytes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
header = (root / 'include/menu/port_keyboard_options.h').read_text()
controller = header[:header.index('static void Option_PcMouse(')] + '\n#endif\n'
source = r'''
#include <cassert>
#include <cstring>
typedef int s32; typedef float f32; typedef unsigned u32;
#include "menu/port_options_save.h"
#define OPT_ITEM_CTRL 4
#define OPT_ITEM_KEYS 17
#define OPT_ITEM_VIB 18
#define OPT_ITEM_CTRL_RESET 19
#define OPT_CLIP_BOTTOM 3
#define OPT_CLIP_ROW_CTRL 6
#define OPT_CLIP_PICK 8
#define PADG_UP 8
#define PADG_DOWN 4
#define PADG_LEFT 2
#define PADG_RIGHT 1
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define OPT_VOICE(n) ((void)0)
static struct {int state,cursor,dirty,picker,value,voiceLine,resetStep;void *flash;} option,*gOption=&option;
static struct {int gamePressed,gameRepeat;} gPad[1];
static int sOptionPcGamepad,enabled=1,clash,zenkai=1;
int Port_ZenkaiEnabled() {return zenkai;}
extern "C" int Port_VirtualGamepad() {return enabled;}
extern "C" void Port_VirtualGamepadApply(int value) {enabled=value;}
extern "C" int Port_ClashSquare() {return clash;}
extern "C" void Port_ClashSquareApply(int value) {clash=value;}
extern "C" int PortInput_PcCapturePoll() {return 0;}
extern "C" void PortInput_GamepadCaptureStart(int) {}
extern "C" void PortInput_PcCaptureStart(int) {}
void Option_UpdateReset() {}
void Option_PlateGoto(int,int,const char *) {}
void Snd_PlaySe(int,int) {}
void Flash_GotoLabel(void *,const char *,int) {}
''' + controller + r'''
static void input(int repeat,int pressed) {
    gPad[0].gameRepeat=repeat;gPad[0].gamePressed=pressed;Option_PcInput();
}
int main() {
    option.state=OPT_ITEM_CTRL;option.cursor=OPT_ITEM_VIB;
#ifdef __ANDROID__
    assert(sOptionControllerOrder[1]==PORT_GAMEPAD_CONTROLLER_ENTRY);
    input(PADG_DOWN,0);assert(option.cursor==PORT_GAMEPAD_CONTROLLER_ENTRY);
    input(PADG_DOWN,0);assert(option.cursor==PORT_CLASH_CONTROLLER_ENTRY);
    input(PADG_DOWN,0);assert(option.cursor==PORT_VIRTUAL_CONTROLLER_ENTRY);
    input(0,PADG_CROSS);assert(sOptionPcClashPicker && option.value==1);
    input(PADG_LEFT,0);assert(option.value==0 && enabled==1 && !option.dirty);
    input(0,PADG_TRIANGLE);assert(enabled==1 && !option.dirty && sOptionPcClashClosing);
    input(0,0);
    input(0,PADG_CROSS);input(PADG_RIGHT,0);input(0,PADG_CROSS);
    assert(enabled==0 && option.dirty && clash==0);
    input(0,0);option.dirty=0;
    input(0,PADG_CROSS);assert(option.value==0);
    input(PADG_RIGHT,0);input(0,PADG_CROSS);assert(enabled==1 && option.dirty);
    input(0,0);input(PADG_DOWN,0);assert(option.cursor==OPT_ITEM_CTRL_RESET);
    input(PADG_DOWN,0);assert(option.cursor==OPT_ITEM_VIB);
#else
    input(PADG_DOWN,0);assert(option.cursor==PORT_PC_CONTROLLER_ENTRY);
#endif
    unsigned char save[0x4000]={},payload[PORT_OPTIONS_FILE_BYTES],before[PORT_OPTIONS_FILE_BYTES];
    int values[PORT_OPTION_SETTING_COUNT],decoded[PORT_OPTION_SETTING_COUNT];
    for(int i=0;i<PORT_OPTION_SETTING_COUNT;i++) values[i]=Port_OptionsDescriptor(i)->defaultValue;
    Port_OptionsFileEncode(payload,save,values);
    assert(Port_OptionsFileVirtualGamepad(payload)==1); // old zeroed reserved byte
    Port_OptionsFileSetClashMode(payload,1);memcpy(before,payload,sizeof(payload));
    Port_OptionsFileSetVirtualGamepad(payload,0);
    assert(!Port_OptionsFileVirtualGamepad(payload) && Port_OptionsFileClashMode(payload)==1);
    assert(Port_OptionsFileDecode(payload,decoded));
    Port_OptionsFileSetVirtualGamepad(payload,1);
    assert(!memcmp(before,payload,sizeof(payload))); // unrelated data preserved
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'controller.cpp'
    path.write_text(source)
    for android in (False, True):
        exe = Path(tmp) / ('android' if android else 'desktop')
        subprocess.run(['c++', '-std=c++17', '-fsanitize=undefined',
                        *(['-D__ANDROID__'] if android else []),
                        '-I' + str(root / 'include'), '-I' + str(root / 'include/menu'),
                        str(path), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
print('Controller options OK: Android/desktop navigation, On/Off, cancel, save compatibility')
