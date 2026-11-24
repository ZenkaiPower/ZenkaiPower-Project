#!/usr/bin/env python3
"""Compare original HUD textures against the actual procedural renderer.

Windows (Docker is selected automatically):
  python port/tools/compare_native_hud.py
Linux with SDL3 development files, cc, pkg-config, Mesa, Xvfb and Pillow:
  python3 port/tools/compare_native_hud.py --no-docker

The source is the extracted common HUD archive entry, not the exported palette swatches.
Output defaults to ignored port/build/native-hud-comparison; open index.html afterward.
"""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = [
    ('r0-t03', 'Timer panels', 3, None),
    ('r0-t08', 'Base shadow atlas', 8, None),
    ('r0-t08-p09', 'Green gauges', 8, 9),
    ('r0-t08-p10', 'Red gauges', 8, 10),
    ('r0-t08-p11', 'Gold gauges', 8, 11),
    ('r0-t08-p12', 'Yellow gauges', 8, 12),
    ('r0-t08-p13', 'Blue gauges', 8, 13),
]


def docker_run(args):
    if not shutil.which('docker'):
        raise RuntimeError('Docker was not found. Start Docker Desktop with Linux containers, or use --no-docker on Linux.')
    image = args.docker_image
    if not image:
        image = 'zenkaipower-hud-compare'
        subprocess.run(['docker', 'build', '-t', image, '-f',
                        str(ROOT/'port/tools/native_hud_compare.Dockerfile'), str(ROOT/'port/tools')], check=True)
    # Separate mounts support input/output folders outside the checkout as well.
    command = ['docker', 'run', '--rm', '--entrypoint', 'python3',
               '--mount', f'type=bind,source={ROOT},target=/work,readonly',
               '--mount', f'type=bind,source={args.file.parent},target=/input,readonly',
               '--mount', f'type=bind,source={args.out},target=/output', '-w', '/work', image,
               'port/tools/compare_native_hud.py', '--no-docker', '--file', '/input/'+args.file.name,
               '--out', '/output', '--scale', str(args.scale), '--filter', args.filter]
    subprocess.run(command, check=True)


def capture(tmp, fixture, scale, filtering):
    # Embed the same current GLSL sources consumed by the game's OpenGL backend.
    # No Python reimplementation of the procedural geometry is used.
    arrays = []
    for path in sorted((ROOT/'port/src/gs/shaders').glob('*')):
        if path.suffix not in ('.vert', '.frag'):
            continue
        name = 'k'+path.stem.capitalize()+path.suffix[1:].capitalize()+'Glsl'
        arrays.append('static const unsigned char '+name+'[]={'+
                      ','.join(map(str, path.read_bytes()+b'\0'))+'};\n')
    # gs_gl.c also references the generated work-buffer/no-depth variants.
    original = (ROOT/'port/src/gs/shaders/gs.frag').read_text()
    for name, defines in [('Gsnz', '#define NO_FRAG_DEPTH\n'),
                          ('Gswb', '#define NO_FRAG_DEPTH\n#define GS_BLOCK\n')]:
        data = original.replace('\n', '\n'+defines, 1).encode()+b'\0'
        arrays.append('static const unsigned char k'+name+'FragGlsl[]={'+','.join(map(str, data))+'};\n')
    (tmp/'shaders.h').write_text(''.join(arrays))
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True))
    libs = shlex.split(subprocess.check_output(['pkg-config', '--libs', 'sdl3'], text=True))
    binary = tmp/'capture'
    subprocess.run(['cc', '-std=gnu99', '-O2', '-ffunction-sections', '-fdata-sections', *flags,
                    '-I'+str(ROOT/'port/include'), '-I'+str(tmp), str(ROOT/'port/tools/native_hud_capture.c'),
                    '-Wl,--gc-sections', *libs, '-lm', '-pthread', '-o', str(binary)], check=True)
    command = [str(binary), str(fixture), str(tmp), str(scale), filtering]
    if not os.environ.get('DISPLAY'):
        if not shutil.which('xvfb-run'):
            raise RuntimeError('Headless capture needs xvfb-run. Install xvfb and xauth, or run with --docker.')
        command = ['xvfb-run', '-a', *command]
    result = subprocess.run(command, check=True, capture_output=True, text=True,
                            env=dict(os.environ, XDG_RUNTIME_DIR=str(tmp)))
    print(result.stdout.strip())
    return result.stdout.strip()


