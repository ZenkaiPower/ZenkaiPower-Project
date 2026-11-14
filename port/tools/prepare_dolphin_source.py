#!/usr/bin/env python3
"""Verify and stage the source retained for the development-only Dolphin tools."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "port/third_party/dolphin-tool/binaries.json"


def checksum(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def prepare_source(out, archive=None, url=None):
    out = Path(out)
    expected = json.loads(MANIFEST.read_text())["source_sha256"]
    if out.is_file() and checksum(out) == expected:
        return out
    if not archive and not url:
        raise ValueError("Supply --archive or set ZENKAIPOWER_DOLPHIN_SOURCE_URL to the hosted matching source archive")
    out.parent.mkdir(parents=True, exist_ok=True)
    # Verify before replacing any existing release asset.
    with tempfile.TemporaryDirectory(prefix="dolphin-source-", dir=out.parent) as temporary:
        candidate = Path(temporary) / "source.tar.gz"
        if archive:
            shutil.copyfile(archive, candidate)
        else:
            with urlopen(url, timeout=60) as response, candidate.open("wb") as target:
                shutil.copyfileobj(response, target)
        if checksum(candidate) != expected:
            raise ValueError("Dolphin source archive checksum does not match the bundled binaries")
        os.replace(candidate, out)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=ROOT / "port/third_party/dolphin-tool/dolphin-tool-source.tar.gz", help="matching development source archive")
    parser.add_argument("--url", default=os.environ.get("ZENKAIPOWER_DOLPHIN_SOURCE_URL"))
    parser.add_argument("--out", type=Path, default=ROOT / "port/build/dolphin-tool-source.tar.gz")
    args = parser.parse_args()
    try:
        print(prepare_source(args.out, None if args.url else args.archive, args.url))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Dolphin source: {error}\n")


if __name__ == "__main__":
    main()
