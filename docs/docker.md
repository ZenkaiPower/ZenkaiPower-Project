# Decompile and build with Docker

The root `Dockerfile` provides the original PS2 compiler, PS2 binutils, splat,
tools for Wii RVZ extraction, and the native Linux port's build
dependencies. Only Docker and a shell are needed on the host; Python,
compilers, extraction, and builds run in containers. Use your own unmodified
**USA PS2 disc image (SLUS-21678)** for PS2 rebuilds.

These commands use a POSIX shell on Linux, macOS, or WSL2 with Docker running.
Run them from the repository root. The toolchain requires `linux/amd64`;
other CPU architectures need Docker's x86 emulation and may be much slower.
Allow several GB for the image and build outputs, plus about 4 GB for extracted
game data and space for your ISO. Building the image requires internet access.

The default `zenkaipower` image also includes Windows cross-compilation, Java,
Android SDK/build tools, ADB and GDB. Desktop release and Android build wrappers
use this same image, including Gradle/APK packaging. See the
[root README](../README.md) for the short all-platform build commands.
The smaller `decomp` and `pc` targets remain available.

## 1. Build the tools image

```sh
docker build --platform linux/amd64 -t zenkaipower .
```

The default image supports both the matching PS2 build and the 64-bit Linux
port. If you only want the PS2 decompilation/rebuild, use the smaller target:

```sh
docker build --platform linux/amd64 --target decomp -t zenkaipower .
```

With this smaller image, skip step 3 and use the optional PS2 rebuild section.

Define this helper in your current shell to run commands in the image:

```sh
zenkaipower() {
  docker run --rm --platform linux/amd64 \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,source=$(pwd),target=/work" \
    zenkaipower "$@"
}
```

The bind mount keeps all outputs in the checkout after the container exits.
Using your UID/GID keeps generated files owned by you on Linux. The entrypoint
creates missing links under ignored `tools/` to the container's toolchain;
these links work inside the container. Existing tools are preserved, so use a
fresh checkout if you need to isolate this workflow from a host toolchain.

Generate a binary starter save with the tools image. From the
repository root, run:

```sh
zenkaipower python3 port/tools/generate_zenkai_save.py
```

This writes `savegame/zenkaipower.sav` in the checkout. Add `--force` to replace
an existing save, or pass `--output <path>` to choose another destination.

Developer extraction utilities use the development-only DolphinTool in `port/third_party/dolphin-tool/`. To extract the Wii online-mode sprite
resources from an RVZ:

```sh
zenkaipower python3 port/tools/extract_wii_online.py \
  "Dragon Ball Z - Budokai Tenkaichi 3 (USA).rvz"
```

This writes the extracted native Wii resources and recognized texture PNGs to
`wii-online-sprites/` in the checkout. The Dragon Net Battle window uses the
Pan sprite at `section-00/resource-002-textures/texture-000.png` when available.
Set `ZENKAIPOWER_WII_ONLINE_SPRITE` to another extracted PNG path to override it. The
unified image uses Ubuntu 22.04 for release compatibility.

