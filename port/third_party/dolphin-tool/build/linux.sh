#!/bin/sh
set -eu
cmake -S /source -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" $(cat /recipes/options.txt)
cmake --build /build --target dolphin-tool --parallel "${BUILD_JOBS:-1}"
cp /build/Binaries/dolphin-tool /output/dolphin-tool
strip --strip-unneeded /output/dolphin-tool
ldd /output/dolphin-tool
/output/dolphin-tool extract --help
