"""Compiler-generated dependencies for incremental port builds."""
import functools
import hashlib
import json
import pathlib
import shlex


def flags(obj):
    # A fixed target avoids drive-letter colons in Windows object paths.
    return ["-MD", "-MF", str(pathlib.Path(obj).with_suffix(".d")), "-MT", "object"]


@functools.lru_cache(maxsize=None)
def modified(path):
    try:
        return pathlib.Path(path).stat().st_mtime_ns
    except OSError:
        return None


def stale(obj, src, since=0):
    obj, src = pathlib.Path(obj), pathlib.Path(src)
    if not obj.is_file() or obj.stat().st_size == 0:
        return True
    built = obj.stat().st_mtime_ns
    if built < max(src.stat().st_mtime_ns, since):
        return True
    if src.suffix not in (".c", ".cpp"):
        return False
    try:
        text = obj.with_suffix(".d").read_text().replace("\\\n", "")
        target, separator, body = text.partition(":")
        dependencies = shlex.split(body)
        if not separator or not dependencies:
            return True
    except (OSError, ValueError):
        return True
    root = pathlib.Path(__file__).resolve().parents[2]
    return any((changed := modified(str(pathlib.Path(path) if pathlib.Path(path).is_absolute() else root / path)))
               is None or changed > built for path in dependencies)


def fingerprint(paths, settings=()):
    """Input list, metadata and configuration, including missing files and removed inputs."""
    entries = []
    for path in paths:
        path = pathlib.Path(path)
        try:
            stat = path.stat()
            entries.append((str(path), stat.st_size, stat.st_mtime_ns))
        except FileNotFoundError:
            entries.append((str(path), None, None))
    return hashlib.sha256(json.dumps((entries, settings), sort_keys=True).encode()).hexdigest()
