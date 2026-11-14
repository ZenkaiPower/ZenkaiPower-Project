#!/usr/bin/env python3
"""Exercise real disc-tool listing/extraction using a synthetic GameCube disc.

No game data is needed. Run with Python 3; --tool accepts a different build.
Use --runner /usr/lib/wine/wine64 --tool .../DolphinTool.exe for Windows via Wine.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def make_disc(path):
    payload = b"ZENKAI Power! bundled disc tool fixture\n"
    data = bytearray(0x4000)
    data[:6] = b"TEST01"
    struct.pack_into(">I", data, 0x1c, 0xc2339f3d)
    data[0x20:0x20 + 19] = b"Synthetic test disc"
    # Root FST node, a file node, then the string table. File offsets are
    # unencrypted GameCube offsets; the tool uses the same extractor CLI for Wii.
    names = b"\0fixture.bin\0"
    fst = struct.pack(">III", 0x01000000, 0, 2)
    fst += struct.pack(">III", 1, 0x2000, len(payload)) + names
    struct.pack_into(">III", data, 0x424, 0x1000, len(fst), len(fst))
    data[0x1000:0x1000 + len(fst)] = fst
    data[0x2000:0x2000 + len(payload)] = payload
    path.write_bytes(data)
    return payload


def exercise(tool, runner=None):
    command = ([runner] if runner else []) + [str(tool)]
    with tempfile.TemporaryDirectory(prefix="zenkaipower-dolphin-test-") as temporary:
        root = Path(temporary)
        disc = root / "disc image.iso"
        payload = make_disc(disc)
        def tool_path(path):
            return "Z:" + str(path).replace("/", "\\") if runner else str(path)
        listing = subprocess.run(command + ["extract", "-i", tool_path(disc), "-g", "-l"],
                                 capture_output=True, text=True, timeout=90)
        assert listing.returncode == 0, listing.stdout + listing.stderr
        assert "fixture.bin" in listing.stdout, listing.stdout + listing.stderr
        output = root / "extracted files"
        subprocess.run(command + ["extract", "-i", tool_path(disc), "-g", "-s", "fixture.bin",
                                  "-o", tool_path(output), "-q"], check=True,
                       capture_output=True, text=True, timeout=90)
        found = list(output.rglob("fixture.bin"))
        assert len(found) == 1 and found[0].read_bytes() == payload, found
        bad = root / "invalid.iso"
        bad.write_bytes(b"not a disc image")
        rejected = subprocess.run(command + ["extract", "-i", tool_path(bad), "-g", "-l"],
                                  capture_output=True, timeout=90)
        assert rejected.returncode != 0, "invalid disc was accepted"
    print(f"PASS: {tool.name}: listing, extraction, paths with spaces, invalid disc rejection")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=Path, default=Path(__file__).resolve().parents[2] /
                        "third_party/dolphin-tool/linux-x64/dolphin-tool")
    parser.add_argument("--runner")
    args = parser.parse_args()
    exercise(args.tool.resolve(), args.runner)
