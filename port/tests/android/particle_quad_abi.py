"""Build a two-translation-unit particle ABI regression for the Android device.

Run inside the Android build container, then push and execute the two binaries
printed at the end. The fixed binary must return 0; the misaligned control must
return 1. Uses the real caller declarations and callee prototypes.
"""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'port/tools'))
import android

out = root / 'port/build/android/particle-abi'
out.mkdir(parents=True, exist_ok=True)
caller = (root / 'src/battle/eft_w.c').read_text()
start = caller.index('/* Match eft_x.c')
block = caller[start:caller.index('extern void EftPart10_UnlinkPtcl', start)]
header = (root / 'include/battle/eft_x.h').read_text()
callee = '#include <string.h>\n#include "battle/eft_x.h"\nint checks;\n'
for name in ('EftPart10_DrawQuad', 'EftPart10_DrawQuadClipped'):
    signature = re.search(r'void '+name+r'\([^;]+;', header).group(0)[:-1]
    callee += signature + ''' {
        const float a[4]={1,2,3,4}, b[4]={5,6,7,8}, c[4]={9,10,11,12};
        if(corner==(Vec4 *)0x11223000 && tex==(EftXTexEntry *)0x12345000 &&
           layer==2 && texIdx==1024 && noDepth==1 &&
           !memcmp(&uv0,a,16) && !memcmp(&uv1,b,16) && !memcmp(&color,c,16)) checks++;
    }
'''
(out / 'callee.c').write_text(callee)
main = '''
extern int checks;
int main(void) {
    Vec4 a={1,2,3,4}, b={5,6,7,8}, c={9,10,11,12};
    EftPart10_DrawQuad((Vec4 *)0x11223000,&a,&b,&c,2,1024,1,(EftWTexSet *)0x12345000);
    EftPart10_DrawQuadClipped((Vec4 *)0x11223000,&a,&b,&c,2,1024,1,(EftWTexSet *)0x12345000);
    return checks==2 ? 0:1;
}
'''
prefix = '#define PORT 1\n#include "battle/eft_w.h"\n'
flags = [android.CLANG] + android.TARGET + android.SOFT + [
    '-std=gnu99', '-O2', '-fms-extensions', '-I'+str(root/'include')]
for name, declarations in [('fixed', block), ('misaligned', block.replace('__attribute__((aligned(16)))', ''))]:
    source = out / (name+'.c')
    source.write_text(prefix+declarations+main)
    subprocess.run(flags+[str(source),str(out/'callee.c'),'-o',str(out/name)], check=True)
    print(out/name)
