"""Exercise production interpolation grouping and atlas checks without a GPU."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'port/src/gs/gs_draw.c').read_text()

def function(name):
    match = re.search(r'^static (?:int|uint32_t) ' + name + r'\([^\n]*\) \{.*?^\}', source, re.M | re.S)
    assert match, name
    return match.group(0)

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
typedef struct { float x,y,z,s,t,q; } Vtx;
typedef struct { uint32_t sig,first,kind,base,view; uint8_t flat2d,tris; } InterpPrim;
typedef struct { unsigned vu,native,count,tex_is_target,tex,pipeline,first; int target; float misc[4]; } GsDraw;
#define INTERP_MAX_PRIMS 16
#define INTERP_PIECE_MAX 16
#define INTERP_SCISSOR_ID(d) 0u
static unsigned gsDrawCount=1;
static int gsTargetCount=1, gGsMainFbp=0;
static struct { unsigned fbp; } gsTargets[1];
static GsDraw gsDraws[1];
static Vtx gsVerts[6];
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
    puts("Interpolation geometry checks passed");
}
'''
with tempfile.TemporaryDirectory(prefix='zenkaipower-interp-geometry-') as tmp:
    path = Path(tmp)
    c = path / 'test.c'
    binary = path / 'test'
    c.write_text(prefix + '\n'.join(function(name) for name in
        ('interp_prim_cmp', 'interp_same_corner', 'interp_same_uv', 'interp_prims')) + suffix)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
