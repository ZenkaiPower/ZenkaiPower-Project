#!/bin/sh
# Runs port/build/zenkaipower under gdb for N seconds (default 3), interrupts it and prints where it is and how many
# vertical blanks have passed. Also prints the backtrace if it crashes first. Usage: port/tools/where.sh [seconds] [lines]
cd "$(dirname "$0")/../.."
( sleep "${1:-3}"; pkill -INT -x zenkaipower ) &
gdb -q -batch -ex "set debuginfod enabled off" -ex "set confirm off" -ex run -ex bt -ex "print gPortVBlanks" -ex kill port/build/zenkaipower 2>&1 |
    grep -E "^#|^\\$|signal|not implemented|exited|zenkaipower:" | cut -c1-200 | head -${2:-14}
wait
