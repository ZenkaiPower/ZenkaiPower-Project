/* Adapted from Dragon Rage; see android/NOTICE.md. */
/*
 * libmain.so: what SDL's Android activity calls (SDL_main). It starts the engine, libzenkaipower.so.
 *
 * The engine is the PC port's 64-bit program built for arm64 (port/tools/android.py): the game keeps addresses in
 * 4 bytes, so its code and data must lie where they were linked, at 0x03000000, below 4 GB (and below the Java
 * heap, which an app's process has from 0x12C00000 up). The program is linked
 * at that address and marked as a shared object; this reserves the address range and asks Android's own loader
 * to load the program exactly there (android_dlopen_ext with ANDROID_DLEXT_RESERVED_ADDRESS), so the loader's
 * load bias is zero and every address in the program is the one it was linked with. The loader also resolves the
 * program's imports (SDL3, the C library) and runs its constructors (plat_mem.c maps the game's memory there).
 *
 * Arguments (GameActivity.getArguments): the game folder (gamedata/, textures/, savegame/, ...),
 * the native-library folder, private cache, and the engine metadata cache extracted from setup/engine.pak.
 *
 * Everything the engine prints (stdout, stderr) goes to the log (adb logcat -s zenkaipower) and to logs/zenkaipower_log.txt in the
 * game's folder, so a player can send it.
 */
#define _GNU_SOURCE 1 /* process_vm_readv */
#include <android/api-level.h>
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <dirent.h>
#include <sys/uio.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <jni.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

#define TAG "zenkaipower"

static FILE *sLog;
static int (*sTouchControlsActive)(void);
static int (*sTouchVibration)(void);
static int (*sBattleActive)(void);
static void (*sPauseRequest)(void);

JNIEXPORT jboolean JNICALL
Java_com_zenkaipower_android_GameActivity_nativeBattleActive(JNIEnv *env, jclass type) {
    (void)env; (void)type;
    int (*query)(void)=__atomic_load_n(&sBattleActive,__ATOMIC_ACQUIRE);
    return query && query() ? JNI_TRUE:JNI_FALSE;
}
JNIEXPORT void JNICALL
Java_com_zenkaipower_android_GameActivity_nativePauseBattle(JNIEnv *env, jclass type) {
    (void)env; (void)type;
    void (*request)(void)=__atomic_load_n(&sPauseRequest,__ATOMIC_ACQUIRE);
    if(request) request();
}

JNIEXPORT jboolean JNICALL
Java_com_zenkaipower_android_GameActivity_nativeTouchControlsActive(JNIEnv *env, jclass type) {
    (void)env; (void)type;
    int (*query)(void) = __atomic_load_n(&sTouchControlsActive, __ATOMIC_ACQUIRE);
    return query && query() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jint JNICALL
Java_com_zenkaipower_android_GameActivity_nativeTouchVibration(JNIEnv *env, jclass type) {
    (void)env; (void)type;
    int (*query)(void)=__atomic_load_n(&sTouchVibration,__ATOMIC_ACQUIRE);
    return query ? query() : 0;
}
static int sPipe[2];

static void *log_thread(void *arg) {
    char buf[1024];
    size_t used = 0;
    (void)arg;
    for (;;) {
        ssize_t n = read(sPipe[0], buf + used, sizeof(buf) - 1 - used);
        if (n <= 0) {
            break;
        }
        used += (size_t)n;
        for (;;) { /* whole lines to the log; a line longer than the buffer goes out in pieces */
            char *nl = memchr(buf, '\n', used);
            size_t len = nl != NULL ? (size_t)(nl - buf) : used == sizeof(buf) - 1 ? used : 0;
            if (len == 0 && nl == NULL) {
                break;
            }
            buf[len] = '\0';
            __android_log_write(ANDROID_LOG_INFO, TAG, buf);
            if (sLog != NULL) {
                fprintf(sLog, "%s\n", buf);
                fflush(sLog);
            }
            len += nl != NULL;
            memmove(buf, buf + len, used - len);
            used -= len;
        }
    }
    return NULL;
}

static void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, TAG, line);
    if (sLog != NULL) {
        fprintf(sLog, "%s\n", line);
        fflush(sLog);
    }
}

