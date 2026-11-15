#!/bin/sh
# Builds the native bot GUI (SDL3 and Dear ImGui).
#   port/bot/build.sh [all]  -> both Linux and Windows (default)
#   port/bot/build.sh linux  -> release/bot/ZenkaiPower-bot (needs g++ and SDL3)
#   port/bot/build.sh win    -> release/bot/ZenkaiPower-bot.exe
#        on Linux: cross-compiled with mingw-w64 (x86_64-w64-mingw32-g++) and SDL3's mingw development package
#                  unpacked under port/build/win (or its x86_64-w64-mingw32 folder named by ZENKAIPOWER_SDL3);
#        Missing local Windows dependencies: use the release Docker container automatically.
#        on Windows (MSYS2 shell): with that shell's g++ and SDL3.
case "${1:-all}" in
    all)
        sh "$0" linux || exit "$?"
        exec sh "$0" win
        ;;
    linux|win) ;;
    *) echo "Usage: $0 [all|linux|win]" >&2; exit 2 ;;
esac
cd "$(dirname "$0")/../.." || exit 1
I=port/third_party/imgui
if [ "$1" = win ]; then
    O=port/build/bot_win; OUT=release/bot/ZenkaiPower-bot.exe; LIBS="-lSDL3 -static -static-libgcc -static-libstdc++ -mwindows -lcrypt32"
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) CXX=g++; SDL="" ;;
        *) CXX=x86_64-w64-mingw32-g++
           S=${ZENKAIPOWER_SDL3:-$(ls -d port/build/win/SDL3-*/x86_64-w64-mingw32 2>/dev/null | tail -1)}
           if ! command -v "$CXX" >/dev/null 2>&1 || [ ! -f "$S/include/SDL3/SDL.h" ]; then
               if [ "${ZENKAIPOWER_BOT_CONTAINER:-0}" = 1 ] || ! command -v docker >/dev/null 2>&1; then
                   echo "Windows bot build requires MinGW-w64 and SDL3 for MinGW (ZENKAIPOWER_SDL3), or Docker." >&2
                   exit 1
               fi
               echo "Windows build tools not found locally; using the release container."
               if [ "${ZENKAIPOWER_SKIP_IMAGE_BUILD:-0}" != 1 ]; then
                   docker build -q --platform linux/amd64 -t zenkaipower . >/dev/null || exit "$?"
               else
                   docker image inspect zenkaipower >/dev/null || exit "$?"
               fi
               exec docker run --rm --platform linux/amd64 --entrypoint /bin/sh -u "$(id -u):$(id -g)" -e HOME=/tmp \
                   -e ZENKAIPOWER_BOT_CREDENTIAL -e ZENKAIPOWER_BOT_CONTAINER=1 -e ZENKAIPOWER_JOBS -e ZENKAIPOWER_FULL_REBUILD \
                   -v "$PWD":/src -w /src zenkaipower port/bot/build.sh win
           fi
           SDL="-I$S/include -L$S/lib" ;;
    esac
else
    O=port/build/bot; OUT=release/bot/ZenkaiPower-bot; CXX=g++; SDL=""; LIBS="-lSDL3 -lpthread -ldl -static-libstdc++ -static-libgcc -Wl,-rpath,\$ORIGIN/../linux-x64/lib"
fi
set -e
mkdir -p "$O" release/bot
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
make --no-print-directory -f port/setup/objects.mk APP_SOURCES=bot APP_DIR=port/bot -j "$JOBS" O="$O" I="$I" CXX="$CXX" SDL="$SDL" objects
if [ "$1" = win ] && [ -n "${S:-}" ] && [ -f "$S/bin/SDL3.dll" ]; then
    cp -u "$S/bin/SDL3.dll" release/bot/SDL3.dll
fi
link=false
[ -f "$OUT" ] || link=true
[ "$O/settings" -nt "$OUT" ] && link=true
for o in "$O"/*.o; do
    [ "$o" -nt "$OUT" ] && link=true
done
if [ "$link" = false ]; then
    echo "up to date: $OUT"
    python3 port/bot/write_config.py "$OUT"
    exit 0
fi
# -Wl,-Bdynamic for SDL3 only: the rest (C++ runtime, threads) is linked in.
if [ "$1" = win ]; then
    $CXX -o "$OUT" "$O"/*.o $SDL -Wl,-Bdynamic -lSDL3 -Wl,-Bstatic -lstdc++ -lwinpthread -static-libgcc -mwindows -lcrypt32
else
    $CXX -o "$OUT" "$O"/*.o $LIBS
fi
echo "built $OUT"
python3 port/bot/write_config.py "$OUT"
