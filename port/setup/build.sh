#!/bin/sh
# Builds the setup window (port/setup/setup.cpp, native.cpp).
#   port/setup/build.sh        -> ./ZenkaiPower-setup            (needs g++ and SDL3; Dear ImGui is in the repository)
#   port/setup/build.sh win    -> port/build/ZenkaiPower-setup.exe
#        on Linux: cross-compiled with mingw-w64 (x86_64-w64-mingw32-g++) and SDL3's mingw development package
#                  unpacked under port/build/win (or its x86_64-w64-mingw32 folder named by ZENKAIPOWER_SDL3);
#        on Windows (MSYS2 shell): with that shell's g++ and SDL3.
cd "$(dirname "$0")/../.." || exit 1
I=port/third_party/imgui
if [ "$1" = win ]; then
    O=port/build/setup_win; OUT=port/build/ZenkaiPower-setup.exe; LIBS="-lSDL3 -static -static-libgcc -static-libstdc++ -mwindows"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) CXX=g++; SDL="" ;;
        *) CXX=x86_64-w64-mingw32-g++
           S=${ZENKAIPOWER_SDL3:-$(ls -d port/build/win/SDL3-*/x86_64-w64-mingw32 2>/dev/null | tail -1)}
           [ -d "$S" ] || { echo "SDL3 for mingw not found: unpack SDL3-devel-*-mingw.tar.gz under port/build/win"; exit 1; }
           SDL="-I$S/include -L$S/lib" ;;
    esac
else
    O=port/build/setup; OUT=${ZENKAIPOWER_SETUP_OUT:-ZenkaiPower-setup}; CXX=g++; SDL=""; LIBS="-lSDL3 -lpthread -ldl -static-libstdc++ -static-libgcc -Wl,-rpath,\$ORIGIN/lib"
fi
set -e
sh port/tools/build_nod.sh "${1:-linux}"
NOD_ROOT=${ZENKAIPOWER_NOD_ROOT:-$(pwd)/port/build/nod}
case "${1:-linux}" in win) NOD_TARGET=x86_64-pc-windows-gnu ;; *) NOD_TARGET=x86_64-unknown-linux-gnu ;; esac
NOD_LIB="$NOD_ROOT/source/target/$NOD_TARGET/release/libnod.a"
SDL="$SDL -DZENKAIPOWER_NOD_INSTALLER=1 -I$NOD_ROOT/source/nod-ffi/include"
mkdir -p "$O"
# A compiler/flag change invalidates cached objects as well as source changes.
# Do not touch the stamp on an unchanged invocation.
{
    printf '%s\n' "$CXX" "$SDL" "$LIBS" '-std=c++17 -O2 -w'
    "$CXX" --version
} > "$O/settings.tmp"
if [ "${ZENKAIPOWER_FULL_REBUILD:-0}" = 1 ] || ! cmp -s "$O/settings.tmp" "$O/settings"; then
    mv "$O/settings.tmp" "$O/settings"
else
    rm "$O/settings.tmp"
fi
JOBS=${ZENKAIPOWER_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
case "$JOBS" in *[!0-9]*|0|'') echo "ZENKAIPOWER_JOBS must be a positive integer" >&2; exit 1 ;; esac
# Keep default memory use bounded; an explicit ZENKAIPOWER_JOBS overrides this limit.
if [ -z "${ZENKAIPOWER_JOBS:-}" ] && [ "$JOBS" -gt 8 ]; then JOBS=8; fi
rm -f "$O/cover_crypto.o" "$O/cover_crypto.o.d"
python3 port/tools/setup_text.py --out "$O/setup_text_defaults.h"
make --no-print-directory -f port/setup/objects.mk -j "$JOBS" O="$O" I="$I" CXX="$CXX" SDL="$SDL" objects
if [ "$1" = win ]; then
    python3 port/tools/windows_resource.py --out "$O/icon.o" --windres "${CXX%g++}windres"
    if [ ! -f "$O/title-launcher.exe" ] || [ port/setup/title_launcher.cpp -nt "$O/title-launcher.exe" ] || [ "$O/settings" -nt "$O/title-launcher.exe" ]; then
        $CXX -std=c++17 -Os -static -municode -mwindows port/setup/title_launcher.cpp -o "$O/title-launcher.exe"
    fi
    if [ ! -f "$O/launcher-resource.o" ] || [ "$O/title-launcher.exe" -nt "$O/launcher-resource.o" ]; then
        printf '200 RCDATA "%s/title-launcher.exe"\n' "$O" > "$O/launcher.rc"
        "${CXX%g++}windres" -I . -i "$O/launcher.rc" -O coff -o "$O/launcher-resource.o"
    fi
fi
link=false
[ -f "$OUT" ] || link=true
[ "$0" -nt "$OUT" ] && link=true
[ "$O/settings" -nt "$OUT" ] && link=true
[ "$NOD_LIB" -nt "$OUT" ] && link=true
for o in "$O"/*.o; do
    [ "$o" -nt "$OUT" ] && link=true
done
if [ "$link" = false ]; then
    echo "up to date: $OUT"
    exit 0
fi
# -Wl,-Bdynamic for SDL3 only: the rest (C++ runtime, threads) is linked in.
# Link on the object filesystem first. In Docker Desktop the repository root
# is a Windows bind mount, while O is on the Linux build volume. GNU ld can
# fail with EIO when writing its output directly through that bind mount.
LINK_OUT="$O/setup-link.tmp"
if [ "$1" = win ]; then
    $CXX -o "$LINK_OUT" "$O"/*.o "$NOD_LIB" $SDL -Wl,-Bdynamic -lSDL3 -Wl,-Bstatic -lstdc++ -lwinpthread -static-libgcc -mwindows -Wl,-Bdynamic -lole32 -luuid -lws2_32 -luserenv -lntdll -lbcrypt
else
    $CXX -o "$LINK_OUT" "$O"/*.o "$NOD_LIB" $LIBS -lm
fi
# Publish only a successfully linked executable. Replace a partial output
# through a sibling staging file so a failed copy leaves the old file intact.
cp "$LINK_OUT" "$OUT.tmp"
mv -f "$OUT.tmp" "$OUT"
rm -f "$LINK_OUT"
echo "built $OUT"
