#!/bin/sh
set -eu
# /host is the Windows checkout. Keep compiler inputs on a Linux filesystem:
# LLVM/NDK symlinks and case-sensitive headers cannot be unpacked onto NTFS.
cd /work
# The unified image sets ZENKAIPOWER_SDL3 to the Windows MinGW package.
# Android must use the arm64 library installed by android_toolchain.sh.
export ZENKAIPOWER_SDL3="${ZENKAIPOWER_ANDROID_SDL3:-${ZENKAIPOWER_TOOLCHAIN:-/work/port/build/android-toolchain}/sdl3}"
# Refresh packaged source resources instead of overlaying deleted/renamed files
# from a previous checkout. Only staged copies in /work are removed; build
# caches and generated APK assets remain available for full and ApkOnly builds.
rm -rf -- port/translations port/third_party/fonts
tar -C /host --exclude=port/build --exclude=port/build_release \
    --exclude=port/third_party/dolphin-tool --exclude=port/third_party/real-esrgan \
    --exclude=android/.gradle --exclude=android/app/build \
    --exclude=android/app/src/main/jniLibs --exclude=android/app/src/main/assets \
    --exclude=android/app/src/main/java/org --exclude=android/adapt.py \
    --exclude=android/app/src/androidTest/assets --exclude=android/app/src/androidTest/jniLibs \
    -cf - src include config assets port android LICENSE game-info.json dragon-net-battle.txt \
    | tar -xf -
if [ "${ZENKAIPOWER_ANDROID_ENGINE:-1}" = 1 ]; then
    sh port/tools/android_toolchain.sh
    python3 port/tools/android.py "$@"
else
    mkdir -p port/build/android
    if [ -f /host/port/build/android/libzenkaipower.so ]; then
        cp /host/port/build/android/libzenkaipower.so* port/build/android/
        cp /host/port/build/android/unused-sdk.txt port/build/android/
    fi
    [ -f port/build/android/libzenkaipower.so ] || { echo "Build the Android engine before using ApkOnly" >&2; exit 1; }
    for dir in jniLibs assets java/org; do
        mkdir -p "android/app/src/main/$dir"
        cp -a "/host/android/app/src/main/$dir/." "android/app/src/main/$dir/"
    done
fi
for dir in jniLibs assets java/org; do
    mkdir -p "/host/android/app/src/main/$dir"
    cp -a "android/app/src/main/$dir/." "/host/android/app/src/main/$dir/"
done
case " $* " in
    *" installer-tests "*)
        mkdir -p /host/android/app/src/androidTest/jniLibs/arm64-v8a
        cp android/app/src/androidTest/jniLibs/arm64-v8a/libreader_checks.so /host/android/app/src/androidTest/jniLibs/arm64-v8a/
        ;;
esac
case " $* " in
    *" storage-tests "*)
        mkdir -p /host/android/app/src/androidTest/jniLibs/arm64-v8a
        cp android/app/src/androidTest/jniLibs/arm64-v8a/libstorage_checks.so /host/android/app/src/androidTest/jniLibs/arm64-v8a/
        ;;
esac
mkdir -p /host/port/build/android
cp port/build/android/libzenkaipower.so* /host/port/build/android/
cp port/build/android/unused-sdk.txt /host/port/build/android/

if [ "${ZENKAIPOWER_ANDROID_APK:-0}" = 1 ]; then
    sh android/seed-gradle-cache.sh
    python3 port/tools/android_resources.py
    # Host SDK paths (especially Windows paths) do not apply inside the container.
    printf 'sdk.dir=%s\n' "$ANDROID_HOME" > android/local.properties
    variant=debug; task=assembleDebug
    if [ "${ZENKAIPOWER_RELEASE:-0}" = 1 ]; then variant=release; task=assembleRelease; fi
    # Git on Windows may check out the Gradle launcher with CRLF.
    python3 -c 'from pathlib import Path; p=Path("android/gradlew"); p.write_bytes(p.read_bytes().replace(b"\r\n", b"\n"))'
    sh android/gradlew -p android "$task" --console=plain
    mkdir -p release/android-arm64
    if [ "$variant" = release ]; then apk=ZenkaiPower-android.apk; else apk=ZenkaiPower-debug.apk; fi
    cp "android/app/build/outputs/apk/$variant/app-$variant.apk" "release/android-arm64/$apk"
    if [ "$variant" = release ]; then python3 port/tools/package_android.py; fi
    # PowerShell exports finished packages through docker cp, avoiding Windows
    # bind-mount I/O failures. Artifacts remain in the persistent /work volume.
    if [ "${ZENKAIPOWER_ANDROID_DOCKER_EXPORT:-0}" = 1 ]; then exit 0; fi
    mkdir -p /host/release/android-arm64
    cp "release/android-arm64/$apk" /host/release/android-arm64/
    if [ "$variant" = release ]; then
        version=$(python3 -c 'import json; print(json.load(open("game-info.json"))["version"].lstrip("vV"))')
        stem="ZenkaiPower-v$version-android-arm64"
        cp "release/android-arm64/$stem.apk" "release/android-arm64/$stem.apk.sha256" /host/release/android-arm64/
        cp "release/$stem.zip" /host/release/
    fi
fi
