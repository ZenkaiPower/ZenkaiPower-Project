"""Build device regressions for the production particle position adapters.

Inside the Android build container (ZENKAIPOWER_TOOLCHAIN set), run this script; push
the fixed and misaligned executables to the device. Expect exits 0 and 1.
The control changes only the caller vector alignment, reproducing the old ABI.
"""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'port/tools'))
import android

out = root / 'port/build/android/particle-position-abi'
out.mkdir(parents=True, exist_ok=True)
declarations, calls, objects = [], [], []
sources = list((root / 'src/battle').glob('*.c'))
for filename in ('eft_i.c', 'eft_h.c', 'eft_k.c'):
    text = (root / 'src/battle' / filename).read_text()
    for match in re.finditer(r'extern void (Eft\w+)\([^;\n]+PortEffectVector [^;\n]+;\n#define [^\n]+', text):
        name = match[1]
        declarations.append(match[0])
        calls.append(f'{name}((void *)0x12345000, &position);')
        # Compile the callee separately, with its actual definition signature/type.
        for source in sources:
            definition = source.read_text(encoding='utf-8')
            signature = re.search(r'^(void|s32) '+name+r'\([^;{]+\) \{', definition, re.M)
            if signature:
                headers = re.findall(r'#include "(battle/[^\"]+)"', definition)
                params = signature[0].split('(', 1)[1].split(')', 1)[0].split(',')
                handle = params[0].split()[-1].lstrip('*')
                vector = params[1].split()[-1]
                stub = '#include <string.h>\n' + ''.join('#include "'+h+'"\n' for h in headers) + 'extern int checks;\n'
                stub += signature[0] + '\nconst float expected[4]={123,-45,67,1};\n'
                stub += f'if ((void *){handle}==(void *)0x12345000 && !memcmp(&{vector},expected,16)) checks++;\n'
                if signature[1] != 'void': stub += 'return 0;\n'
                stub += '}\n'
                path = out / (name+'.c')
                path.write_text(stub)
                objects.append(path)
                break
        else:
            raise RuntimeError('No definition found for '+name)
assert len(calls) == 15, len(calls)
main = '#define PORT 1\n#include "battle/port_effect_vector.h"\n'
main += '\n'.join(declarations)
main += '\nint checks;\nint main(void) { Vec4 position={123,-45,67,1};\n'
main += '\n'.join(calls) + '\nreturn checks==15 ? 0:1;\n}\n'
caller = out / 'caller.c'
caller.write_text(main)
control = out / 'control-include/battle'
control.mkdir(parents=True, exist_ok=True)
control.joinpath('port_effect_vector.h').write_text(
    (root/'include/battle/port_effect_vector.h').read_text().replace('__attribute__((aligned(16)))', ''))
flags = [android.CLANG] + android.TARGET + android.SOFT + ['-std=gnu99', '-O2', '-fms-extensions']
for name in ('fixed', 'misaligned'):
    includes = ['-I'+str(control.parent)] if name == 'misaligned' else []
    subprocess.run(flags+includes+['-I'+str(root/'include'), str(caller), *map(str, objects),
                                 '-o',str(out/name)], check=True)
    print(out/name)
