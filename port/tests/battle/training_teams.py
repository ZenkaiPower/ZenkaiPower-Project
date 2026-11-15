#!/usr/bin/env python3
"""Check production team setup preserves Training rules and all selected members."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'src/menu/menu_e_b.c').read_text()
start = text.index('        cpuLevel = CpuLevel_FromSetting', text.index('s32 TeamSel_Run('))
end = text.index('        BattleSetup_Finish();', start) + len('        BattleSetup_Finish();')
loader = (root / 'src/battle/battle_load.c').read_text()
fix_start = loader.index('void BattleSetup_FixForMode(void) {')
fix_end = loader.index('\n/* Sets a side', fix_start)
source = r'''
#include <assert.h>
#include "common.h"
#include "menu/menu_f.h"
#include "sys/save.h"
#include "battle/btl_ai_level.h"
#include "battle/battle.h"
static BattleSetup battle;
#define SETUP() (&battle)
int Battle_GetMode(void) { return battle.rule.mode; }
int Port_ZenkaiEnabled(void);
''' + loader[fix_start:fix_end] + r'''
static TeamSel selection;
TeamSel *gTeamSel = &selection;
static SaveData save;
SaveData *gSaveData = &save;
static int zenkai, rules[7], counts[2], controls[2], levels[2][5], seen[2], finished;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_NetSession(void) { return 0; }
int Port_NetActive(void) { return 0; }
int Port_NetBotSide(void) { return -1; }
int Port_NetBotLevel(void) { return 29; }
void Port_NetUnlocksApply(int side) {}
void Port_NetSharedUnlocksApply(void) {}
int CpuLevel_FromSetting(int setting) { return 13; }
void Battle_ClearWork(void) { seen[0] = seen[1] = 0; finished = 0; }
void BattleSetup_SetRule(int screen, int mode, int bgm, int time, int announcer, int stage, int extra) {
 int values[] = {screen, mode, bgm, time, announcer, stage, extra};
 for(int i = 0; i < 7; i++) rules[i] = values[i];
 battle.rule.mode = mode;
}
void BattleSetup_SetSide(int side, int control, int pad, int count, int handicap, int change, int lead, void *bits) {
 counts[side] = count; controls[side] = control;
 battle.sides[side].memberCount = count;
 battle.sides[side].control = control;
 assert(change == 1);
}
void BattleSetup_SetMember(int side, int member, int chara, int color, int variant, int level, float health, u16 *items) {
 assert(member < counts[side]);
 assert(chara == side * 10 + member);
 assert(color == member && health == 100.0f);
 assert(items == selection.side[side]->member[member].items.id);
 levels[side][member] = level; seen[side]++;
 battle.sides[side].members[member].cpuLevel = level;
}
void BattleSetup_Finish(void) {
 BattleSetup_FixForMode();
 for(int side = 0; side < 2; side++) {
  counts[side] = battle.sides[side].memberCount;
  controls[side] = battle.sides[side].control;
  for(int j = 0; j < counts[side]; j++) levels[side][j] = battle.sides[side].members[j].cpuLevel;
 }
 finished++;
}
static void setup(int training) {
 int i, j, bgm = 7, timeLimit = 3, cpuLevel, handicap[2] = {1, 1};
''' + text[start:end] + r'''
}
int main(void) {
 selection.side[0] = &selection.sideData[0];
 selection.side[1] = &selection.sideData[1];
 selection.stage = &selection.stageData;
 selection.stage->stage = 4;
 save.rule[2] = 0; save.rule[5] = 0;
 for(int a = 1; a <= 5; a++) for(int b = 1; b <= 5; b++) {
  selection.side[0]->memberCount = a; selection.side[1]->memberCount = b;
  for(int side = 0; side < 2; side++) for(int j = 0; j < 5; j++) {
   selection.side[side]->member[j].chara = side * 10 + j;
   selection.side[side]->member[j].color = j;
  }
  for(zenkai = 0; zenkai <= 1; zenkai++) for(int training = 0; training <= zenkai; training++) {
   setup(training);
   assert(finished == 1 && counts[0] == a && counts[1] == b);
   assert(seen[0] == a && seen[1] == b && controls[0] == 0 && controls[1] == 2);
   assert(rules[0] == 0 && rules[1] == (training ? 6 : 0));
   assert(rules[2] == 7 && rules[3] == (training ? 0 : 3));
   assert(rules[4] == (training ? 1 : 0) && rules[5] == 4 && rules[6] == (training ? 0 : 1));
   for(int side = 0; side < 2; side++) for(int j = 0; j < counts[side]; j++)
    assert(levels[side][j] == (training ? -1 : BtlAiLevel_DuelBase(13)));
  }
 }
 zenkai = 0;
 for(int count = 1; count <= 5; count++) {
  battle.rule.mode = 6;
  for(int side = 0; side < 2; side++) {
   battle.sides[side].memberCount = count;
   battle.sides[side].control = 1;
  }
  BattleSetup_FixForMode();
  assert(battle.sides[0].memberCount == 1 && battle.sides[1].memberCount == 1);
  assert(battle.sides[0].control == 0 && battle.sides[1].control == 2);
  assert(battle.sides[1].members[0].cpuLevel == -1);
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'teams.c'
    exe = Path(tmp) / 'teams'
    path.write_text(source)
    subprocess.run(['cc', '-std=gnu99', '-fno-builtin', '-fsanitize=address,undefined',
                    '-I' + str(root / 'include'), '-I' + str(root / 'port/include'),
                    '-include', str(root / 'port/include/port_compat.h'), str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Training team setup checks passed (1..5 members per side and Duel isolation)')
