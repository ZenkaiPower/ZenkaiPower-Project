#!/usr/bin/env python3
"""Run the production Zenkai tournament selector without disc assets."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]

def main():
    # Check the checked-in mapping against the reviewed policy document.
    doc = (ROOT / 'docs/port/tournament-character-families.md').read_text(encoding='utf-8')
    table = doc.split('## Complete roster mapping')[1].split('## Implementation checks')[0]
    identities, families, expected = {}, {}, {}
    for line in table.splitlines():
        if '`' not in line:
            continue
        identity, family, entries = [s.strip() for s in line.split('|')[1:4]]
        identities.setdefault(identity, len(identities))
        families.setdefault((identity, family), len(families))
        for chara in re.findall(r'`(\d+)`:', entries):
            assert int(chara) not in expected
            expected[int(chara)] = (identities[identity], families[identity, family])
    rules = (ROOT / 'include/menu/port_tour_rules.h').read_text()
    rows = re.findall(r'\{(\d+), (\d+), 0x[0-9A-F]+\}, /\* (\d+) \*/', rules)
    assert len(rows) == 161
    for identity, family, chara in rows:
        assert expected[int(chara)] == (int(identity), int(family))
    flags = ['gcc', '-std=gnu99', '-g', '-fno-builtin', '-fno-strict-aliasing',
             '-ffunction-sections', '-fdata-sections', '-w', '-no-pie',
             '-fsanitize=address,undefined', '-Iinclude', '-Iport/include',
             '-include', 'port_compat.h']
    with tempfile.TemporaryDirectory(prefix='zenkai-tour-') as tmp:
        exe = Path(tmp) / 'tour'
        subprocess.run(flags + ['port/tests/ui/tour_rules.c', 'src/menu/menu_k_f.c',
                                '-Wl,--gc-sections', '-o', str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe)], env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1'), check=True)
        menu_exe = Path(tmp) / 'menu'
        subprocess.run(flags + ['port/tests/ui/tour_menu.c', 'src/menu/menu_j_b.c',
                                '-Wl,--gc-sections', '-o', str(menu_exe)], cwd=ROOT, check=True)
        subprocess.run([str(menu_exe)], check=True)
        # Exercise the actual player-selection slot helpers using a synthetic grid.
        player = (ROOT / 'src/menu/menu_i_d.c').read_text()
        start = player.index('static s32 Port_EntrySlotAvailable(')
        end = player.index('\n#endif', start)
        player_test = Path(tmp) / 'player.c'
        player_test.write_text(r'''
#include "common.h"
#include "menu/menu_j.h"
#include "menu/port_tour_rules.h"
#include <assert.h>
static SaveData save;
SaveData *gSaveData=&save;
static ESel selector;
ESel *gEntrySel=&selector;
static ESelState state;
int Port_ZenkaiEnabled(void) {return 1;}
u32 Rand_Range(u32 n) {assert(n);return n-1;}
int ChrTbl_WrapCostume(int c,int *color) {return c==99?4:2;}
''' + player[start:end] + r'''
int main(void) {
 ESelCell grid[2]={{0},{0}};
 selector.sel=&state;selector.grid=grid;selector.gridCount=2;
 grid[0].id=79;grid[1].id=99;
 SAVE_CHARA_WORD(&save,79)|=(u64)1<<(79%64);
 SAVE_CHARA_WORD(&save,99)|=(u64)1<<(99%64);
 for(int i=0;i<5;i++) {state.entry[i].chara=79;state.entry[i].costume=0;}
 state.cur=4;assert(Port_EntrySlotAvailable(79,0));
 state.cur=5;assert(!Port_EntrySlotAvailable(79,0));
 int cell,form,color;
 assert(Port_EntryRandom(&cell,&form,&color)==99);
 state.cur=4;
 for(int i=0;i<4;i++) {state.entry[i].chara=99;state.entry[i].costume=i;}
 for(int i=0;i<4;i++) assert(!Port_EntrySlotAvailable(99,i));
 assert(Port_EntryRandom(&cell,&form,&color)==79);
 state.cur=1;state.entry[0].chara=109;
 assert(Port_EntrySlotAvailable(109,0));
 state.entry[0].chara=25;assert(!Port_EntrySlotAvailable(25,1));
 return 0;
}
''')
        player_exe = Path(tmp) / 'player'
        subprocess.run(flags + [str(player_test), '-o', str(player_exe)], cwd=ROOT, check=True)
        subprocess.run([str(player_exe)], check=True)
        # Compile every integration point, including the player selector and menu.
        subprocess.run(flags + ['-fsyntax-only', 'src/menu/menu_j_b.c', 'src/menu/menu_i_d.c'],
                       cwd=ROOT, check=True)

if __name__ == '__main__':
    main()
