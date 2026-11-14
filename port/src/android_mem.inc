/* Adapted from Dragon Rage; see android/NOTICE.md. */
/* -------------------------------------------------------------------------------------------------------- Linux */
#include <sys/mman.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
/* Android: ranges the loader claimed for the engine as soon as it was loaded (ZENKAIPOWER_LOW_RESERVED, "lo-hi;lo-hi" in
   hex). The engine maps over its own reservation there (MAP_FIXED); anywhere else it must not replace a mapping. */
static int fixed_flag(uintptr_t lo, size_t len) {
    const char *p = getenv("ZENKAIPOWER_LOW_RESERVED");
    while (p != NULL && *p != '\0') {
        unsigned long a = 0, b = 0;
        if (sscanf(p, "%lx-%lx", &a, &b) == 2 && lo >= a && lo + len <= b) {
            return MAP_FIXED;
        }
        p = strchr(p, ';');
        p = p != NULL ? p + 1 : NULL;
    }
    return MAP_FIXED_NOREPLACE;
}

static void map(uint32_t addr, uint32_t size, const char *what) {
    /* whole pages around the region (the system's pages may be 16 KB: an Android device's) */
    uint32_t page = (uint32_t)sysconf(_SC_PAGESIZE), base = addr & ~(page - 1), len = (addr - base + size + page - 1) & ~(page - 1);
    void *p = mmap((void *)(uintptr_t)base, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | fixed_flag(base, len),
                   -1, 0);

    if (p != (void *)(uintptr_t)base) {
        fprintf(stderr, "zenkaipower: cannot map %s at 0x%08X\n", what, addr);
#ifdef __ANDROID__
        /* (straight to the system log too: the process ends before the app's log thread reads stderr) */
        __android_log_print(6 /* ANDROID_LOG_ERROR */, "zenkaipower", "cannot map %s at 0x%08X", what, addr);
#endif
        exit(2);
    }
}

static void map_heap(void) {
#ifdef __ANDROID__
    /* At its PS2 address if that is free; else, as on Windows, anywhere between 0x30000000 and the scratchpad (0x70000000) (nothing in the
       game depends on the address: Port_Malloc starts wherever the region is). Some phones have something of their
       own there (a Redmi with HyperOS 2: "cannot map the game heap at 0x003BE000", the game closed at once). */
    uint32_t page = (uint32_t)sysconf(_SC_PAGESIZE), len = (HEAP_END - HEAP_BASE + page - 1) & ~(page - 1);
    uintptr_t at;
    void *p = mmap((void *)(uintptr_t)HEAP_BASE, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | fixed_flag(HEAP_BASE, len), -1, 0);
    if (p == (void *)(uintptr_t)HEAP_BASE) {
        return;
    }
    if (p != MAP_FAILED) {
        munmap(p, len);
    }
    for (at = 0x30000000u; at + len <= 0x70000000u; at += 0x01000000u) { /* (below the scratchpad at 0x70000000) */
        p = mmap((void *)at, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == (void *)at) {
            sHeapNext = (uint32_t)at + (HEAP_FIRST - HEAP_BASE);
            sHeapEnd = (uint32_t)at + (HEAP_END - HEAP_BASE);
            fprintf(stderr, "zenkaipower: the game heap's PS2 address is taken in this process: the heap is at 0x%08X instead\n", (unsigned)at);
            __android_log_print(5 /* ANDROID_LOG_WARN */, "zenkaipower", "the game heap is at 0x%08X (its PS2 address is taken)", (unsigned)at);
            return;
        }
        if (p != MAP_FAILED) {
            munmap(p, len);
        }
    }
#endif
    map(HEAP_BASE, HEAP_END - HEAP_BASE, "the game heap");
}

#if defined(__aarch64__)
/* The 64-bit build: the game's pointers are still 4 bytes wide (port/tools/ptr32.py), so everything the game can
   point at has to lie below 4 GB.
     - the program itself: linked at 0x20000000 (port/tools/link.py), clear of the regions above;
     - the C library's heap: kept in the break area that follows the program (no mmap, one arena), because
       handles the port gives to the game (files, sound players) come from it;
     - the stack of the thread that runs the game: the game stores addresses of local variables, so its main()
       runs on a thread whose stack is mapped right above the heap (the process's own stack is far above 4 GB).
       Below the program at 0x20000000 on purpose: the C library's own heap starts at a random place in the
       gigabyte ABOVE the program, and a fixed address there (this stack used to be at 0x60000000) is sometimes
       taken, which stopped the program at start now and then. */
#include <malloc.h>
#include <pthread.h>
#define STACK_BASE 0x02000000u
#define STACK_SIZE 0x01000000u
#ifdef __ANDROID__
/* Android: where an app's Java heap sits below 4 GB differs from device to device (0x02000000 on some), so the
   stack goes at STACK_BASE when that is free, else in the first free place below 4 GB (plat_mem's map_anywhere). */
static uint32_t sStackBase = STACK_BASE;
#define STACK_AT sStackBase
uint8_t gPortHwRegs[0x10000] __attribute__((aligned(65536))); /* the PS2's registers at 0x10000000 (irfix.py) */
uint8_t gPortGsRegs[0x2000] __attribute__((aligned(65536)));  /* and at 0x12000000 */
uint8_t gPortScratchpad[0x4000] __attribute__((aligned(65536)));
#else
#define STACK_AT STACK_BASE
#endif
extern int __real_main(int argc, char **argv);
PORT_HOST static int sArgc = 0, sResult = 0; /* (the process's own arguments: an address on its stack, not game state) */
PORT_HOST static char **sArgv = NULL;

/* Where the game's own part of the thread's stack ends: above this frame lie the thread library's own data for
   the thread (its control block and thread-local variables are at the top of the stack it was given, among them
   the C library's per-thread memory cache). Saving and restoring the game's state must leave those alone. */
static void *game_thread(void *arg) {
    (void)arg;
    sGameStackTop = (uint8_t *)__builtin_frame_address(0);
    sResult = __real_main(sArgc, sArgv);
    return NULL;
}

int __wrap_main(int argc, char **argv) {
    pthread_attr_t attr;
    pthread_t th;

    if (Port_EnsureTextPackage(argv[0]) != 0) return 1;
    Port_ParseLaunchArgs(argc, argv);
    sArgc = argc;
    sArgv = argv;
    if (getenv("ZENKAIPOWER_SIGN_TEST") != NULL) { /* testing the build key's signature (gs/net_match.c): prints and ends */
        extern int Match_SignTest(void);
        return Match_SignTest();
    }
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, (void *)(uintptr_t)STACK_AT, STACK_SIZE);
    if (pthread_create(&th, &attr, game_thread, NULL) != 0) {
        fprintf(stderr, "zenkaipower: cannot start the game thread\n");
        return 2;
    }
    pthread_join(th, NULL);
    return sResult;
}
#endif

