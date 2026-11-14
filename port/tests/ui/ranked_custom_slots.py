#!/usr/bin/env python3
"""Ranked single selection disables custom equipment for every character path."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
source = (root / 'src/menu/menu_c_e.c').read_text()
start = source.index('static void CharSel_SetCustomRows(')
end = source.index('\n}', start) + 2
code = r'''
#include <assert.h>
typedef int s32;
static int session, ranked;
int Port_NetSession(void) { return session; }
int Port_NetStatsRanked(void) { return ranked; }
static struct Side { int customPlate; } sides[2];
static struct Menu { int customRows[2];struct Side *side[2]; } menu, *gCharSel=&menu;
''' + source[start:end] + r'''
int main(void) {
 for(int side=0;side<2;side++) {
  menu.side[side]=&sides[side];
  for(int rows=2;rows<=4;rows+=2) {
   session=ranked=1;sides[side].customPlate=rows-1;
   CharSel_SetCustomRows(side,rows);
   assert(menu.customRows[side]==1 && sides[side].customPlate==0);
   session=1;ranked=0;sides[side].customPlate=rows-1;
   CharSel_SetCustomRows(side,rows);
   assert(menu.customRows[side]==rows && sides[side].customPlate==rows-1);
   session=0;ranked=1;CharSel_SetCustomRows(side,rows);
   assert(menu.customRows[side]==rows && sides[side].customPlate==rows-1);
  }
 }
}
'''
# Grid (normal/Random), form reel and saved-record paths must all use the guard.
assert source.count('CharSel_SetCustomRows(CS_N, ') == 5
assert 'gCharSel->customRows[CS_N] =' not in source
with tempfile.TemporaryDirectory(prefix='ranked-custom-') as directory:
    path = Path(directory) / 'ranked.c'
    exe = Path(directory) / 'ranked'
    path.write_text(code)
    subprocess.run(['cc', '-std=gnu99', '-DPORT', '-fsanitize=address,undefined',
                    str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Ranked Normal-only selection checks passed')
