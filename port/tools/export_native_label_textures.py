#!/usr/bin/env python3
"""Export complete game texture sheets after the native label-font renderer replaces text.

Example:
  python3 port/tools/export_native_label_textures.py --data release/linux-x64/gamedata \
      --translations port/translations/labels --all --output port/build/native-label-textures

Each output PNG is a decoded game texture. By default, labels are rendered by
port/src/gs/menu_text.c. With --before-replacement, export one untouched crop
per labels.json entry, using that entry's exact x/y/width/height replacement box.
Crop filenames use the entry ID, with '/' changed to '__'.
"""
import argparse
import ctypes
import json
import os
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'port/tools'))
import menu_font
from data_package import read_data

LABEL_FILES = (5, 448, 449, 450, 451, 452, 453, 459, 460, 461, 463, 472, 475, 476)
EU_BANK = {'en': 0, 'es': 1, 'de': 2, 'fr': 3, 'it': 4}


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
    for index in range(2, count + 2):
        offset = u32(data, index * 4) & ~3
        if offset < previous or offset > len(data):
            return 0
        previous = offset
    return count


def part(data, index):
    start = u32(data, index * 4) & ~3
    end = u32(data, (index + 1) * 4) & ~3
    return data[start:end] if start <= end <= len(data) else b''


def is_texture_file(data):
    if len(data) < 16:
        return False
    try:
        table = u32(data, 4)
        tex0 = struct.unpack_from('<Q', data, table * 4 + 48)[0]
    except (IndexError, struct.error):
        return False
    return ((tex0 >> 20) & 63) in (19, 20) and ((tex0 >> 26) & 15) <= 9 and ((tex0 >> 30) & 15) <= 9


