#!/usr/bin/env python3
"""Export the European game's full menu/HUD texture assets as PNGs per language.

Example:
  python3 port/tools/export_eu_textures.py --data release/linux-x64/gamedata \
      --output port/build/game-textures

These are the localized textures stored on the European disc (English, Spanish,
German, French, Italian). The JSON label catalogs are not used: these PNGs are
the complete source game textures, not newly typeset label crops.
"""
import argparse
import pathlib
import struct
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'port/tools'))
import menu_font

LANGUAGES = ('en', 'es', 'de', 'fr', 'it')
USA_MENU_FIRST = 448
EU_MENU_FIRST = 451
MENU_COUNT = 31
EU_STRIDE = 31
EU_HUD_FIRST = 5


def u32(data, offset=0):
    return struct.unpack_from('<I', data, offset)[0]


def table_count(data):
    if len(data) < 12:
        return 0
    count = u32(data)
    if not 0 < count <= 16384 or (count + 2) * 4 > len(data):
        return 0
    previous = u32(data, 4) & ~3
    if previous < (count + 2) * 4:
        return 0
    for i in range(2, count + 2):
        offset = u32(data, i * 4) & ~3
        if offset < previous or offset > len(data):
            return 0
        previous = offset
    return count


def part(data, index):
    start = u32(data, index * 4) & ~3
    end = u32(data, (index + 1) * 4) & ~3
    if start > end or end > len(data):
        return b''
    return data[start:end]


def texture_file(data):
    if len(data) < 16:
        return False
    try:
        table = u32(data, 4)
        tex0 = struct.unpack_from('<Q', data, table * 4 + 48)[0]
    except (IndexError, struct.error):
        return False
    return ((tex0 >> 20) & 63) in (19, 20) and ((tex0 >> 26) & 15) <= 9 and ((tex0 >> 30) & 15) <= 9


def export_texture_file(data, output, prefix):
    try:
        count = u32(data)
    except struct.error:
        return 0
    if not 0 < count <= 64:
        return 0
    written = 0
    for index in range(count):
        try:
            entry = data[u32(data, 4) * 4 + index * 64:][:64]
            if len(entry) < 64:
                continue
            tex0 = struct.unpack_from('<Q', entry, 48)[0]
            width = 1 << ((tex0 >> 26) & 15)
            height = 1 << ((tex0 >> 30) & 15)
            rgba = menu_font.texture(data, index)
        except (IndexError, struct.error, ValueError):
            continue
        if rgba is None or len(rgba) != width * height * 4:
            continue
        name = f'{prefix}-texture-{index:02d}.png'
        Image.frombytes('RGBA', (width, height), rgba).save(output / name)
        written += 1
    return written


def walk(data, output, prefix, depth=0):
    if texture_file(data):
        return export_texture_file(data, output, prefix)
    count = table_count(data)
    if count == 0 or depth >= 5:
        return 0
    written = 0
    for index in range(1, count + 1):
        child = part(data, index)
        if child:
            written += walk(child, output, f'{prefix}-section-{index:03d}', depth + 1)
    return written


def export_entry(blob, entry, output, raw=False):
    if raw:
        unpacked = blob
    else:
        try:
            unpacked = menu_font.unpack(blob)
        except (ValueError, IndexError, struct.error):
            return 0
    return walk(unpacked, output, f'entry-{entry:05d}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--data', type=pathlib.Path, default=ROOT / 'gamedata', help='game data root with pzs3eu1.afs or pzs3eu1/')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'port/build/game-textures')
    args = parser.parse_args()
    data = menu_font.resolve_data_root(args.data)
    total = 0
    for bank, locale in enumerate(LANGUAGES):
        folder = args.output / locale
        folder.mkdir(parents=True, exist_ok=True)
        count = 0
        entries = [(EU_HUD_FIRST + bank, True)]
        entries.extend((EU_MENU_FIRST + (number - USA_MENU_FIRST) + bank * EU_STRIDE, False)
                       for number in range(USA_MENU_FIRST, USA_MENU_FIRST + MENU_COUNT))
        for entry, raw in entries:
            blob = menu_font.read_data_entry(data, 'pzs3eu1', entry)
            count += export_entry(blob, entry, folder, raw)
        if count == 0:
            raise SystemExit(f'No decodable EU textures found for {locale} in {data}/pzs3eu1')
        print(f'[{locale}] Exported {count} full texture PNGs to {folder}')
        total += count
    print(f'Exported {total} PNGs across {len(LANGUAGES)} languages.')


if __name__ == '__main__':
    main()
