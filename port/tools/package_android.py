#!/usr/bin/env python3
"""Package the signed APK and checksum beside the desktop release archives."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def main():
    version = json.loads((ROOT / 'game-info.json').read_text())['version']
    if not isinstance(version, str) or not re.fullmatch(r'[vV]?[0-9]+(?:\.[0-9]+)*', version):
        raise ValueError('game-info.json version must be numeric')
    stem = f'ZenkaiPower-v{version.lstrip("vV")}-android-arm64'
    folder = ROOT / 'release/android-arm64'
    apk = folder / (stem + '.apk')
    shutil.copyfile(ROOT / 'android/app/build/outputs/apk/release/app-release.apk', apk)
    checksum = apk.with_suffix('.apk.sha256')
    digest = hashlib.sha256()
    with apk.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''): digest.update(chunk)
    checksum.write_text(f'{digest.hexdigest()}  {apk.name}\n', encoding='ascii')
    # Stage in the writable platform directory, then move to release/.
    staged = folder / (stem + '.zip')
    with zipfile.ZipFile(staged, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in (apk, checksum):
            archive.write(path, 'android-arm64/' + path.name)
    destination = ROOT / 'release' / staged.name
    staged.replace(destination)
    print(f'Android release: {apk.relative_to(ROOT)}')
    print(f'Archive: {destination.relative_to(ROOT)}')

if __name__ == '__main__':
    main()