static void redirect_output(const char *dir) {
    char path[1024];
    pthread_t th;
    char older[1024], old[1024];
    /* the two runs before this one are kept (zenkaipower_log.1.txt, zenkaipower_log.2.txt): a problem is often seen one run late */
    snprintf(path, sizeof(path), "%s/logs", dir);
    mkdir(path, 0700);
    snprintf(path, sizeof(path), "%s/logs/zenkaipower_log.txt", dir);
    snprintf(old, sizeof(old), "%s/logs/zenkaipower_log.1.txt", dir);
    snprintf(older, sizeof(older), "%s/logs/zenkaipower_log.2.txt", dir);
    rename(old, older);
    rename(path, old);
    sLog = fopen(path, "w");
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (pipe(sPipe) == 0) {
        dup2(sPipe[1], 1);
        dup2(sPipe[1], 2);
        pthread_create(&th, NULL, log_thread, NULL);
        pthread_detach(th);
    }
}

/* What is mapped below 4 GB in this process: the first thing to look at when the game's memory does not fit. */
static void log_low_maps(void) {
    char line[512];
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return;
    }
    say("mappings below 4 GB:");
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long lo = strtoul(line, NULL, 16);
        if (lo < 0x100000000ul) {
            line[strcspn(line, "\n")] = '\0';
            say("  %s", line);
        }
    }
    fclose(fp);
}


static const char *engine_library(void) { return "libzenkaipower.so"; }
/* The address range the program's segments take, from its program headers. */
static int program_span(const char *path, uintptr_t *lo, uintptr_t *hi) {
    Elf64_Ehdr eh;
    Elf64_Phdr ph;
    int fd = open(path, O_RDONLY | O_CLOEXEC), i;
    *lo = UINTPTR_MAX;
    *hi = 0;
    if (fd < 0) {
        return 0;
    }
    if (pread(fd, &eh, sizeof(eh), 0) != sizeof(eh) || memcmp(eh.e_ident, ELFMAG, SELFMAG) != 0) {
        close(fd);
        return 0;
    }
    for (i = 0; i < eh.e_phnum; i++) {
        if (pread(fd, &ph, sizeof(ph), (off_t)(eh.e_phoff + (uint64_t)i * eh.e_phentsize)) != sizeof(ph)) {
            break;
        }
        if (ph.p_type == PT_LOAD) {
            if (ph.p_vaddr < *lo) {
                *lo = ph.p_vaddr;
            }
            if (ph.p_vaddr + ph.p_memsz > *hi) {
                *hi = ph.p_vaddr + ph.p_memsz;
            }
        }
    }
    close(fd);
    return *hi > *lo;
}

/* ---- Early reservation of the game's fixed addresses.
   The engine needs fixed places below 4 GB (its own code at 0x03000000, the PS2 heap from 0x003BE000, the game
   thread's stack, the port's region, the hardware registers). Anything else that loads into the process first (the GPU
   driver, the audio stack, a vendor library) might take one of them. So they are claimed, as address space only, the
   moment this library is loaded (before SDL starts the GPU and audio), and handed to the engine: ZENKAIPOWER_LOW_RESERVED lists
   them, and plat_mem.c maps over its own reservation. A range already taken is left out and logged. */
static struct { uintptr_t lo, hi; int ok; } sEarly[6];
static char sEarlyList[256];

static int early_reserve(int i, uintptr_t lo, uintptr_t hi) {
    void *p = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    sEarly[i].lo = lo;
    sEarly[i].hi = hi;
    sEarly[i].ok = p == (void *)lo;
    if (p != MAP_FAILED && p != (void *)lo) {
        munmap(p, hi - lo);
    }
    if (sEarly[i].ok) {
        size_t n = strlen(sEarlyList);
        snprintf(sEarlyList + n, sizeof(sEarlyList) - n, "%s%lx-%lx", n ? ";" : "", (unsigned long)lo, (unsigned long)hi);
    } else {
        __android_log_print(ANDROID_LOG_WARN, TAG, "early: 0x%lx-0x%lx is already taken (%s)", (unsigned long)lo, (unsigned long)hi,
                            strerror(errno));
    }
    return sEarly[i].ok;
}

static uintptr_t sEngineLo, sEngineHi; /* the engine's range, reserved early (0: not) */
static intptr_t sBias;                   /* where the engine was loaded less where it was linked */

