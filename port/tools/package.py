#!/usr/bin/env python3
"""Makes the release folder and zip: the finished port without any game data, plus the setup window.

    ZENKAIPOWER_CC=clang64 python3 port/tools/undefined.py && ZENKAIPOWER_CC=clang64 python3 port/tools/link.py
    port/setup/build.sh
    python3 port/tools/package.py          -> release/linux-x64/ and release/ZenkaiPower-<version>-linux-x64.zip
    ZENKAIPOWER_CC=win64 python3 port/tools/package.py   (after the win64 build and `port/setup/build.sh win`)
                                           -> release/windows-x64/ and release/ZenkaiPower-<version>-windows-x64.zip

Contents:  ZenkaiPower, ZenkaiPower.dat   the game without the data tables of the original programs (strip_data.py)
           ZenkaiPower-setup       the setup window: reads the user's own disc image and unpacks it into gamedata/
           lib/            SDL3, so that it does not have to be installed

           fonts.pak       the fonts of F1 > UI with their licences (port/third_party/fonts as it is)
           translations.pak       bundled label, UI, and port language catalogs
           licenses/
The installer reads all four discs natively; no Python installation is required. Wii disc reading is linked into the installer. Nothing from the disc is in the zip."""
import os, pathlib, re, shutil, subprocess, sys, zipfile, tempfile, json
from contextlib import contextmanager
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

ROOT = pathlib.Path(__file__).resolve().parents[2]
ZIP_LEVEL = int(os.environ.get("ZENKAIPOWER_ZIP_LEVEL", "6"))
if not 0 <= ZIP_LEVEL <= 9:
    sys.exit("ZENKAIPOWER_ZIP_LEVEL must be between 0 and 9")

NAME = "ZenkaiPower"  # the program's name in a release: NAME, NAME.dat, NAME-setup
OUT = ROOT / "release/linux-x64"
def release_zip(platform):
    version = json.loads((ROOT / "game-info.json").read_text())["version"]
    if not isinstance(version,str) or not re.fullmatch(r"[vV]?[0-9]+(?:\.[0-9]+)*",version):
        raise ValueError("game-info.json version must be numeric, such as 1.0.0")
    return ROOT / "release" / f"ZenkaiPower-v{version.lstrip('vV')}-{platform}-x64.zip"

def release_folder(platform):
    if platform not in ("linux", "windows"):
        raise ValueError(f"unsupported release platform: {platform}")
    return f"ZenkaiPower-{platform.title()}"

ZIP = release_zip("linux")

@contextmanager
def release_archive(destination):
    """ZIP seeks stay on local storage, not Docker Desktop's Windows mount."""
    with tempfile.TemporaryDirectory(prefix="zenkai-release-") as temporary:
        staged = pathlib.Path(temporary) / destination.name
        with zipfile.ZipFile(staged, "w", zipfile.ZIP_DEFLATED, compresslevel=ZIP_LEVEL) as archive:
            yield archive
        # Copy sequentially only after the archive has closed successfully.
        # A failed copy must not replace a previously completed release.
        pending = destination.with_name(destination.name + ".tmp")
        try:
            shutil.copyfile(staged, pending)
            pending.replace(destination)
        finally:
            pending.unlink(missing_ok=True)

# Player content survives repeated packaging but is never distributed in ZIPs.
PRESERVED_FOLDERS = {"gamedata", "savegame", "textures", "setup"}
PRESERVED_FILES = {"zenkaipower.ico", "sm.ico", "bt3.exe", "sm.exe", "Budokai Tenkaichi 3.lnk", "Sparking! METEOR.lnk",
                   "ZenkaiPower.ico", "Play ZenkaiPower.lnk"}

def reset_release(out):
    out.mkdir(parents=True, exist_ok=True)
    for child in out.iterdir():
        if child.name in PRESERVED_FOLDERS | PRESERVED_FILES:
            continue
        if child.is_dir() and not child.is_symlink():
            shutil.rmtree(child)
        else:
            child.unlink()

def release_files(out):
    for child in sorted(out.iterdir()):
        if child.name in PRESERVED_FOLDERS | PRESERVED_FILES:
            continue
        if child.is_file():
            yield child
        elif child.is_dir():
            yield from (p for p in sorted(child.rglob("*")) if p.is_file())