def compare(original, native):
    from PIL import Image, ImageChops, ImageOps, ImageStat
    # RGB in fully transparent pixels is arbitrary. Compare premultiplied color
    # (equivalently, appearance over black) and alpha separately, over visible area.
    black = Image.new('RGBA', original.size, (0, 0, 0, 255))
    old_rgb = Image.alpha_composite(black, original).convert('RGB')
    new_rgb = Image.alpha_composite(black, native).convert('RGB')
    rgb_diff = ImageChops.difference(old_rgb, new_rgb)
    old_a, new_a = original.getchannel('A'), native.getchannel('A')
    alpha_diff = ImageChops.difference(old_a, new_a)
    active = ImageChops.lighter(old_a, new_a).point(lambda v: 255 if v else 0)
    count = active.histogram()[255]
    stat = ImageStat.Stat(rgb_diff, active)
    old_mask = old_a.point(lambda v: 255 if v >= 16 else 0)
    new_mask = new_a.point(lambda v: 255 if v >= 16 else 0)
    intersection = ImageChops.darker(old_mask, new_mask).histogram()[255]
    union = ImageChops.lighter(old_mask, new_mask).histogram()[255]
    metrics = {
        'active_pixels': count,
        'premultiplied_rgb_mae_0_255': round(sum(stat.mean)/3, 4) if count else 0,
        'premultiplied_rgb_rmse_0_255': round((sum(v*v for v in stat.rms)/3)**0.5, 4) if count else 0,
        'alpha_mae_0_255': round(ImageStat.Stat(alpha_diff, active).mean[0], 4) if count else 0,
        'max_channel_error_0_255': max([high for low, high in rgb_diff.getextrema()]+[alpha_diff.getextrema()[1]]),
        'silhouette_iou_alpha_ge_16': round(intersection/union, 6) if union else 1,
    }
    error = alpha_diff
    for channel in rgb_diff.split():
        error = ImageChops.lighter(error, channel)
    heat = ImageOps.colorize(error.point(lambda v: min(255, v*4)), '#10151d', '#ff3434')
    return metrics, heat


def previews(original, native, heat, title):
    from PIL import Image, ImageDraw
    w, h = original.size
    checker = Image.new('RGBA', (w, h), '#242b35')
    draw = ImageDraw.Draw(checker)
    for y in range(0, h, 12):
        for x in range(0, w, 12):
            if (x//12+y//12) % 2:
                draw.rectangle((x, y, x+11, y+11), fill='#37414f')
    old = Image.alpha_composite(checker, original).convert('RGB')
    new = Image.alpha_composite(checker, native).convert('RGB')
    overlay = Image.blend(old, new, .5)
    sheet = Image.new('RGB', (3*w+32, h+64), '#171c24')
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 8), title, fill='white')
    for i, (label, picture) in enumerate([('Original texture render', old), ('Native shader render', new), ('Difference x4 (color + alpha)', heat)]):
        x = 8+i*(w+8)
        draw.text((x, 32), label, fill='white')
        sheet.paste(picture, (x, 56))
    return sheet, overlay


def write_html(out, report):
    rows = []
    for item in report['cases']:
        slug, title, m = item['name'], html.escape(item['title']), item['metrics']
        rows.append(f'<section><h2>{title}</h2><p>Visible RGB MAE: {m["premultiplied_rgb_mae_0_255"]}; '
                    f'alpha MAE: {m["alpha_mae_0_255"]}; silhouette IoU: {m["silhouette_iou_alpha_ge_16"]:.3f}.</p>'
                    f'<a href="{slug}/comparison.png"><img src="{slug}/comparison.png" alt="{title}: original, native and difference"></a>'
                    f'<p><a href="{slug}/texture.png">Source texture</a> / <a href="{slug}/original.png">Original render</a> / '
                    f'<a href="{slug}/native.png">Native render</a> / <a href="{slug}/overlay.png">50% overlay</a></p></section>')
    (out/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Native HUD comparison</title>'
        '<style>body{font:16px system-ui;background:#171c24;color:#edf1f6;margin:24px;max-width:1500px}'
        'a{color:#8bc9ff}img{max-width:100%;height:auto}section{margin:32px 0}p{line-height:1.5}</style>'
        '<h1>Original textures vs native HUD shader</h1>'
        f'<p>Actual OpenGL backend; {report["scale"]}x atlas size; original texture filtering: {report["filter"]}. '
        'Both images use identical UVs, viewport and GS alpha conversion. Native rendering is forced for this comparison.</p>'
        '<p>Differences include intended antialiasing as well as shape/color changes. Lower MAE is closer; '
        'silhouette IoU of 1 means matching masks (alpha threshold 16/255). Metrics exclude fully transparent area. '
        'The difference image amplifies the largest premultiplied RGB or alpha error by four. Click images to zoom. '
        '<a href="report.json">Full metrics</a>.</p>'+''.join(rows), encoding='utf-8')


