#!/usr/bin/env python3
"""Embed the editable setup English catalog as a standalone installer fallback."""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# These are the argument contracts of C++ call sites, independent of editable text.
FORMATS = {
    "failure.launch_game": ["%s"],
    "cli.initialization_error": ["%s", "%s"],
    "cli.opengl_fallback": ["%s", "%s"],
    "cli.release_required": ["%s", "%s", "%s"],
    "cli.window_error": ["%s", "%s", "%s", "%s", "%s"],
    "error.could_not_create_s_windows_error_0x_08lx_check_write_permissions_in_the_game": ["%s", "%08lX"],
    "error.could_not_create_the_desktop_shortcut_windows_error_0x_08lx": ["%08lX"],
    "error.setup_the_game_exited_with_code_d_0x_08x_s": ["%d", "%08X", "%s"],
    "error.shortcut_warning": ["%s", "%08lX"],
    "progress.completed_operations": ["%d", "%d"],
    "progress.elapsed": ["%llu"],
    "progress.files_extracted": ["%d", "%d", "%%"],
    "progress.files_processed": ["%d", "%d"],
    "progress.game_files_installed": ["%d"],
    "progress.percent": ["%d", "%%"],
    "updates.available": ["%s", "%s"],
    "updates.current": ["%s"],
    "version.number": ["%s"],
    "welcome.description": ["%s"],
    "window.title": ["%s"],
}
PLACEHOLDER = re.compile(r"%(?:%|[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp])")

def generate(source, output):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"Duplicate setup text key: {key}")
            result[key] = value
        return result
    entries = json.loads(source.read_text(encoding="utf-8-sig"), object_pairs_hook=unique)
    if not isinstance(entries, dict) or not entries:
        raise ValueError("Setup text catalog must be a nonempty object")
    for key, expected in FORMATS.items():
        value = entries.get(key)
        if not isinstance(value, str) or PLACEHOLDER.findall(value) != expected or "%" in PLACEHOLDER.sub("", value):
            raise ValueError(f"Keep the formatting placeholders for {key}: {expected}")
    lines = []
    for key, value in entries.items():
        if not isinstance(value, str) or "\0" in value:
            raise ValueError(f"Invalid setup text: {key}")
        # UTF-8 source literals are supported by both native g++ and MinGW.
        lines.append("    {" + json.dumps(key, ensure_ascii=False) + ", " + json.dumps(value, ensure_ascii=False) + "},")
    text = "// Generated from port/setup/text/en.json. Edit the JSON, not this file.\n#pragma once\n#include <array>\n#include <string>\n#include <utility>\n"
    text += f"static const std::array<std::pair<std::string,std::string>,{len(entries)}> kSetupTextDefaults={{{{\n"
    text += "\n".join(lines) + "\n}};\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != text:
        output.write_text(text, encoding="utf-8", newline="\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    generate(ROOT / "port/setup/text/en.json", args.out)
