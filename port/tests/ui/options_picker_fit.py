#!/usr/bin/env python3
"""Picker captions fit the visible body, excluding transparent padding/glow."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/sys/gfxm_c.c').read_text()
a=s.index('        if(sOptionsPickerCaption && sOptionsCaptionLayer<0 &&')
b=s.index('        if(sOptionsPlateCopy && sOptionsCaptionLayer>=0)',a)
c=s.index('                    if(sOptionsPickerBoundsValid) {',b)
d=s.index('                    Ps_SpriteScreen(id,caption.pt[0][0]',c)
source=r'''
#include <assert.h>
typedef float f32;
struct Quad { float pt[4][2];struct { int u0,v0,u1,v1; } uv; };
static int sOptionsPickerCaption=1,sOptionsCaptionLayer=-1,sOptionsPickerBoundsValid;
static float sOptionsPickerBounds[4];
static int ink=800;
int PortTbo_TextInfo(int,int what) { return what==1 ? 1024:what==2 ? 64:what==7 ? ink:what==8 ? 48:8; }
void capture(Quad *pq) {
'''+s[a:b]+r'''
}
int main() {
 for(float scale:{0.75f,1.0f,1.2f}) {
    Quad body={};body.uv={0,0,256,64};
    body.pt[0][0]=100;body.pt[0][1]=200;
    body.pt[1][0]=100+256*scale;body.pt[2][1]=200+64*scale;
    capture(&body);
    assert(sOptionsPickerBoundsValid && sOptionsPickerBounds[2]==100+168*scale);
    Quad glow=body;glow.uv.v1=128;glow.pt[1][0]=100+512*scale;capture(&glow);
    assert(sOptionsPickerBounds[2]==100+168*scale);
    for(int size:{700,900}) {
        ink=size;int id=1;Quad caption={};
        caption.pt[0][0]=100;caption.pt[0][1]=200;
        caption.pt[3][0]=612;caption.pt[3][1]=232;
'''+s[c:d]+r'''
        float w=caption.pt[3][0]-caption.pt[0][0];
        float left=caption.pt[0][0]+w*8/1024;
        float right=left+w*ink/1024;
        assert(left>=sOptionsPickerBounds[0] && right<=sOptionsPickerBounds[2]+0.001f);
    }
 }
}
'''
source='#include <initializer_list>\n'+source
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'fit.cpp';e=Path(tmp)/'fit';p.write_text(source)
 subprocess.run(['c++','-std=c++17','-fsanitize=undefined',str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('Options picker visible-body fitting checks passed')
