"""Check screen-based text raster quality and the internal setting's fallback."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'port/src/gs/menu_text.c').read_text()
functions = []
for name in ('MenuText_OutputSize', 'MenuText_RasterScale'):
    match = re.search(r'^(?:void|int) ' + name + r'\([^\n]*\) \{.*?^\}', text, re.M | re.S)
    assert match, name
    functions.append(match[0])
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "auto_video.h"
static int sTextOutputScale=2, setting=-1;
static int Port_Setting(const char *name,int fallback) {
    assert(!strcmp(name,"text_screen_size"));
    assert(fallback==1);
    return setting<0 ? fallback:setting;
}
''' + '\n'.join(functions) + r'''
int main(void) {
    assert(MenuText_RasterScale(8)==2); /* safe startup before an output exists */
    MenuText_OutputSize(1920,1080,1778);
    for(int resolution=1;resolution<=8;resolution++) assert(MenuText_RasterScale(resolution)==4);
    MenuText_OutputSize(3840,2160,1778);
    assert(MenuText_RasterScale(1)==8); /* display raster atlas limit */
    MenuText_OutputSize(1080,2400,1778);
    assert(MenuText_RasterScale(8)==3); /* portrait bars are not part of the picture */
    MenuText_OutputSize(0,0,0);
    assert(MenuText_RasterScale(8)==3); /* minimized window preserves valid output */
    MenuText_OutputSize(2560,1080,1333);
    assert(MenuText_RasterScale(1)==3); /* side bars do not increase quality */
    setting=0;
    for(int resolution=1;resolution<=8;resolution++)
        assert(MenuText_RasterScale(resolution)==(resolution<2 ? 2:resolution>4 ? 4:resolution));
    setting=1;
    assert(MenuText_RasterScale(1)==3);
    puts("Text raster scale: default, resize, letterboxing, limits and resolution fallback passed");
}
'''
with tempfile.TemporaryDirectory(prefix='text-raster-scale-') as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-Iport/src/gs',
                    str(source), '-o', str(binary)], cwd=root, check=True)
    subprocess.run([str(binary)], check=True)
