/* Host-only state: never included in a rollback snapshot. */
#define PORT_LOG_IMPLEMENTATION
#include "port_log.h"
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
static SRWLOCK sMutex = SRWLOCK_INIT;
#define LOCK() AcquireSRWLockExclusive(&sMutex)
#define UNLOCK() ReleaseSRWLockExclusive(&sMutex)
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/file.h>
static pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&sMutex)
#define UNLOCK() pthread_mutex_unlock(&sMutex)
#endif

enum { KEEP_SECONDS = 7 * 24 * 60 * 60 };
static char sPath[2][4096];
static int sInitialized;
static int sBotProcess;
static time_t sCleaned[2];
static char sLastVerification[1024];

static void stamp(time_t now, char out[32]) {
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    strftime(out, 32, "[%Y-%m-%d %H:%M:%S UTC] ", &utc);
}

static int file_lock(FILE *file) {
#ifdef _WIN32
    OVERLAPPED offset = {0};
    return LockFileEx((HANDLE)_get_osfhandle(_fileno(file)), LOCKFILE_EXCLUSIVE_LOCK,
                      0, MAXDWORD, MAXDWORD, &offset) != 0;
#else
    return flock(fileno(file), LOCK_EX) == 0;
#endif
}

static void file_unlock(FILE *file) {
#ifdef _WIN32
    OVERLAPPED offset = {0};
    UnlockFileEx((HANDLE)_get_osfhandle(_fileno(file)), 0, MAXDWORD, MAXDWORD, &offset);
#else
    flock(fileno(file), LOCK_UN);
#endif
}

/* Compact in place under an OS lock: another game/bot may use the same files.
   Continuation lines (including native crash reports) follow their dated entry. */
static void cleanup(FILE *file, time_t now) {
    char cutoff[32], line[8192];
    long readAt = 0, writeAt = 0;
    int keep = 0, beginning = 1;
    stamp(now - KEEP_SECONDS, cutoff);
    rewind(file);
    while (fgets(line, sizeof(line), file)) {
        size_t n = strlen(line);
        readAt = ftell(file);
        if (readAt < 0) return;
        if (beginning && n >= 26 && line[0] == '[' && !memcmp(line + 20, " UTC] ", 6))
            keep = memcmp(line, cutoff, 26) >= 0;
        beginning = n > 0 && line[n - 1] == '\n';
        if (keep) {
            if (fseek(file, writeAt, SEEK_SET) || fwrite(line, 1, n, file) != n) return;
            writeAt += (long)n;
        }
        if (fseek(file, readAt, SEEK_SET)) return;
    }
    if (ferror(file) || fflush(file)) return;
#ifdef _WIN32
    _chsize_s(_fileno(file), writeAt);
#else
    if (ftruncate(fileno(file), writeAt)) return;
#endif
    clearerr(file);
}

static FILE *open_log(int online) {
    int fd;
    if (!sPath[online][0]) return NULL;
#ifdef _WIN32
    fd = _open(sPath[online], _O_CREAT | _O_RDWR | _O_BINARY | _O_NOINHERIT,
               _S_IREAD | _S_IWRITE);
    if (fd < 0) return NULL;
    FILE *file = _fdopen(fd, "r+b");
    if (!file) _close(fd);
#else
    fd = open(sPath[online], O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0) return NULL;
    FILE *file = fdopen(fd, "r+b");
    if (!file) close(fd);
#endif
    return file;
}

static void init_locked(const char *base) {
    char folder[4096];
    int i, n;
    time_t now = time(NULL);
    if (sInitialized) return;
    sInitialized = 1;
    sBotProcess = getenv("ZENKAIPOWER_NET_BOT") != NULL;
    if (sBotProcess) return;
    if (!base) base = SDL_GetBasePath();
    if (!base || !*base) return;
    n = snprintf(folder, sizeof(folder), "%s/logs", base);
    if (n < 0 || n >= (int)sizeof(folder)) return;
#ifdef _WIN32
    _mkdir(folder);
#else
    mkdir(folder, 0700);
#endif
    for (i = 0; i < 2; i++) {
        n = snprintf(sPath[i], sizeof(sPath[i]), "%s/%s.log", folder,
                     i ? "dragonnetbattle" : "zenkaipower");
        if (n < 0 || n >= (int)sizeof(sPath[i])) { sPath[i][0] = 0; continue; }
        FILE *file = open_log(i);
        if (!file) continue;
        if (file_lock(file)) {
            cleanup(file, now);
            sCleaned[i] = now;
            file_unlock(file);
        }
        fclose(file);
    }
}

