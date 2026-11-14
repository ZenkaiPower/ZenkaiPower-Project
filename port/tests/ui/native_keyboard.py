#!/usr/bin/env python3
"""Exercise the production SDL-to-game keyboard mailbox without building the game."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
s = (root / 'port/src/gs/gs_input.c').read_text()
start = s.index('static pthread_mutex_t sNativeKeyLock')
end = s.index('/* Mouse events belong', start)
code = r'''
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <SDL3/SDL.h>
#include "menu/port_native_keyboard.h"
int gPortOverlayOpen, gPortChatTyping, gPortInputCapture;
static int enabled=1, swallowed, clipboardReads;
char *SDL_GetClipboardText(void) {
 clipboardReads++;
 return SDL_strdup("Ab7!\n\tCd");
}
int Port_Setting(const char *name,int fallback) { return enabled; }
void PortInput_SwallowKeys(void) { swallowed++; }
SDL_Window *GsDraw_Window(void) { return NULL; }
''' + s[start:end] + r'''
int main(void) {
 SDL_Event ev={0};int i;
 Port_NativeKeyboardSet(1);assert(swallowed==1);
 Port_NativeKeyboardSet(1);assert(swallowed==1);
 assert(Port_NativeKeyboardPage()==1);
 ev.type=SDL_EVENT_KEY_DOWN;ev.key.scancode=SDL_SCANCODE_LSHIFT;ev.key.mod=SDL_KMOD_LSHIFT;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPage()==0);
 ev.type=SDL_EVENT_KEY_UP;ev.key.mod=0;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPage()==1);
 ev.key.mod=SDL_KMOD_CAPS;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPage()==0);
 ev.key.mod=SDL_KMOD_CAPS|SDL_KMOD_RSHIFT;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPage()==1);
 ev.key.mod=SDL_KMOD_LSHIFT|SDL_KMOD_RSHIFT;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPage()==0);
 ev.key.mod=0;
 ev.type=SDL_EVENT_TEXT_INPUT;ev.text.text="kIJ79 a!";
 native_keyboard_event(&ev,0);
 for(i=0;i<8;i++) assert(Port_NativeKeyboardPop()=="kIJ79 a!"[i]);
 assert(Port_NativeKeyboardPop()==0);
 ev.type=SDL_EVENT_KEY_DOWN;ev.key.scancode=SDL_SCANCODE_V;ev.key.mod=SDL_KMOD_LCTRL;
 native_keyboard_event(&ev,0);
 for(i=0;i<6;i++) assert(Port_NativeKeyboardPop()=="Ab7!Cd"[i]);
 assert(Port_NativeKeyboardPop()==0);assert(clipboardReads==1);
 ev.key.repeat=1;native_keyboard_event(&ev,0);assert(clipboardReads==1);
 ev.key.repeat=0;enabled=0;native_keyboard_event(&ev,0);assert(clipboardReads==1);enabled=1;
 gPortOverlayOpen=1;native_keyboard_event(&ev,0);assert(clipboardReads==1);gPortOverlayOpen=0;
 native_keyboard_event(&ev,1);assert(clipboardReads==1);
 ev.key.mod=0;
 ev.type=SDL_EVENT_KEY_DOWN;ev.key.scancode=SDL_SCANCODE_RETURN;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPop()==PORT_KEYBOARD_CONFIRM);
 ev.key.repeat=1;native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPop()==0);
 ev.key.scancode=SDL_SCANCODE_BACKSPACE;
 native_keyboard_event(&ev,0);assert(Port_NativeKeyboardPop()==PORT_KEYBOARD_DELETE);
 ev.type=SDL_EVENT_TEXT_INPUT;ev.text.text="old";native_keyboard_event(&ev,0);
 ev.type=SDL_EVENT_WINDOW_FOCUS_LOST;native_keyboard_event(&ev,0);
 assert(Port_NativeKeyboardPop()==0);
 ev.type=SDL_EVENT_TEXT_INPUT;ev.text.text="overlay";
 gPortOverlayOpen=1;native_keyboard_event(&ev,0);gPortOverlayOpen=0;
 assert(Port_NativeKeyboardPop()==0);
 native_keyboard_event(&ev,1);assert(Port_NativeKeyboardPop()==0);
 native_keyboard_event(&ev,0);enabled=0;assert(Port_NativeKeyboardPop()==0);assert(Port_NativeKeyboardPage()==-1);
 enabled=1;native_keyboard_event(&ev,0);Port_NativeKeyboardSet(0);
 Port_NativeKeyboardSet(1);assert(Port_NativeKeyboardPop()==0);
 ev.text.text="abcdefghijklmnopqrstuvwxyz";
 for(i=0;i<100;i++) native_keyboard_event(&ev,0);
 for(i=0;i<128;i++) assert(Port_NativeKeyboardPop()>0);
 assert(Port_NativeKeyboardPop()==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='native-keyboard-') as folder:
    folder=Path(folder)
    (folder/'test.c').write_text(code)
    flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True).split()
    subprocess.run(['cc','-std=c11','-Iinclude','-fsanitize=undefined',str(folder/'test.c'),
                    '-pthread',*flags,'-o',str(folder/'test')],cwd=root,check=True)
    subprocess.run([str(folder/'test')],check=True)
print('Native keyboard mailbox checks passed.')

menu = (root / 'src/menu/menu_z_d.c').read_text()
def extract(signature):
    begin=menu.index(signature)
    return menu[begin:menu.index('\n}\n',begin)+3]
begin=menu.index('char gDcPassKeys[')
end=menu.index('\n};',begin)+3
keys = '#include <assert.h>\n#include "menu/menu_z.h"\n' + menu[begin:end]
keys += extract('char DcPass_GetKey(') + extract('static s32 DcPass_NativeKey(')
keys += r'''
int main(void) {
 DcKeyPos key; int c;
 for(c='0';c<='9';c++) { assert(DcPass_NativeKey(c,&key));assert(DcPass_GetKey(&key)==c); }
 for(c='A';c<='Z';c++) { assert(DcPass_NativeKey(c,&key));assert(key.page==0); }
 for(c='a';c<='z';c++) { assert(DcPass_NativeKey(c,&key));assert(key.page==1); }
 assert(DcPass_NativeKey('@',&key));assert(DcPass_NativeKey('!',&key));
 assert(!DcPass_NativeKey(' ',&key));assert(!DcPass_NativeKey('~',&key));
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='native-password-') as folder:
    folder=Path(folder)
    (folder/'test.c').write_text(keys)
    subprocess.run(['cc','-std=c99','-fno-builtin','-Iinclude','-fsanitize=undefined',
                    str(folder/'test.c'),'-o',str(folder/'test')],cwd=root,check=True)
    subprocess.run([str(folder/'test')],check=True)
print('Native password character lookup checks passed, including all digits.')
