#!/usr/bin/env python3
"""Export the five battle HUD banks and adjacent pause bank from installed file 00005.bin.

python port/tools/export_battle_hud_textures.py --file port/build/battle-hud-reference/source/00005.bin
Original assets and generated previews stay in the ignored port/build directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import re

from PIL import Image, ImageDraw, ImageFont
import menu_font

ROOT = Path(__file__).resolve().parents[2]
BANKS = [(0, 1, 'gauges'), (1, 4, 'announcements'), (2, 5, 'combo'), (3, 2, 'prompts'), (4, 3, 'captions'),
         ('P', 6, 'pause-parts')]


def display_rgba(rgba, size):
    image = Image.frombytes('RGBA', size, rgba)
    image.putalpha(image.getchannel('A').point(lambda a: min(255, (a * 255 + 64) // 128)))
    return image


def contact(items, destination, title, columns=4):
    cell_w, cell_h = (560, 185) if columns == 2 else (300, 190)
    rows = (len(items) + columns - 1) // columns
    canvas = Image.new('RGB', (columns * cell_w, rows * cell_h + 50), '#171c24')
    draw = ImageDraw.Draw(canvas)
    try:
        font = ImageFont.truetype('DejaVuSans.ttf', 14)
        heading = ImageFont.truetype('DejaVuSans.ttf', 21)
    except OSError:
        font = heading = ImageFont.load_default()
    draw.text((14, 12), title, font=heading, fill='white')
    for n, (image, label) in enumerate(items):
        x, y = n % columns * cell_w, n // columns * cell_h + 50
        draw.text((x+10, y+7), label, font=font, fill='#e7edf6')
        box = (x+8, y+34, x+cell_w-8, y+cell_h-8)
        for yy in range(box[1], box[3], 12):
            for xx in range(box[0], box[2], 12):
                color = '#303946' if ((xx-box[0])//12+(yy-box[1])//12) % 2 else '#252c36'
                draw.rectangle((xx, yy, min(xx+11, box[2]), min(yy+11, box[3])), fill=color)
        factor = min((cell_w-28)/image.width, (cell_h-55)/image.height, 3)
        preview = image.resize((max(1, round(image.width*factor)), max(1, round(image.height*factor))), Image.Resampling.NEAREST).convert('RGBA')
        canvas.paste(preview, (x+(cell_w-preview.width)//2, y+36+(cell_h-48-preview.height)//2), preview)
    canvas.save(destination)
    return canvas


def export(source, output):
    raw = source.read_bytes()
    output.mkdir(parents=True, exist_ok=True)
    (output/'textures').mkdir(exist_ok=True)
    manifest = {'archive': 'pzs3us1.afs', 'file_id': 5, 'source_sha256': hashlib.sha256(raw).hexdigest(), 'banks': [],
                'source_calls': []}
    for source_file in sorted((ROOT/'src/battle').glob('hud_[a-e]*.c')):
        code = source_file.read_text(encoding='utf-8')
        for call in re.finditer(r'^\s*HudSprite_(InitTex|SetTex|SetUv)\((.*?)\);', code, re.M | re.S):
            manifest['source_calls'].append({'file': source_file.relative_to(ROOT).as_posix(),
                                             'line': code.count('\n', 0, call.start())+1,
                                             'operation': call[1], 'arguments': ' '.join(call[2].split())})
    overview = []
    for bank, outer, name in BANKS:
        bank_name = f'R{bank}' if isinstance(bank, int) else bank
        start = menu_font.u32(raw, outer*4) & ~3
        end = next(menu_font.u32(raw, slot*4) & ~3 for slot in range(outer+1, menu_font.u32(raw)+2)
                   if (menu_font.u32(raw, slot*4) & ~3) > start)
        data = raw[start:end]
        count = menu_font.u32(data)
        table = menu_font.u32(data, 4)*4
        record = {'bank': bank, 'section': outer, 'offset': start, 'entries': []}
        items = []
        for index in range(count):
            entry = data[table+index*64:table+(index+1)*64]
            tex0, = struct.unpack_from('<Q', entry, 48)
            width, height, psm = 1 << (tex0 >> 26 & 15), 1 << (tex0 >> 30 & 15), tex0 >> 20 & 63
            item = {'index': index, 'image_bytes': menu_font.u32(entry, 8), 'palette_bytes': menu_font.u32(entry, 12)}
            rgba = menu_font.texture(data, index)
            if rgba is None:
                if item['image_bytes'] != 0 or not item['palette_bytes']:
                    raise ValueError(f'Unsupported HUD entry {bank}:{index}')
                item['kind'] = 'palette'
                palette = menu_font.u32(entry, 4)*4+96
                upload, = struct.unpack_from('<Q', data, palette-48)
                colors = (upload & 4095) * (upload >> 32 & 4095)
                if colors not in (16, 256):
                    raise ValueError(f'Unsupported palette size {colors} at {bank}:{index}')
                rgba = bytearray()
                for color in range(colors):
                    source_index = ((color & 0xE7) | ((color & 8) << 1) | ((color & 16) >> 1)) if colors == 256 else color
                    rgba.extend(data[palette+source_index*4:palette+source_index*4+4])
                side = 4 if colors == 16 else 16
                image = display_rgba(bytes(rgba), (side, side)).resize((128, 128), Image.Resampling.NEAREST)
                filename = f'textures/{bank_name.lower()}-palette-{index:02d}.png'
                image.save(output/filename); item['png'] = filename
                items.append((image, f'{bank_name} / {index:02d}   palette only ({colors} colors)'))
            else:
                image = display_rgba(rgba, (width, height))
                filename = f'textures/{bank_name.lower()}-t{index:02d}.png'
                image.save(output/filename)
                item.update(kind='texture', width=width, height=height, psm=psm, png=filename,
                            rgba_sha256=hashlib.sha256(rgba).hexdigest())
                items.append((image, f'{bank_name} / {index:02d} (0x{index:02X})   {width} x {height}'))
            record['entries'].append(item)
        if bank == 0:
            record['palette_variants'] = []
            for base, alternatives in [(0, range(1, 3)), (8, range(9, 14))]:
                original = record['entries'][base]
                for palette in alternatives:
                    rgba = menu_font.texture(data, base, palette)
                    image = display_rgba(rgba, (original['width'], original['height']))
                    filename = f'textures/r0-t{base:02d}-p{palette:02d}.png'
                    image.save(output/filename)
                    record['palette_variants'].append({'texture': base, 'palette_entry': palette,
                                                       'sub': palette-base, 'png': filename})
                    items.append((image, f'R0 / {base:02d} + palette {palette:02d} (sub {palette-base})'))
        manifest['banks'].append(record)
        sheet = contact(items, output/f'{name}.png', f'Battle HUD - {bank_name} {name}', 2 if bank == 1 else 4)
        overview.append((sheet, f'{bank_name} - {name}'))
    contact(overview, output/'overview.png', 'Battle HUD texture banks', 2)
    (output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(f'Exported {sum(e["kind"]=="texture" for b in manifest["banks"] for e in b["entries"])} textures, '
          f'{sum(e["kind"]=="palette" for b in manifest["banks"] for e in b["entries"])} palette entries, '
          f'and {sum(len(b.get("palette_variants", [])) for b in manifest["banks"])} extra palette variants to {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--file', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'port/build/battle-hud-reference')
    args = parser.parse_args()
    export(args.file, args.output)