The Dockerfile-specific ignore file excludes the checkout and all game files
from the build context. The image contains tools, not game assets or sources.
The original compiler and binutils downloads are checksum-verified. Sources:
[decomp.me compilers](https://github.com/decompme/compilers/releases/tag/compilers)
and [PS2 binutils v0.6](https://github.com/decompals/binutils-mips-ps2-decompals/releases/tag/v0.6).

## 2. Extract and split the original game

Place the ISO at `roms/SLUS-21678.iso`, or substitute its actual path below.
Paths passed to `zenkaipower` are container paths: files inside this checkout are
available relative to `/work`.

```sh
zenkaipower 7z x -y -odisc roms/SLUS-21678.iso SLUS_216.78 BIN
zenkaipower python3 configure.py
```

Extracting explicitly lets you use any ISO filename instead of the hardcoded
filename in `configure.py`. Configuration flattens the executable, verifies
the input hashes through splat, generates disassembly/data under `asm/`, and
writes linker scripts and `build.ninja`.

This prepares the decompilation workspace. It does not automatically translate
new assembly into C: the repository already contains decompiled C in `src/`.
See [the decompilation guide](decomp_guide.md) for the matching workflow.

## 3. Build the native Linux PC port

Use the default image from step 1, not the `decomp`-only target. The installer
can run directly on a fresh checkout, or after step 2:

```sh
zenkaipower python3 install.py --check
zenkaipower python3 install.py --iso roms/SLUS-21678.iso --rebuild
```

This checks the disc, extracts `gamedata/`, splits the executables if needed,
builds `port/build/zenkaipower_64`, and runs the demo fight as a headless self-test inside
Docker. No display or GPU forwarding is needed for that test. Details go to
`install.log`; generated files remain in the checkout. `--rebuild` ensures
source edits are compiled even if the executable already exists. The image
preinstalls splat, so the installer's new `.venv` can inherit it without another
download.

For later builds, repeat the same command. Use `--force` instead of `--rebuild`
when you need the installer to regenerate extracted/split data as well.
If a `.venv` created outside this image already exists, move it aside first
(for example, `zenkaipower mv .venv .venv.host-backup`, using an unused backup name).
Virtual environments are not portable between host and container environments.

The output is a Linux x86-64 executable built on Ubuntu 22.04. For binaries
targeting older Linux systems, use the release builder below.
For binaries targeting older Linux systems, use the release builder below.
Playing with graphics, sound, and input
requires a compatible runtime and display setup; these commands cover building
and the headless test. The generated `play.sh` is a host launcher, not a Docker
launcher. The PS2 target and native PC build are separate outputs; the PC build
does not require a successful PS2 byte-matching rebuild.

For the project's packaged Linux **and Windows** releases, the existing release
builder is also Docker-based and does not require an ISO:

```sh
sh port/release/build.sh
```

## Optional: rebuild and verify the PS2 binaries

```sh
zenkaipower ninja -j4
```

The default Ninja targets compare both rebuilt binaries byte for byte with the
originals. A successful build creates:

| Output | Meaning |
| --- | --- |
| `build/SLUS_216.78.elf` | Linked main executable |
| `build/SLUS_216.78.rom` | Flat main executable image |
| `build/DBZP.elf` | Linked menu overlay |
| `build/DBZP.BIN` | Rebuilt menu overlay binary |
| `build/SLUS_216.78.ok`, `build/DBZP.ok` | Successful byte comparisons |

This does not create a bootable ISO. A failed `CHECK` means the rebuilt bytes
do not match; do not treat the presence of an ELF alone as a successful match.

**Current source limitation:** testing this checkout reaches the PS2 link but
fails with undefined references to `HUD_SCR`. Several HUD sources define that
macro only under `#ifdef PORT` while also using it in the PS2 build. This needs
a source fix before the full matching rebuild can succeed; installing more
Docker dependencies will not resolve it. Extraction, splitting, and individual
module comparison still work. The native PC build defines `PORT`.

After editing C or headers, run `zenkaipower ninja -j4` again. After changing segment
configuration or symbols, rerun `zenkaipower python3 configure.py` first. To compare an
individual module's functions:

```sh
zenkaipower python3 scripts/fdiff.py src/sys/heap.c
```

## Troubleshooting

- **Missing disc or checksum mismatch:** use an unmodified USA PS2 ISO and the
  extraction command above. Other regions, Wii images, and patched ISOs do not
  match these configurations.
- **Permission denied on `/work`:** the mounted checkout must be writable by
  your UID/GID. On SELinux hosts, replace the `--mount` option in the helper
  with `-v "$(pwd):/work:Z"` to label the mount for container access.
- **Compiler cannot execute:** use `--platform linux/amd64` for both image build
  and container runs. The old PS2 compiler also needs 32-bit x86 support; the
  image includes its runtime libraries.
- **Build killed or out of memory:** increase Docker's memory allowance. Lower
  `ninja -j4` for PS2 builds; the native build script uses 16 worker processes,
  so changing Ninja's job count does not limit the native build.
- **Debug interactively:** run the helper with `zenkaipower bash -c 'command'`, or add
  `-it` to `docker run` in the helper and use `zenkaipower bash` for an interactive shell.

## Deploy the Cloudflare online service

Both Docker targets now include Node.js 24 and npm. Rebuild the image after
updating the Dockerfile. Fill `server/cloudflare.local.json` with your
Account ID, D1 database UUID, and a token with Workers deployment and D1 Edit
permissions (or add a separate `deploy_api_token`). See the
[deployment tutorial](../server/docs/CLOUDFLARE_TUTORIAL.md#automatic-deployment-recommended-including-docker)
for token permissions and first-time Cloudflare setup.

From the repository root, using the `zenkaipower` helper defined above:

```sh
docker build --platform linux/amd64 -t zenkaipower .
zenkaipower python3 server/tools/deploy_cloudflare.py --dry-run
zenkaipower python3 server/tools/deploy_cloudflare.py
```

The first script command only installs and bundles locally. The second
also applies remote migrations and deploys. It prints the public Worker URL.
Credentials are read from the mounted checkout, never baked into the image.
No browser login or passing tokens on the command line is required. The script
stops on errors and preserves existing local Wrangler settings. It does not
transfer the LAN database; follow the migration tutorial first if needed.
