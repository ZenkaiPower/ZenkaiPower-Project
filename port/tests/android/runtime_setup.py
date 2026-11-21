"""Exercise production runtime setup and first-link ordering without a compiler."""
import ast
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[3]
tree = ast.parse((ROOT / 'port/tools/android.py').read_text())
functions = ast.Module(body=[node for node in tree.body
                            if isinstance(node, ast.FunctionDef) and node.name in ('rtdir', 'storage')],
                       type_ignores=[])

with tempfile.TemporaryDirectory(prefix='android-runtime-') as temporary:
    root = Path(temporary)
    ndk = root / 'ndk runtimes'
    ndk.mkdir()
    (ndk / 'libclang_rt.builtins-aarch64-android.a').write_bytes(b'builtins')
    (ndk / 'libunwind.a').write_bytes(b'unwind')
    resources = root / 'llvm resources'
    runtime = resources / 'lib/aarch64-unknown-linux-android29'
    links = []

    def run(command):
        if '-print-resource-dir' in command:
            return SimpleNamespace(returncode=0, stdout=str(resources) + '\n', stderr='')
        # The first link must see both files and the explicit runtime search path.
        assert (runtime / 'libclang_rt.builtins.a').read_bytes() == b'builtins'
        assert (runtime / 'libunwind.a').read_bytes() == b'unwind'
        assert '-L' + str(runtime) in command
        links.append(command)
        return SimpleNamespace(returncode=0, stdout='', stderr='')

    namespace = dict(pathlib=__import__('pathlib'), shutil=shutil, run=run,
                     CLANG='mock-clang', TARGET=['--target=aarch64-linux-android29'], API='29',
                     CLANG_RT=ndk, OUT=root / 'build', SDL3=root / 'sdl', ROOT=root)
    exec(compile(functions, 'android-runtime-functions', 'exec'), namespace)
    assert namespace['storage']() and len(links) == 1
    # Repair an interrupted setup even when the other archive already exists.
    for name in ('libunwind.a', 'libclang_rt.builtins.a'):
        (runtime / name).unlink()
        assert namespace['rtdir']() == runtime
        assert (runtime / name).is_file()
    assert namespace['storage']() and len(links) == 2

    namespace['run'] = lambda command: SimpleNamespace(returncode=1, stdout='', stderr='discovery failed')
    try:
        namespace['rtdir']()
    except RuntimeError as error:
        assert 'discovery failed' in str(error)
    else:
        raise AssertionError('Failed runtime-directory discovery was accepted')

print('Android runtime checks passed: fresh first link, partial repair, search paths and discovery failure.')