__attribute__((constructor)) static void reserve_early(void) {
    uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
    Dl_info info;
    early_reserve(0, 0x003BE000u & ~(page - 1), 0x03000000u); /* PS2 heap, then the game thread's stack */
    early_reserve(1, 0x05000000u, 0x0F000000u);                 /* the port's region */
    if (dladdr((void *)reserve_early, &info) != 0 && info.dli_fname != NULL) {
        char engine[1024];
        const char *slash = strrchr(info.dli_fname, '/');
        uintptr_t lo, hi;
        snprintf(engine, sizeof(engine), "%.*s/%s", slash != NULL ? (int)(slash - info.dli_fname) : 1, slash != NULL ? info.dli_fname : ".", engine_library());
        if (program_span(engine, &lo, &hi)) {
            lo &= ~(page - 1);
            hi = (hi + page - 1) & ~(page - 1);
            if (early_reserve(4, lo, hi)) {
                sEngineLo = lo;
                sEngineHi = hi;
            }
        }
    }
    setenv("ZENKAIPOWER_LOW_RESERVED", sEarlyList, 1);
}

/* ---- Moving the engine.
   The engine is linked at 0x03000000; where that range is taken (an app's Java heap sits at different places below
   4 GB on different devices) it is loaded in another free place below 4 GB and its own addresses are moved by the
   difference: libzenkaipower.so.rel (port/tools/android.py, relocations()) lists every 4-byte word that holds one. Its
   initialisers are hidden from Android's loader and called here, after the move. */
typedef struct { uint32_t base, count, initAddr, initSize; uint32_t *words; } EngineRel;
static EngineRel sRel;

static int read_rel(const char *path, EngineRel *r) {
    FILE *fp = fopen(path, "rb");
    char magic[4];
    uint32_t hdr[5];
    memset(r, 0, sizeof(*r));
    if (fp == NULL) {
        return 0;
    }
    if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "BT3R", 4) != 0 || fread(hdr, 4, 5, fp) != 5 || hdr[0] != 1) {
        fclose(fp);
        return 0;
    }
    r->base = hdr[1];
    r->count = hdr[2];
    r->initAddr = hdr[3];
    r->initSize = hdr[4];
    r->words = malloc((size_t)r->count * 4);
    if (r->words == NULL || fread(r->words, 4, r->count, fp) != r->count) {
        free(r->words);
        r->words = NULL;
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

/* A free place below 4 GB for `size` bytes, 64 KB aligned, reserved (address space only). 0 if none. */
static uintptr_t find_low(size_t size) {
    uintptr_t at;
    for (at = 0x02000000u; at + size <= 0xF0000000u; at += 0x00100000u) /* (above the PS2 heap) */ {
        void *p = mmap((void *)at, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == (void *)at) {
            return at;
        }
        if (p != MAP_FAILED) {
            munmap(p, size);
        }
    }
    return 0;
}

/* Adds `bias` to the listed words; the pages are made writable for it and given back their segment's protection. */
static void move_engine(const char *engine, const EngineRel *r, intptr_t bias) {
    uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
    Elf64_Ehdr eh;
    Elf64_Phdr ph[32];
    int fd = open(engine, O_RDONLY | O_CLOEXEC), n = 0, i;
    uint32_t k;
    if (fd >= 0 && pread(fd, &eh, sizeof(eh), 0) == sizeof(eh) && eh.e_phnum <= 32 &&
        pread(fd, ph, (size_t)eh.e_phnum * sizeof(Elf64_Phdr), (off_t)eh.e_phoff) == (ssize_t)(eh.e_phnum * sizeof(Elf64_Phdr))) {
        n = eh.e_phnum;
    }
    if (fd >= 0) {
        close(fd);
    }
    for (i = 0; i < n; i++) { /* every loaded segment writable for the moment */
        if (ph[i].p_type == PT_LOAD) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias + page - 1) & ~(page - 1);
            mprotect((void *)lo, hi - lo, PROT_READ | PROT_WRITE | ((ph[i].p_flags & PF_X) ? PROT_EXEC : 0));
        }
    }
    for (k = 0; k < r->count; k++) {
        uint32_t *w = (uint32_t *)(uintptr_t)(r->words[k] + bias);
        *w = (uint32_t)(*w + (uint32_t)bias);
    }
    for (i = 0; i < n; i++) { /* the segments' own protection, and the read-only part after relocation (RELRO) */
        if (ph[i].p_type == PT_LOAD) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias + page - 1) & ~(page - 1);
            mprotect((void *)lo, hi - lo, ((ph[i].p_flags & PF_R) ? PROT_READ : 0) | ((ph[i].p_flags & PF_W) ? PROT_WRITE : 0) |
                                              ((ph[i].p_flags & PF_X) ? PROT_EXEC : 0));
        }
    }
    for (i = 0; i < n; i++) {
        if (ph[i].p_type == PT_GNU_RELRO) {
            uintptr_t lo = (ph[i].p_vaddr + bias) & ~(page - 1), hi = (ph[i].p_vaddr + ph[i].p_memsz + bias) & ~(page - 1);
            if (hi > lo) {
                mprotect((void *)lo, hi - lo, PROT_READ);
            }
        }
    }
}

