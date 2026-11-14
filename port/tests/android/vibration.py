"""Exercise native gameplay rumble gates and Android vibration lifecycle policy."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'port/src/gs/gs_input.c').read_text()


def function(name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    return source[start:source.index('\n}', start) + 2]


native = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
typedef uint64_t Uint64; typedef uint16_t Uint16;
typedef int SDL_Window;
static int sInit=1,gPortResim,gPortOverlayOpen,gPortInputCapture;
static int sVib[2][2]={{1,100},{1,100}};
static struct {int lastLarge,lastSmall;} sRumble[2];
static Uint64 sAndroidVibration,now=1000;
static int sVirtualGamepad=1,sAndroidTouchActive=1,online,me,focused=1;
#define RUMBLE_MS 250
#define SDL_WINDOW_INPUT_FOCUS 1
static Uint64 SDL_GetTicks(void) {return now;}
static int Port_NetActive(void) {return online;}
static int Port_NetMe(void) {return me;}
static SDL_Window *GsDraw_Window(void) {static SDL_Window window;return &window;}
static int SDL_GetWindowFlags(SDL_Window *w) {(void)w;return focused;}
static void rumble_stop(void) {}
static void *pad_of(int player) {(void)player;return NULL;}
static void rumble_send(int p,void *g,Uint16 low,Uint16 high,int why) {
    (void)p;(void)g;(void)low;(void)high;(void)why;
}
static int Port_VirtualGamepad(void) {return sVirtualGamepad;}
''' + '\n'.join(function(name) for name in (
    'Port_PadVibrate', 'Port_VirtualGamepadApply', 'Port_AndroidBattleActive',
    'Port_AndroidTouchControlsActive', 'Port_AndroidTouchVibration')) + r'''
int main(void) {
    Port_PadVibrate(0,0,127);assert(Port_AndroidTouchVibration()==255);
    Port_PadVibrate(0,0,0);assert(!Port_AndroidTouchVibration());
    Port_PadVibrate(0,1,0);assert(Port_AndroidTouchVibration()==255);
    sVib[0][1]=50;Port_PadVibrate(0,1,0);assert(Port_AndroidTouchVibration()==127);
    sVib[0][0]=0;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());
    sVib[0][0]=1;sVib[0][1]=100;
    Port_PadVibrate(0,1,127);Port_VirtualGamepadApply(0);assert(!Port_AndroidTouchVibration());
    Port_VirtualGamepadApply(1);assert(!Port_AndroidTouchVibration());
    Port_PadVibrate(0,1,127);sAndroidTouchActive=0;assert(!Port_AndroidTouchVibration());
    sAndroidTouchActive=1;now+=251;assert(!Port_AndroidTouchVibration());
    focused=0;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());focused=1;
    gPortOverlayOpen=1;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());gPortOverlayOpen=0;
    gPortInputCapture=1;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());gPortInputCapture=0;
    online=1;me=1;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());
    Port_PadVibrate(1,1,127);assert(Port_AndroidTouchVibration()==255);
    Port_PadVibrate(0,0,0);assert(Port_AndroidTouchVibration()==255); // remote side cannot stop local rumble
    Port_PadVibrate(1,0,0);assert(!Port_AndroidTouchVibration());
    online=0;Port_PadVibrate(1,1,127);assert(!Port_AndroidTouchVibration()); // CPU/local P2
    gPortResim=1;Port_PadVibrate(0,1,127);assert(!Port_AndroidTouchVibration());
    return 0;
}
'''

stubs = {
    'android/content/Context.java': '''package android.content;
public class Context {
 public static final String VIBRATOR_SERVICE="vibrator";
 public android.os.Vibrator vibrator=new android.os.Vibrator();
 public Object getSystemService(String service) {return vibrator;}
}''',
    'android/os/SystemClock.java': '''package android.os;
public class SystemClock {public static long now; public static long uptimeMillis() {return now;}}''',
    'android/os/VibrationEffect.java': '''package android.os;
public class VibrationEffect {
 public static final int DEFAULT_AMPLITUDE=-1;
 public long duration; public int amplitude;
 public static VibrationEffect createOneShot(long ms,int level) {
  VibrationEffect e=new VibrationEffect();e.duration=ms;e.amplitude=level;return e;
 }
}''',
    'android/os/Vibrator.java': '''package android.os;
public class Vibrator {
 public boolean available=true,control=true; public int calls,cancels;
 public VibrationEffect last;
 public boolean hasVibrator() {return available;}
 public boolean hasAmplitudeControl() {return control;}
 public void vibrate(VibrationEffect effect) {calls++;last=effect;}
 public void cancel() {cancels++;}
}''',
    'com/zenkaipower/android/VibrationCheck.java': '''package com.zenkaipower.android;
import android.content.Context; import android.os.*;
public class VibrationCheck {
 public static void main(String[] args) {
  Context c=new Context();DeviceVibration v=new DeviceVibration(c);
  v.update(0);assert c.vibrator.calls==0 && c.vibrator.cancels==0;
  v.update(120);assert c.vibrator.calls==1 && c.vibrator.last.amplitude==120;
  assert c.vibrator.last.duration==160;
  SystemClock.now=32;v.update(120);assert c.vibrator.calls==1;
  SystemClock.now=96;v.update(120);assert c.vibrator.calls==2;
  v.update(200);assert c.vibrator.calls==3 && c.vibrator.last.amplitude==200;
  v.update(0);v.update(0);assert c.vibrator.cancels==1;
  c.vibrator.control=false;v.update(100);assert c.vibrator.last.amplitude==-1;
  c.vibrator.available=false;v.update(255);assert c.vibrator.calls==4;
  c.vibrator=null;new DeviceVibration(c).update(255);
 }
}''',
}

with tempfile.TemporaryDirectory(prefix='android-vibration-') as temporary:
    root = Path(temporary)
    path = root / 'rumble.c'
    path.write_text(native)
    exe = root / 'rumble'
    subprocess.run(['cc', '-D__ANDROID__', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    for name, contents in stubs.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    production = ROOT / 'android/app/src/main/java/com/zenkaipower/android/DeviceVibration.java'
    subprocess.run(['javac', '-d', str(root), str(production),
                    *(str(root / name) for name in stubs)], check=True)
    subprocess.run(['java', '-ea', '-cp', str(root), 'com.zenkaipower.android.VibrationCheck'], check=True)
print('Android gameplay vibration gates, online ownership, expiry, strength and cancellation passed')