README = """Dragon Ball Z: Budokai Tenkaichi 3 - PC port (Linux, 64-bit)

1. Start  ZenkaiPower-setup
2. Choose your own disc image of the USA release (SLUS-21678), an .iso file.
   The setup checks it and unpacks the game's data next to this file. About 4 GB are needed.
   Optional: a disc image of the European release (SLES-54945) as well, for Spanish, German, French and
   Italian (menus, battle screen and text as that release has them). It can also be added later: start the
   setup again and press "Add languages...".
   Optional: the EU Wii disc (RDSPAF) adds Dragon Net Battle assets, and the Japanese PS2
   disc (SLPS-25815) adds Sparking! METEOR artwork and BGM. No Python installation is needed.
   Wii disc reading is built into the installer; notices are in licenses/nod/.
3. Press Play. Later, start  ZenkaiPower  itself (or the setup again, and press Play).

Online server: dragon-net-battle.pak beside the executable contains the server address.
Release builders edit dragon-net-battle.txt in the source tree before packaging.
ZENKAIPOWER_MATCH_URL overrides this file when set.

In the game, F1 opens the settings: resolution, widescreen, controls and sound.
With the European languages added, the first launch shows a language list in the game window before loading game or memory card data. Change it later with F1 > UI > Language.
Game data uses indexed AFS packages and game-text.pak in gamedata/.
Setup generates and packages text resources before the first game launch.
Game saves, settings and login data are kept in savegame/. A file placed in gamedata/mods/<same path as the original> replaces the original.
Texture packs (PCSX2 naming, .dds or .png) go into textures/, stages and music from outside the disc into
stages/ and songs/: see the note in each folder.
F1 > UI has three font settings, all starting on "Original": "Menu font" draws menu labels, "Battle font" draws
announcements and HUD text, and "Dialogue / UI font" draws the game's ordinary text. Their fonts and licences are
in fonts/. To add a .ttf, put it there and list it in fonts.tsv. The third column says where it can be used:
1 = Menu, 2 = Dialogue / UI, 4 = Battle, added up for several (3 = Menu and Dialogue / UI, 7 = all three).
The game makes the menu and battle label list from your disc data on its first start (gamedata/game-text/, in the
background). Menu and Battle font choices appear when the list is ready.

Screen reader (English only): with Prism's library (prism.dll, libprism.so; release 0.18.3) placed next to the program, the menus
and the fight's gauges are spoken and shown in braille. External screen-reader translation files are disabled.

This package contains no game data. The game's data comes from your disc and stays on your computer.

The port's own code is under the GNU AGPL, version 3 (licenses/tenkaichi3decomp-agpl.txt).
Its source, this release's tag included:
https://github.com/ZenkaiPower/ZenkaiPower-Project
"""

WIN_README = README.replace("(Linux, 64-bit)", "(Windows, 64-bit)").replace("Start  ZenkaiPower-setup\n", "Start  ZenkaiPower-setup.exe\n") \
    .replace("start  ZenkaiPower  itself", "start  ZenkaiPower.exe  itself")

TEXTURES_NOTE = """Texture packs go here.

Put a pack's replacement textures into this folder, in subfolders or not: the game looks through all of it when it
starts. The files have to be named the way the PCSX2 emulator names texture replacements for this game
(for example 1dd4c76113969303-56e3d4469d2ad392-00005e54.dds), so a pack made for PCSX2 can be copied in as it is.
Read: .dds files (DXT1, DXT3, DXT5 or plain 32-bit) and .png files.

In the game, F1 > Video has a switch for the pack.
"""

STAGES_NOTE = """Stages from outside the disc.

Put a stage's model file (.unk, the kind made for this game's mods) into this folder. It gets one more cell at the
end of the stage select, named after the file ("My_Map.unk" is shown as "My Map"). Up to 26 stages.

Optional: a file maps.txt here, one line per stage, "file.unk|Name In The Menu", gives other names and an order.

Added stages are not written to your save, and they are not offered in an online match (Dragon Net Battle).
"""

SONGS_NOTE = """Music from outside the disc.

Put a song as an .mp3, .wav, .flac or .adx file (ADX is the game's own music format) into this folder. It is added
to the music choice of the stage select, before "Random", named after the file. Up to 40 songs. An .mp3, .wav or
.flac is played as it is (miniaudio decodes it; there is no conversion pass and no .adx written next to it), so a
new song is in the list from the next start. An .adx is played by the game's own decoder. A file that cannot be
read stays in the list and is silent (the log says why). An .adx that an earlier version wrote next to an .mp3 is
not listed while the .mp3 is there (it can be deleted). Anything else (ogg, m4a...) has to be converted first
(ffmpeg can write the formats above, or ADX: ffmpeg -i song.ogg -ar 24000 -ac 2 -c:a adpcm_adx song.adx).

Optional: a file songs.txt here, one line per song, "file|Name In The Menu", gives other names and an order.

Added songs are not written to your save, and they are not offered in an online match (Dragon Net Battle).
"""


