# Adapted from Dragon Rage; see android/NOTICE.md.
#!/usr/bin/env python3
"""Builds the engine for Android arm64 (Dragon Rage): the same sources and the same steps as the 64-bit PC build
(undefined.py + link.py with ZENKAIPOWER_CC=clang64), with clang's AArch64 target instead of x86-64.

    port/tools/android.py [compile] [link] [loader] [install]        (all four when none is named)
        -> port/build/android/libzenkaipower.so  (+ libzenkaipower.so.dat, the list of where the game's data comes from)

What is different from the PC build:
  - the game's 4-byte pointers (`__ptr32 __uptr`, ptr32.py) need clang/LLVM 20 or newer on AArch64; irfix.py does
    the same repairs as for x86-64
  - the PS2's float arithmetic: software float through the soft ABI (`-mabi=aapcs-soft -mgeneral-regs-only`,
    clang 19 or newer), so every float operation still goes to port/src/softfloat_ps2.c
  - the program: linked at 0x03000000 (the PC's is at 0x20000000, where an Android app has its Java heap), without a program interpreter, and marked as a shared object
    afterwards, so Android's own loader can load it into a region the app reserved at that address (the Android
    side: android/app/src/main/cpp). Code is position-independent (GOT access to the system libraries' data) but
    the link is not: the game's 32-bit addresses of symbols in its data tables are fixed at link time.

Environment (defaults: the folders port/tools/android_toolchain.sh unpacks):
    ZENKAIPOWER_LLVM      folder of an LLVM 20+ installation (bin/clang, bin/llc, lib/libclang.so)
    ZENKAIPOWER_SYSROOT   the NDK's sysroot (usr/include, usr/lib/aarch64-linux-android)
    ZENKAIPOWER_CLANG_RT  folder with libclang_rt.builtins-aarch64-android.a
    ZENKAIPOWER_SDL3      folder with SDL3's include/ and lib/libSDL3.so for Android arm64
    ZENKAIPOWER_GLSLC     the shader compiler (glslc)
    ZENKAIPOWER_JOBS      parallel compiles (default: the number of processors)
    ZENKAIPOWER_SKELETON  1: the game's data tables blank (always for a release); otherwise port/build/gen/data if it exists
"""
import concurrent.futures, hashlib, json, os, pathlib, re, shutil, struct, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TC = pathlib.Path(os.environ.get("ZENKAIPOWER_TOOLCHAIN", str(ROOT / "port/build/android-toolchain")))
LLVM = pathlib.Path(os.environ.get("ZENKAIPOWER_LLVM", str(TC / "llvm")))
SYSROOT = pathlib.Path(os.environ.get("ZENKAIPOWER_SYSROOT", str(TC / "ndk/sysroot")))
CLANG_RT = pathlib.Path(os.environ.get("ZENKAIPOWER_CLANG_RT", str(TC / "ndk/clang_rt")))
SDL3 = pathlib.Path(os.environ.get("ZENKAIPOWER_SDL3", str(TC / "sdl3")))
GLSLC = os.environ.get("ZENKAIPOWER_GLSLC", str(TC / "ndk/bin/glslc"))
API = os.environ.get("ZENKAIPOWER_ANDROID_API", "29")

# the Python side of libclang for ptr32.py: the installation's own if it has one, else what the toolchain unpacked
for p in (LLVM / "lib/python3/site-packages", TC / "clang-bindings"):
    if p.exists():
        sys.path.insert(0, str(p))
os.environ["LD_LIBRARY_PATH"] = os.pathsep.join([str(LLVM / "lib"), str(pathlib.Path(GLSLC).parent), os.environ.get("LD_LIBRARY_PATH", "")])
sys.path.insert(0, str(HERE))
import portsrc, eeconst, irfix, buildkey  # noqa: E402
try:
    import clang.cindex as _ci
    if (LLVM / "lib/libclang.so").exists():
        _ci.Config.set_library_file(str(LLVM / "lib/libclang.so"))
except ImportError:
    pass
import ptr32  # noqa: E402

OUT = ROOT / "port/build/android"
OBJ = OUT / "obj"
DATA = OUT / "obj_data"
GEN = OUT / "gen"
EXE = OUT / "libzenkaipower.so"
BASE = int(os.environ.get("ZENKAIPOWER_ANDROID_BASE", "0x03000000"), 0)  # below an app's Java heap (0x12C00000 up): plat_mem.c

CLANG = str(LLVM / "bin/clang")
LLC = str(LLVM / "bin/llc")
TARGET = [f"--target=aarch64-linux-android{API}", f"--sysroot={SYSROOT}"]
RELEASE = os.environ.get("ZENKAIPOWER_RELEASE") == "1"
BUILD_KEY_FLAGS = buildkey.flags()
if RELEASE:
    TARGET += ["-DPORT_DISTRIBUTION_BUILD=1"]
SOFT = ["-mabi=aapcs-soft", "-mgeneral-regs-only"]
# the game's code: the flags of undefined.py's CC, for this target
GAME = [CLANG] + TARGET + ["-std=gnu89", "-c", "-O2", "-g1", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w",
                           "-DZENKAIPOWER_ANDROID=1", "-fPIE", "-fno-stack-protector", "-fno-builtin", "-fms-extensions", "-Wno-error=return-mismatch"] + SOFT + \
       ["-include", "port_libm.h", "-Iinclude", "-Iport/include", "-include", "port_compat.h"]


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True, **kw)


