#!/usr/bin/env python3
"""Pack numbered game assets into seekable AFS packages without compression.

Usage: python3 port/tools/pack_gamedata.py [gamedata] [--keep-loose]
Only pzs*/NNNNN.bin assets are packed. Disc executables, mods, translations and
custom resources remain loose. Existing packages are merged, preserving entries
when only an optional language subset was re-extracted.
"""
import argparse
import os
from pathlib import Path
import shutil
import struct
import tempfile


def pack_folder(folder, keep_loose=False):
    files = sorted(p for p in folder.glob('*.bin') if len(p.stem) == 5 and p.stem.isdecimal())
    if not files:
        return 0
    package = folder.with_suffix('.afs')
    entries = {}
    old = None
    try:
        if package.exists():
            old = package.open('rb')
            magic, count = struct.unpack('<4sI', old.read(8))
            if magic != b'AFS\0' or count > 100000:
                raise ValueError(f'Invalid AFS: {package.name}')
            table = old.read(count * 8)
            if len(table) != count * 8:
                raise ValueError('Truncated AFS index')
            size = package.stat().st_size
            for i in range(count):
                offset, length = struct.unpack_from('<II', table, i * 8)
                if offset == 0 and length == 0:
                    continue
                if offset < 8 + count * 8 or offset + length > size:
                    raise ValueError('Invalid AFS entry bounds')
                entries[i] = (offset, length)
        replacements = {int(p.stem): p for p in files}
        count = max(set(entries) | set(replacements)) + 1
        offset = (8 + count * 8 + 2047) // 2048 * 2048
        table = []
        for i in range(count):
            length = replacements[i].stat().st_size if i in replacements else entries[i][1] if i in entries else None
            if length is None:
                table.append((0, 0))
            else:
                if offset + length > 0xffffffff:
                    raise ValueError('AFS package exceeds its 4 GiB format limit')
                table.append((offset, length))
                offset = (offset + length + 2047) // 2048 * 2048
        fd, temporary = tempfile.mkstemp(prefix=package.name + '.', suffix='.tmp', dir=package.parent)
        try:
            with os.fdopen(fd, 'wb') as out:
                out.write(struct.pack('<4sI', b'AFS\0', count))
                for entry in table:
                    out.write(struct.pack('<II', *entry))
                for i, (offset, length) in enumerate(table):
                    if offset == 0:
                        continue
                    out.seek(offset)
                    if i in replacements:
                        with replacements[i].open('rb') as source:
                            shutil.copyfileobj(source, out)
                    else:
                        old.seek(entries[i][0])
                        remaining = length
                        while remaining:
                            block = old.read(min(1024 * 1024, remaining))
                            if not block:
                                raise ValueError('Truncated existing package')
                            out.write(block)
                            remaining -= len(block)
                    if out.tell() != offset + length:
                        raise ValueError('Source asset changed while packing')
                    # Ensure a zero-length final entry still has an in-bounds offset.
                    if length == 0:
                        out.truncate(offset)
                out.flush()
                os.fsync(out.fileno())
            if old:
                old.close()
                old = None
            os.replace(temporary, package)
        finally:
            Path(temporary).unlink(missing_ok=True)
        if not keep_loose:
            for p in files:
                p.unlink()
            try:
                folder.rmdir()
            except OSError:
                pass
        return len(files)
    finally:
        if old:
            old.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path, nargs='?', default=Path('gamedata'))
    parser.add_argument('--keep-loose', action='store_true')
    args = parser.parse_args()
    if not args.data.is_dir():
        parser.error('Game-data directory does not exist')
    for folder in sorted(args.data.glob('pzs*')):
        if folder.is_dir():
            count = pack_folder(folder, args.keep_loose)
            if count:
                print(f'{folder.name}.afs: packed {count} assets')


if __name__ == '__main__':
    main()
