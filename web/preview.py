"""Build and serve the website, rebuilding when its source files change."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import threading
import time

from prepare import ROOT, prepare


def snapshot():
    paths = [ROOT / 'README.md', ROOT / 'assets/zp-avatar.png']
    for directory in (ROOT / 'web', ROOT / 'figs'):
        paths.extend(path for path in directory.rglob('*') if path.is_file())
    return tuple(sorted((str(path), path.stat().st_mtime_ns, path.stat().st_size)
                        for path in paths if path.exists() and '__pycache__' not in path.parts))


def build(root):
    build_dir = Path(tempfile.mkdtemp(prefix='build-', dir=root))
    current = root / 'ZenkaiPower-Project'
    old_dir = current.resolve().parent if current.is_symlink() else None
    try:
        prepare(build_dir / 'source')
        output = build_dir / 'site'
        subprocess.run(['jekyll', 'build', '--source', str(build_dir / 'source'),
                        '--destination', str(output)], check=True)
        subprocess.run([sys.executable, str(ROOT / 'web/check.py'), str(output)], check=True)
        link = root / 'next-site'
        link.symlink_to(output, target_is_directory=True)
        link.replace(current)
    except Exception:
        shutil.rmtree(build_dir, ignore_errors=True)
        raise
    if old_dir:
        shutil.rmtree(old_dir, ignore_errors=True)


def main():
    with tempfile.TemporaryDirectory(prefix='pages-preview-') as tmp:
        root = Path(tmp)
        previous = snapshot()
        build(root)
        server = ThreadingHTTPServer(('0.0.0.0', 4000), partial(SimpleHTTPRequestHandler, directory=tmp))
        threading.Thread(target=server.serve_forever, daemon=True).start()
        print('Preview: http://localhost:4000/ZenkaiPower-Project/', flush=True)
        try:
            while True:
                time.sleep(2)
                current = snapshot()
                if current != previous:
                    previous = current
                    print('Changes detected; rebuilding...', flush=True)
                    try:
                        build(root)
                        print('Rebuilt. Refresh http://localhost:4000/ZenkaiPower-Project/', flush=True)
                    except (OSError, ValueError, subprocess.CalledProcessError) as error:
                        print(f'Build failed: {error}', file=sys.stderr, flush=True)
        except KeyboardInterrupt:
            pass
        finally:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    main()
