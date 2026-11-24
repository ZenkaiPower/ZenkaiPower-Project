"""Exercise the production team-slot pointer block on both player sides."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'src/menu/menu_e_b.c').read_text()
a=source.index('            if (CUR_SIDE->state == TEAMSEL_ST_TEAM) {')
b=source.index('            if (CUR_SIDE->state == TEAMSEL_ST_PANEL)',a)
block=source[a:b]
harness=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int s32;typedef float f32;
#define TS_MEMBER_MAX 5
#define TEAMSEL_ST_TEAM 7
#define TEAMSEL_CLIP_TEAM 1
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define PADG_LEFT 1
#define PADG_RIGHT 2
#define PADG_UP 8
#define PADG_DOWN 4
static struct {int gamePressed,gameRepeat;} gPad[2];
#include "menu/port_selection_mouse.h"
static struct {int state,cur,memberCount;} side;
static int currentSide,targetScope,clips,sounds;
static char target[64];
#define CUR_SIDE (&side)
#define CUR_N currentSide
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) {
 assert(pivot==(scope==12 ? 512:0));
 return scope==targetScope && strcmp(name,target)==0;
}
void TeamSel_ClipGoto(int flash,int player,int kind,const char *label) {
 assert(player==currentSide && flash==player+1);clips++;
}
void Snd_PlaySe(int bank,int id) {sounds++;}
static int run(PortMenuMouse mouse,int pad) {
 int hit=-1,scope=CUR_N ? 12:11;float pivot=CUR_N ? 512:0;
'''
tests=r'''
int main(void) {
 int count,chosen,pad;
 PortMenuMouse mouse={0};mouse.confirm=1;
 for(currentSide=0;currentSide<2;currentSide++) for(pad=0;pad<2;pad++)
 for(count=0;count<=5;count++) for(chosen=0;chosen<=5;chosen++) {
  int valid=chosen<5 && chosen<=count;
  memset(gPad,0,sizeof(gPad));side.state=TEAMSEL_ST_TEAM;side.cur=-1;side.memberCount=count;
  targetScope=currentSide ? 12:11;sprintf(target,"mc_team_%d",chosen);
  assert(run(mouse,pad)==(valid ? chosen:-1));
  assert(side.cur==(valid ? chosen:-1));
  assert(gPad[pad].gamePressed==(valid ? PADG_CROSS:0));
  assert(!gPad[pad^1].gamePressed);
 }
 for(currentSide=0;currentSide<2;currentSide++) {
  targetScope=currentSide ? 12:11;side.cur=1;side.memberCount=1;
  strcpy(target,"mc_team_1");memset(gPad,0,sizeof(gPad));
  assert(run(mouse,0)==1 && gPad[0].gamePressed==PADG_CROSS); /* next slot already highlighted */
  strcpy(target,"mc_menu_plate");assert(run(mouse,0)==5); /* finish team remains available */
  mouse.confirm=0;mouse.moved=1;strcpy(target,"mc_team_0");memset(gPad,0,sizeof(gPad));
  assert(run(mouse,0)==0 && !gPad[0].gamePressed);mouse.confirm=1;mouse.moved=0;
 }
 puts("Team pointer checks passed: both sides/pads, next empty slot, full teams, same-slot confirmation, hover and finish.");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 tmp=Path(tmp);(tmp/'test.c').write_text(harness+block+'Port_SelectionMouseButtons(&mouse,pad,hit>=0);return hit;}\n'+tests)
 subprocess.run(['gcc','-std=c99','-fsanitize=address,undefined','-no-pie','-Iinclude',str(tmp/'test.c'),'-o',str(tmp/'test')],cwd=ROOT,check=True)
 subprocess.run([str(tmp/'test')],check=True)
