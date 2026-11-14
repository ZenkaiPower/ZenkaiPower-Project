#!/bin/sh
# Builds both release archives (Linux and Windows) using the root Dockerfile:
#     port/release/build.sh   ->   release/ZenkaiPower-<version>-linux-x64.zip, ZenkaiPower-<version>-windows-x64.zip
# Needs only docker and this repository: no disc, no game data, no build on the host.
# ZENKAIPOWER_BUILD_KEY in the environment (an official release's key: port/tools/buildkey.py, server/README.md) is
# handed on to the build by name, so its value is on no command line; without it the programs have no key.
# Optional: ZENKAIPOWER_JOBS=8 ./port/release/build.sh controls compiler parallelism.
# Optional: ZENKAIPOWER_SKIP_IMAGE_BUILD=1 reuses the existing container image.
# Pass linux or win to build only that platform: port/release/build.sh win
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

cd "$(dirname "$0")/../.."
if [ "${ZENKAIPOWER_SKIP_IMAGE_BUILD:-0}" != 1 ]; then
    docker build -q -t zenkaipower . >/dev/null
else
    docker image inspect zenkaipower >/dev/null
fi
set -- "$platform"
if [ "$release_build" = 1 ]; then set -- "$@" --release; fi
mkdir -p port/build_release
# the repository at /src with the release's own build folder in the place of port/build; files owned by you
# Native releases do not need the decomp entrypoint's repository symlinks.
docker run --rm --entrypoint /bin/sh -u "$(id -u):$(id -g)" -e HOME=/tmp -e ZENKAIPOWER_JOBS -e ZENKAIPOWER_FULL_REBUILD -e ZENKAIPOWER_ZIP_LEVEL -e ZENKAIPOWER_BUILD_KEY -e ZENKAIPOWER_RELEASE_TAG -e ZENKAIPOWER_DOLPHIN_SOURCE_URL -e GITHUB_REPOSITORY -v "$PWD":/src -v "$PWD/port/build_release":/src/port/build -w /src \
    zenkaipower port/release/inside.sh "$@"
echo "binary releases: release/ (publish matching Dolphin source separately)"
