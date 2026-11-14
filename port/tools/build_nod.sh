#!/bin/sh
# Build the same pinned disc reader used by Android for desktop installers.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
TC=${ZENKAIPOWER_NOD_ROOT:-$ROOT/port/build/nod}
SRC=$TC/source
REV=ebd80cac99b48a323200d84365e07a58cc27412d
export CARGO_HOME=${CARGO_HOME:-$TC/cargo} RUSTUP_HOME=${RUSTUP_HOME:-$TC/rustup}
export PATH="$CARGO_HOME/bin:$PATH"
if [ ! -x "$CARGO_HOME/bin/rustup" ]; then
    mkdir -p "$TC"
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o "$TC/rustup-init.sh"
    sh "$TC/rustup-init.sh" -y --profile minimal --default-toolchain 1.94.0
fi
if ! rustup run 1.94.0 rustc --version >/dev/null 2>&1; then
    rustup toolchain install 1.94.0 --profile minimal
fi
if [ ! -d "$SRC/.git" ]; then
    git init "$SRC"
    git -C "$SRC" remote add origin https://github.com/encounter/nod.git
fi
if [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null || true)" != "$REV" ]; then
    git -C "$SRC" fetch --depth 1 origin "$REV"
    git -C "$SRC" checkout --detach "$REV"
fi
python3 "$ROOT/port/tools/android_nod_patch.py" "$SRC"
case "${1:-linux}" in
    linux) TARGET=x86_64-unknown-linux-gnu ;;
    win) TARGET=x86_64-pc-windows-gnu
         case "$(uname -s)" in
             MINGW*|MSYS*|CYGWIN*) NOD_CC=gcc; NOD_AR=ar ;;
             *) NOD_CC=x86_64-w64-mingw32-gcc; NOD_AR=x86_64-w64-mingw32-ar ;;
         esac
         export CC_x86_64_pc_windows_gnu=${CC_x86_64_pc_windows_gnu:-$NOD_CC}
         export AR_x86_64_pc_windows_gnu=${AR_x86_64_pc_windows_gnu:-$NOD_AR}
         export CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=$CC_x86_64_pc_windows_gnu ;;
    *) echo 'Usage: build_nod.sh [linux|win]' >&2; exit 2 ;;
esac
rustup target add --toolchain 1.94.0 "$TARGET"
export CARGO_BUILD_JOBS=${ZENKAIPOWER_JOBS:-2}
cd "$SRC"
cargo +1.94.0 build --locked --release --target "$TARGET" -p nod-ffi --no-default-features \
    --features compress-bzip2-static,compress-lzma-static,compress-zlib-static,compress-zstd-static
