#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <SDL3/SDL.h>

static const char *check(const char *root) {
    char path[4096], target[4096], saved[4096], bytes[8] = {0};
    if (!getcwd(saved, sizeof(saved)) || chdir(root)) return "Cannot enter the granted folder.";
    if (!getcwd(path, sizeof(path)) || strcmp(path, root)) return "Virtual working directory differs from the selected folder.";
    FILE *file = fopen("native.part", "wb");
    if (!file || fwrite("native", 1, 6, file) != 6 || fclose(file)) return "Native write failed.";
    if (rename("native.part", "native.bin")) return "Native publication failed.";
    snprintf(target, sizeof(target), "%s/native.bin", root);
    int fd = open(target, O_RDONLY);
    if (fd < 0 || pread(fd, bytes, 3, 2) != 3 || memcmp(bytes, "tiv", 3)) return "Native descriptor reads failed.";
    close(fd);
    struct stat info;
    if (stat("native.bin", &info) || info.st_size != 6) return "Native metadata failed.";
    DIR *dir = opendir("."); int seen = 0; struct dirent *entry;
    if (!dir) return "Native directory listing failed.";
    while ((entry = readdir(dir))) if (!strcmp(entry->d_name, "native.bin")) seen = 1;
    closedir(dir);
    if (!seen) return "Native listing omitted a game file.";
    SDL_IOStream *io = SDL_IOFromFile("native.bin", "rb");
    if (!io || SDL_GetIOSize(io) != 6) return "SDL read-only archive sizing failed.";
    memset(bytes, 0, sizeof(bytes));
    if (!io || SDL_SeekIO(io, 1, SDL_IO_SEEK_SET) != 1 || SDL_ReadIO(io, bytes, 5) != 5 || strcmp(bytes, "ative"))
        return "SDL read/seek failed.";
    if (SDL_GetIOSize(io) != 6 || SDL_TellIO(io) != 6) return "SDL archive sizing changed its position.";
    SDL_CloseIO(io);
    io = SDL_IOFromFile("native-size.part", "wb");
    if (!io || SDL_WriteIO(io, "native", 6) != 6 || SDL_GetIOSize(io) != 6)
        return "SDL buffered output sizing failed.";
    SDL_CloseIO(io);
    if (!SDL_RemovePath("native-size.part")) return "Cannot remove sizing fixture.";
    // Native optional assets and touch artwork publish a sibling temporary pak.
    // Exercise both first installation and replacement through the SAF bridge.
    for (int version = 0; version < 2; ++version) {
        io = SDL_IOFromFile("touch-buttons.pak.part-test", "wb");
        char value = 'a' + version;
        if (!io || SDL_WriteIO(io, &value, 1) != 1 || !SDL_CloseIO(io))
            return "Cannot stage native package.";
        if (!SDL_RenamePath("touch-buttons.pak.part-test", "touch-buttons.pak"))
            return "Cannot publish native package.";
        io = SDL_IOFromFile("touch-buttons.pak", "rb");
        if (!io || SDL_GetIOSize(io) != 1 || SDL_ReadIO(io, bytes, 1) != 1 || bytes[0] != value)
            return "Published native package differs from staged content.";
        SDL_CloseIO(io);
    }
    if (!SDL_RemovePath("touch-buttons.pak")) return "Cannot remove native package fixture.";
    SDL_PathInfo sdl;
    if (!SDL_GetPathInfo("native.bin", &sdl) || sdl.type != SDL_PATHTYPE_FILE || sdl.size != 6) return "SDL metadata failed.";
    size_t length = 0; char *loaded = SDL_LoadFile("native.bin", &length);
    if (!loaded || length != 6 || memcmp(loaded, "native", 6)) return "SDL file loading failed.";
    SDL_free(loaded);
    if (!SDL_CreateDirectory("nested/child")) return "SDL recursive folder creation failed.";
    if (!SDL_RemovePath("nested/child") || !SDL_RemovePath("nested")) return "SDL folder removal failed.";
    if (!SDL_RenamePath("native.bin", "native.renamed") || !SDL_RemovePath("native.renamed")) return "SDL rename/removal failed.";
    if (chdir(saved)) return "Cannot restore working directory.";
    return "";
}
JNIEXPORT jstring JNICALL Java_com_zenkaipower_android_StorageChecks_nativeCheck(JNIEnv *env, jclass type, jstring path) {
    (void)type;
    const char *root = (*env)->GetStringUTFChars(env, path, NULL);
    const char *failure = check(root);
    (*env)->ReleaseStringUTFChars(env, path, root);
    return (*env)->NewStringUTF(env, failure);
}
