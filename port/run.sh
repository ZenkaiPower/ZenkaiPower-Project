#!/bin/sh
# Runs the PC build in a window with the GPU renderer, playing a replay (default: the validation replay), with
# "demo" the game's own attract battle, or with "menu" the game from its menus. Closing the window quits; Escape opens settings and does not exit the game.
#   port/run.sh [replay-file | demo | menu]        ZENKAIPOWER_WIDE=1 in front: 16:9 widescreen
cd "$(dirname "$0")/.."
# Prefer X11/XWayland on Linux to avoid libdecor GTK initialization failures.
# An explicit SDL driver selection still wins.
if [ "$(uname -s)" = Linux ] && [ -z "${SDL_VIDEO_DRIVER:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
    export SDL_VIDEO_DRIVER=x11
fi
case "${1:-gamedata/validation/replay01.bin}" in
    demo) unset ZENKAIPOWER_REPLAY; export ZENKAIPOWER_DEMO=1 ;;
    menu) unset ZENKAIPOWER_REPLAY ZENKAIPOWER_DEMO ;;
    *) export ZENKAIPOWER_REPLAY="${1:-gamedata/validation/replay01.bin}" ;;
esac
# ZENKAIPOWER_64=1 in front: the 64-bit build (port/build/zenkaipower_64, made with ZENKAIPOWER_CC=clang64 port/tools/undefined.py and link.py)
if [ "$#" -gt 0 ]; then shift; fi
ZENKAIPOWER_GS=gpu exec port/build/zenkaipower${ZENKAIPOWER_64:+_64} "$@"
