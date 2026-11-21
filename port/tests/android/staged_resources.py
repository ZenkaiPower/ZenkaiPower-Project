"""Exercise Android's real source-staging block without Docker or compilation."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
shell = shutil.which('sh')
if not shell:
    git_bash = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe'
    if git_bash.is_file(): shell = str(git_bash)
if not shell: raise SystemExit('A POSIX shell is required (Git Bash on Windows).')

source = (ROOT / 'android/container-build.sh').read_text()
start = source.index('# Refresh packaged source resources')
end = source.index('if [ "${ZENKAIPOWER_ANDROID_ENGINE', start)
stage = source[start:end].replace('tar -C /host ', 'tar -C "$STAGING_TEST_HOST" ')

with tempfile.TemporaryDirectory(prefix='android-staged-resources-') as temporary:
    root = Path(temporary)
    host, work = root / 'host checkout', root / 'work'
    for directory in ('src', 'include', 'config', 'assets', 'port', 'android'):
        (host / directory).mkdir(parents=True)
    work.mkdir()
    for name in ('LICENSE', 'game-info.json', 'dragon-net-battle.txt'):
        (host / name).write_text('fixture')

    def write(base, relative, text):
        path = base / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    preserved = ('port/build/android/obj/cached.o', 'port/build_release/cached.o',
                 'android/.gradle/state', 'android/app/build/outputs/apk/release/app-release.apk',
                 'android/app/src/main/assets/engine/translations.pak',
                 'android/app/src/main/jniLibs/arm64-v8a/libzenkaipower.so',
                 'release/android-arm64/released.apk')
    for name in preserved: write(work, name, 'preserved')
    # Excluded host outputs must not overwrite cached container outputs.
    for name in preserved[:6]: write(host, name, 'host generated output')
    for kind in ('labels', 'ui', 'port'):
        write(host, f'port/translations/{kind}/en.json', 'original')
        write(host, f'port/translations/{kind}/pt.json', 'removed next build')
    write(host, 'port/third_party/fonts/old.ttf', 'font renamed next build')
    helpers = ('android/fixture.sh', 'port/tools/fixture.sh',
               'port/setup/fixture.sh', 'port/release/fixture.sh')
    for name in helpers:
        write(host, name, '')
        (host / name).write_bytes(b'#!/bin/sh\r\nset -eu\r\n')

    # Run the exact PowerShell launcher's shell bootstrap against a CRLF input.
    powershell = (ROOT / 'android/build.ps1').read_text()
    line = next(line for line in powershell.splitlines() if line.strip().startswith('$androidBootstrap = '))
    bootstrap = line.split(' = ', 1)[1][1:-1].replace("''", "'")
    bootstrap = bootstrap.replace('/host/android/container-build.sh', '"$TEST_BOOTSTRAP_SOURCE"')
    bootstrap = bootstrap.replace('/tmp/zenkaipower-container-build.sh', '"$TEST_BOOTSTRAP_TARGET"')
    bootstrap_source, bootstrap_target = root / 'input script.sh', root / 'normalized script.sh'
    bootstrap_source.write_bytes(b"#!/bin/sh\r\nset -eu\r\nprintf 'started\\n'\r\n")
    result = subprocess.check_output([shell, '-eu', '-c', bootstrap], text=True,
        env={**os.environ, 'TEST_BOOTSTRAP_SOURCE': bootstrap_source.as_posix(),
             'TEST_BOOTSTRAP_TARGET': bootstrap_target.as_posix()})
    assert result.strip() == 'started'
    assert b'\r' not in bootstrap_target.read_bytes()
    assert b'\r\n' in bootstrap_source.read_bytes()

    def run():
        subprocess.run([shell, '-eu', '-c', stage], cwd=work,
                       env={**os.environ, 'STAGING_TEST_HOST': host.as_posix()}, check=True)

    run()
    for name in helpers:
        assert (work / name).read_bytes() == b'#!/bin/sh\nset -eu\n'
        assert b'\r\n' in (host / name).read_bytes()
    for kind in ('labels', 'ui', 'port'):
        assert (work / f'port/translations/{kind}/pt.json').is_file()
        (host / f'port/translations/{kind}/pt.json').unlink()
        write(host, f'port/translations/{kind}/en.json', 'updated')
        write(host, f'port/translations/{kind}/pt-BR.json', 'renamed')
    (host / 'port/third_party/fonts/old.ttf').rename(host / 'port/third_party/fonts/new.ttf')
    run()
    for kind in ('labels', 'ui', 'port'):
        assert not (work / f'port/translations/{kind}/pt.json').exists()
        assert (work / f'port/translations/{kind}/en.json').read_text() == 'updated'
        assert (work / f'port/translations/{kind}/pt-BR.json').read_text() == 'renamed'
        assert (host / f'port/translations/{kind}/pt-BR.json').read_text() == 'renamed'
    assert not (work / 'port/third_party/fonts/old.ttf').exists()
    assert (work / 'port/third_party/fonts/new.ttf').read_text() == 'font renamed next build'
    for name in preserved: assert (work / name).read_text() == 'preserved', name

print('Android staging checks passed: resource updates/deletions, cache preservation, CRLF bootstrap and helper normalization.')
