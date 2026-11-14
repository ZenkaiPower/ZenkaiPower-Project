#!/usr/bin/env python3
"""Run with python3 port/tests/build/incremental.py; uses the host C compiler."""
import importlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import builddeps


class Dependencies(unittest.TestCase):
    def test_compiler_dependencies(self):
        with tempfile.TemporaryDirectory(prefix="zenkaipower build ") as temp:
            root = Path(temp)
            header = root / "shared header.h"
            header.write_text("#define VALUE 1\n")
            used = root / "used.c"
            used.write_text('#include "shared header.h"\nint value(void) { return VALUE; }\n')
            unrelated = root / "unrelated.c"
            unrelated.write_text("int other(void) { return 0; }\n")
            for source in (used, unrelated):
                obj = source.with_suffix(".o")
                subprocess.run(["cc", "-c", str(source), "-o", str(obj)] + builddeps.flags(obj), check=True)
                self.assertFalse(builddeps.stale(obj, source))
            # Deterministic timestamps avoid sleeps and filesystem timestamp resolution assumptions.
            newer = max(p.stat().st_mtime_ns for p in root.glob("*.o")) + 1_000_000_000
            os.utime(header, ns=(newer, newer))
            builddeps.modified.cache_clear()
            self.assertTrue(builddeps.stale(used.with_suffix(".o"), used))
            self.assertFalse(builddeps.stale(unrelated.with_suffix(".o"), unrelated))
            header.unlink()
            builddeps.modified.cache_clear()
            self.assertTrue(builddeps.stale(used.with_suffix(".o"), used))
            unrelated.with_suffix(".d").unlink()
            self.assertTrue(builddeps.stale(unrelated.with_suffix(".o"), unrelated))

    def test_preprocessing_dependencies(self):
        # The EE pipeline preprocesses before rewriting constants/pointers. Its depfile must survive that step.
        import undefined
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source, header, obj = root / "source.c", root / "forced.h", root / "source.o"
            header.write_text("#define VALUE 7\n")
            source.write_text("int value(void) { return VALUE; }\n")
            with mock.patch.multiple(undefined, VARIANT="gcc", BITS64=False, MAC=False, WIN=False):
                result = undefined.compile_ee(["gcc", "-c", "-include", str(header)], source, obj)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(builddeps.stale(obj, source))
            newer = obj.stat().st_mtime_ns + 1_000_000_000
            os.utime(header, ns=(newer, newer))
            builddeps.modified.cache_clear()
            self.assertTrue(builddeps.stale(obj, source))

    def test_parallel_host_compilation_writes_dependencies(self):
        import concurrent.futures
        import undefined
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            tasks = []
            for name in ("one", "two"):
                source = root / (name + ".c")
                source.write_text(f"int {name}(void) {{ return 1; }}\n")
                tasks.append((["gcc", "-c"], source, source.with_suffix(".o"), False))
            with mock.patch.multiple(undefined, VARIANT="gcc", BITS64=False, MAC=False, WIN=False), \
                 concurrent.futures.ThreadPoolExecutor(2) as executor:
                for source, obj, result in executor.map(undefined.compile_host, tasks):
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertFalse(builddeps.stale(obj, source))

    def test_link_is_skipped_only_when_complete_and_unchanged(self):
        linker = importlib.import_module("link")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            obj, data, exe = build / "obj", build / "data", build / "game"
            obj.mkdir(parents=True)
            data.mkdir()
            object_file = obj / "one.o"
            object_file.write_bytes(b"object")
            calls = []

            def fake_link(objects, extra):
                calls.append(objects)
                exe.write_bytes(b"executable")
                Path(str(exe) + ".map").write_text("map")
                return subprocess.CompletedProcess([], 0, "", "")

            def postprocess(*args, **kwargs):
                Path(str(exe) + ".mem").write_text("memory")
                return subprocess.CompletedProcess([], 0, "", "")

            with mock.patch.multiple(linker, ROOT=root, B=build, OBJ=obj, DATA=data, EXE=exe), \
                 mock.patch.object(linker, "link", side_effect=fake_link), \
                 mock.patch.object(linker.subprocess, "run", side_effect=postprocess), \
                 mock.patch.dict(os.environ, {"ZENKAIPOWER_FULL_REBUILD": "0"}):
                linker.main()
                count = len(calls)
                linker.main()
                self.assertEqual(len(calls), count)
                object_file.write_bytes(b"changed object")
                linker.main()
                self.assertGreater(len(calls), count)
                count = len(calls)
                Path(str(exe) + ".mem").unlink()
                linker.main()
                self.assertGreater(len(calls), count)
                count = len(calls)
                object_file.unlink()
                linker.main()
                self.assertGreater(len(calls), count)


if __name__ == "__main__":
    unittest.main()
