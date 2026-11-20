#ifndef PORT_LOCATIONS_H
#define PORT_LOCATIONS_H
#include <stdio.h>
#include <string.h>

/* Desktop setup writes absolute UTF-8 paths; an empty value means local.
 * Keep this reader independent of SDL so early game initialization can use it. */
static inline int Port_LocationRead(const char *root, const char *key, char *out, size_t size) {
    char path[4096], line[8192];
    FILE *file;
    out[0] = 0;
    if (snprintf(path, sizeof(path), "%slocations.ini", root) >= (int)sizeof(path)) return 0;
    file = fopen(path, "rb");
    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        size_t length;
        if (!equals) continue;
        *equals++ = 0;
        if (strcmp(line, key)) continue;
        length = strcspn(equals, "\r\n");
        if (length >= size) { fclose(file); return 0; }
        memcpy(out, equals, length); out[length] = 0;
        fclose(file);
        return 1;
    }
    fclose(file);
    return 0;
}
#endif
