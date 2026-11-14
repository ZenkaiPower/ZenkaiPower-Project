# Dolphin corresponding source and provenance

Version: **2609b**
Revision: **3a5b3743b18da3e8f04cbc44f20dc50fbd3a9ef3**
Upstream: https://github.com/dolphin-emu/dolphin/tree/3a5b3743b18da3e8f04cbc44f20dc50fbd3a9ef3

Both development executables were rebuilt from this source. The downloaded source
ZIP's SHA-256 is `2d091225e7f9f8809ee758c9aefcb69d470a211360ec6e12400b6eb93cda88bd`.
The archive's directory uses the annotated tag object `847cc451`; the commit
above is the actual source revision.

## Development source archive

The complete matching source is retained beside these development tools as
`dolphin-tool-source.tar.gz`. SHA-256:

`beaa7d154b4de54c45d69137ce50ffc3bafebc3f85b93143a95fdd9f9058d029`

DolphinTool and its runtime DLLs are development dependencies. Desktop and
Android game releases use the linked nod reader and do not distribute them.
When sharing the development executables, provide this source archive and
retain the applicable notices.

The source bundle includes pinned source submodules in `dolphin/SOURCES.json`,
build recipes in `recipes/`, and resulting executable hashes in `BUILD.json`.
The GUI/media-only prebuilt Qt and FFmpeg-bin submodules are omitted and disabled.

## Source modification

`build/msvc-2022.patch` lowers the precompiled-header compiler-version gate from
MSVC 19.51 to MSVC 19.44, allowing the standalone target to build with the installed
Visual Studio 2022 17.14 toolchain. This patch is already applied in the retained source
archive. No gameplay, image parsing or extraction logic was changed.

## Linux x86-64

Built using `build/linux.Dockerfile`, `build/linux.sh` and `build/options.txt`:
Ubuntu 22.04, GCC 12, CMake 3.31.6, Release configuration, bundled source libraries,
and statically linked GCC C++ runtimes. The source archive records the installed
Linux package and compiler versions. System glibc and libm remain dynamic.

## Windows x86-64

Built using `build/windows.ps1` and `build/options.txt`: Visual Studio 2022,
MSVC 19.44.35213.0, Windows SDK 10.0.26100.0, CMake 3.31.6-msvc6, Release x64.
This is a local rebuild, rather than the previously bundled official 2609-127
executable.

The adjacent Microsoft Visual C++ runtime DLLs are from version 14.44.35211:
https://download.visualstudio.microsoft.com/download/pr/990c720a-46ce-4dc7-87b9-c2eb615d34b9/CC0FF0EB1DC3F5188AE6300FAEF32BF5BEEBA4BDD6E8E445A9184072096B713B/VC_redist.x64.exe

Redistributable SHA-256: `cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b`

Microsoft terms are in `LICENSES/microsoft-vc-runtime.rtf`. Dolphin and dependency
notices are in `COPYING`, `LICENSES/` and the corresponding-source archive. The
Microsoft runtime is separate from Dolphin source. Windows 10/11 supplies the
Universal CRT and other operating-system DLLs. `binaries.json` records the hashes
of every shipped executable/DLL and the matching source archive.
