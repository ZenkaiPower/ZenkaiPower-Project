#!/usr/bin/env python3
"""Training restarts after the KO and end sequence, preserving explicit exits."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/btl_seq.c').read_text()
a=s.index('static s32 Port_TrainingResetOnDefeat(void) {')
b=s.index('\n#endif',a)
# The restart hook must run at sequence exit, never during fight-end detection.
update=s[s.index('s32 BtlSeq_Update(void) {'):s.index('/* Returns the battle clock. */')]
assert update.index('gBtlSeq->state == BTL_SEQ_EXIT') < update.index('Port_TrainingResetOnDefeat()')
check=s[s.index('s32 BtlSeq_CheckBattleEnd(void) {'):s.index('/* State 3 enter:')]
assert 'Port_TrainingResetOnDefeat' not in check
source=r'''
#include <assert.h>
#include "battle/battle.h"
static BattleWork work;
static int zenkai, mode, ko, aborted;
int Port_ZenkaiEnabled(void) {return zenkai;}
s32 Battle_GetMode(void) {return mode;}
BattleWork *Battle_GetWork(void) {return &work;}
s32 BattleResult_IsKo(void) {return ko;}
s32 BattleResult_IsAborted(void) {return aborted;}
'''+s[a:b]+r'''
int main(void) {
 for(zenkai=0;zenkai<=1;zenkai++) for(mode=0;mode<=8;mode++)
 for(ko=0;ko<=1;ko++) for(aborted=0;aborted<=1;aborted++) {
  work.flags=0;
  int expected=zenkai && (mode==5 || mode==6) && ko && !aborted;
  assert(Port_TrainingResetOnDefeat()==expected);
  assert(!!(work.flags&BATTLE_FLAG_RESTART)==expected);
 }
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'training.c';e=Path(tmp)/'training';p.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('Training defeat restart checks passed')
