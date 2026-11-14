#!/usr/bin/env python3
"""Test atomic Wii catalog replacement with scratch on another filesystem."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='zenkaipower-catalog-') as destination:
    root = Path(destination)
    scratch_parent = next((p for p in (Path('/dev/shm'), Path.cwd())
                           if p.is_dir() and os.access(p, os.W_OK)
                           and p.stat().st_dev != root.stat().st_dev), None)
    if scratch_parent is None:
        raise SystemExit('No second writable filesystem available for catalog regression')
    with tempfile.TemporaryDirectory(prefix='zenkaipower-scratch-', dir=scratch_parent) as scratch:
        target = root/'ui/en.json'
        target.parent.mkdir()
        target.write_text('original catalog')
        result = subprocess.run([str(Path(sys.argv[1]).resolve()), 'publish-catalog', str(root)],
                                env={**os.environ, 'TMPDIR': scratch}, capture_output=True)
        assert result.returncode == 0, result.stderr
        assert target.read_text() == 'replacement catalog'
        assert not list(root.rglob('.zenkaipower-extract-*'))
        # A failed publication must clean its staging directory as well.
        target.unlink()
        target.mkdir()
        (target/'keep').write_text('untouched')
        result = subprocess.run([str(Path(sys.argv[1]).resolve()), 'publish-catalog', str(root)],
                                env={**os.environ, 'TMPDIR': scratch}, capture_output=True)
        assert result.returncode == 1
        assert (target/'keep').read_text() == 'untouched'
        assert not list(root.rglob('.zenkaipower-extract-*'))
print('PASS: cross-filesystem catalog replacement and failure cleanup')