def compile_ee(cmd, src, o, game_text=True):
    """undefined.compile_ee for this target: preprocess, the PS2 compiler's float constants (eeconst), 4-byte
    pointers in the game's text (ptr32), LLVM IR, irfix, llc."""
    pre = [a for a in cmd if a != "-c"] + ["-E", str(src)]
    r = run(pre)
    if r.returncode:
        return r
    i = pathlib.Path(str(o)[:-2] + ".i")
    i.write_text(eeconst.transform(r.stdout))
    std = next((a for a in cmd if a.startswith("-std=")), "-std=gnu89")
    if game_text:
        try:
            text = ptr32.transform(str(i), ["-x", "cpp-output"] + TARGET + [std, "-fms-extensions", "-w", "-Wno-error=return-mismatch"] + SOFT)
        except RuntimeError as e:
            return subprocess.CompletedProcess(cmd, 1, "", f"ptr32: {e}\n")
        i.write_text(text, encoding="latin-1")
    c2, skip = [], False
    for a in cmd:  # the forced includes are already in the preprocessed text
        if skip:
            skip = False
        elif a == "-include":
            skip = True
        else:
            c2.append(a)
    ll = pathlib.Path(str(o)[:-2] + ".ll")
    r = run(c2 + ["-S", "-emit-llvm", "-x", "cpp-output", str(i), "-o", str(ll)])
    if r.returncode:
        return r
    text, _ = irfix.fix(ll.read_text(), stores=True)
    ll.write_text(text)
    r = run([LLC, "-O2", "-filetype=obj", "-relocation-model=pic", str(ll), "-o", str(o)])
    if r.returncode == 0 and not os.environ.get("ZENKAIPOWER_KEEP"):
        i.unlink(missing_ok=True)
        ll.unlink(missing_ok=True)
    return r


def game_source(f):
    o = OBJ / (str(f.relative_to(ROOT / "src")).replace("/", "_")[:-2] + ".o")
    src, done, missing = portsrc.prepare(f)
    r = compile_ee(GAME + [f"-I{f.parent}"], src, o)
    return (o if r.returncode == 0 else None), f.name, r.stderr


def newlib_source(f):
    o = OBJ / ("nl_" + f.stem + ".o")
    cmd = [CLANG] + TARGET + ["-std=gnu89", "-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w"] + SOFT + \
          ["-Iport/include", f"-I{f.parent}", "-include", "newlib_shim.h"]
    r = compile_ee(cmd, f, o, game_text=False)
    return (o if r.returncode == 0 else None), f.name, r.stderr


HARD = [CLANG] + TARGET + ["-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-fms-extensions", "-DZENKAIPOWER_ANDROID=1"]


