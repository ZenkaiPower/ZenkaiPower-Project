"""Package the development-only DolphinTool with its matching source and notices.

Game releases use bundle_nod.py; this helper is only for sharing developer tools.
"""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
VENDOR = ROOT / 'port/third_party/dolphin-tool'


def bundle_dolphin(out, windows=False):
    out = Path(out)
    platform = 'windows-x64' if windows else 'linux-x64'
    manifest = json.loads((VENDOR / 'binaries.json').read_text())
    source = VENDOR / platform
    actual = {p.relative_to(source).as_posix() for p in source.rglob('*') if p.is_file()}
    if actual != set(manifest[platform]):
        raise RuntimeError(f'Development Dolphin file list changed: {platform}')
    for name, digest in manifest[platform].items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Changed development Dolphin binary: {name}')
    archive = VENDOR / 'dolphin-tool-source.tar.gz'
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest['source_sha256']:
        raise RuntimeError('Development Dolphin source checksum mismatch')
    shutil.copytree(source, out / 'tools/dolphin', dirs_exist_ok=True)
    notices = out / 'licenses/dolphin-tool'
    shutil.copytree(VENDOR / 'LICENSES', notices, dirs_exist_ok=True)
    for name in ('COPYING', 'SOURCE.md', 'binaries.json'):
        shutil.copy2(VENDOR / name, notices / name)
    shutil.copy2(archive, out / archive.name)
