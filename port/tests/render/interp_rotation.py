"""Exercise production rotation blending through half turns and continuous spins."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'port/src/gs/gs_draw.c').read_text()

def function(name):
    match = re.search(r'^static (?:int|void) ' + name + r'\([^\n]*\) \{.*?^\}', source, re.M | re.S)
    assert match, name
    return match[0]

code = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
''' + '\n'.join(function(name) for name in
    ('interp_inverse_d', 'interp_mul_d', 'interp_sqrt_d', 'interp_bone_quat', 'interp_bone')) + r'''
static void yaw(float *m, float angle) {
    memset(m,0,16*sizeof(float));
    m[0]=m[10]=cosf(angle); m[2]=-sinf(angle); m[8]=sinf(angle); m[5]=m[15]=1;
}
static void shape(const float *m, const float *length) {
    for (int i=0;i<3;i++) {
        float norm=0;
        for (int k=0;k<3;k++) { assert(isfinite(m[i*4+k])); norm+=m[i*4+k]*m[i*4+k]; }
        assert(fabsf(sqrtf(norm)-length[i])<0.0001f);
        for (int j=i+1;j<3;j++) {
            float dot=0;
            for (int k=0;k<3;k++) dot+=m[i*4+k]*m[j*4+k];
            assert(fabsf(dot)<0.0001f);
        }
    }
}
int main(void) {
    const float pi=3.14159265358979323846f, unit[]={1,1,1};
    float p[16],c[16],o[16];
    /* The old axis average loses two dimensions at a half turn. */
    yaw(p,0);yaw(c,pi);
    for (int step=0;step<=8;step++) {
        interp_bone(p,c,step/8.0f,0,o);shape(o,unit);
    }
    /* Cross the angle wrap on the short arc. */
    yaw(p,179*pi/180);yaw(c,-179*pi/180);
    interp_bone(p,c,0.5f,0,o);shape(o,unit);assert(o[0]<-0.999f);
    /* Camera-relative translation still follows the existing orbit radius. */
    yaw(p,0);yaw(c,0.4f);p[12]=10;c[14]=10;
    interp_bone(p,c,0.5f,1,o);shape(o,unit);
    assert(fabsf(sqrtf(o[12]*o[12]+o[14]*o[14])-10)<0.0001f);
    /* Repeated rotation with larger gaps between samples must retain volume. */
    for (int degrees=0;degrees<1440;degrees+=7) {
        for (int gap=7;gap<=175;gap+=28) {
            yaw(p,degrees*pi/180);yaw(c,(degrees+gap)*pi/180);
            for (int step=1;step<8;step++) {
                interp_bone(p,c,step/8.0f,0,o);shape(o,unit);
            }
        }
    }
    /* Different rotation axes, nonuniform scale and linear translation. */
    yaw(p,0.7f);yaw(c,0);
    c[5]=c[10]=cosf(2.1f);c[6]=sinf(2.1f);c[9]=-sinf(2.1f);
    for (int col=0;col<3;col++) for (int k=0;k<3;k++) {
        p[col*4+k]*=(col+1);c[col*4+k]*=(col+2);
    }
    p[12]=10;c[12]=30;
    { const float lengths[]={1.5f,2.5f,3.5f};
      interp_bone(p,c,0.5f,0,o);shape(o,lengths);assert(o[12]==20); }
    /* Mirrored models retain their reflection rather than crossing zero. */
    yaw(p,0);yaw(c,pi);
    for(int k=0;k<3;k++) {p[8+k]=-p[8+k];c[8+k]=-c[8+k];}
    interp_bone(p,c,0.5f,0,o);shape(o,unit);
    { float det=o[0]*(o[5]*o[10]-o[6]*o[9])-o[4]*(o[1]*o[10]-o[2]*o[9])+o[8]*(o[1]*o[6]-o[2]*o[5]);
      assert(det<-0.999f); }
    /* Authored shear falls back to a real basis, without averaging it flat. */
    yaw(p,0);yaw(c,pi);p[4]=0.4f;c[4]=-0.4f;
    interp_bone(p,c,0.25f,0,o);
    for (int col=0;col<3;col++) for(int k=0;k<3;k++) assert(o[col*4+k]==p[col*4+k]);
    /* An unresolved stage-camera half turn must be rejected, not drawn as
     * the element-wise average of the two projection transforms. */
    { double camera[16]={0},root[16];
      camera[0]=camera[10]=-1;camera[5]=camera[15]=1;
      assert(!interp_sqrt_d(camera,root)); }
    puts("Interpolation rotation checks passed");
}
'''

with tempfile.TemporaryDirectory(prefix='interp-rotation-') as tmp:
    path = Path(tmp)
    c, binary = path / 'test.c', path / 'test'
    c.write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(c), '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