def port_source(f):
    o = OBJ / ("pc_" + f.stem + ".o")
    names = ["-include", "vu0_names.h", "-DREF_VU0_EXTERN_ARITH"] if f.parent.name == "port" and f.parent.parent.name == "src" else []
    hard = f.name in ("plat_libm.c", "plat_fastvec.c", "plat_log.c")
    cmd = [CLANG] + TARGET + ["-std=gnu99", "-c", "-O2", "-g1", "-fPIE", "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w",
                              "-fms-extensions", "-DZENKAIPOWER_ANDROID=1", "-Iinclude", "-Iport/include", "-Iport/src", f"-I{SDL3 / 'include'}"]
    if not hard:
        cmd += SOFT + ["-DSF_FAST_CALLS", "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
    cmd += names
    if f.name == "plat_log.c":
        # include/sys/file.h is the game's file system, not bionic's flock API.
        cmd.remove("-Iinclude")
    if f.name == "port_archive.c":
        # Feature selection must precede forced libc headers for bionic funopen.
        cmd += ["-D_BSD_SOURCE=1"]
    if not hard and not names:
        r = compile_ee(cmd, f, o)
    else:
        r = run(cmd + [str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def gs_source(f):
    o = OBJ / ("gs_" + f.stem + ".o")
    key = BUILD_KEY_FLAGS if f.name == "net_sign.c" else []
    r = run(HARD + key + ["-std=gnu99", "-Wall", "-Wno-unused", "-Wno-misleading-indentation", f"-I{GEN / 'gs'}", f"-I{SDL3 / 'include'}", str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def cxx_source(f):
    o = OBJ / ("cxx_" + f.stem + ".o")
    imgui = ROOT / "port/third_party/imgui"
    r = run([str(LLVM / "bin/clang++")] + TARGET + ["-std=c++17", "-c", "-O2", "-g1", "-fPIE", "-fms-extensions", "-w", "-DZENKAIPOWER_ANDROID=1",
                                                   f"-I{imgui}", f"-I{ROOT / 'port/src/gs'}", f"-I{GEN / 'gs'}", f"-I{SDL3 / 'include'}", str(f), "-o", str(o)])
    return (o if r.returncode == 0 else None), f.name, r.stderr


def shaders():
    """port/src/gs/shaders as SPIR-V and as source text, in the header the renderer includes (as undefined.py)."""
    gen = GEN / "gs"
    gen.mkdir(parents=True, exist_ok=True)
    version = json.loads((ROOT / "game-info.json").read_text())["version"] if RELEASE else "dev"
    head = '/* generated by port/tools/android.py */\n#define PORT_VERSION ' + json.dumps(version) + '\n'
    if not (gen / "port_version.h").exists() or (gen / "port_version.h").read_text() != head:
        (gen / "port_version.h").write_text(head)
    arrays = []
    # gsnz.frag: gs.frag without its depth output (NO_FRAG_DEPTH), for the draws that do not need it (undefined.py)
    nz = gen / "gsnz.frag"
    nz_text = (ROOT / "port/src/gs/shaders/gs.frag").read_text().replace("\n", "\n#define NO_FRAG_DEPTH\n", 1)
    if not nz.exists() or nz.read_text() != nz_text:
        nz.write_text(nz_text)
    # gswb.frag: that, and the GS's 1x sampling of a work buffer (GS_BLOCK), for the glow chain's draws (key bit 25)
    wb = gen / "gswb.frag"
    wb_text = (ROOT / "port/src/gs/shaders/gs.frag").read_text().replace("\n", "\n#define NO_FRAG_DEPTH\n#define GS_BLOCK\n", 1)
    if not wb.exists() or wb.read_text() != wb_text:
        wb.write_text(wb_text)
    sources = sorted((ROOT / "port/src/gs/shaders").glob("*.*")) + [nz, wb]
    for src in sources:
        stage = src.suffix[1:]
        name = "k" + src.stem.capitalize() + stage.capitalize() + "Spv"
        spv = gen / (src.name + ".spv")
        r = run([GLSLC, f"-fshader-stage={stage}", str(src), "-o", str(spv)])
        if r.returncode:
            raise RuntimeError(f"Shader {src.name} failed: {r.stderr}")
        arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in spv.read_bytes()) + "};\n")
    for src in sources:
        if src.suffix in (".vert", ".frag"):
            name = "k" + src.stem.capitalize() + src.suffix[1:].capitalize() + "Glsl"
            data = src.read_bytes() + b"\0"
            arrays.append(f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in data) + "};\n")
    (gen / "shaders.h").write_text("/* generated from port/src/gs/shaders by port/tools/android.py */\n" + "".join(arrays))



def compile_cached_job(task):
    fn, f, common, old = task
    key = fn.__name__ + '/' + f.relative_to(ROOT).as_posix()
    signing = repr(BUILD_KEY_FLAGS).encode() if f.name == 'net_sign.c' else b''
    if f.name == "plat_content.c":
        signing += (ROOT / "port/build/generated/content_assets.inc").read_bytes()
    stamp = hashlib.sha256(common + f.read_bytes() + signing).hexdigest()
    if old.get('stamp') == stamp and pathlib.Path(old.get('object', '')).is_file():
        return pathlib.Path(old['object']), f.name, '', key, stamp
    if old.get('object'):
        pathlib.Path(old['object']).unlink(missing_ok=True)
    o, name, err = fn(f)
    return o, name, err, key, stamp


def compile_all():
    from assets.build import generate
    generate()
    OBJ.mkdir(parents=True, exist_ok=True)
    DATA.mkdir(parents=True, exist_ok=True)
    shaders()
    jobs = []
    jobs += [(game_source, f) for f in portsrc.sources()]
    jobs += [(newlib_source, f) for f in sorted((ROOT / "port/third_party/newlib_libm").glob("*.c"))]
    jobs += [(port_source, f) for f in [ROOT / "src/port/vu0_a.c", ROOT / "src/port/vu0_b.c"] + sorted((ROOT / "port/src").glob("*.c"))]
    jobs += [(gs_source, f) for f in sorted((ROOT / "port/src/gs").glob("*.c"))]
    jobs += [(cxx_source, f) for f in sorted((ROOT / "port/third_party/imgui").glob("*.cpp")) + sorted((ROOT / "port/src/gs").glob("*.cpp"))]
    only = os.environ.get("ZENKAIPOWER_ONLY")  # a few files: ZENKAIPOWER_ONLY=btl_obj,plat_mem
    if only:
        names = only.split(",")
        jobs = [j for j in jobs if any(n in j[1].name for n in names)]
    # One conservative header/build-settings key, plus each source's contents.
    # A failed recompile must never leave an older object available to the linker.
    common = hashlib.sha256()
    for folder in (ROOT / 'include', ROOT / 'port/include', ROOT / 'port/src',
                   ROOT / 'port/third_party', GEN):
        for f in sorted(folder.rglob('*')):
            if f.is_file() and f.suffix in ('.h', '.inc', '.hpp'):
                common.update(str(f).encode()); common.update(f.read_bytes())
    for f in sorted(HERE.glob('*.py')):
        common.update(f.read_bytes())
    common.update(repr((TARGET, SOFT, GAME, HARD, LLVM, os.environ.get('ZENKAIPOWER_SKELETON'))).encode())
    cache_file = OUT / 'compile-cache.json'
    try:
        cache = json.loads(cache_file.read_text())
    except (OSError, ValueError):
        cache = {}
    failed = []
    tasks = [(fn, f, common.digest(), cache.get(fn.__name__ + '/' + f.relative_to(ROOT).as_posix(), {})) for fn, f in jobs]
    # Pointer rewriting walks Python AST objects; separate processes avoid the
    # interpreter lock and isolate each libclang translation unit.
    with concurrent.futures.ProcessPoolExecutor(int(os.environ.get("ZENKAIPOWER_JOBS", min(4, os.cpu_count() or 2)))) as ex:
        for n, (o, name, err, key, stamp) in enumerate(ex.map(compile_cached_job, tasks), 1):
            if o is None:
                cache.pop(key, None)
                failed.append(name)
                (OUT / (name + '.error.txt')).write_text(err)
                lines = [l for l in err.splitlines() if "error" in l or l.startswith("ptr32") or "Cannot select" in l]
                print("FAILED", name, "\n   ".join((lines or err.strip().splitlines() or ["?"])[:6])[:900], flush=True)
            else:
                cache[key] = {'stamp': stamp, 'object': str(o)}
            if n % 20 == 0 or n == len(jobs):
                print(f'Android sources: {n}/{len(jobs)}', flush=True)
                cache_file.write_text(json.dumps(cache, indent=2))
    cache_file.write_text(json.dumps(cache, indent=2))
    # Always ship blank data tables. The user's installed disc supplies their bytes.
    for old in DATA.glob("*.o"):
        old.unlink()
    for f in sorted((ROOT / "port/data").glob("*.s")):
        r = run([CLANG] + TARGET + ["-c", str(f), "-o", str(DATA / (f.name[:-2] + ".o"))])
        if r.returncode:
            failed.append(f.name)
            print("FAILED", f.name, r.stderr.strip()[:300])
    (DATA / "SKELETON").write_text("blank\n")
    print(f"compiled: {len(jobs) - len([n for n in failed if not n.endswith('.s')])} of {len(jobs)} sources; {len(failed)} failed")
    return not failed


def abi_check():
    """Catch AArch64 pointer layout, widened stores and hardware-float regressions."""
    OUT.mkdir(parents=True, exist_ok=True)
    obj = OUT / "abi-check.o"
    src = ROOT / "port/tests/android/abi.c"
    cmd = [CLANG] + TARGET + SOFT + ["-std=gnu11", "-c", "-O2", "-fms-extensions", "-fPIE"]
    result = compile_ee(cmd, src, obj, game_text=False)
    if result.returncode:
        print(result.stderr)
        return False
    result = run([LLVM / "bin/llvm-objdump", "-dr", obj])
    if result.returncode or not all(re.search(pattern, result.stdout) for pattern in
                                    (r"\bstrb\b", r"\bstrh\b", r"__addsf3")):
        print("AArch64 ABI check failed:\n", result.stdout, result.stderr)
        return False
    print("AArch64 ABI OK: 32-bit pointers, byte/halfword stores, software float")
    return True


def rtdir():
    """clang's runtime folder for this target, filled from the NDK's files (the upstream LLVM has none for Android)."""
    result = run([CLANG] + TARGET + ["-print-resource-dir"])
    if result.returncode or not result.stdout.strip():
        raise RuntimeError("Could not locate Clang's Android runtime directory: " + result.stderr.strip())
    out = pathlib.Path(result.stdout.strip()) / f"lib/aarch64-unknown-linux-android{API}"
    out.mkdir(parents=True, exist_ok=True)
    for source, target in (("libclang_rt.builtins-aarch64-android.a", "libclang_rt.builtins.a"),
                           ("libunwind.a", "libunwind.a")):
        if not (out / target).exists():
            shutil.copy(CLANG_RT / source, out / target)
    return out


STORAGE_WRAPS = "open open64 __open_2 __open64_2 fopen fopen64 stat stat64 lstat lstat64 access mkdir unlink rmdir remove rename opendir readdir readdir64 closedir rewinddir chdir getcwd realpath SDL_IOFromFile SDL_LoadFile SDL_GetPathInfo SDL_CreateDirectory SDL_EnumerateDirectory SDL_RenamePath SDL_RemovePath SDL_LoadPNG SDL_SavePNG".split()

def storage_flags():
    return ['-L' + str(OUT), '-lstorage'] + ['-Wl,--wrap=' + name for name in STORAGE_WRAPS]

def storage():
    OUT.mkdir(parents=True, exist_ok=True)
    # This is the first target link in link(), loader() and installer(). Prepare
    # LLVM's Android runtimes before it, not only in the later game link_cmd().
    runtime = rtdir()
    command = [CLANG] + TARGET + ['-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
        '-I' + str(SDL3 / 'include'), str(ROOT / 'android/app/src/main/cpp/storage.c'),
        '-L' + str(SDL3 / 'lib'), '-L' + str(runtime), '-lSDL3', '-llog', '-Wl,--no-undefined',
        '-Wl,-soname,libstorage.so', '-Wl,-z,max-page-size=16384', '-o', str(OUT / 'libstorage.so')]
    result = run(command)
    if result.returncode: print(result.stderr)
    return result.returncode == 0


def storage_tests():
    if not storage(): return False
    target = ROOT / 'android/app/src/androidTest/jniLibs/arm64-v8a/libstorage_checks.so'
    target.parent.mkdir(parents=True, exist_ok=True)
    result = run([CLANG] + TARGET + ['-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
        '-I' + str(SDL3 / 'include'), str(ROOT / 'android/app/src/androidTest/cpp/storage_checks.c')]
        + storage_flags() + ['-L' + str(SDL3 / 'lib'), '-lSDL3', '-Wl,--no-undefined',
                            '-Wl,-z,max-page-size=16384', '-o', str(target)])
    if result.returncode: print(result.stderr)
    return result.returncode == 0


def link_cmd(objs, extra, out=None, base=None):
    out = out or EXE
    base = BASE if base is None else base
    return [str(LLVM / "bin/clang++")] + TARGET + [
        "-fuse-ld=lld", "-no-pie", "-nostartfiles", "-o", str(out)] + extra + objs + storage_flags() + [
        str(SYSROOT / f"usr/lib/aarch64-linux-android/{API}/crtbegin_so.o"),
        f"-L{SDL3 / 'lib'}", f"-L{rtdir()}", "-lSDL3", "-llog", "-landroid", "-lm", "-ldl", "-lGLESv3",
        "-static-libstdc++",
        str(SYSROOT / f"usr/lib/aarch64-linux-android/{API}/crtend_so.o"),
        f"-Wl,--image-base={base:#x}", "-Wl,--no-dynamic-linker", "-Wl,--export-dynamic", "-Wl,--hash-style=both",
        "-Wl,-z,max-page-size=16384", "-Wl,--wrap=main", "-Wl,--wrap=Progress_Main", "-Wl,--defsym=D_3BE71C=0x3BE71C",
        "-Wl,--unresolved-symbols=report-all", "-Wl,--error-limit=0", f"-Wl,-Map={EXE}.map", "-Wl,--build-id=sha1"]


def link():
    if not storage(): return False
    objs = [str(o) for o in sorted(OBJ.glob("*.o")) + sorted(DATA.glob("*.o"))]
    r = run(link_cmd(objs, []))
    need = sorted(set(re.findall(r"undefined symbol: (\w+)", r.stderr)))
    if need:
        # Original PS2 SDK dispatch tables survive in the assembly data. The
        # native port replaces those libraries, as on desktop. Permit traps only
        # for symbols referenced by those data tables and never by compiled code.
        refs = run([LLVM / 'bin/llvm-objdump', '-r'] + [str(o) for o in sorted(OBJ.glob('*.o'))])
        if refs.returncode:
            print(refs.stderr)
            return False
        referenced = set(re.findall(r'\b(?:' + '|'.join(map(re.escape, need)) + r')\b', refs.stdout))
        tables = '\n'.join(f.read_text() for f in sorted((ROOT / 'port/data').glob('*.s')))
        table_refs = set(re.findall(r'^\s*\.long\s+(\w+)\s*$', tables, re.M))
        unsupported = referenced | (set(need) - table_refs)
        if unsupported:
            print('Missing Android runtime symbols:', ', '.join(sorted(unsupported)))
            return False
        (OUT / 'unused-sdk.txt').write_text('\n'.join(need) + '\n')
    lines = ['/* Generated by port/tools/android.py: one stub per symbol the Android build does not provide yet. */',
             '#include <android/log.h>', 'extern void abort(void);',
             'static void missing(const char *n) { __android_log_print(6, "zenkaipower", "not implemented yet: %s", n); abort(); }']
    lines += [f'void {s}(void) {{ missing("{s}"); }}' for s in need]
    GEN.mkdir(parents=True, exist_ok=True)
    (GEN / "stubs.c").write_text("\n".join(lines) + "\n")
    (OUT / "stubs.txt").write_text("\n".join(need) + "\n")
    stub_o = GEN / "stubs.o"
    if need:
        s = run([CLANG] + TARGET + ["-c", "-O2", "-fPIE", str(GEN / "stubs.c"), "-o", str(stub_o)])
        if s.returncode:
            print(s.stderr[:600])
            return False
    r = run(link_cmd(objs, [str(stub_o)] if need else []))
    errs = [l for l in r.stderr.splitlines() if "error" in l]
    print(f"{len(need)} unused PS2 SDK table traps; link {'OK -> ' + str(EXE.relative_to(ROOT)) if r.returncode == 0 else 'FAILED'}")
    for l in errs[:20]:
        print("  " + l[-200:])
    if r.returncode:
        return False
    state_list(EXE)  # (from this link's map, before the second link writes its own)
    # the same program linked RELOC_DELTA higher: the words that differ by exactly that are the program's own
    # addresses, which the loader moves when the program cannot be loaded at BASE (relocations())
    alt = OUT / "libzenkaipower.alt.so"
    r = run(link_cmd(objs, [str(stub_o)] if need else [], out=alt, base=BASE + RELOC_DELTA))
    if r.returncode:
        print("second link FAILED", r.stderr[-600:])
        return False
    if not relocations(EXE, alt):
        return False
    alt.unlink()
    finish()
    return True


# The game's own variables in the program, for saving and restoring its state (online play, gs/state.c): what
# make_state.py writes from a GNU map for the PC, here from lld's. The same rule for what counts as the game's
# (plat_game too, as on the PC since the Tag Team mod: its animation layers change during a fight and a rollback
# must put them back)
# (everything but the port's renderer, sound, input and UI files and some of its pc_* files); read-only data left
# out (.data.rel.ro is made read-only by the loader after the move). Addresses as linked (BASE): the engine adds
# the move (ZENKAIPOWER_ENGINE_BIAS).
STATE_PORT = {"headless", "mathf_pc", "plat_file", "plat_game", "plat_libm", "plat_mc", "plat_mem", "plat_sndstate", "plat_stub", "plat_sys", "softfloat_ps2",
              "vu0_a", "vu0_b", "gs_marker"}
STATE_WRITABLE = (".data", ".bss", ".sdata", ".sbss", ".tbss", ".tdata")


def state_list(exe):
    def is_game(path):
        p = pathlib.Path(path.split("(")[0])
        if p.parent == DATA:
            return True
        if p.parent != OBJ or p.name.startswith(("gs_", "cxx_", "ui", "imgui")):
            return False
        if p.name.startswith("pc_"):
            return p.stem[3:] in STATE_PORT
        return True

    ranges, others, out, marks = [], [], None, {}
    line_re = re.compile(r"^\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+(\d+) (.*)$")
    for l in pathlib.Path(str(exe) + ".map").read_text(errors="replace").splitlines():
        m = line_re.match(l)
        if not m:
            continue
        rest = m.group(5)
        level = len(rest) - len(rest.lstrip(" "))
        name = rest.strip()
        if level == 0:
            out = name  # an output section
            continue
        if level > 8 and name in ("gPortResim", "gPortVBlanks"):
            marks[name] = int(m.group(1), 16)  # which program the list belongs to (state.c compares them)
        if level != 8 or out is None or not out.startswith(STATE_WRITABLE) or out.startswith(".data.rel.ro"):
            continue
        addr, size = int(m.group(1), 16), int(m.group(3), 16)
        if not size or ":(" not in name:
            continue
        (ranges if is_game(name.rsplit(":(", 1)[0]) else others).append((addr, addr + size))
    ranges.sort()
    others.sort()
    merged = []
    for a, b in ranges:  # joined over alignment gaps, never over the port's own variables (make_state.py)
        if merged and a <= merged[-1][1] + 64 and not any(x < a and y > merged[-1][1] for x, y in others):
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    if len(marks) != 2:
        sys.exit("state list: gPortResim / gPortVBlanks are not in the map")
    head = f"0 {marks['gPortResim']:x}\n1 {marks['gPortVBlanks']:x}\n"
    pathlib.Path(str(exe) + ".mem").write_text(head + "".join(f"{a:x} {b - a:x}\n" for a, b in merged))
    print(f"state list: {len(merged)} ranges of game variables, {sum(b - a for a, b in merged):,} bytes -> {exe.name}.mem")


RELOC_DELTA = 0x10000000
SKIP_SECTIONS = {".dynsym", ".gnu.version", ".gnu.version_r", ".gnu.version_d", ".gnu.hash", ".hash", ".dynstr", ".rela.dyn",
                 ".rela.plt", ".dynamic", ".note.android.ident", ".note.gnu.build-id"}


def elf_sections(data):
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    raw = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shentsize) for i in range(shnum)]
    strtab = raw[shstrndx][4]
    out = {}
    for sh in raw:
        name = data[strtab + sh[0]:data.index(b"\0", strtab + sh[0])].decode()
        out[name] = {"type": sh[1], "flags": sh[2], "addr": sh[3], "off": sh[4], "size": sh[5]}
    return out


