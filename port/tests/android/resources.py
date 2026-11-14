"""Verify Android ships the shared catalogs and server address as named packages."""
from pathlib import Path
import struct
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'port/tools'))
from android_resources import prepare_resources


def entry(package, name):
    with package.open('rb') as stream:
        magic, count = struct.unpack('<8sI', stream.read(12))
        assert magic == b'BT3TEXT1'
        for _ in range(count):
            length, offset, size = struct.unpack('<IQQ', stream.read(20))
            key = stream.read(length).decode('utf-8')
            if key == name:
                stream.seek(offset)
                return stream.read(size)
    raise AssertionError(f'Missing {name} in {package}')


with tempfile.TemporaryDirectory() as temporary:
    work = Path(temporary)
    metadata = work / 'metadata'
    metadata.mkdir()
    for suffix in ('dat', 'rel', 'mem'):
        (metadata / ('libzenkaipower.so.' + suffix)).write_bytes(suffix.encode())
    assets = work / 'assets'
    prepare_resources(assets, metadata)
    engine = assets / 'engine'
    assert not (engine / 'setup/en.json').exists()
    assert not (engine / 'dragon-net-battle.txt').exists()
    assert entry(engine / 'setup.pak', 'en.json') == (root / 'port/setup/text/en.json').read_bytes()
    assert entry(engine / 'dragon-net-battle.pak', 'server.txt') == (root / 'dragon-net-battle.txt').read_bytes()
    for suffix in ('dat', 'rel', 'mem'):
        assert entry(engine / 'setup/engine.pak', 'libzenkaipower.so.' + suffix) == suffix.encode()
    # Rebuilding replaces stale assets rather than keeping old loose copies.
    (engine / 'setup/en.json').write_text('{}')
    (engine / 'dragon-net-battle.txt').write_text('obsolete')
    prepare_resources(assets, metadata)
    assert not (engine / 'setup/en.json').exists()
    assert not (engine / 'dragon-net-battle.txt').exists()
print('PASS: Android shared setup/server packages and metadata')
