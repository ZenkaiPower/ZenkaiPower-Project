"""Check both installed layouts and exclusion of saves, mods and login files."""
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from data_package import pack_directory

with TemporaryDirectory(prefix='zenkai-android-export-') as temporary:
    base = Path(temporary)
    data = base / 'gamedata'
    for name in ('disc/SLUS_216.78', 'disc/BIN/DBZP.BIN', 'game-text.pak',
                 'pzs3us0.afs', 'savegame/save.dat', 'mods/example.bin', 'mods.pak', 'net_account.dat', 'net_account.pak'):
        path = data / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'test fixture')
    for packed in (False, True):
        if packed:
            pack_directory(data / 'disc')
        output = base / ('packed.zip' if packed else 'loose.zip')
        subprocess.run([sys.executable, str(ROOT / 'port/tools/android_data.py'),
                        '--data', str(data), '--output', str(output)], check=True)
        with ZipFile(output) as archive:
            names = set(archive.namelist())
            assert 'gamedata/game-text.pak' in names
            assert 'gamedata/pzs3us0.afs' in names
            assert all('savegame' not in n and '/mods/' not in n and 'net_account' not in n for n in names)
            assert 'gamedata/mods.pak' not in names
            assert ('gamedata/disc.pak' if packed else 'gamedata/disc/SLUS_216.78') in names
print('Android export OK: loose/packed installations; private data excluded')
