#!/usr/bin/env python3
"""Extract the title logo, soundtrack, opening and staff credits from the player's Japanese PS2 disc."""
import argparse
import pathlib
import struct
import sys
import tempfile

from data_package import pack_directory

from extract_eu import iso_file, u32
from make_fingerprint import bpe_decode, pack_lenient

JP_BGM_FIRST = 63602
JP_BGM_COUNT = 24


def afs_entry(disc, archive, index, limit):
    location = iso_file(disc, archive)
    if location is None:
        raise ValueError(f'missing {archive}')
    base, size = location
    disc.seek(base)
    head = disc.read(8)
    if len(head) != 8 or head[:4] != b'AFS\0':
        raise ValueError(f'invalid {archive}')
    count = u32(head, 4)
    if index >= count or 8 + count * 8 > size:
        raise ValueError('invalid archive index')
    disc.seek(base + 8 + index * 8)
    record = disc.read(8)
    if len(record) != 8:
        raise ValueError('disc image is cut short')
    offset, length = struct.unpack('<II', record)
    if offset < 8 + count * 8 or offset + length > size or length > limit:
        raise ValueError('invalid archive entry bounds')
    disc.seek(base + offset)
    blob = disc.read(length)
    if len(blob) != length:
        raise ValueError('disc image is cut short')
    return blob


def screen(blob, member):
    archive = pack_lenient(blob)
    packed = archive[member - 1][1] if archive and len(archive) >= member else None
    raw = bpe_decode(packed) if packed else None
    result = pack_lenient(raw) if raw else None
    if not result:
        raise ValueError('unexpected Japanese menu layout')
    return result


def extract(iso, data):
    # Validate every input before changing an existing installation.
    assets = {}
    with open(iso, 'rb') as disc:
        if iso_file(disc, 'SLPS_258.15') is None:
            raise ValueError('expected the Japanese PS2 release (SLPS-25815)')
        title = screen(afs_entry(disc, 'DATA/PZS3JP1.AFS', 448, 4 * 1024 * 1024), 1)
        logo = title[4][1] if len(title) == 14 else None
        if not logo or len(logo) != 132480 or u32(logo) != 1:
            raise ValueError('unexpected Japanese title logo layout')
        assets['title/sparking-meteor.bin'] = logo
        select = screen(afs_entry(disc, 'DATA/PZS3JP1.AFS', 447, 4 * 1024 * 1024), 2)
        for part, name in [(3, 'selected'), (4, 'normal')]:
            names = select[part - 1][1] if len(select) >= part else None
            if not names or len(names) != 431424 or u32(names) != 4:
                raise ValueError('unexpected Japanese music name layout')
            assets[f'soundtrack/meteor/names-{name}.bin'] = names
        for slot in range(JP_BGM_COUNT):
            track = afs_entry(disc, 'DATA/PZS3JP2.AFS', JP_BGM_FIRST + slot, 8 * 1024 * 1024)
            if (len(track) < 2048 or track[:8] != bytes.fromhex('800007fc03120402') or
                    int.from_bytes(track[8:12], 'big') != 24000 or track[36:40] != b'\0\0\0\1'):
                raise ValueError(f'unexpected Japanese music format in slot {slot}')
            assets[f'soundtrack/meteor/{slot:02d}.adx'] = track
        # Each movie picture and its separate soundtrack must be installed together.
        for name, limit, signature in [('ZS3JPED.PSS', 160 * 1024 * 1024, b'\0\0\1\xba'),
                                       ('ZS3JPED.ADX', 8 * 1024 * 1024, b'\x80\0'),
                                       ('ZS3JPOP.PSS', 160 * 1024 * 1024, b'\0\0\1\xba'),
                                       ('ZS3JPOP.ADX', 8 * 1024 * 1024, b'\x80\0')]:
            location = iso_file(disc, f'DATA/{name}')
            if location is None:
                raise ValueError(f'missing Japanese movie file {name}')
            offset, length = location
            if length < 64 or length > limit:
                raise ValueError(f'invalid Japanese movie size: {name}')
            disc.seek(offset)
            blob = disc.read(length)
            if len(blob) != length or not blob.startswith(signature):
                raise ValueError(f'damaged Japanese movie file {name}')
            assets[f'disc/DATA/{name}'] = blob
    data = pathlib.Path(data)
    data.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='meteor-', dir=data) as temporary:
        staging = pathlib.Path(temporary)
        for relative, blob in assets.items():
            destination = staging / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(blob)
        pack_directory(staging, package=data / 'sparking-meteor.pak')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso')
    parser.add_argument('--data', default='gamedata')
    args = parser.parse_args()
    try:
        extract(args.iso, args.data)
    except (OSError, ValueError, IndexError, struct.error) as error:
        print(f'extract_jp: {error}', file=sys.stderr)
        return 1
    print('sparking-meteor.pak: title logo, soundtrack (24 tracks), music names, opening and staff credits installed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