def relocations(exe, alt):
    """libzenkaipower.so.rel: every 4-byte word of the program that holds one of its own addresses (32-bit game pointers and the
    low half of 64-bit ones: the program is below 4 GB), found by comparing two links; the loader adds the load bias to
    them. Also where the program's initialisers are: the loader calls them itself after moving (finish() hides them from
    Android's loader, which would call them before)."""
    a, b = exe.read_bytes(), alt.read_bytes()
    secs, secs_b = elf_sections(a), elf_sections(b)
    for name, sec in secs.items():  # (the allocated part must be laid out the same; names in .strtab may differ)
        if sec["flags"] & 2 and (name not in secs_b or secs_b[name]["off"] != sec["off"] or secs_b[name]["size"] != sec["size"]):
            print("relocations: the two links are laid out differently at", name)
            return False
    skip = set()
    for name in (".rela.dyn", ".rela.plt"):  # slots Android's loader writes itself
        sec = secs.get(name)
        if sec:
            for k in range(sec["size"] // 24):
                off, = struct.unpack_from("<Q", a, sec["off"] + k * 24)
                skip.add(off)
    words, odd = [], 0
    for name, sec in secs.items():
        if not (sec["flags"] & 2) or sec["type"] == 8 or name in SKIP_SECTIONS:  # allocated, with bytes in the file
            continue
        for k in range(0, sec["size"] - 3, 4):
            o = sec["off"] + k
            x, = struct.unpack_from("<I", a, o)
            y, = struct.unpack_from("<I", b, o)
            if x == y:
                continue
            va = sec["addr"] + k
            if (y - x) & 0xFFFFFFFF == RELOC_DELTA and va not in skip and va - 4 not in skip:
                words.append(va)
            elif va not in skip and va - 4 not in skip:
                odd += 1
    if odd:
        print(f"relocations: {odd} words differ by something else than the move (not moved)")
    init = secs.get(".init_array", {"addr": 0, "size": 0})
    rel = bytearray(b"BT3R" + struct.pack("<IIIII", 1, BASE, len(words), init["addr"], init["size"]))
    for w in words:
        rel += struct.pack("<I", w)
    pathlib.Path(str(exe) + ".rel").write_bytes(rel)
    print(f"relocations: {len(words)} words -> {exe.name}.rel")
    return True


def nm(path):
    syms = {}
    for l in run([LLVM / "bin/llvm-nm", "--defined-only", str(path)]).stdout.splitlines():
        p = l.split()
        if len(p) == 3:
            syms[p[2]] = int(p[0], 16)
    return syms


def file_offset(exe, addr):
    """Offset in the file of a virtual address, from the program headers."""
    data = exe.read_bytes()
    phoff, = struct.unpack_from("<Q", data, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
    for i in range(phnum):
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from("<IIQQQQ", data, phoff + i * phentsize)
        if p_type == 1 and p_vaddr <= addr < p_vaddr + p_filesz:
            return p_offset + addr - p_vaddr
    raise SystemExit(f"address {addr:#x} is not in the file")


def finish():
    """After the link: the list of the game's data (make_dat.py, from the symbol table instead of a GNU map) when the
    data tables are blank, and the ELF type changed to a shared object for Android's loader."""
    syms = nm(EXE)
    image = bytearray(EXE.read_bytes())
    if (DATA / "SKELETON").exists():
        where, checksum = {}, None
        for l in (ROOT / "port/data/index.txt").read_text().splitlines():
            p = l.split()
            if len(p) == 3 and not l.startswith("#"):
                where[p[0]] = (int(p[1]), int(p[2], 16))
            elif len(p) == 2 and p[0] == "checksum":
                checksum = int(p[1], 16)
        records = []
        for name in sorted(where):
            which, so = where[name]
            text = (ROOT / "port/data" / (name + ".s")).read_text().splitlines()
            # where the object's data starts: the first label's address less what the file puts before that label
            first, before = None, 0
            for l in text:
                if re.match(r"^[A-Za-z_.$][\w.$]*:$", l.strip()):
                    first = l.strip()[:-1]
                    break
                p = l.split()
                if p and p[0] in (".space", ".zero"):
                    before += int(p[1])
                elif p and p[0] == ".long":
                    before += 4
            if first is None or first not in syms:
                raise SystemExit(f"{name}: its first label is not in the program")
            start, off = syms[first] - before, 0
            for l in text:
                p = l.split()
                if not p or p[0].startswith("#"):
                    continue
                if p[0] == ".space":
                    records.append((start + off, which, so + off, int(p[1])))
                    off += int(p[1])
                elif p[0] == ".zero":
                    off += int(p[1])
                elif p[0] == ".long":
                    off += 4
        dat = bytearray(b"BT3D" + struct.pack("<IQ", len(records), checksum))
        for r in records:
            dat += struct.pack("<IIII", *r)
        pathlib.Path(str(EXE) + ".dat").write_bytes(dat)
        k = file_offset(EXE, syms["gPortDataStripped"])
        image[k:k + 4] = struct.pack("<I", 1)
        print(f"data list: {len(records)} records -> {EXE.name}.dat")
    dyn = elf_sections(bytes(image))[".dynamic"]
    for k in range(dyn["size"] // 16):  # DT_INIT_ARRAY(SZ) -> tags Android's loader ignores: the loader calls them (relocations())
        tag, = struct.unpack_from("<q", image, dyn["off"] + k * 16)
        if tag in (25, 27):
            struct.pack_into("<q", image, dyn["off"] + k * 16, 0x6FFFF000 + tag)
    struct.pack_into("<H", image, 0x10, 3)  # e_type: ET_EXEC -> ET_DYN (loaded at its own addresses, android/app/src/main/cpp)
    EXE.write_bytes(image)


APP = ROOT / "android/app/src/main"


def loader():
    if not storage(): return False
    OUT.mkdir(parents=True, exist_ok=True)
    r = run([CLANG] + TARGET + ["-shared", "-fPIC", "-O2", "-Wall", str(APP / "cpp/loader.c")] + storage_flags() + ["-o", str(OUT / "libmain.so"), "-llog", "-ldl", "-landroid", "-Wl,-z,max-page-size=16384"])
    if r.returncode: print(r.stderr)
    return r.returncode == 0

def installer(test=False):
    if not storage(): return False
    r = run(['sh', 'port/tools/android_nod.sh'], env={**os.environ, 'ZENKAIPOWER_TOOLCHAIN': str(TC)})
    if r.returncode:
        print(r.stdout, r.stderr)
        return False
    # setup_text's CLI generates the same defaults used by the desktop installer.
    r = run([sys.executable, 'port/tools/setup_text.py', '--out', str(OUT / 'setup_text_defaults.h')])
    if r.returncode:
        print(r.stderr)
        return False
    if not test:
        for source, name, cpp, dependencies in [
            (APP / 'cpp/setup_artwork.cpp', 'setup_artwork', True,
             [ROOT / 'port/setup/native.h'])]:
            obj, stamp = OUT / (name + '.o'), OUT / (name + '.sha256')
            command = [str(LLVM / 'bin/clang++') if cpp else CLANG] + TARGET + ['-c', '-fPIC', '-O2', '-I' + str(OUT)]
            if cpp: command += ['-std=c++17']
            key = hashlib.sha256(repr(command).encode() + str(pathlib.Path(CLANG).stat().st_mtime_ns).encode())
            for dependency in [source] + dependencies: key.update(dependency.read_bytes())
            digest = key.hexdigest()
            if obj.is_file() and stamp.is_file() and stamp.read_text() == digest: continue
            r = run(command + [str(source), '-o', str(obj)])
            if r.returncode:
                print(r.stderr)
                return False
            stamp.write_text(digest)
    nod = TC / 'nod'
    cmd = [str(LLVM / 'bin/clang++')] + TARGET + ['-shared', '-fPIC', '-std=c++17', '-O2',
        '-DZENKAIPOWER_ANDROID_INSTALLER=1', '-I' + str(SDL3 / 'include'), '-I' + str(OUT),
        '-I' + str(nod / 'nod-ffi/include')]
    cmd += ([str(APP.parent / 'androidTest/cpp/readers.cpp')] if test else
            [str(ROOT / 'port/setup/native.cpp'), str(APP / 'cpp/installer.cpp'),
             str(OUT / 'setup_artwork.o')])
    target = APP.parent / 'androidTest/jniLibs/arm64-v8a/libreader_checks.so' if test else OUT / 'libinstaller.so'
    target.parent.mkdir(parents=True, exist_ok=True)
    cmd += storage_flags()
    cmd += [str(ROOT / 'port/setup/text.cpp'),
        str(nod / 'target/aarch64-linux-android/release/libnod.a'),
        '-L' + str(SDL3 / 'lib'), '-lSDL3', '-llog', '-landroid', '-ldl', '-lz',
        '-static-libstdc++', '-Wl,-z,max-page-size=16384', '-Wl,--no-undefined',
        '-o', str(target)]
    r = run(cmd)
    if r.returncode: print(r.stderr)
    return r.returncode == 0

def install():
    jni = APP / "jniLibs/arm64-v8a"
    jni.mkdir(parents=True, exist_ok=True)
    libraries = (EXE, OUT / "libmain.so", OUT / "libinstaller.so", OUT / "libstorage.so", SDL3 / "lib/libSDL3.so")
    # This folder contains generated runtime libraries; discard obsolete engine names.
    for old in jni.glob("*.so"):
        if old.name not in {f.name for f in libraries}: old.unlink()
    for f in libraries:
        shutil.copy2(f, jni / f.name)
    assets = APP / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    shutil.copytree(SDL3 / "java/org", APP / "java/org", dirs_exist_ok=True)
    for name in ("LICENSE",):
        shutil.copy2(ROOT / name, assets / name)
    licenses = assets / 'licenses'
    licenses.mkdir(exist_ok=True)
    for name in ('LICENSE-MIT', 'LICENSE-APACHE'):
        shutil.copy2(TC / 'nod' / name, licenses / ('nod-' + name + '.txt'))
    # Preserve upstream and bundled compression-library notices from the locked sources.
    for registry in (TC / 'cargo/registry/src').glob('*'):
        for crate in registry.iterdir():
            if not crate.is_dir(): continue
            for notice in crate.rglob('*'):
                if notice.is_file() and notice.name.lower().startswith(('license', 'licence', 'copying', 'copyright')):
                    destination = licenses / 'nod-dependencies' / crate.name / notice.relative_to(crate)
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(notice, destination)
    shutil.copy2(TC / 'nod/Cargo.lock', licenses / 'nod-Cargo.lock')
    for f in (ROOT / 'android/licenses').glob('*.txt'):
        shutil.copy2(f, licenses / f.name)
    sdl_header = (SDL3 / 'include/SDL3/SDL.h').read_text()
    (licenses / 'SDL3.txt').write_text(sdl_header.split('*/', 1)[0] + '*/\n')
    from android_resources import prepare_resources
    prepare_resources(assets, OUT)
    print("Android libraries and assets installed into", APP)
    return True

def main():
    args = sys.argv[1:] or ["abi", "compile", "link", "loader", "installer", "install"]
    stages = {"abi": abi_check, "compile": compile_all, "link": link, "loader": loader, "installer": installer,
              "installer-tests": lambda: installer(True), "storage-tests": storage_tests, "install": install}
    if any(a not in stages for a in args): sys.exit("Usage: android.py [abi] [compile] [link] [loader] [install]")
    for name in args:
        if not stages[name](): sys.exit(1)
if __name__ == "__main__": main()
