#!/usr/bin/env python3
"""Exercise production pointer mapping when Android stretches the swapchain."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'port/src/gs/gs_input.c').read_text()
begin = source.index('int PortInput_MousePoint(')
end = source.index('\nstatic int pc_wheel_binding', begin)
harness = r'''
#include <assert.h>
#include <math.h>
#include <stddef.h>
typedef int SDL_Window;
#define SDL_WINDOW_INPUT_FOCUS 1
static int ww=2400,wh=1080,pw=2400,ph=1080,ow=2184,oh=1017;
static int gUiPresentX=0,gUiPresentY=0,gUiPresentW=2184,gUiPresentH=1017;
static SDL_Window window;
static SDL_Window *GsDraw_Window(void) { return &window; }
static int SDL_GetWindowFlags(SDL_Window *w) { return 1; }
static int SDL_GetWindowSize(SDL_Window *w,int *x,int *y) { *x=ww;*y=wh;return 1; }
static int SDL_GetWindowSizeInPixels(SDL_Window *w,int *x,int *y) { *x=pw;*y=ph;return 1; }
static int GsDraw_OutputSize(int *x,int *y) { *x=ow;*y=oh;return ow>0 && oh>0; }
''' + source[begin:end] + r'''
static void check(float x,float y,float expectedX,float expectedY) {
    float gx,gy;
    assert(PortInput_MousePoint(x,y,&gx,&gy));
    assert(fabsf(gx-expectedX)<.001f && fabsf(gy-expectedY)<.001f);
}
int main(void) {
    /* The old window-pixel conversion overshot most on player 2's side. */
    check(600,270,128,112);
    check(1800,810,384,336);
    check(2280,1026,486.4f,425.6f);
    /* Letterboxing is in swapchain pixels too. */
    gUiPresentX=200;gUiPresentW=1784;
    check(1200,540,256,224);
    check((200+1784*.9f)*2400/2184,540,460.8f,224);
    float x,y;
    assert(!PortInput_MousePoint(0,540,&x,&y));
    /* Desktop HiDPI and the pre-first-frame fallback retain their mapping. */
    ww=1200;wh=540;pw=ow=2400;ph=oh=1080;
    gUiPresentX=0;gUiPresentW=2400;gUiPresentH=1080;
    check(900,405,384,336);
    ow=oh=0;
    check(900,405,384,336);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'touch.c'
    path.write_text(harness)
    exe = Path(tmp) / 'touch'
    subprocess.run(['cc', '-std=gnu99', '-fsanitize=undefined', str(path), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Android stretched-swapchain pointer checks passed')
