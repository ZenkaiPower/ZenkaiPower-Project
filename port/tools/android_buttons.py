#!/usr/bin/env python3
"""Decode the user's disc button artwork for the Android touch controller."""
import argparse
import struct
import zlib
from pathlib import Path
import menu_font

# FontIcon_Init: boot file (global ID 5 -> AFS index 4), section 8.
# The same textures are used by FontIcon and the battle recovery prompts.
ICONS = {'select': 8, 'start': 9, 'circle': 20, 'square': 21,
         'triangle': 22, 'cross': 23, 'l1': 14, 'l2': 15,
         'l3': 16, 'r1': 17, 'r2': 18, 'r3': 19, 'up': 4, 'down': 5}


def png_cell(rgba, width, column):
    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload +
                struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))
    rows = bytearray()
    for y in range(32):
        rows.append(0)  # PNG filter: None.
        start = (y * width + column * 32) * 4
        row = bytearray(rgba[start:start + 128])
        if len(row) != 128:
            raise ValueError('Truncated game button texture')
        # PS2 alpha 0x80 is opaque.
        for a in range(3, len(row), 4):
            row[a] = min(255, row[a] * 2)
        rows.extend(row)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def analog_images(hud):
    """Full 64x64 HUD poses retain a common pivot; never crop poses separately."""
    prompts = menu_font.section(hud, 2)
    for frame in range(6):
        rgba = menu_font.texture(prompts, frame + 1)
        if rgba is None or len(rgba) != 64 * 64 * 4:
            raise ValueError(f'Cannot decode game analog pose {frame}')
        # Match the button PNG/alpha conversion while preserving the full pose.
        def chunk(kind, payload):
            return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
        rows = bytearray()
        for y in range(64):
            row = bytearray(rgba[y*256:(y+1)*256])
            for a in range(3, len(row), 4): row[a] = min(255, row[a]*2)
            rows.extend(b'\0' + row)
        yield f'analog-{frame}.png', (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 64, 8, 6, 0, 0, 0)) +
                                     chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def button_images(data):
    boot = menu_font.read_data_entry(data, 'pzs3us1', 4)
    icons = menu_font.section(boot, 8)
    for name, index in ICONS.items():
        tex0 = struct.unpack_from('<Q', icons, menu_font.u32(icons, 4) * 4 + index * 64 + 48)[0]
        width, height = 1 << ((tex0 >> 26) & 15), 1 << ((tex0 >> 30) & 15)
        rgba = menu_font.texture(icons, index)
        if rgba is None:
            raise ValueError(f'Cannot decode game button {name}')
        if width < 32 or height < 32:
            padded = bytearray(32 * 32 * 4)
            for row in range(min(height, 32)):
                count = min(width, 32) * 4
                padded[row * 128:row * 128 + count] = rgba[row * width * 4:row * width * 4 + count]
            rgba, width = padded, 32
        yield name + '.png', png_cell(rgba, width, 0)
        if index in (20, 21, 22, 23):
            yield name + '-pressed.png', png_cell(rgba, width, 1)
    yield from analog_images(menu_font.read_data_entry(data, 'pzs3us1', 5))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, png in button_images(args.data):
        (args.output / name).write_bytes(png)
