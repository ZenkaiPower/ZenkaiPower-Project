"""Release cleanup preserves player folders and excludes them from ZIP input."""
import sys
import tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from package import reset_release, release_files, PRESERVED_FOLDERS, PRESERVED_FILES
assert "setup" in PRESERVED_FOLDERS  # Local artwork must survive builds and stay out of ZIPs.
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    for folder in PRESERVED_FOLDERS | {"licenses"}:
        (root / folder / "nested").mkdir(parents=True)
        (root / folder / "nested/user").write_bytes(b"keep")
    for name in PRESERVED_FILES:
        (root / name).write_bytes(b"player icon or shortcut")
    (root / "old.exe").write_bytes(b"old")
    reset_release(root)
    for folder in PRESERVED_FOLDERS:
        assert (root / folder / "nested/user").read_bytes() == b"keep"
    for name in PRESERVED_FILES:
        assert (root / name).read_bytes() == b"player icon or shortcut"
    assert not (root / "old.exe").exists() and not (root / "licenses").exists()
    (root / "game.exe").write_bytes(b"new")
    (root / "licenses").mkdir()
    (root / "licenses/curl.txt").write_bytes(b"notice")
    assert {p.relative_to(root).as_posix() for p in release_files(root)} == {"game.exe", "licenses/curl.txt"}
print("PASS: release cleanup preserves player folders and excludes them from archives")