void Port_LogInit(const char *base) {
    LOCK();
    init_locked(base);
    UNLOCK();
}

/* Also safe for a crash handler: no allocation, locks, or initialization. */
const char *Port_LogPath(int online) { return sPath[online != 0]; }

static int is_online(const char *source, const char *text) {
    const char *name = strrchr(source, '/');
    const char *backslash = strrchr(source, '\\');
    if (backslash && (!name || backslash > name)) name = backslash;
    name = name ? name + 1 : source;
    return !strncmp(name, "net", 3) || !strcmp(name, "state.c") ||
           !strncmp(text, "zenkaipower: net:", 9) || !strncmp(text, "zenkaipower: lobby:", 11) ||
           !strncmp(text, "relay:", 6) || !strncmp(text, "zenkaipower: online", 11);
}

static void record(const char *source, const char *text) {
    FILE *file;
    char date[32];
    int online = is_online(source, text), i, writeEntry = 1;
    time_t now = time(NULL);
    LOCK();
    init_locked(NULL);
    /* Console diagnostics remain intact; only the persistent files are filtered. */
    if (sBotProcess) {
        UNLOCK();
        return;
    }
    if (!strncmp(text, "zenkaipower: net: CPU opponent accepted", 31) ||
        !strncmp(text, "zenkaipower: net: playing as CPU", 24) ||
        !strncmp(text, "zenkaipower: bot login failed:", 22) ||
        !strcmp(text, "zenkaipower: stats: battle/get saved\n")) {
        writeEntry = 0;
    }
    if (!strncmp(text, "zenkaipower: online data:", 17) &&
        strstr(text, "required files present; binary signature ") &&
        strlen(text) < sizeof(sLastVerification)) {
        if (!strcmp(sLastVerification, text)) {
            writeEntry = 0;
        }
        snprintf(sLastVerification, sizeof(sLastVerification), "%s", text);
    }
    /* Clean both files even when only one category is producing messages. */
    for (i = 0; i < 2; i++) {
        if ((i != online || !writeEntry) && now - sCleaned[i] < 3600) continue;
        file = open_log(i);
        if (!file) continue;
        if (file_lock(file)) {
            if (now - sCleaned[i] >= 3600) {
                cleanup(file, now);
                sCleaned[i] = now;
            }
            if (i == online && writeEntry) {
                const char *at = text, *end;
                stamp(now, date);
                fseek(file, 0, SEEK_END);
                while (*at) {
                    end = strchr(at, '\n');
                    fputs(date, file);
                    fwrite(at, 1, end ? (size_t)(end - at) : strlen(at), file);
                    fputc('\n', file);
                    if (!end) break;
                    at = end + 1;
                }
                fflush(file);
            }
            file_unlock(file);
        }
        fclose(file);
    }
    UNLOCK();
}

int Port_LogVPrintf(const char *source, FILE *stream, const char *format, va_list args) {
    va_list copy;
    int result, n;
    char small[2048], *text = small;
    va_copy(copy, args);
    result = vfprintf(stream, format, copy);
    va_end(copy);
    if (stream != stdout && stream != stderr) return result;
    va_copy(copy, args);
    n = vsnprintf(small, sizeof(small), format, copy);
    va_end(copy);
    if (n < 0) return result;
    if (n >= (int)sizeof(small)) {
        text = malloc((size_t)n + 1);
        if (!text) return result;
        va_copy(copy, args);
        vsnprintf(text, (size_t)n + 1, format, copy);
        va_end(copy);
    }
    record(source, text);
    if (text != small) free(text);
    return result;
}

int Port_LogPrintf(const char *source, FILE *stream, const char *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    result = Port_LogVPrintf(source, stream, format, args);
    va_end(args);
    return result;
}

int Port_LogPuts(const char *source, const char *text, FILE *stream) {
    int result = fputs(text, stream);
    if (stream == stdout || stream == stderr) record(source, text);
    return result;
}

int Port_LogLine(const char *source, const char *text) {
    int result = puts(text);
    record(source, text);
    return result;
}

#if defined(PORT_DISTRIBUTION_BUILD) && PORT_DISTRIBUTION_BUILD
__attribute__((constructor(102))) static void log_start(void) {
    Port_LogInit(NULL);
    record("startup", "Game process started.");
}
#endif
