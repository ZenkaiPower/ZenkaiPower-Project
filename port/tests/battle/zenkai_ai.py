#!/usr/bin/env python3
"""Exercise the real controller, rate, reaction and charge functions without disc data."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
import portsrc


def main():
    flags = ['gcc', '-std=gnu99', '-g', '-fno-builtin', '-fno-strict-aliasing',
             '-ffunction-sections', '-fdata-sections', '-w', '-no-pie',
             '-fsanitize=address,undefined', '-Iinclude', '-Iport/include',
             '-include', 'port_compat.h']
    with tempfile.TemporaryDirectory(prefix='zenkai-ai-') as directory:
        tmp = Path(directory)
        objects = {}
        for name in ('mgr', 'seq', 'cond', 'act'):
            source, _, _ = portsrc.prepare(ROOT / f'src/battle/btl_ai_{name}.c')
            obj = tmp / f'{name}.o'
            subprocess.run(flags + ['-c', str(source), '-o', str(obj)], cwd=ROOT, check=True)
            objects[name] = str(obj)
        # Compile the pause-menu caller as well: effective levels must be
        # converted back before selecting one of the original five labels.
        subprocess.run(flags + ['-c', 'src/battle/hud_0.c', '-o', str(tmp / 'hud.o')],
                       cwd=ROOT, check=True)
        source, _, _ = portsrc.prepare(ROOT / 'src/battle/btl_script_cmd.c')
        subprocess.run(flags + ['-c', str(source), '-o', str(tmp / 'script.o')],
                       cwd=ROOT, check=True)
        for name, define, sources in (
            ('controller', [], ['mgr', 'seq']),
            ('reaction', ['-DTEST_REACT'], ['cond']),
            ('charge', ['-DTEST_CHARGE'], ['act']),
        ):
            exe = tmp / name
            subprocess.run(flags + define + ['port/tests/battle/zenkai_ai.c'] +
                           [objects[s] for s in sources] + ['-Wl,--gc-sections', '-lm', '-o', str(exe)],
                           cwd=ROOT, check=True)
            env = dict(os.environ, UBSAN_OPTIONS='halt_on_error=1')
            subprocess.run([str(exe)], env=env, check=True)
        fallback = tmp / 'original.c'
        fallback.write_text('''#include "battle/btl_ai_level.h"
#include <assert.h>
int main(void) {
    int level;
    assert(!BtlAiLevel_ZenkaiEnabled());
    assert(BtlAiLevel_StoryOffset(0) == 0);
    assert(BtlAiLevel_StoryOffset(1) == 6);
    assert(BtlAiLevel_StoryOffset(2) == 9);
    for (level = -999; level <= 29; level++) {
        assert(BtlAiLevel_FromBase(level) == level);
        assert(BtlAiLevel_ToBase(level) == level);
    }
    return 0;
}
''')
        exe = tmp / 'original'
        subprocess.run(['gcc', '-std=gnu89', '-Iinclude', str(fallback), '-o', str(exe)],
                       cwd=ROOT, check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
