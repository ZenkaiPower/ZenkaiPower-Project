#!/usr/bin/env python3
"""Package the source tree actually used for both DolphinTool builds."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tarfile
import tempfile
import zipfile


def checksum(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--linux', type=Path, required=True)
    parser.add_argument('--windows', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    records = json.loads((args.source / 'SOURCES.json').read_text())
    if records[0]['revision'] != '3a5b3743b18da3e8f04cbc44f20dc50fbd3a9ef3':
        raise ValueError('This recipe packages the recorded Dolphin 2609b source revision')
    args.out.mkdir(parents=True, exist_ok=True)
    recipes = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix='dolphin-package-', dir=args.out) as tmp:
        root = Path(tmp) / 'dolphin-tool-source'
        root.mkdir()
        shutil.copytree(recipes, root / 'recipes')
        for name in ('linux-packages.txt', 'compiler-version.txt', 'cmake-version.txt'):
            evidence = args.linux.parent / name
            if evidence.is_file():
                (root / 'build-info').mkdir(exist_ok=True)
                shutil.copy2(evidence, root / 'build-info' / name)
        (root / 'BUILD.json').write_text(json.dumps({
            'dolphin_revision': records[0]['revision'],
            'upstream_tag': '2609b',
            'input_zip_sha256': '2d091225e7f9f8809ee758c9aefcb69d470a211360ec6e12400b6eb93cda88bd',
            'patches': ['recipes/msvc-2022.patch'],
            'linux-x64': {'dolphin-tool': checksum(args.linux)},
            'windows-x64': {'DolphinTool.exe': checksum(args.windows)},
            'linux_environment': 'Ubuntu 22.04, GCC 12, CMake 3.31.6',
            'windows_environment': 'Visual Studio 2022, MSVC 19.44.35213.0, Windows SDK 10.0.26100.0, CMake 3.31.6-msvc6',
        }, indent=2) + '\n', encoding='utf-8')
        (root / 'README.md').write_text('''# DolphinTool corresponding source

This bundle contains the source used to rebuild the ZenkaiPower Linux and
Windows disc tools from Dolphin's 2609b release. BUILD.json identifies the
resulting binaries. dolphin/SOURCES.json records the upstream source and
dependency revisions. Dolphin and dependency license notices remain in their
original source directories. No game disc data is included.

The initial Dolphin source was supplied as `dolphin 2609b source code.zip`.
Its archive directory uses the annotated tag object 847cc451; the actual
source commit is 3a5b3743b18da3e8f04cbc44f20dc50fbd3a9ef3.
Dependency source submodules are included. The prebuilt GUI/media-only Qt
and FFmpeg-bin submodules are omitted; both are disabled for these builds.
The only source patch is recipes/msvc-2022.patch: the precompiled-header
compiler-version gate accepts MSVC 19.44 instead of requiring MSVC 19.51.
It is already applied to dolphin/Source/PCH/pch.h. Both binaries were built
from this tree. Build directories are separate.

## Windows x64

Install Visual Studio 2022 C++ Build Tools, Windows 10/11 SDK, Python 3 and
CMake 3.25 or newer (the CMake bundled with Visual Studio also works).
From this directory, run PowerShell:

    ./recipes/windows.ps1

The executable is generated below build-windows/Binaries. Microsoft Visual
C++ runtime DLLs distributed with ZenkaiPower are separate redistributables;
their license and provenance are retained in the game's Dolphin notices.

## Linux x64

Install Docker. From this directory, create an output directory, then run:

    mkdir -p output
    docker build --platform linux/amd64 -f recipes/linux.Dockerfile -t zenkaipower-dolphin-build .
    docker run --rm --platform linux/amd64 -v "$PWD/dolphin:/source:ro" -v "$PWD/recipes:/recipes:ro" -v "$PWD/output:/output" -v dolphin-2609b-build:/build zenkaipower-dolphin-build

The output is output/dolphin-tool. The recipe uses bundled source dependencies
and static GCC C++ runtimes. System glibc and libm remain dynamic dependencies.
BUILD_JOBS defaults to 1 to fit small Docker VMs; raise it using Docker -e.
Use a fresh build volume when switching source revisions or toolchains.

recipes/options.txt contains the options shared by both builds.
The source archive can be rebuilt with recipes/package_source.py.
''', encoding='utf-8')
        members = [(p, 'dolphin-tool-source/dolphin/' + p.relative_to(args.source).as_posix())
                   for p in sorted(args.source.rglob('*'))
                   if '.git' not in p.relative_to(args.source).parts and (p.is_file() or p.is_symlink())]
        members += [(p, 'dolphin-tool-source/' + p.relative_to(root).as_posix())
                    for p in sorted(root.rglob('*')) if p.is_file()]
        for name in ('dolphin-tool-source.zip', 'dolphin-tool-source.tar.gz'):
            target = args.out / name
            partial = target.with_name(name + '.part')
            if name.endswith('.zip'):
                with zipfile.ZipFile(partial, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
                    for path, arcname in members:
                        if path.is_symlink():
                            info = zipfile.ZipInfo(arcname)
                            info.create_system = 3
                            info.external_attr = 0o120777 << 16
                            archive.writestr(info, os.readlink(path))
                        else:
                            archive.write(path, arcname)
                with zipfile.ZipFile(partial) as archive:
                    if archive.testzip() is not None:
                        raise ValueError('ZIP integrity check failed')
            else:
                with tarfile.open(partial, 'w:gz', compresslevel=6) as archive:
                    for path, arcname in members:
                        archive.add(path, arcname=arcname, recursive=False)
            partial.replace(target)
            target.with_name(name + '.sha256').write_text(checksum(target) + '  ' + name + '\n')
            print(f'Created {target} ({target.stat().st_size} bytes)', flush=True)


if __name__ == '__main__':
    main()
