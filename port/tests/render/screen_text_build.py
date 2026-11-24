"""Build real renderer shaders and syntax-check both screen-text backends."""
from pathlib import Path
import shlex
import subprocess
import tempfile
import sys
import os

root = Path(__file__).resolve().parents[3]
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'sdl3'], text=True))
with tempfile.TemporaryDirectory(prefix='screen-text-build-') as tmp:
    tmp = Path(tmp)
    sources = list((root / 'port/src/gs/shaders').glob('*.vert')) + list((root / 'port/src/gs/shaders').glob('*.frag'))
    original = (root / 'port/src/gs/shaders/gs.frag').read_text()
    for name, defines in [('gsnz', '#define NO_FRAG_DEPTH\n'), ('gswb', '#define NO_FRAG_DEPTH\n#define GS_BLOCK\n')]:
        path = tmp / (name + '.frag')
        path.write_text(original.replace('\n', '\n' + defines, 1))
        sources.append(path)
    arrays = []
    for path in sources:
        out = tmp / (path.name + '.spv')
        subprocess.run(['glslangValidator', '-V', str(path), '-o', str(out)], check=True, capture_output=True)
        name = 'k' + path.stem.capitalize() + path.suffix[1:].capitalize()
        for suffix, data in [('Spv', out.read_bytes()), ('Glsl', path.read_bytes() + b'\0')]:
            arrays.append('static const unsigned char ' + name + suffix + '[]={' + ','.join(map(str, data)) + '};\n')
    (tmp / 'shaders.h').write_text(''.join(arrays))
    for backend in ['gs_draw', 'gs_gl', 'gs_gpu']:
        subprocess.run(['cc', '-std=gnu99', '-fsyntax-only', *flags, '-Iport/include', '-I'+str(tmp),
                        'port/src/gs/' + backend + '.c'], cwd=root, check=True)
    unit = tmp / 'screen-text'
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', 'port/tests/render/screen_text.c', '-o', str(unit)],
                   cwd=root, check=True)
    subprocess.run([str(unit)], check=True)
    for normal in range(2):
        for info in range(2):
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                            '-DZENKAIPOWER_TEXT_FOLLOW_RES='+str(normal),
                            '-DZENKAIPOWER_HUD_INFO_FOLLOW_RES='+str(info),
                            'port/tests/render/text_resolution.c', '-o', str(unit)], cwd=root, check=True)
            subprocess.run([str(unit)], check=True)
    if '--gpu' in sys.argv:
        libs = shlex.split(subprocess.check_output(['pkg-config', '--libs', 'sdl3'], text=True))
        for backend in ['gpu', 'gl']:
            gpu = tmp / ('screen-text-'+backend)
            subprocess.run(['cc', '-std=gnu99', '-ffunction-sections', '-fdata-sections', *flags, '-Iport/include',
                            '-I'+str(tmp), 'port/tests/render/screen_text_'+backend+'.c', '-Wl,--gc-sections', *libs,
                            '-lm', '-pthread', '-o', str(gpu)], cwd=root, check=True)
            subprocess.run(['xvfb-run', '-a', str(gpu)], cwd=root, check=True,
                           env=dict(os.environ, XDG_RUNTIME_DIR=str(tmp)))
    print('Screen-text shader and both backend build checks passed')