def fnv64(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def menu_font_choices(fonts_dir):
    entries = menu_font.font_entries(fonts_dir)
    menu = next((i for i, (_, _, roles) in enumerate(entries, 1) if menu_font.FONT_MENU in roles), None)
    battle = next((i for i, (_, _, roles) in enumerate(entries, 1) if menu_font.FONT_BATTLE in roles), None)
    if menu is None or battle is None:
        raise ValueError(f'No Menu and Battle fonts found in {fonts_dir}/fonts.tsv')
    return menu, battle


def load_renderer(locale, catalog_path, labels_tsv, fonts_dir, temp_root):
    work = temp_root / locale
    (work / 'translations/labels').mkdir(parents=True)
    (work / 'labels.tsv').write_bytes(labels_tsv)
    (work / 'translations/labels' / f'{locale}.json').write_bytes(catalog_path.read_bytes())
    host = work / 'host.c'
    host.write_text('#include <stdlib.h>\nconst char *Port_FileRoot(void) { const char *p=getenv("ZENKAIPOWER_DATA"); return p?p:"gamedata"; }\nint PortSongs_JapaneseSoundtrack(void) { return 0; }\nint Port_NetSession(void) { return 0; }\n')
    library = work / 'menu_text.so'
    command = ['cc', '-shared', '-fPIC', '-O2', '-DUI_TEXT_HOST_ALLOC', str(ROOT / 'port/src/gs/menu_text.c'),
               str(ROOT / 'port/src/gs/menu_labels.c'), str(ROOT / 'port/src/gs/menu_translation.c'),
               str(ROOT / 'port/src/gs/ui_text_extract.c'), str(ROOT / 'port/src/plat_settings.c'),
               str(ROOT / 'port/src/plat_zenkai.c'), str(ROOT / 'port/src/gs/ui_text_translate.c'), str(host),
               '-o', str(library), '-lm', '-lpthread', '-lSDL3']
    subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    os.environ['ZENKAIPOWER_MENU_FONT'] = str(work)
    os.environ['ZENKAIPOWER_FONTS'] = str(fonts_dir.resolve())
    os.environ['ZENKAIPOWER_MENU_LANG'] = locale
    lib = ctypes.CDLL(str(library))
    lib.MenuText_SetFont.argtypes = [ctypes.c_int]
    lib.MenuText_SetBattleFont.argtypes = [ctypes.c_int]
    lib.MenuText_Render.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    lib.MenuText_Render.restype = ctypes.c_void_p
    return lib


def export_locale(locale, catalog_path, data_root, output, fonts_dir, label_rows, labels_tsv, temp_root, before_replacement):
    old_env = {key: os.environ.get(key) for key in ('ZENKAIPOWER_MENU_FONT', 'ZENKAIPOWER_FONTS', 'ZENKAIPOWER_MENU_LANG', 'ZENKAIPOWER_DATA')}
    os.environ['ZENKAIPOWER_DATA'] = str(data_root.resolve())
    renderer = None
    libc = None
    if not before_replacement:
        renderer = load_renderer(locale, catalog_path, labels_tsv, fonts_dir, temp_root)
        menu, battle = menu_font_choices(fonts_dir)
        renderer.MenuText_SetFont(menu)
        renderer.MenuText_SetBattleFont(battle)
        if renderer.MenuText_GetFont() != menu or renderer.MenuText_GetBattleFont() != battle:
            raise ValueError('The native label renderer could not load the configured fonts')
        libc = ctypes.CDLL(None)
        libc.free.argtypes = [ctypes.c_void_p]
    output.mkdir(parents=True, exist_ok=True)
    by_hash = {}
    for row in label_rows:
        try:
            if not row.get('enabled', True):
                continue
            by_hash.setdefault(int(row['hash'], 16), []).append(row)
        except (KeyError, ValueError):
            continue
    manifest = {}
    count = 0

    def save_crops(rows, rgba, usa_rgba, width, height):
        nonlocal count
        source_image = Image.frombytes('RGBA', (width, height), rgba)
        usa_image = Image.frombytes('RGBA', (width, height), usa_rgba)
        difference = ImageChops.difference(source_image, usa_image).convert('L').point(lambda value: 255 if value > 24 else 0)
        for row in rows:
            label_id = row['id']
            x, y = row.get('x', 0), row.get('y', 0)
            w = row.get('width', row.get('texture_width', width - x))
            h = row.get('height', 0)
            if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > width or y + h > height:
                continue
            # A localized disc word can be wider than its English source box. If the
            # EU/USA pixel difference continues immediately left of the box, include
            # that overflow so the source crop does not cut the first localized glyph.
            left = x
            blank = 0
            for px in range(x - 1, max(-1, x - 49), -1):
                if difference.crop((px, y, px + 1, y + h)).getbbox():
                    left = px
                    blank = 0
                else:
                    blank += 1
                    if blank > 3:
                        break
            crop_width = x + w - left
            safe_id = re.sub(r'[^A-Za-z0-9_.-]+', '__', label_id.replace('/', '__'))
            name = f'{safe_id}.png'
            source_image.crop((left, y, left + crop_width, y + h)).save(output / name)
            manifest[label_id] = name
            count += 1

    def walk_pair(usa, localized, prefix, depth=0):
        """Follow the same archive path in USA and EU banks; crop EU text at USA label coordinates."""
        if is_texture_file(usa) and is_texture_file(localized):
            try:
                n = u32(usa)
                eu_n = u32(localized)
            except struct.error:
                return
            if n != eu_n or not 0 < n <= 64:
                return
            for tex_index in range(n):
                for palette_index in range(n):
                    try:
                        original = menu_font.texture(usa, tex_index, palette_index)
                        if original is None:
                            continue
                        rows = by_hash.get(fnv64(original), ())
                        if not rows:
                            continue
                        translated = menu_font.texture(localized, tex_index, palette_index)
                        entry = usa[u32(usa, 4) * 4 + tex_index * 64:][:64]
                        tex0 = struct.unpack_from('<Q', entry, 48)[0]
                        width, height = 1 << ((tex0 >> 26) & 15), 1 << ((tex0 >> 30) & 15)
                    except (IndexError, struct.error, ValueError):
                        continue
                    if translated is not None and len(translated) == width * height * 4:
                        save_crops(rows, translated, original, width, height)
            return
        n = table_count(usa)
        if n == 0 or table_count(localized) != n or depth >= 5:
            return
        for index in range(1, n + 1):
            usa_child, eu_child = part(usa, index), part(localized, index)
            if usa_child and eu_child:
                walk_pair(usa_child, eu_child, f'{prefix}-s{index:03d}', depth + 1)

    def walk(blob, prefix, depth=0):
        nonlocal count
        if is_texture_file(blob):
            try:
                textures = u32(blob)
            except struct.error:
                return
            if not 0 < textures <= 64:
                return
            for index in range(textures):
                try:
                    entry = blob[u32(blob, 4) * 4 + index * 64:][:64]
                    tex0 = struct.unpack_from('<Q', entry, 48)[0]
                    width, height = 1 << ((tex0 >> 26) & 15), 1 << ((tex0 >> 30) & 15)
                    rgba = menu_font.texture(blob, index)
                except (IndexError, struct.error, ValueError):
                    continue
                if rgba is None or len(rgba) != width * height * 4:
                    continue
                rows = by_hash.get(fnv64(rgba), ())
                if not rows:
                    continue
                source_image = Image.frombytes('RGBA', (width, height), rgba)
                source = ctypes.create_string_buffer(rgba)
                out_w, out_h = ctypes.c_int(), ctypes.c_int()
                pointer = renderer.MenuText_Render(source, width, height, 1, 1, ctypes.byref(out_w), ctypes.byref(out_h))
                if not pointer:
                    continue
                try:
                    rendered = ctypes.string_at(pointer, out_w.value * out_h.value * 4)
                finally:
                    libc.free(pointer)
                image = Image.frombytes('RGBA', (out_w.value, out_h.value), rendered)
                safe_id = re.sub(r'[^A-Za-z0-9_.-]+', '__', rows[0]['id'].replace('/', '__'))
                name = f'{safe_id}--{prefix}-tex{index:02d}.png'
                image.save(output / name)
                for row in rows:
                    manifest[row['id']] = name
                count += 1
            return
        n = table_count(blob)
        if n == 0 or depth >= 5:
            return
        for index in range(1, n + 1):
            child = part(blob, index)
            if child:
                walk(child, f'{prefix}-s{index:03d}', depth + 1)

    bank = EU_BANK.get(locale)
    if before_replacement and bank is None:
        raise ValueError(f'No matching European texture bank for locale {locale!r}; supported: {", ".join(EU_BANK)}')
    for archive in LABEL_FILES:
        blob = menu_font.read_data_entry(data_root, 'pzs3us1', archive)
        sections = table_count(blob)
        if sections == 0:
            continue
        if before_replacement:
            eu_entry = 5 + bank if archive == 5 else 451 + (archive - 448) + bank * 31
            eu_blob = menu_font.read_data_entry(data_root, 'pzs3eu1', eu_entry)
            if table_count(eu_blob) != sections:
                continue
        for section_index in range(1, sections + 1):
            member = part(blob, section_index)
            if archive != 5:
                try:
                    member = menu_font.unpack(member)
                except (ValueError, IndexError, struct.error):
                    continue
            prefix = f'pzs3us1-{archive:05d}-m{section_index:03d}'
            if before_replacement:
                eu_member = part(eu_blob, section_index)
                if archive != 5:
                    try:
                        eu_member = menu_font.unpack(eu_member)
                    except (ValueError, IndexError, struct.error):
                        continue
                walk_pair(member, eu_member, prefix)
            else:
                walk(member, prefix)
    (output / 'labels.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if before_replacement:
        count = len(manifest)
    state = 'untouched per-entry source crops' if before_replacement else 'native-rendered texture sheets'
    print(f'[{locale}] Exported {count} {state} to {output}')
    for key, value in old_env.items():
        if value is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = value


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--data', type=pathlib.Path, default=ROOT / 'gamedata', help='game data root with pzs3us1 data')
    parser.add_argument('--labels', type=pathlib.Path, help='generated labels.json; default: read from game-text.pak')
    parser.add_argument('--translations', type=pathlib.Path, default=ROOT / 'port/translations/labels', help='locale catalog or catalog directory')
    parser.add_argument('--all', action='store_true', help='export each *.json in --translations')
    parser.add_argument('--fonts', type=pathlib.Path, default=ROOT / 'port/third_party/fonts')
    parser.add_argument('--before-replacement', action='store_true', help='save source textures before native font replacement')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'port/build/native-label-textures')
    args = parser.parse_args()
    data_root = menu_font.resolve_data_root(args.data)
    if args.labels:
        labels_data = args.labels.read_bytes()
        labels_tsv = read_data(args.labels.with_name('labels.tsv'))
    else:
        labels_data = read_data(data_root / 'game-text' / 'labels.json')
        labels_tsv = read_data(data_root / 'game-text' / 'labels.tsv')
    layout = json.loads(labels_data.decode('utf-8'))
    source = args.translations
    catalogs = sorted(source.glob('*.json')) if args.all else [source]
    if not catalogs:
        raise SystemExit(f'No locale catalogs found in {source}')
    with tempfile.TemporaryDirectory(prefix='zenkaipower-native-labels-') as temporary:
        temp_root = pathlib.Path(temporary)
        for catalog_path in catalogs:
            locale = json.loads(catalog_path.read_text(encoding='utf-8')).get('locale') or catalog_path.stem
            export_locale(locale, catalog_path, data_root, args.output / locale, args.fonts,
                          layout.get('labels', []), labels_tsv, temp_root, args.before_replacement)


if __name__ == '__main__':
    main()
