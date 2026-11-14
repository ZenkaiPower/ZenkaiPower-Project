#!/usr/bin/env python3
"""Export localized Dragon Net Battle text artwork as per-entry PNG crops.

Example:
  python3 port/tools/export_wii_menu_textures.py --data gamedata/dragon-net-battle \
      --output port/build/dragon-net-textures

Reads loose extracted files or dragon-net-battle.pak. Output is split by
language and named from IDs in port/translations/port/en.json.
"""
import argparse
import io
import json
import pathlib
import struct

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
LOCALES = ('en', 'es', 'de', 'fr', 'it')

# (translation ID, source texture relative to languages/<locale>, text row).
# A None row means the texture contains one text item. Row numbers count
# non-empty alpha bands from top to bottom for label atlases.
TEXT_CROPS = (
    ('dragon-net/home/title', 'resource-004-textures/texture-001.png', None),
    ('dragon-net/home/friends', 'resource-004-textures/texture-002.png', None),
    ('dragon-net/home/settings', 'resource-004-textures/texture-003.png', None),
    ('dragon-net/friends/list', 'resource-004-textures/texture-004.png', None),
    ('dragon-net/friends/code', 'resource-004-textures/texture-005.png', None),
    ('dragon-net/home/battle', 'resource-005-textures/texture-000.png', 0),
    ('dragon-net/home/friends', 'resource-005-textures/texture-000.png', 1),
    ('dragon-net/home/settings', 'resource-005-textures/texture-000.png', 2),
    ('dragon-net/friends/title', 'resource-005-textures/texture-001.png', 0),
    ('dragon-net/friends/code', 'resource-005-textures/texture-001.png', 1),
    ('dragon-net/settings/name', 'resource-005-textures/texture-002.png', 0),
    ('dragon-net/settings/initialize', 'resource-005-textures/texture-002.png', 1),
    ('dragon-net/name/title', 'resource-013-part-02-textures/texture-004.png', None),
    ('dragon-net/battle/title', 'section-01/resource-004-textures/texture-001.png', None),
    ('dragon-net/battle/custom', 'section-01/resource-003-textures/texture-029.png', 0),
    ('dragon-net/battle/normal', 'section-01/resource-003-textures/texture-029.png', 1),
    ('dragon-net/battle/friend', 'section-01/resource-003-textures/texture-029.png', 2),
    ('dragon-net/battle/ranking', 'section-01/resource-003-textures/texture-029.png', 3),
    ('dragon-net/battle/view-ranking', 'section-01/resource-003-textures/texture-029.png', 4),
    ('dragon-net/battle/record', 'section-01/resource-003-textures/texture-029.png', 5),
    ('dragon-net/custom/title', 'section-01/resource-004-textures/texture-002.png', None),
    ('dragon-net/normal/title', 'section-01/resource-004-textures/texture-003.png', None),
)


def package_pngs(package):
    """Yield PNG paths and bytes from the runtime's named package."""
    with package.open('rb') as source:
        header = source.read(12)
        if len(header) != 12 or header[:8] != b'BT3TEXT1':
            raise ValueError(f'{package}: invalid data package')
        count, = struct.unpack('<I', header[8:])
        if count > 100000:
            raise ValueError(f'{package}: unreasonable entry count')
        records = []
        total = package.stat().st_size
        for _ in range(count):
            raw = source.read(20)
            if len(raw) != 20:
                raise ValueError(f'{package}: truncated index')
            length, offset, size = struct.unpack('<IQQ', raw)
            if not 0 < length < 1200 or offset > total or size > total - offset:
                raise ValueError(f'{package}: invalid entry bounds')
            name = source.read(length).decode('utf-8')
            path = pathlib.PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or '\\' in name:
                raise ValueError(f'{package}: unsafe entry path {name!r}')
            if len(path.parts) >= 4 and path.parts[0] == 'languages' and path.parts[1] in LOCALES and path.suffix.lower() == '.png':
                records.append((name, offset, size))
        for name, offset, size in records:
            source.seek(offset)
            data = source.read(size)
            if len(data) != size:
                raise ValueError(f'{package}: truncated entry {name}')
            yield name, data


