#!/usr/bin/env python3
"""Test exact ending capture and encrypted save persistence, without the game UI."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'port/tools'))
import portsrc

def main():
    env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1')
    flags=['gcc','-std=gnu99','-g','-w','-fno-builtin','-fno-strict-aliasing','-no-pie',
           '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
           '-Iinclude','-Iport/include','-include','port_compat.h']
    with tempfile.TemporaryDirectory(prefix='story-endings-') as directory:
        tmp=Path(directory)
        objects=[]
        for source in ('src/battle/port_story_endings.c','src/battle/btl_script_cmd.c',
                       'src/battle/btl_script.c','src/menu/menu_c_c.c','src/menu/menu_b_c.c'):
            prepared,_,_=portsrc.prepare(ROOT/source)
            obj=tmp/(Path(source).stem+'.o')
            subprocess.run(flags+['-c',str(prepared),'-o',str(obj)],cwd=ROOT,check=True)
            if source in ('src/battle/port_story_endings.c','src/battle/btl_script_cmd.c'):
                objects.append(str(obj))
        exe=tmp/'capture'
        subprocess.run(flags+['port/tests/battle/story_endings.c']+objects+
                       ['-Wl,--gc-sections','-lm','-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe)],env=env,check=True)
        exe=tmp/'encrypted'
        subprocess.run(['gcc','-std=gnu11','-g','-no-pie','-Iinclude','-fsanitize=address,undefined',
                        'port/tests/save/zenkai.c','port/src/plat_zenkai.c','port/src/zenkai_crypto.c',
                        '-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe)],env=env,check=True)

if __name__=='__main__':
    main()
