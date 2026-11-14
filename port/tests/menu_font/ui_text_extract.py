#!/usr/bin/env python3
"""Regression checks for the expanded EU text-entry mapping."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("extract_eu", ROOT / "port/tools/extract_eu.py")
EU = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EU)


def main():
    entries = EU.entries()
    assert len(entries) == 410
    assert entries[:5] == [5, 6, 7, 8, 9]
    assert entries[5:160] == sorted(set(range(451, 606)))
    assert entries[-250:] == list(range(656, 906))

    extract_source = (ROOT / "port/src/gs/ui_text_extract.c").read_text()
    assert '"extracted USA game text, version 3"' in extract_source
    assert '"European disc, version 2"' in extract_source
    assert "kUtTextFiles" in extract_source
    assert "usa >= 560 ? 656" in extract_source

    print("Expanded EU story-text extraction mapping passed")


if __name__ == "__main__":
    main()
