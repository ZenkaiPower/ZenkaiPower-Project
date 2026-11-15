# Building releases

## Optional installer artwork

Place optional JPEG/PNG images in `setup/` inside the game folder, for example
`release/windows-x64/setup/` when that is your game folder. On Android, use
`setup/` inside the selected game folder. Images are read at runtime and are not
embedded in the executable or APK. Missing files leave placeholders.
The cover appears without disc verification. Title logos still come from the
verified disc data and can appear without a cover.

Accepted filenames:

- `cover.jpg`
- `SLUS-21678-front.png`, `SLUS-21678-back.png`, `SLUS-21678-disc.png`
- `SLES-54945-front.png`, `SLES-54945-back.png`, `SLES-54945-disc.png`
- `RDSPAF-front.png`, `RDSPAF-back.png`, `RDSPAF-disc.png`
- `SLPS-25815-front.png`, `SLPS-25815-back.png`, `SLPS-25815-disc.png`

Restart setup after adding, changing or removing images; no rebuild is needed.
Use images up to 4096 pixels per side and 16 MiB each.

## Build commands

From the repository root, with Docker running:

```sh
./port/release/build.sh
```

To build only Windows, run `./port/release/build.sh win`; for Linux only, use
`./port/release/build.sh linux`. The default builds both platforms.

Both platform folders and ZIP archives are written under `release/`. Compiler
objects remain in `port/build_release/` between runs; keep that directory for
incremental builds. The first build still compiles the container dependencies
and the game. Subsequent builds reuse unchanged game, installer and test objects.
Installer compilation runs in parallel and tracks included headers, so changing
an extraction header does not rebuild Dear ImGui. Linux tests run whenever the Linux release is built.

Control compilation parallelism (higher values use more RAM):

```sh
ZENKAIPOWER_JOBS=8 ./port/release/build.sh
```

After building the Docker image once, skip Docker's image build check:

```sh
ZENKAIPOWER_SKIP_IMAGE_BUILD=1 ./port/release/build.sh
```

Leave that option unset after changing the root `Dockerfile` or its dependency
versions. ZIP compression defaults to level 6 for faster packaging. Set
`ZENKAIPOWER_ZIP_LEVEL=9` for maximum compression or `ZENKAIPOWER_ZIP_LEVEL=1` for faster packaging
with larger archives. Force recompilation with `ZENKAIPOWER_FULL_REBUILD=1`.

Rebuilding preserves `gamedata/`, `savegame/`, and `textures/` inside each release
folder. These folders are excluded from release ZIPs. Default player files use
`savegame/`.

On Windows installation, setup extracts the US title logo into `zenkaipower.ico`
and embeds it in a `zenkaipower.exe` launcher. `Budokai Tenkaichi 3.lnk` points to that
launcher, with the game folder as its working directory. Installing the Japanese
disc also creates `sm.ico`, the `sm.exe` launcher, and `Sparking! METEOR.lnk`.
Both launchers start `ZenkaiPower.exe` and forward command-line arguments.
These generated files survive rebuilds and are excluded from ZIPs.

ZIP names include the version from `game-info.json`, for example
`ZenkaiPower-v1.0.0-windows-x64.zip` and `ZenkaiPower-v1.0.0-linux-x64.zip`.
Their top-level folders are `ZenkaiPower-Windows/` and `ZenkaiPower-Linux/`,
respectively.
Use a matching GitHub release tag (for example `v1.0.0`) so setup finds its update archive.

Windows setup runs update checks, checksum checks, self-tests, text preparation,
and Dolphin Wii extraction without opening helper console windows. Output remains
available to setup and helper exit codes are checked for failures.

The default build (`./port/release/build.sh all`) produces separate Linux and
Windows folders and ZIP archives. No unified folder is created.

Build both bots with `./port/bot/build.sh` or `./port/bot/build.sh all`.
Use `./port/bot/build.sh linux` or `./port/bot/build.sh win` for one platform.
Outputs are `release/bot/ZenkaiPower-bot`
and `release/bot/ZenkaiPower-bot.exe`; the Windows SDL DLL is copied beside it.

The Windows game and setup generate their executable icons directly from
`assets/zp.png` during the build. Changing the PNG automatically
regenerates the ICO and resource and relinks both executables. Generated ICO/RC
files stay under `port/build/`. This requires Pillow (`python3-pil`, or
`python3 -m pip install Pillow`) and MinGW `windres`; the release container includes both.

When local MinGW-w64 or its SDL3 headers are missing, the bot script automatically
builds Windows in the release Docker container. Run it without `sudo` when your
user has access to Docker and write access to the repository.

Both game and bot builds use the root `Dockerfile` and the `zenkaipower` image.
To build both bots from PowerShell using that image:

```powershell
docker run --rm --platform linux/amd64 --entrypoint /bin/sh -e ZENKAIPOWER_BOT_CREDENTIAL --mount "type=bind,source=${PWD},target=/src" -w /src zenkaipower port/bot/build.sh all
```

Pass `--release` to compile out cheats. Launch with `--settings` to enable the F1
settings window; the Cheats tab is present in builds with cheats enabled. Native
in-game Options remain available. The compile cache is invalidated when switching
modes.

```sh
./port/release/build.sh linux --release
./port/release/build.sh win --release
```

On Windows: `powershell.exe -File port/release/build.ps1 win --release`
(`-Release` is also accepted). VS Code includes matching build tasks labelled
“release”; the existing build-and-run tasks retain development access.

The PowerShell wrapper keeps compiler outputs and release staging in separate
Docker Linux volumes. Desktop ZIPs and the versioned Android APK/checksum are
exported to `release/`; unpacked desktop files for VS Code go to `build/`.
Extra or older release outputs are preserved under `build/release-history/`.
Exports use
`docker cp`. This avoids Docker Desktop bind-mount input/output errors during
linking, metadata flushes, and ZIP creation. Local player folders are not
deleted during export. If exporting fails, the built files remain in the
checkout's `zenkaipower-release-<hash>-artifacts` Docker volume for recovery.

## Wii disc reader

Windows and Linux installers statically link the same pinned nod reader used
on Android. `port/setup/build.sh` prepares it through `port/tools/build_nod.sh`,
using Rust 1.94.0 and locked dependencies. The first build downloads Rust and
nod; subsequent builds reuse `port/build/nod`. `ZENKAIPOWER_NOD_ROOT` overrides
that cache, and `CARGO_HOME` / `RUSTUP_HOME` can reuse an existing Rust cache.
The unified Docker image includes the required C/C++ cross-compilers.

Game releases include nod attribution under `licenses/nod/`. DolphinTool and
its complete matching source archive are retained for development under
`port/third_party/dolphin-tool/`.
