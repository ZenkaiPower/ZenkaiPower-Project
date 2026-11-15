#!/usr/bin/env python3
"""Training keeps its side's CPU choice across character-controller reloads."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'src/battle/btl_ai_mgr.c').read_text()
start = text.index('void BtlAiMgr_ResetSide(s32 side) {')
end = text.index('\n}', start) + 2
source = r'''
#include <assert.h>
#define PORT 1
typedef int s32;
#include "battle/btl_ai_level.h"
static int zenkai, online, mode, levels[2], resetCount, restored;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_NetSession(void) { return online; }
int Port_NetActive(void) { return online; }
int Battle_GetMode(void) { return mode; }
int BtlAiMgr_GetLevel(int side) { return levels[side]; }
void BtlAiMgr_SetLevel(int side, int base) { levels[side] = BtlAiLevel_FromBase(base); restored++; }
void AiThink_ResetSide(int side, int owned) { levels[side] = -1; resetCount++; }
''' + text[start:end] + r'''
int main(void) {
 for(zenkai=0;zenkai<=1;zenkai++) for(online=0;online<=1;online++)
 for(mode=0;mode<=9;mode++) for(int side=0;side<2;side++)
 for(int base=-3;base<=29;base++) {
  int selected=BtlAiLevel_FromBase(base), other=side^1;
  levels[side]=selected;levels[other]=17;resetCount=restored=0;
  BtlAiMgr_ResetSide(side);
  int keep=zenkai && !online && (mode==5 || mode==6);
  assert(resetCount==1 && restored==keep);
  assert(levels[side]==(keep ? selected:-1) && levels[other]==17);
  BtlAiMgr_ResetSide(side);
  assert(levels[side]==(keep ? selected:-1));
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'cpu.c'
    exe = Path(tmp) / 'cpu'
    path.write_text(source)
    subprocess.run(['cc', '-std=gnu99', '-fsanitize=address,undefined',
                    '-I' + str(root / 'include'), str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Training CPU choice survives controller reloads; other modes and online remain isolated')
