#!/usr/bin/env python3
"""Export translated menu/battle labels as transparent PNGs.

Example:
  python3 port/tools/export_label_pngs.py --labels gamedata/game-text/labels.json \
      --translations port/translations/labels/en.json --output port/build/labels-en
  python3 port/tools/export_label_pngs.py --labels gamedata/game-text/labels.json --all

The label IDs and layout come from labels.json; translated wording comes from
the selected translations catalog. IDs use '/' as '__' in filenames.
Requires Pillow (python -m pip install Pillow).
"""
import argparse
import json
import pathlib
import re

from PIL import Image, ImageDraw, ImageFont

from data_package import read_data

ROOT = pathlib.Path(__file__).resolve().parents[2]


def rgb(value, fallback):
    if isinstance(value, list) and len(value) == 3 and all(type(c) is int and 0 <= c <= 255 for c in value):
        return tuple(value)
    return fallback


def draw_label(row, text, styles, font_path, scale):
    width = row.get('width', row.get('texture_width', 512))
    height = row['height']
    image = Image.new('RGBA', (width * scale, height * scale), (0, 0, 0, 0))
    if not text:
        return image
    palette = styles.get(row.get('color_style'), {})
    top = rgb(palette.get('top'), (255, 255, 255))
    bottom = rgb(palette.get('bottom'), top)
    outline = rgb(palette.get('outline'), (0, 0, 0))
    outline_width = max(0, round(float(palette.get('outline_width', 1.5)) * scale))
    margin = max(0, round(row.get('margin', 0) * scale))
    cap = max(1, round(row.get('cap_height', height) * scale))
    font = ImageFont.truetype(str(font_path), cap)
    lines = text.split('\n')[:2]
    draw = ImageDraw.Draw(image)
    line_height = height * scale // len(lines)
    for index, line in enumerate(lines):
        # Fit each line inside the label's usable width, retaining its catalog font size when possible.
        while font.size > 1 and draw.textbbox((0, 0), line, font=font, stroke_width=outline_width)[2] > width * scale - margin * 2:
            font = ImageFont.truetype(str(font_path), font.size - 1)
        bounds = draw.textbbox((0, 0), line, font=font, stroke_width=outline_width)
        text_width = bounds[2] - bounds[0]
        x = (width * scale - text_width) // 2 if row.get('align') == 'center' else margin - bounds[0]
        y = index * line_height + (line_height - (bounds[3] - bounds[1])) // 2 - bounds[1]
        draw.text((x, y), line, font=font, fill=top, stroke_width=outline_width, stroke_fill=outline)
        # Color the glyph fill with the style's vertical gradient while keeping its outline.
        if top != bottom:
            mask = Image.new('L', image.size, 0)
            ImageDraw.Draw(mask).text((x, y), line, font=font, fill=255)
            gradient = Image.new('RGBA', image.size)
            gd = ImageDraw.Draw(gradient)
            y0, y1 = index * line_height, min((index + 1) * line_height, image.height)
            for py in range(y0, y1):
                t = (py - y0) / max(1, y1 - y0 - 1)
                color = tuple(round(a * (1 - t) + b * t) for a, b in zip(top, bottom))
                gd.line((0, py, image.width, py), fill=(*color, 255))
            image.alpha_composite(Image.composite(gradient, Image.new('RGBA', image.size), mask))
    if scale > 1:
        image = image.resize((width, height), Image.Resampling.LANCZOS)
    return image


def export_catalog(layout, catalog_path, output, font_path, scale):
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    locale = catalog.get('locale') or catalog_path.stem
    output.mkdir(parents=True, exist_ok=True)
    styles = layout.get('color_styles', {})
    entries = catalog.get('labels', {})
    exported = skipped = 0
    for row in layout.get('labels', []):
        label_id = row.get('id', '')
        entry = entries.get(label_id)
        if not label_id or not row.get('enabled', True) or not isinstance(entry, dict):
            skipped += 1
            continue
        text = entry.get('text') or entry.get('original') or row.get('original', '')
        if not text:
            skipped += 1
            continue
        image = draw_label(row, text, styles, font_path, scale)
        filename = re.sub(r'[^A-Za-z0-9_.-]+', '_', label_id.replace('/', '__')) + '.png'
        image.save(output / filename)
        exported += 1
    print(f'[{locale}] Exported {exported} PNGs to {output} ({skipped} labels skipped).')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--labels', type=pathlib.Path, required=True, help='generated game-text labels.json')
    translation_group = parser.add_mutually_exclusive_group()
    translation_group.add_argument('--translations', type=pathlib.Path, default=ROOT / 'port/translations/labels/en.json')
    translation_group.add_argument('--all', action='store_true', help='export every *.json catalog in port/translations/labels')
    parser.add_argument('--translations-dir', type=pathlib.Path, default=ROOT / 'port/translations/labels', help='catalog folder used by --all')
    parser.add_argument('--fonts', type=pathlib.Path, default=ROOT / 'port/third_party/fonts')
    parser.add_argument('--font', help='font file relative to --fonts (default: first Menu font in fonts.tsv)')
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'port/build/label-pngs')
    parser.add_argument('--scale', type=int, default=3, help='render scale before downsampling (default: 3)')
    args = parser.parse_args()
    if not 1 <= args.scale <= 8:
        parser.error('--scale must be from 1 to 8')
    try:
        label_data = read_data(args.labels)
    except FileNotFoundError:
        # Source checkouts may already contain a packaged release with the generated catalog.
        label_data = None
        for package in sorted((ROOT / 'release').glob('*/gamedata/game-text.pak')):
            candidate = package.parent / 'game-text' / 'labels.json'
            try:
                label_data = read_data(candidate)
                print(f'Using packaged label layout: {package}')
                break
            except FileNotFoundError:
                continue
        if label_data is None:
            raise SystemExit(
                f'No label layout at {args.labels}, and no labels.json was found in a release game-text.pak. '
                'Start the game once with its game data installed to generate labels.json, then rerun this command.'
            )
    layout = json.loads(label_data.decode('utf-8'))
    fonts = args.fonts
    font_rel = args.font
    if not font_rel:
        for line in (fonts / 'fonts.tsv').read_text(encoding='utf-8').splitlines():
            if line and not line.startswith('#'):
                fields = line.split('\t')
                use = fields[2].strip() if len(fields) > 2 else '3'
                if use.isdigit() and int(use) & 1:
                    font_rel = fields[0]
                    break
    if not font_rel:
        raise ValueError('No Menu font found; supply --font')
    font_path = fonts / font_rel
    catalogs = sorted(args.translations_dir.glob('*.json')) if args.all else [args.translations]
    if not catalogs:
        raise SystemExit(f'No locale catalogs found in {args.translations_dir}')
    for catalog_path in catalogs:
        catalog_output = args.output / (json.loads(catalog_path.read_text(encoding='utf-8')).get('locale') or catalog_path.stem) if args.all else args.output
        export_catalog(layout, catalog_path, catalog_output, font_path, args.scale)


if __name__ == '__main__':
    main()
