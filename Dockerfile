# syntax=docker/dockerfile:1
# Build with --platform linux/amd64: the original compiler is an x86 binary.
FROM node:24.22.1-bookworm-slim AS node-tools

FROM ubuntu:26.04 AS decomp
# Node/npm are tooling only; credentials and sources enter through the runtime mount.
COPY --from=node-tools /usr/local/bin/node /usr/local/bin/node
COPY --from=node-tools /usr/local/lib/node_modules /usr/local/lib/node_modules
RUN ln -s ../lib/node_modules/npm/bin/npm-cli.js /usr/local/bin/npm \
    && ln -s ../lib/node_modules/npm/bin/npx-cli.js /usr/local/bin/npx \
    && node --version && npm --version

ENV LANG=C.UTF-8 TZ=UTC PYTHONUNBUFFERED=1 PIP_DISABLE_PIP_VERSION_CHECK=1
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates openssl curl xz-utils libc6-i386 lib32stdc++6 \
        python3 python3-pip python3-venv python3-nacl ninja-build p7zip-full \
        jekyll ruby-kramdown-parser-gfm \
    && rm -rf /var/lib/apt/lists/*
# Install into the disposable image's system Python so install.py's venv
# (--system-site-packages) also sees splat without downloading it at runtime.
RUN python3 -m pip install --break-system-packages --no-cache-dir 'splat64[mips]==0.50.0'

# Versions used by configure.py; verify the downloaded toolchain archives.
RUN mkdir -p /opt/zenkaipower-tools/ee-gcc2.96 /opt/zenkaipower-tools/binutils /opt/zenkaipower-tools/bin \
    && curl -fsSL --retry 3 -o /tmp/ee.tar.xz https://github.com/decompme/compilers/releases/download/compilers/ee-gcc2.96.tar.xz \
    && echo '0590d2ca9da8f5903889d66761220d14b47a8d14ba987ca53db84a1650a1fd0a  /tmp/ee.tar.xz' | sha256sum -c - \
    && tar -xJf /tmp/ee.tar.xz -C /opt/zenkaipower-tools/ee-gcc2.96 \
    && curl -fsSL --retry 3 -o /tmp/binutils.tar.gz https://github.com/decompals/binutils-mips-ps2-decompals/releases/download/v0.6/binutils-mips-ps2-decompals-linux-x86-64.tar.gz \
    && echo 'b9d929fa7509404ec417fdd33e618e93fca069ea2796c6cc52c6b1b723beb38c  /tmp/binutils.tar.gz' | sha256sum -c - \
    && tar -xzf /tmp/binutils.tar.gz -C /opt/zenkaipower-tools/binutils \
    && for tool in /opt/zenkaipower-tools/binutils/mips-ps2-decompals-*; do \
         ln -s "$tool" "/opt/zenkaipower-tools/bin/mips-linux-gnu-${tool##*decompals-}"; \
       done \
    && rm /tmp/ee.tar.xz /tmp/binutils.tar.gz \
    && /opt/zenkaipower-tools/ee-gcc2.96/bin/ee-gcc --version \
    && /opt/zenkaipower-tools/binutils/mips-ps2-decompals-as --version

ENV PATH=/opt/zenkaipower-tools/bin:$PATH HOME=/tmp
WORKDIR /work
# The checkout is bind-mounted at runtime. Expose the paths expected by the
# existing scripts without overwriting a user's already installed tools.
COPY --chmod=755 <<'EOF' /usr/local/bin/zenkaipower-entrypoint
#!/bin/sh
set -eu
if [ ! -f configure.py ]; then
    echo 'Mount the repository at /work (see docker.md).' >&2
    exit 1
fi
mkdir -p tools
for name in ee-gcc2.96 binutils bin; do
    if [ -L "tools/$name" ] && [ ! -e "tools/$name" ]; then
        ln -sfn "/opt/zenkaipower-tools/$name" "tools/$name"
    elif [ ! -e "tools/$name" ] && [ ! -L "tools/$name" ]; then
        ln -s "/opt/zenkaipower-tools/$name" "tools/$name"
    fi
done
exec "$@"
EOF
ENTRYPOINT ["zenkaipower-entrypoint"]
CMD ["bash"]

# The default image also builds the native 64-bit Linux port. Keep its LLVM
# and SDL versions aligned with the unified release stage below.
FROM decomp AS pc
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gnupg build-essential binutils cmake pkg-config git glslc ffmpeg python3-pil python3-fonttools \
        libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev libx11-dev libxext-dev libxrandr-dev \
        libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libpulse-dev libpipewire-0.3-dev libasound2-dev \
        libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libvulkan-dev libudev-dev libdbus-1-dev \
        libibus-1.0-dev libusb-1.0-0-dev \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL --retry 3 https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo 'deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/resolute/ llvm-toolchain-resolute-22 main' > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang-22 llvm-22 libclang-22-dev python3-clang-22 \
    && rm -rf /var/lib/apt/lists/* \
    && ln -s /usr/bin/clang-22 /usr/local/bin/clang \
    && ln -s /usr/bin/llc-22 /usr/local/bin/llc
ARG SDL_VERSION=3.4.12
RUN curl -fsSL --retry 3 -o /tmp/sdl.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build -G Ninja \
         -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
         -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build --parallel 4 \
    && cmake --install /tmp/sdl-build && ldconfig \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build



# Unified tools image; retain Ubuntu 22.04 for distributable Linux binaries.
FROM ubuntu:22.04 AS all
ENV DEBIAN_FRONTEND=noninteractive TZ=UTC LANG=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates openssl curl gnupg build-essential binutils python3 cmake ninja-build pkg-config git p7zip-full \
        jekyll ruby-kramdown-parser-gfm \
        libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev libx11-dev libxext-dev libxrandr-dev \
        libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libpulse-dev libpipewire-0.3-dev libasound2-dev \
        libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libvulkan-dev libudev-dev libdbus-1-dev \
        libibus-1.0-dev libusb-1.0-0-dev \
    && rm -rf /var/lib/apt/lists/*
# clang / LLVM 22 (the version ptr32.py and irfix.py are written against), with its Python bindings
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/jammy/ llvm-toolchain-jammy-22 main" > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends clang-22 llvm-22 libclang-22-dev python3-clang-22 \
    && rm -rf /var/lib/apt/lists/* \
    && ln -s /usr/bin/clang-22 /usr/local/bin/clang && ln -s /usr/bin/llc-22 /usr/local/bin/llc
# SDL3 from source, for any x86-64 CPU; this is also the copy the release carries in lib/
ARG SDL_VERSION=3.4.12
RUN curl -fsSL -o /tmp/sdl.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build && cmake --install /tmp/sdl-build && ldconfig \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build
# The shader compiler (glslangValidator), and for the Windows release: the mingw-w64 cross-compiler (the variant with
# POSIX threads) and SDL3's development package for it
RUN apt-get update && apt-get install -y --no-install-recommends glslang-tools mingw-w64 zip unzip \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
    && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix \
    && curl -fsSL -o /tmp/sdl-mingw.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-devel-${SDL_VERSION}-mingw.tar.gz \
    && mkdir -p /opt/sdl3-mingw && tar -xzf /tmp/sdl-mingw.tar.gz -C /opt/sdl3-mingw && rm /tmp/sdl-mingw.tar.gz
ENV ZENKAIPOWER_SDL3=/opt/sdl3-mingw/SDL3-${SDL_VERSION}/x86_64-w64-mingw32
WORKDIR /src

# Generate Windows ICO resources from the source PNG without rebuilding the toolchain.
RUN apt-get update && apt-get install -y --no-install-recommends python3-pil \
    && rm -rf /var/lib/apt/lists/*

COPY --from=node-tools /usr/local/bin/node /usr/local/bin/node
COPY --from=node-tools /usr/local/lib/node_modules /usr/local/lib/node_modules
COPY --from=decomp /opt/zenkaipower-tools /opt/zenkaipower-tools
COPY --from=decomp /lib/x86_64-linux-gnu /opt/ps2-runtime/lib
COPY --from=decomp /usr/local/bin/zenkaipower-entrypoint /usr/local/bin/zenkaipower-entrypoint
RUN ln -s ../lib/node_modules/npm/bin/npm-cli.js /usr/local/bin/npm \
    && ln -s ../lib/node_modules/npm/bin/npx-cli.js /usr/local/bin/npx \
    && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       python3-pip python3-venv python3-nacl python3-fonttools libc6-i386 lib32stdc++6 ffmpeg \
    && rm -rf /var/lib/apt/lists/* \
    && python3 -m pip install --no-cache-dir 'splat64[mips]==0.50.0'
# New PS2 binutils require a newer libc. Run only those tools with their bundled
# loader; native builds still link against the release image's glibc 2.35.
RUN for tool in /opt/zenkaipower-tools/binutils/mips-ps2-decompals-*; do \
        [ -f "$tool" ] || continue; mv "$tool" "$tool.real"; \
        printf '#!/bin/sh\nexec /opt/ps2-runtime/lib/ld-linux-x86-64.so.2 --library-path /opt/ps2-runtime/lib "%s.real" "$@"\n' "$tool" > "$tool"; chmod +x "$tool"; \
    done \
    && /opt/zenkaipower-tools/binutils/mips-ps2-decompals-as --version
ENV PATH=/opt/zenkaipower-tools/bin:$PATH HOME=/tmp LANG=C.UTF-8 TZ=UTC PYTHONUNBUFFERED=1
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        mingw-w64 glslang-tools zip unzip openjdk-17-jdk-headless gdb sqlite3 \
        libxml2 libzstd1 libtinfo6 \
    && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
    && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix \
    && rm -rf /var/lib/apt/lists/*
ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 ANDROID_HOME=/opt/android-sdk ANDROID_SDK_ROOT=/opt/android-sdk
ARG ANDROID_COMMAND_TOOLS=13114758
ARG ANDROID_COMMAND_TOOLS_SHA256=7ec965280a073311c339e571cd5de778b9975026cfcbe79f2b1cdcb1e15317ee
RUN curl -fsSL --retry 3 -o /tmp/android-tools.zip https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_COMMAND_TOOLS}_latest.zip \
    && echo "$ANDROID_COMMAND_TOOLS_SHA256  /tmp/android-tools.zip" | sha256sum -c - \
    && mkdir -p "$ANDROID_HOME/cmdline-tools" \
    && unzip -q /tmp/android-tools.zip -d "$ANDROID_HOME/cmdline-tools" \
    && mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest" \
    && rm /tmp/android-tools.zip
ENV PATH=/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:$PATH
# Building this image accepts the Android SDK licenses for its build environment.
RUN yes | sdkmanager --licenses >/dev/null \
    && sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0" \
    && chmod -R a+rX "$ANDROID_HOME"
ENV ZENKAIPOWER_TOOLCHAIN=/opt/zenkaipower-android-toolchain
ARG ANDROID_LLVM_VERSION=21.1.0
COPY port/tools/android_toolchain.sh /opt/zenkaipower-image-tools/android_toolchain.sh
RUN sed -i 's/\r$//' /opt/zenkaipower-image-tools/android_toolchain.sh \
    && ZENKAIPOWER_LLVM_VERSION="$ANDROID_LLVM_VERSION" sh /opt/zenkaipower-image-tools/android_toolchain.sh
COPY port/tools/android_nod.sh port/tools/android_nod_patch.py /opt/zenkaipower-image-tools/
RUN sed -i 's/\r$//' /opt/zenkaipower-image-tools/android_nod.sh \
    && CARGO_BUILD_JOBS=1 sh /opt/zenkaipower-image-tools/android_nod.sh

# Preload the wrapper, AGP and its build-time dependencies. Copy only the
# configuration/resources needed for resolution; no game data or signing keys.
ENV ZENKAIPOWER_GRADLE_SEED=/opt/zenkaipower-gradle
COPY android/gradlew android/build.gradle android/settings.gradle android/gradle.properties /tmp/zenkaipower-image-project/android/
COPY android/gradle /tmp/zenkaipower-image-project/android/gradle
COPY android/app/build.gradle /tmp/zenkaipower-image-project/android/app/build.gradle
COPY android/app/src/main/AndroidManifest.xml /tmp/zenkaipower-image-project/android/app/src/main/AndroidManifest.xml
COPY android/app/src/main/res /tmp/zenkaipower-image-project/android/app/src/main/res
COPY game-info.json /tmp/zenkaipower-image-project/game-info.json
COPY android/cache-dependencies.gradle /tmp/zenkaipower-image-project/android/cache-dependencies.gradle
RUN python3 -c 'from pathlib import Path; p=Path("/tmp/zenkaipower-image-project/android/gradlew"); p.write_bytes(p.read_bytes().replace(b"\r\n", b"\n"))' \
    && GRADLE_USER_HOME="$ZENKAIPOWER_GRADLE_SEED" sh /tmp/zenkaipower-image-project/android/gradlew \
        -p /tmp/zenkaipower-image-project/android --no-daemon --console=plain --max-workers=1 \
        '-Dorg.gradle.jvmargs=-Xmx1024m -Dfile.encoding=UTF-8' \
        -I /tmp/zenkaipower-image-project/android/cache-dependencies.gradle \
        cacheAndroidDependencies :app:mergeDebugResources :app:mergeReleaseResources \
        -x :app:checkEngine -x :app:generateProjectIcon -x :app:checkReleaseSigning \
    && cd /tmp/zenkaipower-image-project/android \
    && sha256sum build.gradle settings.gradle gradle.properties app/build.gradle \
        gradle/wrapper/gradle-wrapper.properties gradle/wrapper/gradle-wrapper.jar cache-dependencies.gradle \
        | sha256sum | cut -d ' ' -f 1 > "$ZENKAIPOWER_GRADLE_SEED/.zenkaipower-image-seed" \
    && find "$ZENKAIPOWER_GRADLE_SEED" -type f \( -name '*.lock' -o -name '*.lck' \) -delete \
    && rm -rf /tmp/zenkaipower-image-project "$ZENKAIPOWER_GRADLE_SEED/daemon"
# Consult bundled dependency metadata alongside any existing writable cache.
ENV GRADLE_RO_DEP_CACHE=/opt/zenkaipower-gradle/caches
WORKDIR /work

ENTRYPOINT ["zenkaipower-entrypoint"]
CMD ["bash"]
