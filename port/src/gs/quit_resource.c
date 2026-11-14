/* Read quit artwork without touching the game's file handles or request queues. */
#include "../port_pack.h"
extern int Port_FilePath(int partition, int index, const char *name, char *out, int size);

int PortQuit_ReadArchive(int partition, int index, void *dst, int capacity) {
    char path[1200];
    struct stat st;
    FILE *file;
    int size;
    if (!Port_FilePath(partition, index, NULL, path, sizeof(path)) ||
        Port_PackStat(path, &st) != 0 || st.st_size <= 0 || st.st_size > INT_MAX) return -1;
    size = (int)st.st_size;
    if (dst == NULL) return size;
    if (size > capacity) return -1;
    file = Port_PackFopen(path, "rb");
    if (file == NULL) return -1;
    if (fread(dst, 1, size, file) != (size_t)size) size = -1;
    fclose(file);
    return size;
}
