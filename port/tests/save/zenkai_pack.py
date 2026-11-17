#!/usr/bin/env python3
"""Exercise production pack policy, money entry points, groups and saved locks."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
source=(ROOT/'src/menu/menu_p_d.c').read_text()
start=source.index('s32 UbScore_Transfer(')
end=source.index('\n}',start)+2
transfer=source[start:end].replace('UbScore_Transfer(', 'TestPackTransfer(')
flags=['PORT_ZENKAI_GIANTS','PORT_ZENKAI_STORY_UNLOCKS','PORT_ZENKAI_TOURNAMENT',
       'PORT_ZENKAI_RAISED_DIFFICULTY','PORT_ZENKAI_STORY_RESTRICTIONS','PORT_ZENKAI_SSJ2_MOVESET','PORT_ZENKAI_KAIOKEN','PORT_ZENKAI_GINYU_BODY_CHANGE','PORT_ZENKAI_EXTRA_OUTFITS','PORT_ZENKAI_FUTURE_GOHAN_GRAB']
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory)
    (tmp/'transfer.c').write_text('#include "common.h"\n#include "sys/save.h"\n#include "sys/port_zenkai.h"\n'
        '#define P_SAVE gSaveData\nextern int gPortUbPointsPending;\nextern void Snd_PlaySe(int,int);\n'+transfer)
    configurations=[[],[f'-D{x}=0' for x in flags]]+[[f'-D{x}=0'] for x in flags]
    for defines in configurations:
        subprocess.run(['gcc','-std=gnu99','-fno-builtin','-ffunction-sections','-fdata-sections',
            '-Wl,--gc-sections','-fsanitize=undefined','-Iinclude','-Iport/include','-include','port_compat.h']+
            defines+['port/tests/save/zenkai_pack.c','src/sys/port_zenkai_pack.c','src/sys/save.c',str(tmp/'transfer.c'),
                     '-o',str(tmp/'pack')],cwd=ROOT,check=True)
        subprocess.run([str(tmp/'pack')],check=True)
print('Pack checks passed: ten IDs, subset, twelve flag configurations, Zeni locks, saved state and original roster.')
