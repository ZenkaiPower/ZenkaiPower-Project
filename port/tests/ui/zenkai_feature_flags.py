#!/usr/bin/env python3
"""Exercise all 16 combinations of the four independent Zenkai build switches."""
from itertools import product
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
flags = ['PORT_ZENKAI_STORY_UNLOCKS', 'PORT_ZENKAI_TOURNAMENT',
         'PORT_ZENKAI_RAISED_DIFFICULTY', 'PORT_ZENKAI_STORY_RESTRICTIONS']
with tempfile.TemporaryDirectory(prefix='zenkai-features-') as directory:
    exe = Path(directory) / 'features'
    for values in product((0, 1), repeat=4):
        subprocess.run(['gcc', '-std=gnu99', '-fno-builtin', '-ffunction-sections',
                        '-fdata-sections', '-w', '-no-pie', '-fsanitize=address,undefined',
                        '-Iinclude', '-Iport/include', '-include', 'port_compat.h'] +
                       [f'-D{name}={value}' for name, value in zip(flags, values)] +
                       ['port/tests/ui/zenkai_feature_flags.c', 'port/tests/save/zenkai_feature_flags.c',
                        'src/sys/save.c', 'src/sys/port_zenkai_pack.c', 'src/menu/menu_c_c.c', 'src/menu/menu_a_d.c',
                        'src/menu/menu_k_f.c', 'src/menu/menu_i_d.c', 'src/menu/menu_j_b.c',
                        '-Wl,--gc-sections', '-o', str(exe)],
                       cwd=ROOT, check=True)
        subprocess.run([str(exe)], env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1'), check=True)
print('All 16 Zenkai build-flag combinations passed: starting roster, rewards, AI, restrictions and tournaments')