def take_server(out):
    """Package the editable source server address beside the release executable."""
    from data_package import pack_directory
    with tempfile.TemporaryDirectory() as temporary:
        directory = pathlib.Path(temporary) / "dragon-net-battle"
        directory.mkdir()
        shutil.copy2(ROOT / "dragon-net-battle.txt", directory / "server.txt")
        package = pack_directory(directory)
        shutil.copy2(package, out / "dragon-net-battle.pak")


def take_setup_text(out):
    """Package the shared installer catalog on every platform."""
    from data_package import pack_directory
    with tempfile.TemporaryDirectory() as temporary:
        directory = pathlib.Path(temporary) / "setup"
        directory.mkdir()
        shutil.copy2(ROOT / "port/setup/text/en.json", directory / "en.json")
        shutil.copy2(pack_directory(directory), out / "setup.pak")


def take_program(exe, out):
    """The program without the game's data, and its list, into the release folder. A program built from the blank
    tables (port/data) is that already; one built with the tables' values has them taken out by strip_data.py."""
    import toolchain
    out.mkdir(parents=True, exist_ok=True)
    name = NAME + ".exe" if exe.suffix == ".exe" else NAME
    # where the game's own variables lie in the program (make_state.py, at link time): the program reads it from
    # beside itself to save and restore its state, which an online session is built on
    mem = pathlib.Path(str(exe) + ".mem")
    if not mem.exists():
        sys.exit(f"missing {mem.name}: link.py makes it")
    shutil.copy2(mem, out / (name + ".mem"))
    if (toolchain.DATA / "SKELETON").exists():
        dat = pathlib.Path(str(exe)[:-4] + ".dat" if exe.suffix == ".exe" else str(exe) + ".dat")
        if not dat.exists():
            sys.exit(f"missing {dat.name}: link.py makes it")
        shutil.copy2(exe, out / name)
        shutil.copy2(dat, out / (NAME + ".dat"))
        print("program built from the blank data tables (no disc involved)")
        return
    r = subprocess.run([sys.executable, str(ROOT / "port/tools/strip_data.py"), "--exe", str(exe), "--out", str(out)], capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode or "NOT matched" in r.stdout:
        sys.exit("strip_data.py failed or left game data in the program: not packaging\n" + r.stderr)
    (out / ("zenkaipower.exe" if exe.suffix == ".exe" else "zenkaipower")).rename(out / name)  # strip_data.py writes zenkaipower and zenkaipower.dat
    (out / "zenkaipower.dat").rename(out / (NAME + ".dat"))

def take_translations(out):
    """Bundled label, UI, and port-text catalogs."""
    target = out / "translations"
    for kind in ("labels", "ui", "port"):
        (target / kind).mkdir(parents=True, exist_ok=True)
        for source in sorted((ROOT / "port/translations" / kind).glob("*.json")):
            if re.fullmatch(r"[a-z]{2,3}(?:-[A-Za-z]{2,3})?", source.stem):
                shutil.copy2(source, target / kind / source.name)
    from data_package import pack_directory
    pack_directory(target)


def take_fonts(out, lic):
    """Ship fonts in fonts.pak and retain external license notices."""
    src = ROOT / "port/third_party/fonts"
    for p in sorted(src.rglob("*")):
        if p.is_file():
            (out / "fonts" / p.relative_to(src)).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, out / "fonts" / p.relative_to(src))
            if p.name == "LICENSE":
                shutil.copy2(p, lic / f"{p.parent.name}-fonts.txt")

    from data_package import pack_directory
    pack_directory(out / "fonts")


