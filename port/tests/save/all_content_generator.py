"""Validate all-content save unlocks, checksums and safe CLI output."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from generate_all_content_save import all_content_save
from generate_dragon_history_save import dragon_history_save, EPISODE_COUNTS
from generate_zenkai_save import seal_save, verify_envelope

save, items = all_content_save()
assert items == 0
words = struct.unpack_from('<QQQ', save, 0xC10)
assert sum(word << (64 * i) for i, word in enumerate(words)) == (1 << 161) - 1
assert struct.unpack_from('<Q', save, 0xC28)[0] == (1 << 35) - 1
assert struct.unpack_from('<I', save, 0xC30)[0] == ((1 << 20) - 1) | (0x1F << 26)
assert struct.unpack_from('<II', save, 0x208) == (1, 20)
assert not any(save[0x28C:0x778])  # mission and Survival records stay empty
assert struct.unpack_from('<I', save, 0x100C)[0] == 5
assert not any(save[0x2EC8:0x3026])  # items remain unowned and purchasable
assert struct.unpack_from('<I', save, 0x3028)[0] == 0
assert save[0x3EF0:0x3EF8] == b'SPK\x01\0\0\0\0'
for saga, count in enumerate(EPISODE_COUNTS):
    assert struct.unpack_from('<4I', save, 0x10 + saga * 16) == (3, (1 << count)-1, 0, (1 << count)-1)
base, _ = dragon_history_save()
allowed = set(range(8)) | set(range(0x208,0x210)) | set(range(0xC10,0xC34)) | set(range(0x100C,0x1010))
assert all(a == b or i in allowed for i,(a,b) in enumerate(zip(save,base)))
assert verify_envelope(seal_save(bytes(save)), check_starter_roster=False) == bytes(save)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'zenkaipower.sav'
    cmd = [sys.executable, str(ROOT / 'port/tools/generate_all_content_save.py'), '--output', str(path)]
    subprocess.run(cmd, check=True, capture_output=True)
    original = path.read_bytes()
    assert verify_envelope(original, check_starter_roster=False) == bytes(save)
    assert subprocess.run(cmd, capture_output=True).returncode != 0
    assert path.read_bytes() == original
    subprocess.run(cmd + ['--force'], check=True, capture_output=True)
    assert verify_envelope(path.read_bytes(), check_starter_roster=False) == bytes(save)
print('All-content save checks passed: unlocks, empty records, shop stock, checksums and overwrite protection.')
