#!/usr/bin/env python3
"""Approve final release binaries for the server's fast online compatibility check."""
import argparse
import hashlib
import json
import sys
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DESTINATION = ROOT / 'server/data/game_data_signature.json'
FILES = 68605


def build_signature(executable):
    exe = Path(executable).resolve(strict=True)
    if exe.suffix.lower() == '.apk':
        return android_signature(exe)
    binaries = [('exe', exe), ('mem', Path(str(exe) + '.mem')),
                ('data', exe.with_suffix('.dat') if exe.suffix == '.exe' else Path(str(exe) + '.dat')),
                ('sdlwin', exe.parent / 'SDL3.dll'), ('sdllinux', exe.parent / 'lib/libSDL3.so.0'),
                ('sdlmac', exe.parent / 'lib/libSDL3.0.dylib')]
    records = f'zenkaipower-binaries-v2\nfiles={FILES}\n'
    for role, path in binaries:
        digest = '-'
        if path.exists():
            with path.open('rb') as source:
                checksum = hashlib.sha256()
                for chunk in iter(lambda: source.read(1024 * 1024), b''): checksum.update(chunk)
                digest = checksum.hexdigest()
        records += f'{role}={digest}\n'
    return hashlib.sha256(records.encode('ascii')).hexdigest()


def android_signature(apk):
    # Hash exactly the files the Android loader and Port_BuildSignature use.
    with zipfile.ZipFile(apk) as archive:
        engine = archive.read('lib/arm64-v8a/libzenkaipower.so')
        sdl = archive.read('lib/arm64-v8a/libSDL3.so')
        pak = archive.read('assets/engine/setup/engine.pak')
    from tempfile import TemporaryDirectory
    from data_package import restore_directory
    with TemporaryDirectory() as tmp:
        package = Path(tmp) / 'engine.pak'
        package.write_bytes(pak)
        restore_directory(Path(tmp) / 'engine')
        metadata = Path(tmp) / 'engine'
        values = [engine, (metadata / 'libzenkaipower.so.mem').read_bytes(),
                  (metadata / 'libzenkaipower.so.dat').read_bytes(), None, sdl, None]
    records = f'zenkaipower-binaries-v2\nfiles={FILES}\n'
    for role, data in zip(('exe','mem','data','sdlwin','sdllinux','sdlmac'), values):
        records += f'{role}={hashlib.sha256(data).hexdigest() if data is not None else "-"}\n'
    return hashlib.sha256(records.encode('ascii')).hexdigest()


def build_metadata(executable, version=None):
    """Read the artifact's version, never the potentially newer checkout's."""
    from data_package import read_data
    from tempfile import TemporaryDirectory
    exe = Path(executable).resolve(strict=True)
    if exe.suffix.lower() == '.apk':
        platform = 'android-arm64'
        if version is None:
            with TemporaryDirectory() as tmp:
                root = Path(tmp)
                with zipfile.ZipFile(exe) as archive:
                    (root / 'game-info.pak').write_bytes(archive.read('assets/engine/game-info.pak'))
                version = read_data(root / 'game-info/version.txt').decode('utf-8').strip()
    else:
        magic = exe.open('rb')
        with magic:
            header = magic.read(4)
        platform = 'windows-x64' if header[:2] == b'MZ' else 'linux-x64' if header == b'\x7fELF' else None
        if platform is None:
            raise ValueError(f'Unsupported executable platform: {exe}')
        if version is None:
            version = read_data(exe.parent / 'game-info/version.txt').decode('utf-8').strip()
    if not version:
        raise ValueError(f'Missing version for {exe}; supply --version explicitly')
    return {'version': version, 'platform': platform}


def policy(builds=(), reset=False, version=None):
    previous = json.loads(DESTINATION.read_text()) if DESTINATION.exists() and not reset else {}
    signatures = previous.get('signatures', {}) if previous.get('scheme') == 'zenkaipower-binaries-v2' else {}
    # Preserve approvals when upgrading a policy using the former array format.
    metadata = dict(signatures) if isinstance(signatures, dict) else {
        signature: previous.get('signature_metadata', {}).get(signature,
                    {'version': 'unknown', 'platform': 'unknown'}) for signature in signatures}
    for build in builds:
        signature = build_signature(build)
        details = build_metadata(build, version)
        metadata[signature] = details
    return {'scheme': 'zenkaipower-binaries-v2', 'files': FILES,
            'signatures': {signature: metadata[signature] for signature in sorted(metadata)}}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, action='append', default=[], help='Final approved executable; companions and bundled SDL are hashed too. Repeat for multiple builds.')
    parser.add_argument('--reset', action='store_true', help='Remove previous approvals before adding these builds.')
    parser.add_argument('--version', help='Override the artifact version, e.g. 1.0.0, when packaged metadata is unavailable.')
    parser.add_argument('--stdout', action='store_true', help='Print policy JSON instead of writing through the container mount.')
    args = parser.parse_args()
    result = policy(args.build, args.reset, args.version)
    serialized = json.dumps(result, indent=2) + '\n'
    if args.stdout:
        print(serialized, end='')
        sys.exit(0)
    try:
        DESTINATION.write_text(serialized, encoding='utf-8')
    except PermissionError:
        parser.exit(1, 'Cannot write policy. Use --stdout and save its output on the host (see README.md).\n')
    print(f'{DESTINATION}: {len(result["signatures"])} approved builds')
