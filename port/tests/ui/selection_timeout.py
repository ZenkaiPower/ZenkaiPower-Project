#!/usr/bin/env python3
"""Exercise production timeout handlers and Random stage/music selections."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
single = (ROOT / 'src/menu/menu_c_e.c').read_text()
team = (ROOT / 'src/menu/menu_e_b.c').read_text()

def function(source, name):
    start = source.index('static ', source.index(name) - 12)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

def stage_block(source):
    start = source.index('s32 cell, chosen = -1, count = 0, song, chosenSong = -1;')
    end = source.index('/* Leave the selected stage and song visible', start)
    return source[start:end].rstrip().removesuffix('}')

code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int s32;
enum {MCHR_RANDOM=161, TS_RANDOM=161, TS_COLS=7, TS_MEMBER_MAX=5, TS_CUR_MENU=6,
 CHARSEL_ST_DONE=10, CHARSEL_ST_COLOR=5, TEAMSEL_ST_DONE=10, TEAMSEL_ST_COLOR=5,
 CHARSEL_SIDE_FORMS=1,CHARSEL_SIDE_CUSTOM=2,CHARSEL_SIDE_ITEMS=4,
 TEAMSEL_SIDE_FORM=1,TEAMSEL_SIDE_CUSTOM=2,TEAMSEL_SIDE_PANEL=4,
 CHARSEL_STAGE_RANDOM=35,TS_STAGE_RANDOM=35,CHARSEL_BGM_RANDOM=24,TS_BGM_RANDOM=24,
 CHARSEL_BGM_LOCKED=25,TS_BGM_LOCKED=25,STGGRID_ID_LOCKED=62,
 CHARSEL_STAGE_COLS=6,TS_STAGE_COLS=6,CHARSEL_CLIP_STAGE_CHIP=0,TEAMSEL_CLIP_STAGE_CHIP=0};
#define CHARSEL_BGM_FILE(x) (x)
#define PORT_BGM_FILE(x) (x)
typedef struct {int id,formCount,form[7];} TsCell;
typedef struct {int col,row,chara,color,items[8];} TsMember;
typedef struct {int state,col,row,chara,picked,colorPlate,customPlate,items[8],flags,mask;
 int cur,memberCount,cost; TsMember member[5];} TeamSelSide;
typedef TeamSelSide CharSelSide;
typedef struct {int col,row,stage,bgm,bgmCursor,bgmPlaying;} Stage;
typedef struct {TeamSelSide *side[2];TsCell *cells[2];int masterCount[2],flash[7];
 Stage *stage;int stageIds[5],bgmIds[5],stageCount,bgmCount;} Menu;
static Menu cs,ts,*gCharSel=&cs,*gTeamSel=&ts;
static struct {int gamePressed;} gPad[2];
static int sTeamSelTimedOut, sTeamSelRandomCell = 2, randomCalls, songsPlayed;
static void ItemHelp_Close(void) {}
static void ItemPanel_Hide(int side) {}
static void CharSel_SetRowChips(int side) {}
static void CharSel_RequestFace(int side) {}
static void TeamSel_SetChips(int side) {}
static void TeamSel_RequestFace(int side) {}
static void TeamSel_SetTeamTex(int side) {}
static void Flash_GotoLabel(void *f,const char *label,int x) {}
static void CharSel_ClipGoto(int a,int b,int c,const char *label) {}
static void TeamSel_ClipGoto(int a,int b,int c,const char *label) {}
static void CharSel_SetStageChips(void) {}
static void CharSel_RequestStage(void) {}
static void TeamSel_SetStageChips(void) {}
static void TeamSel_RequestStage(void) {}
static int Rand_Range(int n) {randomCalls++;return 0;}
static void Bgm_Play(int song) {songsPlayed++;assert(song!=24);}
static void TeamSel_SumCost(int side,int skip) {
 ts.side[side]->cost=0;
 for(int i=0;i<ts.side[side]->memberCount;i++) ts.side[side]->cost+=ts.side[side]->member[i].chara;
}
static int TeamSel_IsCharaFree(int side,int chara) {
 for(int i=0;i<ts.side[side]->memberCount;i++)
  if(ts.side[side]->member[i].chara==chara) return 0;
 return 1;
}
static int TeamSel_FitsDp(int side,int chara) {return ts.side[side]->cost+chara<=10;}
'''
code += function(single, 'CharSel_Timeout(void)') + '\n'
code += function(team, 'TeamSel_TimeoutHasRoom(s32 side)') + '\n'
code += function(team, 'TeamSel_Timeout(void)') + '\n'
code += 'static void single_stage(int selectFrames) {' + stage_block(single) + '}\n'
code += 'static void team_stage(int selectFrames) {' + stage_block(team) + '}\n'
code += r'''
int main(void) {
 TsCell cells[3]={{2,0,{0}},{3,1,{4}},{161,0,{0}}};
 TeamSelSide sides[2]={{0}};Stage stage={0};
 for(int i=0;i<2;i++) {cs.side[i]=ts.side[i]=&sides[i];cs.cells[i]=ts.cells[i]=cells;cs.masterCount[i]=ts.masterCount[i]=3;}
 sides[1].state=CHARSEL_ST_DONE;sides[1].picked=9;
 CharSel_Timeout();
 assert(sides[0].chara==MCHR_RANDOM && sides[0].col==2 && sides[0].row==0);
 assert(sides[0].state==CHARSEL_ST_COLOR && gPad[0].gamePressed==0x200);
 assert(sides[0].colorPlate==0 && sides[0].customPlate==0 && !randomCalls);
 assert(sides[1].picked==9 && !gPad[1].gamePressed);
 memset(sides,0,sizeof(sides));sides[1].state=TEAMSEL_ST_DONE;
 sides[0].memberCount=1;sides[0].member[0].chara=2;
 cells[2].id=164; /* DP hides the native Random cell until timeout. */
 TeamSel_Timeout();
 assert(cells[2].id==TS_RANDOM);
 assert(sTeamSelTimedOut && sides[0].memberCount==1 && sides[0].member[0].chara==2);
 assert(sides[0].cur==1 && sides[0].member[1].col==2 && sides[0].member[1].chara==TS_RANDOM);
 assert(sides[0].state==TEAMSEL_ST_COLOR && !randomCalls);
 /* No eligible fighter fits the remaining DP: finish the confirmed team. */
 sides[0].member[0].chara=9;TeamSel_Timeout();
 assert(sides[0].state==TEAMSEL_ST_DONE && sides[0].memberCount==1);
 /* Forms count as eligible; related/used fighters and exhausted slots do not. */
 sides[0].cost=6;assert(TeamSel_TimeoutHasRoom(0));
 sides[0].cost=9;assert(!TeamSel_TimeoutHasRoom(0));
 sides[0].memberCount=5;assert(!TeamSel_TimeoutHasRoom(0));
 cs.stage=ts.stage=&stage;
 for(int i=0;i<2;i++) {
  Menu *m=i ? &ts:&cs;m->stageCount=m->bgmCount=4;
  m->stageIds[0]=0;m->stageIds[1]=62;m->stageIds[2]=35;m->stageIds[3]=7;
  m->bgmIds[0]=8;m->bgmIds[1]=25;m->bgmIds[2]=24;m->bgmIds[3]=9;
  memset(&stage,0,sizeof(stage));
  if(i) team_stage(0);else single_stage(0);
  assert(stage.stage==35 && stage.col==2 && stage.row==0 && stage.bgm==2);
  assert((i ? stage.bgmCursor:stage.bgmPlaying)==2);
  assert(!randomCalls && !songsPlayed);
 }
 puts("Selection timeout Random-option checks passed");
}
'''
with tempfile.TemporaryDirectory(prefix='selection-timeout-') as directory:
    source = Path(directory) / 'timeout.c'
    exe = Path(directory) / 'timeout'
    source.write_text(code)
    subprocess.run(['cc', '-std=gnu99', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
