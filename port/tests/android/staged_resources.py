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

    def run():
        subprocess.run([shell, '-eu', '-c', stage], cwd=work,
                       env={**os.environ, 'STAGING_TEST_HOST': host.as_posix()}, check=True)

    run()
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

print('Android staging checks passed: catalog additions/edits/deletions/renames, font renames, and cache/artifact preservation.')
