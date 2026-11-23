"""Exercise Options caption rendering and versioning at physical output scales."""
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
source=(root/'port/src/gs/tbo_text.c').read_text()
structure=re.search(r'typedef struct \{\s*char text\[TEXT_MAX\];.*?\} TboLine;',source,re.S)[0]
functions=[]
for name in ('TboText_Version','TboText_Pixels'):
    functions.append(re.search(r'^uint32_t \*?'+name+r'\([^\n]*\) \{.*?^\}',source,re.M|re.S)[0])
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define TEXT_MAX 128
#define TBO_TEXT_BASE 100
#define TBO_TEXT_SLOTS 128
'''+structure+r'''
static TboLine sLines[TBO_TEXT_SLOTS];
static pthread_mutex_t sLock=PTHREAD_MUTEX_INITIALIZER;
static int scale=2;
int MenuText_RasterScale(int internal) {(void)internal;return scale;}
uint32_t MenuText_RasterKey(void) {return scale;}
uint8_t *MenuText_RenderWiiLabel(const char *text,int style,int raster,int *w,int *h) {
    (void)text;*w=512*raster;*h=(style==2 ? 64:32)*raster;
    return calloc((size_t)*w**h,4);
}
uint8_t *MenuText_RenderPauseHeading(const char *t,int *w,int *h) {(void)t;(void)w;(void)h;return NULL;}
uint8_t *MenuText_HeadingRender(const char *t,int *w,int *h) {(void)t;(void)w;(void)h;return NULL;}
uint8_t *MenuText_LineRender(int k,const char *t,int c,int p,int *w,int *h,int *b) {
    (void)k;(void)t;(void)c;(void)p;(void)w;(void)h;(void)b;return NULL;
}
'''+ '\n'.join(functions)+r'''
int main(void) {
    TboLine *line=&sLines[0];line->version=1;line->advance=100;line->w=1024;
    for(int kind=3;kind<=5;kind++) {
        line->kind=kind;line->h=kind==5 ? 128:64;
        uint32_t previous=0;
        for(scale=2;scale<=8;scale++) {
            uint32_t version,w,h;
            uint32_t *pixels=TboText_Pixels(TBO_TEXT_BASE,&version,&w,&h);
            assert(pixels && w==512u*scale && h==(unsigned)line->h*scale/2);
            assert(version==TboText_Version(TBO_TEXT_BASE) && version!=previous);
            previous=version;free(pixels);
            assert(line->w==1024); /* the layout and UV measurements stay unchanged */
        }
    }
    puts("Options captions: output raster scale, stable layout and cache refresh passed");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.c';exe=Path(tmp)/'test';src.write_text(code)
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-pthread',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
