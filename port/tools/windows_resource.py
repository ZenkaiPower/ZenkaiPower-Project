#!/usr/bin/env python3
"""Convert zp-avatar.png and compile the shared Windows icon resource (MinGW/MSYS2)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import builddeps

ROOT = Path(__file__).resolve().parents[2]


def build_icon(output, compiler):
    output = Path(output)
    source = ROOT / 'assets/zp-avatar.png'
    metadata = ROOT / 'game-info.json'
    info = json.loads(metadata.read_text(encoding='utf-8'))
    title = info.get('title') or 'ZenkaiPower'
    if not isinstance(title, str) or any(ord(c) < 32 for c in title):
        raise ValueError('game-info.json title must be a single-line string')
    resource_title = title.replace('\\', '\\\\').replace('"', '\\"')
    inputs = [Path(__file__), source, metadata]
    signature = builddeps.fingerprint(inputs, {
        'compiler': compiler,
        'png_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    })
    stamp = output.with_suffix('.resource-settings')
    icon = output.with_suffix('.ico')
    resource = output.with_suffix('.rc')
    if output.is_file() and icon.is_file() and resource.is_file() and stamp.is_file() and stamp.read_text() == signature:
        return output
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit('Windows icon generation requires Pillow: install python3-pil or run python3 -m pip install Pillow')
    output.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(source) as image:
        image = image.convert('RGBA')
        bounds = image.getchannel('A').getbbox()
        if bounds is None:
            raise ValueError('zp-avatar.png is fully transparent')
        image = image.crop(bounds)
        scale = 240 / max(image.size)
        image = image.resize((max(1, round(image.width * scale)),
                              max(1, round(image.height * scale))),
                             getattr(Image, 'Resampling', Image).LANCZOS)
        canvas = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
        canvas.alpha_composite(image, ((256 - image.width) // 2, (256 - image.height) // 2))
        canvas.save(icon, format='ICO', sizes=[(16, 16), (24, 24), (32, 32), (48, 48),
                                             (64, 64), (128, 128), (256, 256)], bitmap_format='bmp')
    icon_path = icon.resolve().as_posix().replace('"', '\\"')
    resource.write_text(f'''// Generated from zp-avatar.png and game-info.json.
#pragma code_page(65001)
1 ICON "{icon_path}"
1 VERSIONINFO
FILEVERSION 1,0,0,0
PRODUCTVERSION 1,0,0,0
FILEOS 0x40004
FILETYPE 0x1
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "FileDescription", "{resource_title}\\0"
            VALUE "ProductName", "{resource_title}\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
''', encoding='utf-8')
    temporary = output.with_suffix('.o.tmp')
    subprocess.run([compiler, '-i', str(resource.resolve()), '-O', 'coff', '-o', str(temporary.resolve())],
                   cwd=ROOT, check=True)
    temporary.replace(output)
    stamp.write_text(signature)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--windres', default='x86_64-w64-mingw32-windres')
    args = parser.parse_args()
    build_icon(args.out, args.windres)
