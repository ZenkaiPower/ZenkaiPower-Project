"""Exercise production interpolation grouping and atlas checks without a GPU."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'port/src/gs/gs_draw.c').read_text()

def function(name):
    match = re.search(r'^static (?:int|uint32_t|void) ' + name + r'\([^\n]*\) \{.*?^\}', source, re.M | re.S)
    assert match, name
    return match.group(0)

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
typedef struct { float x,y,z,s,t,q; } Vtx;
typedef struct { uint32_t sig,first,kind,base,view; uint8_t flat2d,tris; } InterpPrim;
typedef struct { unsigned vu,native,count,tex_is_target,tex,pipeline,first; int target,flat2d,screenText,sceneResolution; float misc[4]; } GsDraw;
#define INTERP_MAX_PRIMS 16
#define INTERP_PIECE_MAX 16
#define INTERP_SCISSOR_ID(d) 0u
static unsigned gsDrawCount=1;
static int gsTargetCount=1, gGsMainFbp=0;
static struct { unsigned fbp; } gsTargets[1];
static GsDraw gsDraws[1];
#define INTERP_WINDOW 48
static Vtx gsVerts[12], previousVerts[12];
static Vtx *sInterpPrevVerts=previousVerts;
static InterpPrim currentPrims[4],previousPrims[4];
static InterpPrim *sInterpPrims=currentPrims,*sInterpPrevPrims=previousPrims;
static unsigned char used[4],*sInterpPrimUsed=used;
'''
suffix = r'''
static void quad(void) {
    gsDraws[0]=(GsDraw){.count=6};
    gsVerts[0]=(Vtx){.x=0,.y=0,.q=1};
    gsVerts[1]=(Vtx){.x=1,.y=0,.s=1,.q=1};
    gsVerts[2]=(Vtx){.x=0,.y=1,.t=1,.q=1};
    gsVerts[3]=gsVerts[2]; gsVerts[4]=gsVerts[1];
    gsVerts[5]=(Vtx){.x=1,.y=1,.s=1,.t=1,.q=1};
}
int main(void) {
    InterpPrim pieces[16]; Vtx previous[6];
    quad(); assert(interp_prims(pieces)==1 && pieces[0].tris==2);
    uint32_t worldSig=pieces[0].sig,worldKind=pieces[0].kind,worldBase=pieces[0].base;
    gsDraws[0].flat2d=1;assert(interp_prims(pieces)==1 && pieces[0].flat2d);
    assert(pieces[0].sig!=worldSig && pieces[0].kind!=worldKind && pieces[0].base!=worldBase);
    gsDraws[0].flat2d=0;
    gsDraws[0].screenText=1;assert(interp_prims(pieces)==1 && pieces[0].flat2d);
    gsDraws[0].screenText=0;gsDraws[0].sceneResolution=1;
    assert(interp_prims(pieces)==1 && pieces[0].flat2d);
    gsDraws[0].sceneResolution=0;
    /* Overlapping triangles at different depths are separate surfaces. */
    gsVerts[3].z=gsVerts[4].z=1;
    assert(interp_prims(pieces)==2);
    quad(); gsVerts[3].q=gsVerts[4].q=2;
    assert(interp_prims(pieces)==2);
    /* An atlas seam must not join unrelated sprites. */
    quad(); gsVerts[3].s+=4; gsVerts[4].s+=4;
    assert(interp_prims(pieces)==2);
    quad();
    for (int k=0;k<6;k++) previous[k]=gsVerts[k];
    assert(interp_same_uv(previous,gsVerts,6));
    /* Previously only vertices 0 and 2 were checked. */
    previous[1].s+=1; assert(!interp_same_uv(previous,gsVerts,6));
    previous[1]=gsVerts[1]; previous[5].t+=1;
    assert(!interp_same_uv(previous,gsVerts,6));
    /* A moving duplicate at x=1 must not take a stationary glyph's x=0
       predecessor before that stationary glyph is visited. */
    quad();
    for(int k=0;k<6;k++) {
        previousVerts[k]=gsVerts[k];previousVerts[k+6]=gsVerts[k];
        previousVerts[k+6].x+=4;
        gsVerts[k+6]=gsVerts[k];gsVerts[k].x+=1;
    }
    currentPrims[0]=(InterpPrim){.first=0,.flat2d=1,.tris=2};
    currentPrims[1]=(InterpPrim){.first=6,.flat2d=1,.tris=2};
    previousPrims[0]=currentPrims[0];previousPrims[1]=currentPrims[1];
    interp_reserve_flat(0,2,0,2);
    assert(used[0]==2 && used[1]==0);
    assert(!interp_stationary(gsVerts,previousVerts,6));
    assert(interp_stationary(gsVerts+6,previousVerts,6));
    memset(used,0,sizeof(used));currentPrims[1].flat2d=0;
    interp_reserve_flat(0,2,0,2);assert(!used[0] && !used[1]);
    puts("Interpolation geometry and stationary UI reservation checks passed");
}
'''
with tempfile.TemporaryDirectory(prefix='zenkaipower-interp-geometry-') as tmp:
    path = Path(tmp)
    c = path / 'test.c'
    binary = path / 'test'
    c.write_text(prefix + '\n'.join(function(name) for name in
        ('interp_prim_cmp', 'interp_same_corner', 'interp_same_uv', 'interp_prims', 'interp_stationary', 'interp_reserve_flat')) + suffix)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
