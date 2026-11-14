"""Read loose files or entries in the runtime's named .pak format."""
from pathlib import Path
import struct


def read_data(path):
    path = Path(path)
    if path.is_file():
        return path.read_bytes()
    parents = list(path.parents)
    parents.sort(key=lambda p: p.name == "translations")
    candidates = []
    for parent in parents:
        if parent.name == 'game-text':
            relative = path.relative_to(parent).as_posix()
            if relative.startswith('translations/skills/'):
                candidates.append((parent.parent / 'game-skills.pak', relative[len('translations/skills/'):]))
    for parent in parents:
        relative = path.relative_to(parent).as_posix()
        if relative.startswith(('soundtrack/meteor/', 'title/sparking-meteor.bin', 'disc/DATA/ZS3JP')):
            candidates.append((parent / 'sparking-meteor.pak', relative))
    for parent in parents:
        if parent.name in ('disc', 'game-text', 'game-info', 'fonts', 'dragon-net-battle', 'translations'):
            candidates.append((parent.with_suffix('.pak'), path.relative_to(parent).as_posix()))
    for package, relative in candidates:
        if not package.is_file():
            continue
        with package.open('rb') as source:
            header = source.read(12)
            if len(header) != 12 or header[:8] != b'BT3TEXT1':
                raise ValueError('Invalid data-package header')
            count, = struct.unpack('<I', header[8:])
            if count > 100000:
                raise ValueError('Invalid data-package entry count')
            total = package.stat().st_size
            for _ in range(count):
                record = source.read(20)
                if len(record) != 20:
                    raise ValueError('Truncated data-package index')
                length, offset, size = struct.unpack('<IQQ', record)
                if not 0 < length < 1200 or offset > total or size > total - offset:
                    raise ValueError('Invalid data-package entry bounds')
                name = source.read(length)
                if len(name) != length:
                    raise ValueError('Truncated data-package filename')
                if name.decode('utf-8') == relative:
                    source.seek(offset)
                    data = source.read(size)
                    if len(data) != size:
                        raise ValueError('Truncated data-package entry')
                    return data
    raise FileNotFoundError(path)


def pack_directory(directory, keep_loose=False, package=None):
    """Publish a complete named package, then remove only its input files."""
    import os
    import shutil
    import tempfile
    directory = Path(directory)
    files = sorted(p for p in directory.rglob('*') if p.is_file())
    if not files:
        raise ValueError('Cannot package an empty resource directory')
    entries = []
    offset = 12
    for path in files:
        name = path.relative_to(directory).as_posix().encode('utf-8')
        if not 0 < len(name) < 1200:
            raise ValueError('Invalid package filename length')
        size = path.stat().st_size
        entries.append((path, name, size))
        offset += 20 + len(name)
    if offset + sum(size for _, _, size in entries) > 0x7fffffff:
        raise ValueError('Package exceeds the portable runtime size limit')
    package = Path(package) if package is not None else directory.with_suffix('.pak')
    fd, temporary = tempfile.mkstemp(prefix=package.name+'.', suffix='.tmp', dir=package.parent)
    try:
        with os.fdopen(fd, 'wb') as out:
            out.write(struct.pack('<8sI', b'BT3TEXT1', len(entries)))
            for path, name, size in entries:
                out.write(struct.pack('<IQQ', len(name), offset, size))
                out.write(name)
                offset += size
            for path, _, size in entries:
                start = out.tell()
                with path.open('rb') as source:
                    shutil.copyfileobj(source, out)
                if out.tell() - start != size:
                    raise ValueError('Source file changed while packing')
            out.flush()
            os.fsync(out.fileno())
        os.replace(temporary, package)
    finally:
        Path(temporary).unlink(missing_ok=True)
    if keep_loose:
        return package
    for path, _, _ in entries:
        path.unlink()
    for folder in sorted((p for p in directory.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
        try:
            folder.rmdir()
        except OSError:
            pass
    try:
        directory.rmdir()
    except OSError:
        pass
    return package


def restore_directory(directory):
    """Restore a package for an update, preserving existing loose overrides."""
    import os
    directory = Path(directory)
    package = directory.with_suffix('.pak')
    if not package.is_file():
        return
    with package.open('rb') as source:
        header = source.read(12)
        if len(header) != 12 or header[:8] != b'BT3TEXT1':
            raise ValueError('Invalid package header')
        count, = struct.unpack('<I', header[8:])
        if count > 100000:
            raise ValueError('Invalid entry count')
        total = package.stat().st_size
        entries = []
        for _ in range(count):
            record = source.read(20)
            if len(record) != 20:
                raise ValueError('Truncated package index')
            length, offset, size = struct.unpack('<IQQ', record)
            if not 0 < length < 1200 or offset > total or size > total-offset:
                raise ValueError('Invalid package bounds')
            name = source.read(length).decode('utf-8')
            rel = Path(name)
            if rel.is_absolute() or '..' in rel.parts or '\\' in name or '\0' in name:
                raise ValueError('Invalid package path')
            entries.append((rel, offset, size))
        for rel, offset, size in entries:
            path = directory / rel
            if not path.resolve().is_relative_to(directory.resolve()):
                raise ValueError('Package path escapes its resource directory')
            if path.exists():
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            try:
                output = path.open('xb')
            except FileExistsError:
                continue
            try:
                with output:
                    source.seek(offset)
                    while size:
                        block = source.read(min(1024 * 1024, size))
                        if not block:
                            raise ValueError('Truncated package entry')
                        output.write(block)
                        size -= len(block)
            except Exception:
                path.unlink(missing_ok=True)
                raise
