"""Smoke-check the shared tools image: run this script inside zenkaipower."""
from pathlib import Path
import importlib
import shutil
import subprocess
import tempfile

for command in ('python3', 'node', 'npm', 'ninja', 'cmake', 'clang', 'llc', 'g++',
                'x86_64-w64-mingw32-g++', 'x86_64-w64-mingw32-windres',
                'glslangValidator', '7z', 'java', 'javac', 'keytool', 'sdkmanager',
                'adb', 'gdb', 'sqlite3', 'ffmpeg', 'git', 'curl'):
    assert shutil.which(command), command
for module in ('PIL', 'fontTools', 'nacl', 'splat', 'clang.cindex'):
    importlib.import_module(module)
assert subprocess.check_output(['getconf', 'GNU_LIBC_VERSION'], text=True).strip() == 'glibc 2.35'
subprocess.run(['/opt/zenkaipower-tools/ee-gcc2.96/bin/ee-gcc', '--version'], check=True, stdout=subprocess.DEVNULL)
subprocess.run(['/opt/zenkaipower-tools/binutils/mips-ps2-decompals-as', '--version'], check=True, stdout=subprocess.DEVNULL)
subprocess.run(['node', '--input-type=module', '-e', "import {DatabaseSync} from 'node:sqlite'; new DatabaseSync(':memory:').close()"], check=True)
assert Path('/opt/android-sdk/platforms/android-35/android.jar').is_file()
assert Path('/opt/android-sdk/build-tools/35.0.0/apksigner').is_file()
with tempfile.TemporaryDirectory() as temporary:
    folder = Path(temporary)
    source = folder / 'check.cpp'
    source.write_text('int main() { return 0; }')
    for compiler, name in [('g++', 'linux'), ('x86_64-w64-mingw32-g++', 'windows.exe')]:
        subprocess.run([compiler, str(source), '-o', str(folder / name)], check=True)
    subprocess.run([str(folder / 'linux')], check=True)
    assert (folder / 'windows.exe').read_bytes().startswith(b'MZ')
print('PASS: unified image tools, PS2 runtime, Linux/Windows compilers, Python/Node and Android SDK')
