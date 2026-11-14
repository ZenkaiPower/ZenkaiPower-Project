The Android engine build, low-address loader, AArch64 IR store corrections,
register save/restore, and memory allocation support are adapted from
[Dragon Rage / Tenkaichi3DecompAndroid](https://github.com/dfdx047/Tenkaichi3DecompAndroid),
revision recorded in `reference-revision.txt`.

Copyright (c) 2026 z3xox and contributors. The reference repository provides
AGPL-3.0 and MIT license texts. This project retains its existing license;
the reference license texts are included in `licenses/` for attribution.

SDL3 is distributed under the zlib license. Its Android Java sources and native
library are build dependencies, copied into generated app folders by the engine
build. No game data or private signing keys are included in the APK.

The APK reuses this repository's desktop native installer for PS2 validation,
extraction and optional content. Wii ISO/RVZ reading uses
[nod](https://github.com/encounter/nod), revision
`ebd80cac99b48a323200d84365e07a58cc27412d` (2.0.0-alpha.12), under MIT/Apache-2.0.
`port/tools/android_nod.sh` pins the Rust toolchain and dependency lockfile.
`android_nod_patch.py` adds overflow and allocation bounds to malformed DOL
metadata, returning a disc-format error instead of panicking across the C API.
Upstream, Rust dependency and compression-library notices are included in the
APK and copied to the game folder's `licenses/` directory. All three installers share `port/setup/disc_reader.h`. DolphinTool is retained
only for development and is not included in game releases.

Metropolis Bold by Chris Simpson is used for pause and Training Settings
headings, under the Unlicense. Its full license and source attribution are
included in `fonts.pak`; the license is also copied to `licenses/metropolis-fonts.txt`.
