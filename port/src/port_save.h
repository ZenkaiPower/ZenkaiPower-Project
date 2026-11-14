#ifndef PORT_SAVE_H
#define PORT_SAVE_H
/* Default player files share one directory. Explicit environment paths retain
 * their existing meaning, including the isolated online-session memory card. */
#include "../../include/sys/port_zenkai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#define SAVE_MKDIR(p) _mkdir(p)
#define SAVE_OPEN _open
#define SAVE_CLOSE _close
#define SAVE_FDOPEN _fdopen
#else
#include <unistd.h>
#define SAVE_MKDIR(p) mkdir(p, 0700)
#define SAVE_OPEN open
#define SAVE_CLOSE close
#define SAVE_FDOPEN fdopen
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static inline void Port_SaveMigrate(const char *old, const char *target) {
    FILE *src, *dst;
    char bytes[4096], temp[1200];
    size_t n;
    int fd, ok = 1;
    struct stat st;
    if (stat(target, &st) == 0 || !(src = fopen(old, "rb"))) return;
#ifdef _WIN32
    snprintf(temp, sizeof(temp), "%s.migrate.%lu.tmp", target, (unsigned long)_getpid());
#else
    snprintf(temp, sizeof(temp), "%s.migrate.%lu.tmp", target, (unsigned long)getpid());
#endif
    fd = SAVE_OPEN(temp, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600);
    if (fd < 0) { fclose(src); return; }
    dst = SAVE_FDOPEN(fd, "wb");
    if (!dst) { SAVE_CLOSE(fd); fclose(src); remove(temp); return; }
    while ((n = fread(bytes, 1, sizeof(bytes), src)) != 0)
        if (fwrite(bytes, 1, n, dst) != n) { ok = 0; break; }
    if (ferror(src)) ok = 0;
    fclose(src);
    if (fclose(dst) != 0) ok = 0;
    if (ok) {
#ifdef _WIN32
        /* Windows rename refuses an existing destination. */
        ok = rename(temp, target) == 0;
#else
        /* Publish a complete file without replacing an existing destination. */
        ok = link(temp, target) == 0;
#endif
        if (ok) remove(old);
    }
    remove(temp);
}

static inline void Port_SavePrepare(void) {
    SAVE_MKDIR("savegame");
}

static inline const char *Port_SaveUserFile(const char *name, const char *target) {
    Port_SavePrepare();
    Port_SaveMigrate(name, target);
    return target;
}

static inline void Port_SaveCardPath(char *out, size_t size, const char *name) {
    const char *root = getenv("ZENKAIPOWER_SAVES");
    if (root) {
        const char *data = "BASLUS-21678DBZT3/BASLUS-21678DBZT3";
        if (strcmp(name, data) == 0) {
            snprintf(out, size, "%s/card1/BASLUS-21678DBZT3/game.sav", root);
        } else {
            snprintf(out, size, "%s/card1/%s", root, name);
        }
    } else {
        const char *prefix = "BASLUS-21678DBZT3";
        Port_SavePrepare();
        if (strncmp(name, prefix, strlen(prefix)) == 0 && (name[strlen(prefix)] == '/' || name[strlen(prefix)] == 0)) {
            name += strlen(prefix);
            if (*name == '/') name++;
        }
        snprintf(out, size, "savegame%s%s", *name ? "/" : "",
                 strcmp(name, prefix) == 0 ? (Port_ZenkaiSelected() ? "zenkaipower.sav" : "game.sav") : name);
    }
}
#endif
