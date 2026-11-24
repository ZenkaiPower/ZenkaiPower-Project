"""Check screen-based text raster quality and the internal setting's fallback."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'port/src/gs/menu_text.c').read_text()
functions = []
for name in ('MenuText_OutputSize', 'MenuText_SceneScale', 'MenuText_RasterScaleFor', 'MenuText_RasterScale', 'MenuText_RasterKey'):
    match = re.search(r'^(?:void|int|uint32_t) ' + name + r'\([^\n]*\) \{.*?^\}', text, re.M | re.S)
    assert match, name
    functions.append(match[0])
code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "auto_video.h"
static int sTextOutputScale=2,sTextSceneScale=2,normal=1,info=0;
static int PortText_FollowsResolution(int special) {return special ? info : normal;}
''' + '\n'.join(functions) + r'''
int main(void) {
    MenuText_OutputSize(1920,1080,1778);
    for(int resolution=1;resolution<=8;resolution++) {
        assert(MenuText_RasterScale(resolution)==(resolution<2 ? 2:resolution));
        assert(MenuText_RasterScaleFor(resolution,1)==4);
    }
    normal=0;info=1;
    assert(MenuText_RasterScale(1)==4);
    assert(MenuText_RasterScaleFor(3,1)==3);
    MenuText_OutputSize(3840,2160,1778);
    assert(MenuText_RasterScale(1)==8);
    MenuText_OutputSize(1080,2400,1778);
    assert(MenuText_RasterScale(1)==3);
    normal=1;MenuText_SceneScale(5);
    assert(MenuText_RasterKey()==5);
    puts("Text raster scale: game resolution, screen override, resize and scene cache generation passed");
}
'''
with tempfile.TemporaryDirectory(prefix='text-raster-scale-') as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Iport/src/gs',
                    str(source), '-o', str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], check=True)
