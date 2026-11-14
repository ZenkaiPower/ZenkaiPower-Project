"""Exercise runtime setup artwork without embedding any images."""
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='setup-art-') as temporary:
    folder = Path(temporary)
    game = folder / 'game'
    artwork = game / 'setup'
    artwork.mkdir(parents=True)
    source = folder / 'check.cpp'
    source.write_text(r'''#include "port/setup/artwork.h"
#include <cassert>
#include <cstdlib>
int main(int argc,char **argv) {
    auto image=Setup_LoadArtwork(argv[1],argv[3],128,128);
    int expected=std::atoi(argv[2]);
    if(!expected) assert(!image);
    else { assert(image); assert(image->w==expected); assert(image->h==128); SDL_DestroySurface(image); }
}
''')
    binary = folder / 'check'
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl3'], text=True).split()
    subprocess.run(['c++', '-std=c++17', '-I' + str(ROOT), str(source), '-o', str(binary)] + flags, check=True)
    def check(expected, name="cover.jpg"):
        subprocess.run([str(binary), str(game), str(expected), name], check=True)
    check(0)
    (artwork / 'cover.jpg').write_bytes(b'invalid PNG')
    check(0)
    Image.new('RGB', (200, 400), 'red').save(artwork / 'cover.jpg')
    check(64)
    Image.new('RGB', (400, 400), 'blue').save(artwork / 'cover.jpg')
    check(128)  # Same binary reads changed files without a rebuild.
    Image.new('RGB', (400, 400), 'green').save(artwork / 'disc.png')
    check(128, 'disc.png')
    Image.new('RGB', (4097, 1), 'red').save(artwork / 'cover.jpg')
    check(0)
    (artwork / 'cover.jpg').unlink()
    check(0)
    subprocess.run(['c++', '-std=c++17', '-fsyntax-only', '-Iport/third_party/imgui',
                    'port/setup/setup.cpp'], cwd=ROOT, check=True)
print('Runtime artwork: missing, corrupt, resized, changed, oversized and removed files passed; desktop syntax passed')
