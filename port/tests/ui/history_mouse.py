#!/usr/bin/env python3
"""Compile the production history mouse handlers in isolation; no game build."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
def handler(path, name):
    text = (root / path).read_text()
    start = text.index('static void ' + name + '(void) {')
    end = text.index('\n}\n', start) + 3
    return text[start:end]

harness = r'''
#include "common.h"
#include "menu/menu_c.h"
#include "menu/menu_b.h"
#include "sys/pad.h"
#include "menu/port_selection_mouse.h"
extern void abort(void);
extern int strcmp(const char *, const char *);
#define CHECK(x) do { if (!(x)) abort(); } while (0)
Pad gPad[PAD_COUNT];
HistSel *gHistSel;
ModeMenu *gModeMenu;
static PortMenuMouse pending;
static const char *target;
static int voices, sounds, images, highlights;
void PortMenuMouse_Read(PortMenuMouse *out) { *out = pending; }
int Port_FlashMenuMouseHit(int scope, const char *name, float w, float h, float pivot, float x, float y) {
    CHECK((scope == 5 || scope == 6) && pivot == 0);
    return target && strcmp(target, name) == 0;
}
void HistSel_PlateGoto(s32 a,s32 b,char *label) { highlights++; }
void HistSel_PlayVoice(void) { voices++; }
s32 Snd_PlaySe(u32 a,s32 b) { sounds++; return 0; }
void ModeMenu_PlateGoto(s32 a,s32 b,char *label) { highlights++; }
void ModeMenu_ChangeImage(void) { images++; }
void Voice_PlayWithSubtitle(void *a,s32 b,s32 c) { voices++; }
#define MODEMENU_MOVED() ModeMenu_PlateGoto(0,0,"fl_on_start"); ModeMenu_ChangeImage(); Snd_PlaySe(1,0)
'''
harness += handler('src/menu/menu_b_d.c', 'HistSel_MouseInput')
harness += handler('src/menu/menu_a_d.c', 'ModeMenu_MouseInput')
harness += handler('src/menu/menu_a_d.c', 'ModeMenu_MouseScriptInput')
harness += r'''
static void clear(void) { memset(gPad,0,sizeof(gPad)); memset(&pending,0,sizeof(pending)); target=0; }
int main(void) {
    HistSel saga = {0}; ModeMenu episode = {0};
    gHistSel=&saga; gModeMenu=&episode;
    pending.moved=1; target="mc_menu_plate_2";
    HistSel_MouseInput(); CHECK(saga.cursor[0]==1 && voices==1 && highlights==2);
    HistSel_MouseInput(); CHECK(voices==1 && highlights==2); /* stable hover */
    pending.confirm=1; HistSel_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
    clear(); pending.wheel=1; pending.moved=1; target="mc_menu_plate_1";
    HistSel_MouseInput(); CHECK(saga.cursor[0]==1 && (gPad[0].gameRepeat & PADG_DOWN));
    clear(); saga.focus=2; pending.moved=1; target="mc_level_plate_3";
    HistSel_MouseInput(); CHECK(saga.cursor[2]==2 && saga.voiceLine==9);
    clear(); saga.focus=1; pending.confirm=1; target="mc_scenario_1";
    HistSel_MouseInput(); CHECK((gPad[0].gameRepeat & PADG_LEFT) && !gPad[0].gamePressed);
    clear(); pending.confirm=1; target="mc_scenario_2";
    HistSel_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
    clear(); pending.cancel=1; HistSel_MouseInput(); CHECK(gPad[0].gamePressed & PADG_TRIANGLE);
    clear(); pending.wheel=1; HistSel_MouseInput(); CHECK(gPad[0].gameRepeat & PADG_RIGHT);
    episode.top=2; episode.bottom=4; episode.itemCount=6; episode.itemMax=99;
    episode.items[2]=2; episode.items[3]=99; episode.items[4]=4;
    clear(); pending.moved=1; target="mc_menu_plate_3";
    ModeMenu_MouseInput(); CHECK(episode.cursor[0]==4 && images==1);
    ModeMenu_MouseInput(); CHECK(images==1);
    clear(); pending.confirm=1; target="mc_menu_plate_2";
    ModeMenu_MouseInput(); CHECK(episode.cursor[0]==4 && !gPad[0].gamePressed); /* locked */
    clear(); pending.confirm=1; target="mc_menu_plate_3";
    ModeMenu_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
    clear(); pending.confirm=1; target="mc_yajirusi_right";
    ModeMenu_MouseInput(); CHECK((gPad[0].gameRepeat & PADG_RIGHT) && !gPad[0].gamePressed);
    clear(); pending.wheel=-1; ModeMenu_MouseInput(); CHECK(gPad[0].gameRepeat & PADG_LEFT);
    clear(); episode.focus=1; pending.confirm=1; ModeMenu_MouseInput(); CHECK(gPad[0].gamePressed & PADG_CROSS);
    clear(); pending.cancel=1; ModeMenu_MouseInput(); CHECK(gPad[0].gamePressed & PADG_TRIANGLE);
    clear(); episode.step=MODEMENU_STEP_NARR_LINE; pending.confirm=1;
    ModeMenu_MouseScriptInput(); CHECK(episode.step==MODEMENU_STEP_GO && !gPad[0].gamePressed);
    clear(); episode.step=MODEMENU_STEP_NARR_WAIT; pending.confirm=1;
    ModeMenu_MouseScriptInput(); CHECK(episode.step==MODEMENU_STEP_GO);
    clear(); episode.step=MODEMENU_STEP_GUIDE_WAIT; pending.confirm=1;
    ModeMenu_MouseScriptInput(); CHECK(episode.step==MODEMENU_STEP_GUIDE_WAIT && (gPad[0].gamePressed & PADG_CROSS));
    clear(); episode.step=MODEMENU_STEP_NARR_WAIT; pending.cancel=1;
    ModeMenu_MouseScriptInput(); CHECK(episode.step==MODEMENU_STEP_NARR_WAIT && (gPad[0].gamePressed & PADG_TRIANGLE));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'history_mouse.c'
    source.write_text(harness)
    binary = Path(tmp) / 'history_mouse'
    subprocess.run(['cc', '-std=gnu99', '-fno-builtin', '-Iinclude', '-Iport/include',
                    '-include', 'port_compat.h', str(source), '-o', str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], check=True)
print('Dragon History mouse checks passed')
