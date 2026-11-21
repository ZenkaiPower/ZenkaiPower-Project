#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
release=0
case "${1:-}" in --release) release=1 ;; '') ;; *) echo "Usage: $0 [--release]" >&2; exit 2 ;; esac
if [ "${ZENKAIPOWER_SKIP_IMAGE_BUILD:-0}" != 1 ]; then docker build --platform linux/amd64 -t zenkaipower .; fi
# Optional signing files are mounted read-only; no keys enter the image.
set --
ZENKAI_SIGNING_PROPERTIES=${ZENKAI_SIGNING_PROPERTIES:-$HOME/.android/zenkaipower-release.properties}
if [ -f "$ZENKAI_SIGNING_PROPERTIES" ]; then
    signing_dir=$(cd "$(dirname "$ZENKAI_SIGNING_PROPERTIES")" && pwd)
    signing_file="$signing_dir/$(basename "$ZENKAI_SIGNING_PROPERTIES")"
    key_file=$(sed -n 's/^storeFile=//p' "$signing_file" | tr -d '\r')
    case "$key_file" in /*) ;; *) key_file="$signing_dir/$key_file" ;; esac
    [ -f "$key_file" ] || { echo "Signing keystore not found" >&2; exit 1; }
    set -- --mount "type=bind,source=$signing_file,target=/signing/release.properties,readonly" \
        --mount "type=bind,source=$key_file,target=/signing/release.jks,readonly" \
        -e ZENKAI_SIGNING_PROPERTIES=/signing/release.properties -e ZENKAI_KEYSTORE=/signing/release.jks
fi
docker run --rm --platform linux/amd64 --entrypoint /bin/sh \
    --mount "type=bind,source=$PWD,target=/host" -w /work \
    --mount type=volume,source=zenkaipower-android-work,target=/work \
    --mount type=volume,source=zenkaipower-gradle,target=/root/.gradle \
    --mount type=volume,source=zenkaipower-android-user,target=/root/.android \
    -e HOME=/root -e ZENKAIPOWER_JOBS -e ZENKAIPOWER_ONLY -e ZENKAIPOWER_BUILD_KEY \
    -e ZENKAIPOWER_SKELETON=1 -e "ZENKAIPOWER_RELEASE=$release" -e ZENKAIPOWER_ANDROID_APK=1 \
    "$@" zenkaipower /host/android/container-build.sh