static void run_initialisers(const EngineRel *r, intptr_t bias) {
    extern char **environ;
    uint32_t i;
    for (i = 0; i < r->initSize / 8; i++) {
        void (*fn)(int, char **, char **) = *(void (**)(int, char **, char **))(uintptr_t)(r->initAddr + bias + i * 8);
        if (fn != NULL && (uintptr_t)fn != (uintptr_t)-1) {
            fn(0, NULL, environ);
        }
    }
}

/* SDL caches the environment before SDL_main. Mirror the loader's paths into
   that snapshot so SDL_getenv and libc getenv see the same installation. */
static int sync_sdl_environment(void *library) {
    void *(*get_environment)(void) = dlsym(library, "SDL_GetEnvironment");
    bool (*set_variable)(void *, const char *, const char *, bool) = dlsym(library, "SDL_SetEnvironmentVariable");
    static const char *names[] = {"ZENKAIPOWER_DATA", "ZENKAIPOWER_EXE", "ZENKAIPOWER_TMPDIR", "ZENKAIPOWER_SETTINGS", "ZENKAIPOWER_GS",
                                  "ZENKAIPOWER_ENGINE_BIAS", "ZENKAIPOWER_LOW_RESERVED", "SDL_VIDEO_DRIVER",
                                  "ZENKAIPOWER_PREPARE_TEXT", "ZENKAIPOWER_PREPARE_RESULT", "ZENKAIPOWER_DEMO"};
    if (!get_environment || !set_variable) { say("SDL environment API unavailable"); return 0; }
    void *environment = get_environment();
    for (size_t i=0; i<sizeof(names)/sizeof(names[0]); i++) {
        const char *value=getenv(names[i]);
        if (value && !set_variable(environment,names[i],value,true)) return 0;
    }
    return 1;
}