def bundle_sdl_dependencies(dll, out, lic, prefix):
    """MSYS2 SDL can import libiconv; the official SDL archive is self-contained.
    Collect vendor DLLs from the SDL bin folder, never Windows system DLLs."""
    todo, seen = [dll], set()
    while todo:
        current = todo.pop()
        if current.name.lower() in seen:
            continue
        seen.add(current.name.lower())
        result = subprocess.run([prefix + "objdump", "-p", str(current)], capture_output=True, text=True, check=True)
        for name in re.findall(r"DLL Name:\s*(\S+)", result.stdout):
            source = dll.parent / name
            if source.exists() and name.lower() not in seen:
                shutil.copy2(source, out / name)
                todo.append(source)
            elif name.lower().startswith("lib") and not source.exists():
                sys.exit("missing SDL runtime dependency: " + name)
    if "libiconv-2.dll" in seen:
        notices = dll.parent.parent / "share/licenses/libiconv"
        shutil.copy2(notices / "COPYING.LIB", lic / "libiconv-lgpl.txt")
        shutil.copy2(notices / "README", lic / "libiconv.txt")
        (lic / "libiconv-source.txt").write_text(
            "libiconv source: https://ftp.gnu.org/pub/gnu/libiconv/\n"
            "MSYS2 build recipe: https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-libiconv\n")

def main_win():
    """ZENKAIPOWER_CC=win64: zenkaipower.exe (cross-built or built in MSYS2), ZenkaiPower-setup.exe (port/setup/build.sh win), SDL3.dll."""
    import toolchain
    out, zpath = ROOT / "release/windows-x64", release_zip("windows")
    exe, setup = ROOT / "port/build/zenkaipower.exe", ROOT / "port/build/ZenkaiPower-setup.exe"
    dll = next((p for p in [toolchain.sdl3() / "bin/SDL3.dll"] if p.exists()), None) if toolchain.sdl3() else None
    if dll is None:
        dll = next((pathlib.Path(d) / "SDL3.dll" for d in os.environ.get("PATH", "").split(os.pathsep) if (pathlib.Path(d) / "SDL3.dll").exists()), None)
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (ZENKAIPOWER_CC=win64 undefined.py and link.py; port/setup/build.sh win)")
    if dll is None:
        sys.exit("SDL3.dll not found")
    reset_release(out)
    take_program(exe, out)
    take_server(out)
    from pack_game_info import pack_game_info
    pack_game_info(out)
    take_translations(out)
    shutil.copy2(setup, out / "ZenkaiPower-setup.exe")
    take_setup_text(out)
    shutil.copy2(dll, out / "SDL3.dll")
    lic = out / "licenses"
    lic.mkdir()
    bundle_sdl_dependencies(dll, out, lic, toolchain.PREFIX)
    shutil.copy2(ROOT / "LICENSE", lic / "tenkaichi3decomp-agpl.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/stb/LICENSE", lic / "stb-image.txt")
    shutil.copy2(ROOT / "port/third_party/nlohmann/LICENSE.MIT", lic / "nlohmann-json.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/miniaudio/LICENSE", lic / "miniaudio.txt")
    shutil.copy2(ROOT / "port/third_party/monocypher/LICENCE.md", lic / "monocypher.txt")

    from bundle_nod import bundle_nod
    bundle_nod(out)
    take_fonts(out, lic)
    (out / "textures").mkdir(exist_ok=True)
    (lic / "sdl3.txt").write_text("SDL3 (SDL3.dll) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (out / "ZenkaiPower.exe", out / "ZenkaiPower-setup.exe"):
        subprocess.run([toolchain.PREFIX + "strip", "--strip-debug", str(f)], check=False)
    archive_root = release_folder("windows") + "/"
    with release_archive(zpath) as z:
        for p in release_files(out):
            if p.is_file():
                z.write(p, archive_root + str(p.relative_to(out)))
    print(f"{zpath.relative_to(ROOT)}: {zpath.stat().st_size / 1e6:.1f} MB")
    for p in release_files(out):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(out)}")

