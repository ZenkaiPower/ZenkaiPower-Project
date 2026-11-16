"""Check generated episode availability, unchanged progression and CLI safeguards."""
from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from generate_dragon_history_save import dragon_history_save, EPISODE_COUNTS, STARTING_CHARACTERS, STORY_STAGES
from generate_zenkai_save import fresh_save, seal_save, verify_envelope, HEADER_SIZE, SAVE_SIZE

save, items = dragon_history_save()
assert items == 0 and len(save) == SAVE_SIZE
for saga, count in enumerate(EPISODE_COUNTS):
    flags, unlocked, cleared, new = struct.unpack_from('<4I', save, 0x10 + saga * 0x10)
    assert flags == 3 and unlocked == new == (1 << count) - 1 and cleared == 0
assert sum(EPISODE_COUNTS) == 48 and save[0x90:0xA0] == bytes(16)
words = struct.unpack_from('<QQQ', save, 0xC10)
assert sum(word << (64 * i) for i, word in enumerate(words)) == sum(1 << c for c in STARTING_CHARACTERS)
assert struct.unpack_from('<I', save, 0x3028)[0] == 0
assert save[0x3EF0:0x3EF8] == b'SPK\x01\0\0\0\0'
# Only checksums, story slots, roster and the pack record differ from the base.
base, _ = fresh_save(None)
assert verify_envelope(seal_save(bytes(base))) == bytes(base)
assert not struct.unpack_from('<Q', save, 0xC28)[0] & sum(1 << stage for stage in STORY_STAGES)
allowed = set(range(8)) | set(range(0x10, 0x90)) | set(range(0xC10, 0xC30)) | set(range(0x3EF0, 0x3EF8))
assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(save, base)))
encoded = seal_save(bytes(save))
assert verify_envelope(encoded, check_starter_roster=False) == bytes(save)
try:
    verify_envelope(encoded)
    raise AssertionError('Original generator roster validation was bypassed')
except ValueError:
    pass
assert encoded[24:56] == hashlib.blake2b(save, digest_size=32).digest()
assert len(encoded) == HEADER_SIZE + SAVE_SIZE
corrupt = bytearray(encoded); corrupt[-1] ^= 1
try:
    verify_envelope(bytes(corrupt), check_starter_roster=False)
    raise AssertionError('Damaged envelope accepted')
except ValueError:
    pass
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'zenkaipower.sav'
    command = [sys.executable, str(ROOT / 'port/tools/generate_dragon_history_save.py'), '--output', str(path)]
    subprocess.run(command, check=True, capture_output=True)
    original = path.read_bytes()
    assert verify_envelope(original, check_starter_roster=False) == bytes(save)
    assert subprocess.run(command, capture_output=True).returncode != 0
    assert path.read_bytes() == original
    subprocess.run(command + ['--force'], check=True, capture_output=True)
    assert verify_envelope(path.read_bytes(), check_starter_roster=False) == bytes(save)
print('Dragon History generator checks passed: 48 available, none cleared, checksums and overwrite protection.')
