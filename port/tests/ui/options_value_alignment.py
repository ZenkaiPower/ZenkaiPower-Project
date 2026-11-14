#!/usr/bin/env python3
"""Check styled caption bounds with unequal left/right and top/bottom padding."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'port/src/gs/tbo_text.c').read_text()
a=s.index('    if (l->kind == 3 || l->kind == 4) {')
b=s.index('    if (l->kind >= 3 && l->kind <= 5) {',a)
source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
typedef struct { char *text; int kind,w,h,cap,pad,base,outline,inkX,inkY,inkW,inkH,advance; } TboLine;
uint8_t *MenuText_RenderWiiLabel(const char *text,int style,int scale,int *w,int *h) {
    *w=1024;*h=64;uint8_t *p=calloc(1024*64,4);
    for(int y=13+style;y<53;y++) for(int x=10+style;x<110+style;x++) p[(y*1024+x)*4+3]=128;
    return p;
}
int measure(TboLine *l) {
'''+s[a:b]+r'''
    return 0;
}
int main(void) {
    for(int style=0;style<2;style++) {
        TboLine l={0};l.kind=3+style;l.text="Auto";
        assert(measure(&l));
        assert(l.inkX==10+style && l.inkW==100);
        assert(l.inkY==13+style && l.inkH==40-style);
        float x=420-(l.inkX+l.inkW*.5f)*.5f;
        float y=136-(l.inkY+l.inkH*.5f)*.5f;
        assert(fabsf(x+(l.inkX+l.inkW*.5f)*.5f-420)<.001f);
        assert(fabsf(y+(l.inkY+l.inkH*.5f)*.5f-136)<.001f);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'align.c';src.write_text(source);exe=Path(tmp)/'align'
    subprocess.run(['cc','-std=c99','-fsanitize=undefined',str(src),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Options styled value alignment checks passed')