def main():
    exe = ROOT / "port/build/zenkaipower_64"
    setup = ROOT / os.environ.get("ZENKAIPOWER_SETUP_OUT", "ZenkaiPower-setup")
    for need in (exe, setup, pathlib.Path(str(exe) + ".map")):
        if not need.exists():
            sys.exit(f"missing {need.relative_to(ROOT)}: build first (see the top of this file)")
    reset_release(OUT)
    take_program(exe, OUT)
    take_server(OUT)
    from pack_game_info import pack_game_info
    pack_game_info(OUT)
    take_translations(OUT)
    shutil.copy2(setup, OUT / "ZenkaiPower-setup")
    take_setup_text(OUT)
    (OUT / "lib").mkdir()
    lib = next((p for d in ("/usr/local/lib", "/usr/lib", "/usr/lib64", "/usr/lib/x86_64-linux-gnu") for p in sorted(pathlib.Path(d).glob("libSDL3.so.0"))), None)
    if lib is None:
        sys.exit("libSDL3.so.0 not found")
    shutil.copy2(lib.resolve(), OUT / "lib/libSDL3.so.0")
    lic = OUT / "licenses"
    lic.mkdir()
    shutil.copy2(ROOT / "LICENSE", lic / "tenkaichi3decomp-agpl.txt")
    shutil.copy2(ROOT / "port/third_party/imgui/LICENSE.txt", lic / "dear-imgui.txt")
    shutil.copy2(ROOT / "port/third_party/stb/LICENSE", lic / "stb-image.txt")
    shutil.copy2(ROOT / "port/third_party/nlohmann/LICENSE.MIT", lic / "nlohmann-json.txt")
    shutil.copy2(ROOT / "port/third_party/newlib_libm/COPYING.NEWLIB", lic / "newlib.txt")
    shutil.copy2(ROOT / "port/third_party/xxhash/LICENSE", lic / "xxhash.txt")
    shutil.copy2(ROOT / "port/third_party/miniaudio/LICENSE", lic / "miniaudio.txt")
    shutil.copy2(ROOT / "port/third_party/monocypher/LICENCE.md", lic / "monocypher.txt")

    from bundle_nod import bundle_nod
    bundle_nod(OUT)
    take_fonts(OUT, lic)
    (OUT / "textures").mkdir(exist_ok=True)
    (lic / "sdl3.txt").write_text("SDL3 (lib/libSDL3.so.0) is distributed under the zlib license: https://www.libsdl.org/license.php\n")
    for f in (OUT / "ZenkaiPower", OUT / "ZenkaiPower-setup"):
        subprocess.run(["strip", "--strip-debug", str(f)], check=False)  # the symbol names stay (crash reports use them)
    archive_root = release_folder("linux") + "/"
    with release_archive(ZIP) as z:
        for p in release_files(OUT):
            if p.is_file():
                info = zipfile.ZipInfo.from_file(p, archive_root + str(p.relative_to(OUT)))
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, p.read_bytes(), compresslevel=ZIP_LEVEL)
    print(f"{ZIP.relative_to(ROOT)}: {ZIP.stat().st_size / 1e6:.1f} MB")
    for p in release_files(OUT):
        if p.is_file():
            print(f"  {p.stat().st_size:>10,}  {p.relative_to(OUT)}")
    portable(OUT)

def portable(out):
    """Says whether the release would run on other people's machines: no CPU level above the baseline demanded
    or used, and no newer C library than Ubuntu 22.04's (2.35). Build with port/release/build_linux.sh to pass."""
    bad = []
    for f in ("ZenkaiPower", "ZenkaiPower-setup", "lib/libSDL3.so.0"):
        note = subprocess.run(["readelf", "-n", str(out / f)], capture_output=True, text=True).stdout
        need = next((l.split(":", 1)[1].strip() for l in note.splitlines() if "ISA needed" in l), "")
        if any(v in need for v in ("x86-64-v2", "x86-64-v3", "x86-64-v4")):
            bad.append(f"{f}: stamped as needing {need}")
        dis = subprocess.run(["objdump", "-d", str(out / f)], capture_output=True, text=True).stdout
        # nod's compression libraries dispatch optimized routines
        # after detecting the CPU; their presence does not raise its baseline.
        dispatched = f == "ZenkaiPower-setup"
        if not dispatched and f != "lib/libSDL3.so.0" and ("%ymm" in dis or "%zmm" in dis):
            bad.append(f"{f}: contains AVX instructions")
        if not dispatched and "%zmm" in dis:
            bad.append(f"{f}: contains AVX-512 instructions")
        syms = subprocess.run(["objdump", "-T", str(out / f)], capture_output=True, text=True).stdout
        vers = sorted({tuple(int(x) for x in v.split(".")) for v in __import__("re").findall(r"GLIBC_(\d+\.\d+)", syms)})
        if vers and vers[-1] > (2, 35):
            bad.append(f"{f}: needs glibc {vers[-1][0]}.{vers[-1][1]}")
    print("portable: yes (baseline x86-64, glibc <= 2.35)" if not bad else "NOT PORTABLE:\n  " + "\n  ".join(bad))

if __name__ == "__main__":
    if sys.argv[1:]:
        sys.exit("Usage: package.py")
    if os.environ.get("ZENKAIPOWER_CC") == "win64":
        main_win()
    else:
        main()