int SDL_main(int argc, char **argv) {
    if (argc < 3) return 1;
    const char *dir = argv[1], *libdir = argv[2];
    const char *metadata = argc > 4 ? argv[4] : dir;
    char engine[1024], exe[1024], data[1024];
    uintptr_t lo, hi, page = (uintptr_t)sysconf(_SC_PAGESIZE);
    void *region, *handle;
    int (*entry)(int, char **);
    char *args[] = {"ZenkaiPower", "--settings", NULL};
    if (chdir(dir)) return 1;
    redirect_output(dir);
    if (argc > 3) setenv("ZENKAIPOWER_TMPDIR", argv[3], 1);
    snprintf(engine, sizeof(engine), "%s/libzenkaipower.so", libdir);
    setenv("ZENKAIPOWER_ANDROID_ENGINE", engine, 1);
    setenv("ZENKAIPOWER_ANDROID_METADATA", metadata, 1);
    snprintf(exe, sizeof(exe), "%s/libzenkaipower.so", metadata);
    snprintf(data, sizeof(data), "%s/gamedata", dir);
    setenv("ZENKAIPOWER_DATA", data, 1);
    setenv("ZENKAIPOWER_EXE", exe, 1);
    if (mkdir("savegame", 0700) != 0 && errno != EEXIST) {
        say("Cannot create save folder: %s", strerror(errno));
        return 1;
    }
    setenv("ZENKAIPOWER_SETTINGS", "savegame/settings.dat", 1);
    setenv("ZENKAIPOWER_GS", "gpu", 1);
    if (argc > 5 && strcmp(argv[5], "prepare-text") == 0) {
        char result[1024];
        snprintf(result, sizeof(result), "%s/engine/text-result", argv[3]);
        setenv("ZENKAIPOWER_PREPARE_TEXT", "1", 1);
        setenv("ZENKAIPOWER_PREPARE_RESULT", argc > 6 && argv[6][0] ? argv[6] : result, 1);
        setenv("ZENKAIPOWER_DEMO", "1", 1);
        setenv("ZENKAIPOWER_SETTINGS", "", 1);
        setenv("ZENKAIPOWER_GS", "none", 1);
    }
    setenv("SDL_VIDEO_DRIVER", "android", 1);
    if (!program_span(engine, &lo, &hi)) {
        say("cannot read the engine %s", engine);
        return 1;
    }
    lo &= ~(page - 1);
    hi = (hi + page - 1) & ~(page - 1);
    say("early reservations: %s", sEarlyList[0] ? sEarlyList : "(none)");
    {
        char relpath[1100];
        int haveRel;
        snprintf(relpath, sizeof(relpath), "%s/libzenkaipower.so.rel", metadata);
        haveRel = read_rel(relpath, &sRel);
        if (sEngineLo == lo && sEngineHi == hi) {
            region = (void *)lo; /* (claimed when this library was loaded) */
        } else {
            region = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        }
        if (region == (void *)lo && getenv("ZENKAIPOWER_FORCE_MOVE") != NULL && atoi(getenv("ZENKAIPOWER_FORCE_MOVE")) != 0) {
            /* (testing the move on a device where the usual range is free: zenkaipower_env.txt ZENKAIPOWER_FORCE_MOVE=1) */
            void *keep = region; /* held while another place is found, so it is not found again */
            region = (void *)find_low(hi - lo);
            munmap(keep, hi - lo);
            say("ZENKAIPOWER_FORCE_MOVE: the engine goes to %p", region);
            if (region == NULL || !haveRel) {
                say("cannot move the engine");
                return 1;
            }
        } else if (region != (void *)lo) {
            if (region != MAP_FAILED) {
                munmap(region, hi - lo);
            }
            say("the engine's usual range 0x%lx-0x%lx is taken in this process", (unsigned long)lo, (unsigned long)hi);
            log_low_maps();
            if (!haveRel) {
                say("no relocation list (%s): the engine cannot be moved", relpath);
                return 1;
            }
            region = (void *)find_low(hi - lo);
            if (region == NULL) {
                say("no free place below 4 GB for the engine (%lu KB)", (unsigned long)((hi - lo) >> 10));
                return 1;
            }
        }
        sBias = (intptr_t)((uintptr_t)region - lo);
    }
    if (sBias == 0) {
        log_low_maps(); /* (what the game's own fixed regions will have to fit around: plat_mem.c) */
    }
    {
        android_dlextinfo info;
        char biasText[32];
        memset(&info, 0, sizeof(info));
        info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
        info.reserved_addr = region;
        info.reserved_size = hi - lo;
        say("loading the engine at 0x%lx (%lu KB)%s", (unsigned long)(uintptr_t)region, (unsigned long)((hi - lo) >> 10),
            sBias != 0 ? ", moved" : "");
        handle = android_dlopen_ext(engine, RTLD_NOW | RTLD_GLOBAL, &info);
        snprintf(biasText, sizeof(biasText), "%ld", (long)sBias);
        setenv("ZENKAIPOWER_ENGINE_BIAS", biasText, 1); /* plat_mem.c: where the game's data tables are now */
    }
    if (handle == NULL) {
        say("the engine did not load: %s", dlerror());
        log_low_maps();
        return 1;
    }
    if (!sync_sdl_environment(handle)) return 1;
    if (sRel.words != NULL) {
        if (sBias != 0) {
            move_engine(engine, &sRel, sBias);
            say("moved %u addresses of the engine by 0x%lx", sRel.count, (unsigned long)sBias);
        }
        run_initialisers(&sRel, sBias);
    }
    __atomic_store_n(&sTouchControlsActive,
        (int (*)(void))dlsym(handle, "Port_AndroidTouchControlsActive"), __ATOMIC_RELEASE);
    __atomic_store_n(&sTouchVibration,
        (int (*)(void))dlsym(handle, "Port_AndroidTouchVibration"), __ATOMIC_RELEASE);
    __atomic_store_n(&sBattleActive,
        (int (*)(void))dlsym(handle,"Port_AndroidBattleActive"),__ATOMIC_RELEASE);
    __atomic_store_n(&sPauseRequest,
        (void (*)(void))dlsym(handle,"Port_AndroidPauseRequest"),__ATOMIC_RELEASE);
    entry = (int (*)(int, char **))dlsym(handle, "__wrap_main");
    if (!entry) { say("Missing engine entry: %s", dlerror()); return 1; }
    say("Starting ZenkaiPower");
    int result = entry(2, args);
    __atomic_store_n(&sTouchControlsActive, NULL, __ATOMIC_RELEASE);
    __atomic_store_n(&sTouchVibration,NULL,__ATOMIC_RELEASE);
    __atomic_store_n(&sBattleActive,NULL,__ATOMIC_RELEASE);
    __atomic_store_n(&sPauseRequest,NULL,__ATOMIC_RELEASE);
    say("Game ended (%d)", result);
    return result;
}
