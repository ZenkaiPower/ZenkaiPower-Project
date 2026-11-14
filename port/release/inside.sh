#!/bin/sh
# Runs INSIDE the unified container (root Dockerfile), in the repository's top folder: builds both release
# archives from the repository alone. No disc and no game data are involved: the game's data tables are built blank
# (port/data) and the finished program fetches their values from the user's disc at start.
#   -> release/ZenkaiPower-<version>-linux-x64.zip, release/ZenkaiPower-<version>-windows-x64.zip
# port/release/build.sh starts this in the container; the GitHub workflow does the same.
set -e
platform=all
release_build=0
for arg in "$@"; do
    case "$arg" in
        all|linux|win) platform=$arg ;;
        --release) release_build=1 ;;
        *) echo "Usage: $0 [all|linux|win] [--release]" >&2; exit 2 ;;
    esac
done

export ZENKAIPOWER_SKELETON=1
export ZENKAIPOWER_RELEASE=$release_build
# Keep the Linux installer in Docker's build volume rather than linking or
# copying it through the Windows repository bind mount.
export ZENKAIPOWER_SETUP_OUT=port/build/ZenkaiPower-setup
if [ "$ZENKAIPOWER_RELEASE" = 1 ]; then echo "Release build: cheats and external settings compiled out"; fi
logged() {
    log=$1
    shift
    : > "$log"
    "$@" > "$log" 2>&1 &
    build_pid=$!
    tail --pid="$build_pid" -n +1 -f "$log" &
    log_pid=$!
    if wait "$build_pid"; then build_status=0; else build_status=$?; fi
    wait "$log_pid" || true
    return "$build_status"
}
run() { # the build of one variant; a compile failure must stop the release (undefined.py only reports it)
    logged port/build/compile_$ZENKAIPOWER_CC.txt python3 -u port/tools/undefined.py || exit 1
    if grep -q "^FAILED" port/build/compile_$ZENKAIPOWER_CC.txt; then grep "^FAILED" port/build/compile_$ZENKAIPOWER_CC.txt | head -20; exit 1; fi
    echo "== Linking $ZENKAIPOWER_CC"
    logged port/build/link_$ZENKAIPOWER_CC.txt python3 -u port/tools/link.py || exit 1
}
mkdir -p port/build
# the build key (port/tools/buildkey.py reads it from the environment; it is never printed)
if [ -n "$ZENKAIPOWER_BUILD_KEY" ]; then echo "build key: given"; else echo "build key: none (the programs do not sign their matchmaking requests)"; fi
if [ "$platform" != win ]; then
    echo "== Linux"
    export ZENKAIPOWER_CC=clang64
    run
    sh port/setup/build.sh
    # Extraction parity tests use synthetic discs only. Python stays in the build
    # container as a reference/test tool and is not needed by release installers.
    # Reuse the test driver when its source and transitive headers are unchanged.
    make --no-print-directory -f port/release/tests.mk release-logging-test installer-test-driver installer-retry-test native-bot-test bot-preferences-test bot-battle-log-test
    port/build/installer-retry-test
    port/build/native-bot-test
    port/build/bot-preferences-test
    port/build/bot-battle-log-test
    python3 port/tests/installer/native_extract.py
    python3 port/tests/installer/preflight.py
    python3 port/tools/package.py > port/build/package_linux.txt 2>&1 || { cat port/build/package_linux.txt; exit 1; }
    cat port/build/package_linux.txt
    grep -q "^portable: yes" port/build/package_linux.txt
    make --no-print-directory -f port/release/tests.mk installer-font-test hud-name-test
    (cd release/linux-x64 && ../../port/build/installer-font-test && ../../port/build/hud-name-test)
fi
if [ "$platform" = linux ]; then
    ls -la "$(python3 -c 'import sys; sys.path.insert(0,"port/tools"); from package import release_zip; print(release_zip("linux"))')"
    exit 0
fi
echo "== Windows"
export ZENKAIPOWER_CC=win64
run
sh port/setup/build.sh win
python3 port/tools/package.py
ls -la release/*.zip
