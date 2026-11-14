#!/usr/bin/env python3
"""Exercise read-only installer preflight using synthetic ISO images."""
import subprocess
import tempfile
from pathlib import Path
from native_extract import iso_record

ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / 'port/build/native-extract-test'

with tempfile.TemporaryDirectory(prefix='zenkaipower-preflight-') as temporary:
    root = Path(temporary)
    image = root / 'valid fixture.iso'
    data = bytearray(24 * 2048)
    pvd = bytearray(2048)
    pvd[:7] = b'\1CD001\1'
    record = iso_record(b'\0', 20, 2048, True)
    pvd[156:156 + len(record)] = record
    data[16*2048:17*2048] = pvd
    directory = record + iso_record(b'\1', 20, 2048, True)
    directory += iso_record(b'BIN', 21, 2048, True)
    directory += iso_record(b'SLUS_216.78;1', 22, 64)
    data[20*2048:20*2048+len(directory)] = directory
    directory = iso_record(b'\0', 21, 2048, True) + record
    directory += iso_record(b'DBZP.BIN;1', 23, 12)
    data[21*2048:21*2048+len(directory)] = directory
    data[23*2048:23*2048+12] = b'fixture menu'
    image.write_bytes(data)

    def verify(mode, path, expected):
        before = {p.name: p.read_bytes() for p in root.iterdir()}
        result = subprocess.run([str(DRIVER), mode, str(path)], capture_output=True, timeout=20)
        assert result.returncode == expected, result.stderr
        assert before == {p.name: p.read_bytes() for p in root.iterdir()}, 'Preflight wrote files'
        return result.stderr

    # Reinstallation must overwrite disc data even when .installed already exists.
    installation = root / 'installed'
    (installation / 'gamedata/disc/BIN').mkdir(parents=True)
    (installation / 'gamedata/.installed').touch()
    target = installation / 'gamedata/disc/BIN/DBZP.BIN'
    target.write_bytes(b'damaged previous installation')
    (installation / 'savegame').mkdir()
    save = installation / 'savegame/player.dat'
    save.write_bytes(b'keep my save')
    subprocess.run([str(DRIVER), 'reinstall-data', str(installation), str(image)], check=True, timeout=20)
    assert target.read_bytes() == b'fixture menu'
    marker = installation / 'gamedata/.installed'
    assert not marker.exists(), 'Extraction alone must not mark installation complete'
    game = installation / 'ZenkaiPower'
    def finish(script, succeeds):
        game.write_text('#!/bin/sh\n' + script)
        game.chmod(0o755)
        result = subprocess.run([str(DRIVER), 'finish-install', str(installation)],
                                capture_output=True, timeout=20)
        assert (result.returncode == 0) == succeeds, result.stderr
        assert marker.exists() == succeeds
    finish('exit 1\n', False)
    # A zero exit code alone is not proof that text preparation completed.
    finish('echo "preparation did not complete"; exit 0\n', False)
    finish('test "$ZENKAIPOWER_PREPARE_TEXT" = 1 || exit 1\necho "text resources prepared and packaged"\n', True)
    assert save.read_bytes() == b'keep my save'
    # Remove the fixture install before checking read-only preflight behavior below.
    import shutil
    shutil.rmtree(installation)
    verify('verify-fixture', image, 0)
    assert b'checksums' in verify('verify-us', image, 1)
    # Matching the region ID alone must not bypass an optional-disc hash.
    for disc, serial in [(1, b'SLES_549.45'), (3, b'SLPS_258.15')]:
        image.write_bytes(data.replace(b'SLUS_216.78', serial))
        result = subprocess.run([str(DRIVER), 'verify-disc', str(ROOT)+'/', str(disc), str(image)],
                                capture_output=True, timeout=20)
        assert result.returncode == 1 and b'checksum' in result.stderr, result.stderr
    data[23*2048] ^= 1
    image.write_bytes(data)
    assert b'checksums' in verify('verify-fixture', image, 1)
    image.write_bytes(data[:2048])
    verify('verify-fixture', image, 1)
    verify('verify-us', root/'missing.iso', 1)
print('Preflight passed: accepted fixture, rejected wrong checksums, corruption, truncation and missing file; no writes.')
