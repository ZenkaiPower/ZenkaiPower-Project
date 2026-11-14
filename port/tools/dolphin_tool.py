"""Locate the development-only Dolphin disc tool, with PATH as the final fallback."""
from pathlib import Path
import os
import shutil


def find_dolphin_tool(root=None):
    root = Path(root) if root is not None else Path(__file__).resolve().parents[2]
    names = ("DolphinTool.exe", "dolphin-tool.exe") if os.name == "nt" else ("dolphin-tool",)
    platform = "windows-x64" if os.name == "nt" else "linux-x64"
    for directory in (root / "tools/dolphin",
                      root / "port/third_party/dolphin-tool" / platform,
                      root / "port/build/dolphin/Binaries"):
        for name in names:
            candidate = directory / name
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return str(candidate.resolve())
    return next((found for name in names if (found := shutil.which(name))), None)
