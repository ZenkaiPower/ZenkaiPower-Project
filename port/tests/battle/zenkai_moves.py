#!/usr/bin/env python3
"""Compile the focused test and check against the user's original game data.
python3 port/tests/battle/zenkai_moves.py release/linux-x64/gamedata
No disc assets are stored in this test.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from make_fingerprint import bpe_decode

def main():
    data = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'gamedata'
    with tempfile.TemporaryDirectory(prefix='zenkaipower-zenkai-moves-') as tmp:
        tmp = Path(tmp)
        with (data / 'pzs3us1.afs').open('rb') as archive:
            for chara in (4, 5, 6):
                for base in (0x590, 0x598, 0x599):
                    fid = base + chara * 10
                    archive.seek(8 + (fid - 1) * 8)
                    offset, size = struct.unpack('<II', archive.read(8))
                    archive.seek(offset)
                    blob = archive.read(size)
                    (tmp / f'{fid}.bin').write_bytes(bpe_decode(blob) or blob)
        exe = tmp / 'test'
        subprocess.run(['gcc', '-std=gnu99', '-g', '-fsanitize=address,undefined', '-fno-builtin', '-ffunction-sections', '-fdata-sections',
                        '-Iinclude', '-Iport/include', '-include', 'port_compat.h',
                        'port/tests/battle/zenkai_moves.c', '-Wl,--gc-sections', '-o', str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe), str(tmp)], check=True)
        queue_test = tmp / 'voice_queue'
        subprocess.run(['gcc', '-std=gnu99', '-g', '-no-pie', '-fno-builtin', '-Wno-pointer-to-int-cast',
                        '-ffunction-sections', '-fdata-sections', '-Iinclude', '-Iport/include',
                        '-include', 'port_compat.h', 'port/tests/battle/zenkai_voice_queue.c',
                        '-Wl,--gc-sections', '-o', str(queue_test)], cwd=ROOT, check=True)
        subprocess.run([str(queue_test)], check=True)
        print('Technique voice queue checks passed (charge/fire, both fighter sides)')

if __name__ == '__main__':
    main()
