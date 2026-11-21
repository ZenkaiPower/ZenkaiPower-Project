"""Check bundled Android cache wiring and wrapper seeding without Docker or a build."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import fnmatch
import shlex

ROOT = Path(__file__).resolve().parents[3]
shell = shutil.which('sh')
if not shell:
    git_bash = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe'
    if git_bash.is_file(): shell = str(git_bash)
if not shell: raise SystemExit('A POSIX shell is required (Git Bash on Windows).')

for name in ('android/build.sh', 'android/container-build.sh', 'android/seed-gradle-cache.sh',
             'port/tools/android_nod.sh'):
    subprocess.run([shell, '-n', str(ROOT / name)], check=True)

with tempfile.TemporaryDirectory(prefix='android-image-cache-') as temporary:
    root = Path(temporary)
    seed, cache = root / 'seed', root / 'cache'
    seed.mkdir()
    marker = '.zenkaipower-image-seed'
    env = {**os.environ, 'ZENKAIPOWER_GRADLE_SEED': seed.as_posix(),
           'GRADLE_USER_HOME': cache.as_posix()}

    def run():
        subprocess.run([shell, str(ROOT / 'android/seed-gradle-cache.sh')], env=env, check=True)

    run()  # Older images lacking a seed are supported.
    assert not cache.exists()
    wrapper = seed / 'wrapper/dists/test/bin/gradle'
    wrapper.parent.mkdir(parents=True)
    wrapper.write_text('bundled wrapper')
    (seed / marker).write_text('first image')
    run()
    installed = cache / 'wrapper/dists/test/bin/gradle'
    assert installed.read_text() == 'bundled wrapper'
    assert (cache / marker).read_text() == 'first image'
    installed.write_text('existing valid wrapper')
    run()
    assert installed.read_text() == 'existing valid wrapper'
    # A newer image adds its wrapper without deleting the user's existing cache.
    newer = seed / 'wrapper/dists/new/bin/gradle'
    newer.parent.mkdir(parents=True)
    newer.write_text('new wrapper')
    (seed / marker).write_text('second image')
    run()
    assert (cache / 'wrapper/dists/new/bin/gradle').read_text() == 'new wrapper'
    assert installed.read_text() == 'existing valid wrapper'
    assert (cache / marker).read_text() == 'second image'

    # Execute only the production wrapper-generation block, with a fake clang.
    # Rust passes -lunwind explicitly, so this must work before android.py rtdir().
    toolchain = root / 'toolchain with spaces'
    compiler = toolchain / 'llvm/bin/clang'
    compiler.parent.mkdir(parents=True)
    compiler.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
    compiler.chmod(0o755)
    nod = (ROOT / 'port/tools/android_nod.sh').read_text()
    start = nod.index('cat > "$TC/android-cc" <<EOF')
    end = nod.index('export CC_aarch64_linux_android=', start)
    env = {**os.environ, 'TC': toolchain.as_posix()}
    subprocess.run([shell, '-c', nod[start:end]], env=env, check=True)
    args = subprocess.check_output([shell, str(toolchain / 'android-cc'),
                                    '-lunwind', 'object with spaces.o'], text=True).splitlines()
    assert '--target=aarch64-linux-android29' in args
    assert '--sysroot=' + (toolchain / 'ndk/sysroot').as_posix() in args
    assert '-L' + (toolchain / 'ndk/clang_rt').as_posix() in args
    assert args[-2:] == ['-lunwind', 'object with spaces.o']

docker = (ROOT / 'Dockerfile').read_text()
# The Dockerfile-specific context filter must expose every local COPY input.
# Its rules are an exclude-all followed by literal paths and the res/** subtree.
rules = [line.strip() for line in (ROOT / 'Dockerfile.dockerignore').read_text().splitlines()
         if line.strip() and not line.lstrip().startswith('#')]

def included(path):
    for part in (*reversed(path.parents), path):
        if part == Path('.'): continue
        allowed = True
        for rule in rules:
            include = rule.startswith('!')
            pattern = rule.lstrip('!').rstrip('/')
            if fnmatch.fnmatchcase(part.as_posix(), pattern): allowed = include
        if not allowed: return False
    return True

for line in docker.splitlines():
    if not line.startswith('COPY ') or '--from=' in line or '<<' in line: continue
    for item in shlex.split(line)[1:-1]:
        if item.startswith('--'): continue
        path = Path(item)
        assert (ROOT / path).exists() and included(path), item
        if (ROOT / path).is_dir():
            for child in (ROOT / path).rglob('*'):
                assert included(child.relative_to(ROOT)), child
for name in ('.signing/release.jks', 'android/local.properties',
             'android/app/src/main/jniLibs/arm64-v8a/libzenkaipower.so',
             'port/build/android/obj/game.o', 'roms/game.iso'):
    assert not included(Path(name)), name
assert 'ENV ZENKAIPOWER_TOOLCHAIN=/opt/zenkaipower-android-toolchain' in docker
assert 'ENV GRADLE_RO_DEP_CACHE=/opt/zenkaipower-gradle/caches' in docker
for name in ('android/build.sh', 'android/build.ps1'):
    assert 'source=zenkaipower-android-toolchain' not in (ROOT / name).read_text()
assert 'sh android/seed-gradle-cache.sh' in (ROOT / 'android/container-build.sh').read_text()
print('Android image checks passed: shell syntax, cache seeding/upgrades, linker runtime paths and build-context wiring.')
