"""Carry the statically linked disc reader's attribution into desktop releases."""
import os
from pathlib import Path
import shutil


def bundle_nod(out):
    root = Path(__file__).resolve().parents[2]
    cache = Path(os.environ.get('ZENKAIPOWER_NOD_ROOT', root / 'port/build/nod'))
    source = cache / 'source'
    cargo = Path(os.environ.get('CARGO_HOME', cache / 'cargo'))
    destination = Path(out) / 'licenses/nod'
    destination.mkdir(parents=True, exist_ok=True)
    for name in ('LICENSE-MIT', 'LICENSE-APACHE', 'Cargo.lock'):
        shutil.copy2(source / name, destination / name)
    for notice in source.glob('NOTICE*'):
        if notice.is_file():
            shutil.copy2(notice, destination / notice.name)
    shutil.copy2(root / 'port/tools/android_nod_patch.py', destination / 'bounds-patch.py')
    for registry in (cargo / 'registry/src').glob('*'):
        for crate in registry.iterdir():
            if not crate.is_dir():
                continue
            for notice in crate.rglob('*'):
                if notice.is_file() and notice.name.lower().startswith(('license', 'licence', 'copying', 'copyright', 'notice')):
                    target = destination / 'dependencies' / crate.name / notice.relative_to(crate)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(notice, target)
    (destination / 'SOURCE.md').write_text(
        'Wii disc reading uses nod, linked into the installer.\n'
        'Upstream: https://github.com/encounter/nod\n'
        'Revision: ebd80cac99b48a323200d84365e07a58cc27412d\n'
        'Licensed under MIT OR Apache-2.0; see the accompanying notices.\n'
        'The bounds patch and Cargo.lock identify the local changes and dependencies.\n', encoding='utf-8')