def save_png(picture, path):
    # Windows image previews can keep the old PNG mapped. Write a new file and
    # replace its directory entry instead of truncating the open image.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix='.'+path.stem+'-', suffix='.png', delete=False) as fp:
            temporary = Path(fp.name)
            picture.save(fp, format='PNG')
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def run(args):
    from PIL import Image
    import menu_font
    raw = args.file.read_bytes()
    bank = menu_font.section(raw, 1)
    report = {'scale': args.scale, 'filter': args.filter, 'backend': 'OpenGL',
              'source_sha256': hashlib.sha256(raw).hexdigest(),
              'shader_sha256': hashlib.sha256((ROOT/'port/src/gs/shaders/gs.frag').read_bytes()).hexdigest(),
              'native_shader_sha256': hashlib.sha256((ROOT/'port/src/gs/shaders/nativehud.frag').read_bytes()).hexdigest(),
              'cases': []}
    with tempfile.TemporaryDirectory(prefix='native-hud-compare-') as folder:
        tmp = Path(folder)
        fixture = tmp/'source.rgba'
        textures = []
        with fixture.open('wb') as fp:
            for kind, (slug, title, index, palette) in enumerate(CASES, 1):
                rgba = menu_font.texture(bank, index, palette)
                if rgba is None:
                    raise RuntimeError(f'Cannot decode {slug} from {args.file}')
                w, h = (64 if kind == 1 else 256), 64
                if len(rgba) != w*h*4:
                    raise RuntimeError(f'Unexpected source dimensions for {slug}')
                fp.write(struct.pack('<III', kind, w, h)+rgba)
                textures.append((w, h, rgba))
        report['renderer'] = capture(tmp, fixture, args.scale, args.filter)
        for kind, ((slug, title, index, palette), (w, h, rgba)) in enumerate(zip(CASES, textures), 1):
            size = w*args.scale, h*args.scale
            original = Image.frombytes('RGBA', size, (tmp/f'{kind:02d}-original.rgba').read_bytes())
            native = Image.frombytes('RGBA', size, (tmp/f'{kind:02d}-native.rgba').read_bytes())
            metrics, heat = compare(original, native)
            sheet, overlay = previews(original, native, heat, title)
            destination = args.out/slug
            destination.mkdir(exist_ok=True)
            texture = Image.frombytes('RGBA', (w, h), rgba)
            texture.putalpha(texture.getchannel('A').point(lambda a: min(255, (a*255+64)//128)))
            for name, picture in [('texture', texture), ('original', original), ('native', native),
                                  ('difference', heat), ('comparison', sheet), ('overlay', overlay)]:
                save_png(picture, destination/(name+'.png'))
            report['cases'].append({'name': slug, 'title': title, 'native_id': kind,
                                    'texture_index': index, 'palette_index': palette, 'metrics': metrics})
            print(f'{slug}: RGB MAE {metrics["premultiplied_rgb_mae_0_255"]:.2f}, '
                  f'alpha MAE {metrics["alpha_mae_0_255"]:.2f}, IoU {metrics["silhouette_iou_alpha_ge_16"]:.3f}')
    (args.out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    write_html(args.out, report)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--file', type=Path, default=ROOT/'port/build/battle-hud-reference/source/00005.bin')
    parser.add_argument('--out', type=Path, default=ROOT/'port/build/native-hud-comparison')
    parser.add_argument('--scale', type=int, choices=range(1, 9), default=4)
    parser.add_argument('--filter', choices=['linear', 'nearest'], default='linear')
    parser.add_argument('--docker', action=argparse.BooleanOptionalAction, default=os.name == 'nt')
    parser.add_argument('--docker-image', help='Reuse an existing image with SDL3, compiler, Pillow, Mesa and Xvfb.')
    args = parser.parse_args()
    args.file, args.out = args.file.resolve(), args.out.resolve()
    if not args.file.is_file():
        parser.error(f'HUD archive entry not found: {args.file}. Supply the extracted common HUD entry with --file.')
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        docker_run(args) if args.docker else run(args)
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        parser.exit(1, f'Comparison failed: {error}\n')
    print(f'Comparison report: {args.out/"index.html"}')


if __name__ == '__main__':
    main()
