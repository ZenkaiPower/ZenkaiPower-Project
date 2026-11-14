#!/usr/bin/env python3
"""Story rule setup must preserve the live-battle gate used by PC mouse input."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'src/battle/btl_script_cmd.c').read_text()
start = text.index('s32 BtlScriptCmd_SetRule(')
function = text[start:text.index('\n}\n', start) + 3]
source = r'''
#include <assert.h>
#include <string.h>
typedef int s32;
typedef unsigned u32;
#define GSC_PHASE_BEGIN 0
#define GSC_PHASE_BEGIN_ABORT 1
struct BattleWork { int rule, running; } battle;
static int reads, clears, rules;
struct BattleWork *Battle_GetWork(void) { return &battle; }
int Gsc_GetInt(void) { return ++reads; }
void Battle_ClearWork(void) { memset(&battle, 0, sizeof(battle)); clears++; }
void BattleSetup_SetRule(int screen, int mode, int bgm, int time, int announcer, int stage, int unk) {
    assert(screen==0 && mode==1 && stage==1 && bgm==2 && time==3 && announcer==4 && unk==5);
    battle.rule=mode; rules++;
}
'''
source += function
source += r'''
int main(void) {
    int phase, active;
    for (phase=0; phase<=1; phase++) for (active=0; active<=1; active++) {
        battle.running=active; battle.rule=99; reads=clears=rules=0;
        assert(BtlScriptCmd_SetRule(phase, 0)==1);
        assert(battle.running==active && battle.rule==1);
        assert(reads==5 && clears==1 && rules==1);
    }
    reads=clears=rules=0;
    BtlScriptCmd_SetRule(2,0);
    assert(reads==0 && clears==0 && rules==0);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'story.c'
    exe = Path(tmp) / 'story'
    src.write_text(source)
    subprocess.run(['cc', '-std=gnu89', '-DPORT=1', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Story battle mouse lifecycle checks passed')
