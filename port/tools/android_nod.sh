#!/bin/sh
# Pinned MIT/Apache-2.0 disc reader; built into the installer, never executed as a helper.
set -eu
TC=${ZENKAIPOWER_TOOLCHAIN:-/opt/android-toolchain}
export CARGO_HOME="$TC/cargo" RUSTUP_HOME="$TC/rustup"
export PATH="$CARGO_HOME/bin:$PATH"
SRC="$TC/nod"
REV=ebd80cac99b48a323200d84365e07a58cc27412d
if [ ! -d "$SRC/.git" ]; then
    git init "$SRC"
    git -C "$SRC" remote add origin https://github.com/encounter/nod.git
fi
if [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null || true)" != "$REV" ]; then
    git -C "$SRC" fetch --depth 1 origin "$REV"
    git -C "$SRC" checkout --detach "$REV"
fi
python3 "$(dirname "$0")/android_nod_patch.py" "$SRC"
if [ ! -x "$CARGO_HOME/bin/rustup" ]; then
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o "$TC/rustup-init.sh"
    sh "$TC/rustup-init.sh" -y --profile minimal --default-toolchain 1.94.0
fi
rustup target add --toolchain 1.94.0 aarch64-linux-android
cat > "$TC/android-cc" <<EOF
#!/bin/sh
# Rust explicitly links -lunwind. Upstream LLVM does not search the separately
# packaged NDK runtime directory unless it is supplied here.
exec "$TC/llvm/bin/clang" --target=aarch64-linux-android29 --sysroot="$TC/ndk/sysroot" -L"$TC/ndk/clang_rt" "\$@"
EOF
chmod +x "$TC/android-cc"
export CC_aarch64_linux_android="$TC/android-cc"
export AR_aarch64_linux_android="$TC/llvm/bin/llvm-ar"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$TC/android-cc"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_RUSTFLAGS='-C link-arg=-Wl,-z,max-page-size=16384'
cd "$SRC"
cargo +1.94.0 build --locked --release --target aarch64-linux-android -p nod-ffi --no-default-features \
    --features compress-bzip2-static,compress-lzma-static,compress-zlib-static,compress-zstd-static
