#!/usr/bin/env python3
"""Synthetic GSC parser checks; no game files or UI required."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from story_routes import analyze, decode_script, trigger_description


def chunk(tag, payload, arg=0):
    return struct.pack('<4sIIi', tag, 16, len(payload), arg) + payload + struct.pack('<4sIII', b'EOFC', 16, 0, 0)


def fixture(words, pool, extra=b''):
    code = chunk(b'GSAC', struct.pack(f'<{len(words)}I', *words), -1) + extra
    return chunk(b'GSCF', chunk(b'GSHD', bytes(16)) + chunk(b'GSCD', code) +
                 chunk(b'GSDT', struct.pack(f'<{len(pool)}i', *pool)), 1)


def rejects(data):
    try:
        decode_script(data)
    except ValueError:
        return
    raise AssertionError('Malformed script accepted')


def main():
    # Main registers side-0 opponent KO -> action 10030, then CALL_WAITs it.
    words = [(7 << 16) | 1, (2 << 16) | (ord('a') << 8) | 8, 10, (1 << 8) | 10,
             (10030 << 16) | 3, 0]
    end = chunk(b'GSAC', struct.pack('<III', (9 << 16) | (1 << 8) | 1, (2 << 8) | 10, 0), 10030)
    data = fixture(words, [0x48, 10030, 1], end)
    actions = decode_script(data)
    assert actions[-1][0]['options'] == [['a', [0x48, 10030]]]
    assert actions[-1][1] == dict(call=10030, wait=True)
    analysis = analyze(actions, b'')
    ending = analysis['endings'][0]
    assert ending['result'] == 1 and ending['reached_in_static_graph']
    assert len(ending['incoming']) == 2
    assert ending['incoming'][0]['description'] == 'side 0: opponent knocked out'
    assert trigger_description(0x8049) == 'side 1: fighter flag 7 (ring-out/forced defeat)'
    assert trigger_description(2) == 'side 0: sub-clock reaches 20 seconds'
    assert trigger_description(0xFFFF) == 'wait flag off'
    rejects(data[:-1])
    rejects(fixture([4, 0], []))  # unknown instruction
    rejects(fixture([(9 << 16) | (1 << 8) | 1, (99 << 8) | 10, 0], [1]))
    rejects(fixture([(9 << 16) | (2 << 8) | 1, 10, 0], [1]))
    rejects(fixture([(9 << 16) | (1 << 8) | 1, 10], [1]))  # missing END
    print('GSC parser checks passed: chunks, operands/options, CALL_WAIT, result routes, triggers, malformed input.')


if __name__ == '__main__':
    main()
