#!/usr/bin/env python3
"""Generate Android launcher resources from the shared project avatar."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def generate_icons(source, output):
    from PIL import Image

    output = Path(output)
    with Image.open(source) as original:
        image = original.convert('RGBA')
        bounds = image.getchannel('A').getbbox()
        if bounds is None:
            raise ValueError('The Android icon source is fully transparent')
        image = image.crop(bounds)

    def write_icon(relative, size, artwork_size):
        ratio = artwork_size / max(image.size)
        artwork = image.resize((max(1, round(image.width * ratio)),
                                max(1, round(image.height * ratio))), Image.Resampling.LANCZOS)
        canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        canvas.alpha_composite(artwork, ((size-artwork.width)//2, (size-artwork.height)//2))
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(target)

    for density, size in [('mdpi', 48), ('hdpi', 72), ('xhdpi', 96),
                          ('xxhdpi', 144), ('xxxhdpi', 192)]:
        write_icon(f'mipmap-{density}/ic_launcher.png', size, round(size * 0.9))
    # 108 dp adaptive canvas at xxxhdpi, with artwork inside the 66 dp safe zone.
    # Android applies its own launcher mask; keep the existing background/XML.
    write_icon('drawable-nodpi/ic_launcher_foreground.png', 432, 264)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'assets/zp-avatar.png')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate_icons(args.source, args.output)