def loose_pngs(root):
    for locale in LOCALES:
        folder = root / 'languages' / locale
        if folder.is_dir():
            for path in sorted(folder.rglob('*.png')):
                yield path.relative_to(root).as_posix(), path.read_bytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--data', type=pathlib.Path, default=ROOT / 'gamedata/dragon-net-battle',
                        help='loose extracted directory or its parent (with dragon-net-battle.pak)')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'port/build/dragon-net-text-crops')
    parser.add_argument('--translations', type=pathlib.Path, default=ROOT / 'port/translations/port/en.json',
                        help='catalog whose label IDs determine output filenames')
    args = parser.parse_args()

    root = args.data
    if (root / 'languages').is_dir():
        images = loose_pngs(root)
    elif (root / 'dragon-net-battle.pak').is_file():
        root = root / 'dragon-net-battle'
        images = package_pngs(root.with_suffix('.pak'))
    elif root.with_suffix('.pak').is_file():
        images = package_pngs(root.with_suffix('.pak'))
    else:
        parser.error(f'no loose languages/ folder or Dragon Net Battle package found at {args.data}')

    translations = json.loads(args.translations.read_text(encoding='utf-8'))
    catalog = translations.get('labels', {})
    selected = []
    seen_keys = set()
    for key, source, row in TEXT_CROPS:
        if key in catalog and key not in seen_keys:
            selected.append((key, source, row))
            seen_keys.add(key)
    if not selected:
        parser.error(f'no Dragon Net Battle text IDs from {args.translations} match the exporter map')
    source_images = {}
    for relative, data in images:
        path = pathlib.PurePosixPath(relative)
        if len(path.parts) >= 4 and path.parts[0] == 'languages' and path.parts[1] in LOCALES:
            source_images[(path.parts[1], '/'.join(path.parts[2:]))] = data

    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {'source': str(args.data), 'translation_catalog': str(args.translations), 'locales': {}}
    count = 0
    for locale in LOCALES:
        locale_rows = []
        for key, source, row in selected:
            data = source_images.get((locale, source))
            if data is None:
                continue
            try:
                with Image.open(io.BytesIO(data)) as opened:
                    image = opened.convert('RGBA')
            except Exception as exc:
                raise ValueError(f'languages/{locale}/{source}: invalid PNG ({exc})') from exc
            if row is None:
                box = image.getchannel('A').getbbox()
            else:
                # Regional strings may touch adjacent alpha bands. Each atlas
                # entry still has a fixed 32px slot, so crop it independently.
                y0, y1 = row * 32, min((row + 1) * 32, image.height)
                local = image.getchannel('A').crop((0, y0, image.width, y1)).getbbox()
                box = (local[0], local[1] + y0, local[2], local[3] + y0) if local else None
            if not box:
                continue
            crop = image.crop(box)
            filename = key.replace('/', '_') + '.png'
            destination = args.output / locale / filename
            destination.parent.mkdir(parents=True, exist_ok=True)
            crop.save(destination)
            locale_rows.append({'id': key, 'file': f'{locale}/{filename}', 'source': f'languages/{locale}/{source}',
                                'width': crop.width, 'height': crop.height})
            count += 1
        if locale_rows:
            manifest['locales'][locale] = locale_rows
    if count == 0:
        parser.error(f'no per-language Dragon Net Battle PNGs found in {args.data}; install/extract the Wii disc first')
    for files in manifest['locales'].values():
        files.sort(key=lambda item: item['file'])
    (args.output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Exported {count} localized Dragon Net Battle text crops across {len(manifest["locales"])} locales to {args.output}')


if __name__ == '__main__':
    main()
