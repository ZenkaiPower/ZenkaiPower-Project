/* Folder-scoped SAF adapter. Linked into the engine, loader and installer with
 * --wrap; unrelated app/cache/system paths still use bionic directly. */
#define _GNU_SOURCE 1
#include <jni.h>
#include <SDL3/SDL.h>
#include <android/log.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static JavaVM *vm;
static jclass bridge;
static jmethodID stat_method, list_method, open_method, mkdir_method, delete_method, rename_method;
static char root[PATH_MAX], working[PATH_MAX];
static pthread_mutex_t paths_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t attached;
static pthread_once_t attached_once = PTHREAD_ONCE_INIT;
static void detach(void *p) { if (p && vm) (*vm)->DetachCurrentThread(vm); }
static void make_key(void) { pthread_key_create(&attached, detach); }
static JNIEnv *environment(void) {
    JNIEnv *env = NULL;
    if (!vm || !bridge) { errno = EACCES; return NULL; }
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) { errno = EIO; return NULL; }
        pthread_once(&attached_once, make_key);
        pthread_setspecific(attached, (void *)1);
    }
    return env;
}
static int failed(JNIEnv *env) {
    if (!(*env)->ExceptionCheck(env)) return 0;
    jthrowable error = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);
    jclass missing = (*env)->FindClass(env, "java/io/FileNotFoundException");
    jclass denied = (*env)->FindClass(env, "java/lang/SecurityException");
    errno = (*env)->IsInstanceOf(env, error, missing) ? ENOENT :
            (*env)->IsInstanceOf(env, error, denied) ? EACCES : EIO;
    (*env)->DeleteLocalRef(env, missing); (*env)->DeleteLocalRef(env, denied);
    (*env)->DeleteLocalRef(env, error);
    return 1;
}
/* Lexical normalization only: realpath would require a raw filesystem grant. */
static int normalize(const char *input, char out[PATH_MAX]) {
    char copy[PATH_MAX], *part, *save = NULL;
    size_t used = 0;
    if (strlen(input) >= sizeof(copy)) { errno = ENAMETOOLONG; return -1; }
    strcpy(copy, input); out[0] = '/'; out[1] = 0;
    for (part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) {
            while (used && out[used] != '/') used--;
            out[used] = 0;
            continue;
        }
        size_t length = strlen(part);
        if (used + length + 2 >= PATH_MAX) { errno = ENAMETOOLONG; return -1; }
        out[used++] = '/'; memcpy(out + used, part, length + 1); used += length;
    }
    if (!used) strcpy(out, "/");
    return 0;
}
/* 1: selected tree; 0: normal OS path; -1: malformed/too long. */
static int mapped(const char *path, char out[PATH_MAX]) {
    char full[PATH_MAX];
    if (!path || !*path) { errno = ENOENT; return -1; }
    pthread_mutex_lock(&paths_lock);
    if (!root[0]) { pthread_mutex_unlock(&paths_lock); return 0; }
    if (path[0] == '/') {
        if (strlen(path) >= sizeof(full)) { pthread_mutex_unlock(&paths_lock); errno = ENAMETOOLONG; return -1; }
        strcpy(full, path);
    } else if (working[0]) {
        if (snprintf(full, sizeof(full), "%s/%s", working, path) >= (int)sizeof(full)) {
            pthread_mutex_unlock(&paths_lock); errno = ENAMETOOLONG; return -1;
        }
    } else { pthread_mutex_unlock(&paths_lock); return 0; }
    int result = normalize(full, out);
    size_t length = strlen(root);
    if (!result) result = !strncmp(out, root, length) && (out[length] == 0 || out[length] == '/');
    pthread_mutex_unlock(&paths_lock);
    return result;
}
static int integer_call(const char *path, jmethodID method, int argument, int has_argument) {
    JNIEnv *env = environment(); if (!env) return -1;
    jstring name = (*env)->NewStringUTF(env, path);
    int result = has_argument ? (*env)->CallStaticIntMethod(env, bridge, method, name, argument)
                             : (*env)->CallStaticIntMethod(env, bridge, method, name);
    (*env)->DeleteLocalRef(env, name);
    if (failed(env)) return -1;
    if (result < 0) { errno = -result; return -1; }
    return result;
}
int __wrap_open(const char *path, int flags, ...) {
    mode_t mode = 0600;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    char name[PATH_MAX]; int match = mapped(path, name);
    if (match < 0) return -1;
    if (!match) return open(path, flags, mode);
    if (flags & O_DIRECTORY) { errno = ENOTSUP; return -1; }
    int fd = integer_call(name, open_method, flags, 1);
    if (fd >= 0 && (flags & O_APPEND)) {
        int current = fcntl(fd, F_GETFL);
        if (current < 0 || fcntl(fd, F_SETFL, current | O_APPEND)) {
            int saved = errno; close(fd); errno = saved; return -1;
        }
    }
    return fd;
}
int __wrap_open64(const char *path, int flags, ...) {
    mode_t mode = 0600;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return __wrap_open(path, flags, mode);
}
int __wrap___open_2(const char *path, int flags) { return __wrap_open(path, flags); }
int __wrap___open64_2(const char *path, int flags) { return __wrap_open(path, flags); }
FILE *__wrap_fopen(const char *path, const char *mode) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (match < 0) return NULL;
    if (!match) return fopen(path, mode);
    int flags;
    if (mode[0] == 'r') flags = O_RDONLY;
    else if (mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') flags = O_WRONLY | O_CREAT | O_APPEND;
    else { errno = EINVAL; return NULL; }
    if (strchr(mode, '+')) flags = (flags & ~O_ACCMODE) | O_RDWR;
    if (strchr(mode, 'x')) flags |= O_EXCL;
    int fd = __wrap_open(name, flags | O_CLOEXEC, 0600);
    if (fd < 0) return NULL;
    FILE *file = fdopen(fd, mode);
    if (!file) { int saved = errno; close(fd); errno = saved; }
    return file;
}
FILE *__wrap_fopen64(const char *path, const char *mode) { return __wrap_fopen(path, mode); }
int __wrap_stat(const char *path, struct stat *info) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (match < 0) return -1;
    if (!match) return stat(path, info);
    JNIEnv *env = environment(); if (!env) return -1;
    jstring value = (*env)->NewStringUTF(env, name);
    jlongArray array = (*env)->CallStaticObjectMethod(env, bridge, stat_method, value);
    (*env)->DeleteLocalRef(env, value);
    if (failed(env)) return -1;
    if (!array) { errno = ENOENT; return -1; }
    jlong fields[3]; (*env)->GetLongArrayRegion(env, array, 0, 3, fields);
    (*env)->DeleteLocalRef(env, array);
    memset(info, 0, sizeof(*info));
    info->st_mode = (fields[0] ? S_IFDIR | 0700 : S_IFREG | 0600);
    info->st_size = fields[1]; info->st_mtime = fields[2];
    info->st_nlink = 1; info->st_blksize = 4096; info->st_blocks = (fields[1] + 511) / 512;
    info->st_uid = getuid(); info->st_gid = getgid();
    return 0;
}
int __wrap_stat64(const char *path, struct stat64 *info) { return __wrap_stat(path, (struct stat *)info); }
int __wrap_lstat(const char *path, struct stat *info) {
    char name[PATH_MAX]; int match = mapped(path, name);
    return match ? (match < 0 ? -1 : __wrap_stat(path, info)) : lstat(path, info);
}
int __wrap_lstat64(const char *path, struct stat64 *info) { return __wrap_lstat(path, (struct stat *)info); }
int __wrap_access(const char *path, int mode) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return access(path, mode);
    if (match < 0) return -1;
    if (mode & X_OK) { errno = EACCES; return -1; }
    struct stat info; return __wrap_stat(path, &info);
}
int __wrap_mkdir(const char *path, mode_t mode) {
    char name[PATH_MAX]; int match = mapped(path, name);
    return match < 0 ? -1 : match ? integer_call(name, mkdir_method, 0, 0) : mkdir(path, mode);
}
int __wrap_unlink(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    return match < 0 ? -1 : match ? integer_call(name, delete_method, JNI_FALSE, 1) : unlink(path);
}
int __wrap_rmdir(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    return match < 0 ? -1 : match ? integer_call(name, delete_method, JNI_TRUE, 1) : rmdir(path);
}
int __wrap_remove(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return remove(path);
    if (match < 0) return -1;
    struct stat info;
    if (__wrap_stat(path, &info)) return -1;
    return S_ISDIR(info.st_mode) ? __wrap_rmdir(path) : __wrap_unlink(path);
}
int __wrap_rename(const char *from, const char *to) {
    char a[PATH_MAX], b[PATH_MAX]; int ma = mapped(from, a), mb = mapped(to, b);
    if (ma < 0 || mb < 0) return -1;
    if (!ma && !mb) return rename(from, to);
    if (!ma || !mb) { errno = EXDEV; return -1; }
    JNIEnv *env = environment(); if (!env) return -1;
    jstring source = (*env)->NewStringUTF(env, a), target = (*env)->NewStringUTF(env, b);
    int result = (*env)->CallStaticIntMethod(env, bridge, rename_method, source, target);
    (*env)->DeleteLocalRef(env, source); (*env)->DeleteLocalRef(env, target);
    if (failed(env)) return -1;
    if (result < 0) { errno = -result; return -1; }
    return 0;
}
/* Never pass an emulated DIR to libc. The registry also supports native DIRs. */
typedef struct TreeDir {
    struct TreeDir *next;
    char **names; size_t count, position;
    struct dirent entry;
} TreeDir;
static TreeDir *directories;
static pthread_mutex_t directory_lock = PTHREAD_MUTEX_INITIALIZER;
DIR *__wrap_opendir(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return opendir(path);
    if (match < 0) return NULL;
    struct stat info;
    if (__wrap_stat(path, &info)) return NULL;
    if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return NULL; }
    JNIEnv *env = environment(); if (!env) return NULL;
    jstring value = (*env)->NewStringUTF(env, name);
    jobjectArray array = (*env)->CallStaticObjectMethod(env, bridge, list_method, value);
    (*env)->DeleteLocalRef(env, value);
    if (failed(env) || !array) return NULL;
    TreeDir *dir = calloc(1, sizeof(*dir));
    if (!dir) { (*env)->DeleteLocalRef(env, array); errno = ENOMEM; return NULL; }
    dir->count = (size_t)(*env)->GetArrayLength(env, array) + 2;
    dir->names = calloc(dir->count, sizeof(char *));
    if (!dir->names) { free(dir); (*env)->DeleteLocalRef(env, array); errno = ENOMEM; return NULL; }
    dir->names[0] = strdup("."); dir->names[1] = strdup("..");
    for (size_t i = 2; i < dir->count; i++) {
        jstring item = (*env)->GetObjectArrayElement(env, array, (jsize)i - 2);
        const char *text = (*env)->GetStringUTFChars(env, item, NULL);
        dir->names[i] = text ? strdup(text) : NULL;
        if (text) (*env)->ReleaseStringUTFChars(env, item, text);
        (*env)->DeleteLocalRef(env, item);
    }
    (*env)->DeleteLocalRef(env, array);
    for (size_t i = 0; i < dir->count; i++) if (!dir->names[i]) {
        for (size_t j = 0; j < dir->count; j++) free(dir->names[j]);
        free(dir->names); free(dir); errno = ENOMEM; return NULL;
    }
    pthread_mutex_lock(&directory_lock); dir->next = directories; directories = dir; pthread_mutex_unlock(&directory_lock);
    return (DIR *)dir;
}
struct dirent *__wrap_readdir(DIR *opaque) {
    pthread_mutex_lock(&directory_lock);
    TreeDir *dir; for (dir = directories; dir && (DIR *)dir != opaque; dir = dir->next) { }
    if (!dir) { pthread_mutex_unlock(&directory_lock); return readdir(opaque); }
    if (dir->position == dir->count) { pthread_mutex_unlock(&directory_lock); return NULL; }
    const char *name = dir->names[dir->position++];
    memset(&dir->entry, 0, sizeof(dir->entry));
    dir->entry.d_ino = dir->position; dir->entry.d_off = dir->position; dir->entry.d_type = DT_UNKNOWN;
    dir->entry.d_reclen = sizeof(dir->entry);
    snprintf(dir->entry.d_name, sizeof(dir->entry.d_name), "%s", name);
    struct dirent *result = &dir->entry;
    pthread_mutex_unlock(&directory_lock); return result;
}
struct dirent64 *__wrap_readdir64(DIR *dir) { return (struct dirent64 *)__wrap_readdir(dir); }
int __wrap_closedir(DIR *opaque) {
    pthread_mutex_lock(&directory_lock);
    TreeDir **link = &directories; while (*link && (DIR *)*link != opaque) link = &(*link)->next;
    TreeDir *dir = *link;
    if (dir) *link = dir->next;
    pthread_mutex_unlock(&directory_lock);
    if (!dir) return closedir(opaque);
    for (size_t i = 0; i < dir->count; i++) free(dir->names[i]);
    free(dir->names); free(dir); return 0;
}
void __wrap_rewinddir(DIR *opaque) {
    pthread_mutex_lock(&directory_lock);
    TreeDir *dir; for (dir = directories; dir && (DIR *)dir != opaque; dir = dir->next) { }
    if (dir) dir->position = 0;
    pthread_mutex_unlock(&directory_lock);
    if (!dir) rewinddir(opaque);
}
int __wrap_chdir(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (match < 0) return -1;
    if (match) {
        struct stat info; if (__wrap_stat(path, &info)) return -1;
        if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return -1; }
    } else if (chdir(path)) return -1;
    pthread_mutex_lock(&paths_lock); strcpy(working, match ? name : ""); pthread_mutex_unlock(&paths_lock);
    return 0;
}
char *__wrap_getcwd(char *buffer, size_t size) {
    char cwd[PATH_MAX];
    pthread_mutex_lock(&paths_lock); strcpy(cwd, working); pthread_mutex_unlock(&paths_lock);
    if (!cwd[0]) return getcwd(buffer, size);
    size_t needed = strlen(cwd) + 1;
    if (!buffer) { size = size ? size : needed; buffer = malloc(size); if (!buffer) return NULL; }
    else if (size < needed) { errno = ERANGE; return NULL; }
    if (size < needed) { free(buffer); errno = ERANGE; return NULL; }
    memcpy(buffer, cwd, needed); return buffer;
}
char *__wrap_realpath(const char *path, char *resolved) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return realpath(path, resolved);
    if (match < 0) return NULL;
    struct stat info; if (__wrap_stat(path, &info)) return NULL;
    if (!resolved) return strdup(name);
    strcpy(resolved, name); return resolved;
}

JNIEXPORT void JNICALL Java_com_zenkaipower_android_GameStorage_nativeConfigure(
        JNIEnv *env, jclass type, jstring path) {
    (void)type;
    (*env)->GetJavaVM(env, &vm);
    if (!bridge) {
        jclass local = (*env)->FindClass(env, "com/zenkaipower/android/TreeStorage");
        bridge = (*env)->NewGlobalRef(env, local); (*env)->DeleteLocalRef(env, local);
        stat_method = (*env)->GetStaticMethodID(env, bridge, "stat", "(Ljava/lang/String;)[J");
        list_method = (*env)->GetStaticMethodID(env, bridge, "list", "(Ljava/lang/String;)[Ljava/lang/String;");
        open_method = (*env)->GetStaticMethodID(env, bridge, "open", "(Ljava/lang/String;I)I");
        mkdir_method = (*env)->GetStaticMethodID(env, bridge, "mkdir", "(Ljava/lang/String;)I");
        delete_method = (*env)->GetStaticMethodID(env, bridge, "delete", "(Ljava/lang/String;Z)I");
        rename_method = (*env)->GetStaticMethodID(env, bridge, "rename", "(Ljava/lang/String;Ljava/lang/String;)I");
    }
    const char *text = path ? (*env)->GetStringUTFChars(env, path, NULL) : "";
    pthread_mutex_lock(&paths_lock);
    if (strcmp(root, text)) {
        snprintf(root, sizeof(root), "%s", text);
        working[0] = 0;
    }
    pthread_mutex_unlock(&paths_lock);
    if (path) (*env)->ReleaseStringUTFChars(env, path, text);
}
JNIEXPORT jint JNICALL Java_com_zenkaipower_android_GameStorage_nativeOpen(JNIEnv *env, jclass type, jstring path, jint flags) {
    (void)type; const char *text = (*env)->GetStringUTFChars(env, path, NULL);
    int fd = __wrap_open(text, flags, 0600), saved = errno;
    (*env)->ReleaseStringUTFChars(env, path, text); return fd < 0 ? -saved : fd;
}
JNIEXPORT jlongArray JNICALL Java_com_zenkaipower_android_GameStorage_nativeStat(JNIEnv *env, jclass type, jstring path) {
    (void)type; const char *text = (*env)->GetStringUTFChars(env, path, NULL);
    struct stat info; int result = __wrap_stat(text, &info);
    (*env)->ReleaseStringUTFChars(env, path, text);
    if (result) return NULL;
    jlong fields[] = {S_ISDIR(info.st_mode), info.st_size, info.st_mtime};
    jlongArray array = (*env)->NewLongArray(env, 3); (*env)->SetLongArrayRegion(env, array, 0, 3, fields); return array;
}
JNIEXPORT jobjectArray JNICALL Java_com_zenkaipower_android_GameStorage_nativeList(JNIEnv *env, jclass type, jstring path) {
    (void)type;
    return (*env)->CallStaticObjectMethod(env, bridge, list_method, path);
}
JNIEXPORT jint JNICALL Java_com_zenkaipower_android_GameStorage_nativeChange(JNIEnv *env, jclass type, jstring path, jint operation) {
    (void)type; const char *text = (*env)->GetStringUTFChars(env, path, NULL);
    int result = operation == 0 ? __wrap_mkdir(text, 0700) : operation == 1 ? __wrap_unlink(text) : __wrap_rmdir(text);
    int saved = errno; (*env)->ReleaseStringUTFChars(env, path, text); return result < 0 ? -saved : 0;
}
JNIEXPORT jboolean JNICALL Java_com_zenkaipower_android_GameStorage_nativeRename(JNIEnv *env, jclass type, jstring from, jstring to) {
    (void)type; const char *a = (*env)->GetStringUTFChars(env, from, NULL), *b = (*env)->GetStringUTFChars(env, to, NULL);
    int result = __wrap_rename(a, b);
    (*env)->ReleaseStringUTFChars(env, from, a); (*env)->ReleaseStringUTFChars(env, to, b); return result == 0;
}

static Sint64 io_size(void *p) {
    struct stat info;
    int fd = fileno(p), flags = fcntl(fd, F_GETFL);
    /* Bionic rejects fflush on read-only streams. Archive readers query size
       before reading, so flushing an rb stream aborts installation here. */
    if (flags < 0 || ((flags & O_ACCMODE) != O_RDONLY && fflush(p))) return -1;
    return fstat(fd, &info) ? -1 : info.st_size;
}
static Sint64 io_seek(void *p, Sint64 offset, SDL_IOWhence whence) {
    int origin = whence == SDL_IO_SEEK_SET ? SEEK_SET : whence == SDL_IO_SEEK_CUR ? SEEK_CUR : SEEK_END;
    return fseeko(p, offset, origin) ? -1 : ftello(p);
}
static size_t io_read(void *p, void *out, size_t size, SDL_IOStatus *status) {
    size_t count = fread(out, 1, size, p);
    if (count < size) *status = ferror(p) ? SDL_IO_STATUS_ERROR : SDL_IO_STATUS_EOF;
    return count;
}
static size_t io_write(void *p, const void *in, size_t size, SDL_IOStatus *status) {
    size_t count = fwrite(in, 1, size, p);
    if (count < size) *status = SDL_IO_STATUS_ERROR;
    return count;
}
static bool io_flush(void *p, SDL_IOStatus *status) {
    if (fflush(p)) { *status = SDL_IO_STATUS_ERROR; return false; }
    return true;
}
static bool io_close(void *p) { return fclose(p) == 0; }
SDL_IOStream *__wrap_SDL_IOFromFile(const char *path, const char *mode) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_IOFromFile(path, mode);
    if (match < 0) return NULL;
    FILE *file = __wrap_fopen(path, mode);
    if (!file) { SDL_SetError("Cannot open game file: %s", strerror(errno)); return NULL; }
    SDL_IOStreamInterface callbacks; SDL_INIT_INTERFACE(&callbacks);
    callbacks.size = io_size; callbacks.seek = io_seek; callbacks.read = io_read;
    callbacks.write = io_write; callbacks.flush = io_flush; callbacks.close = io_close;
    SDL_IOStream *io = SDL_OpenIO(&callbacks, file);
    if (!io) fclose(file);
    return io;
}
void *__wrap_SDL_LoadFile(const char *path, size_t *size) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_LoadFile(path, size);
    if (match < 0) return NULL;
    return SDL_LoadFile_IO(__wrap_SDL_IOFromFile(path, "rb"), size, true);
}
bool __wrap_SDL_GetPathInfo(const char *path, SDL_PathInfo *info) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_GetPathInfo(path, info);
    if (info) memset(info, 0, sizeof(*info));
    struct stat st;
    if (match < 0 || __wrap_stat(path, &st)) return SDL_SetError("Cannot inspect game path: %s", strerror(errno));
    if (info) {
        info->type = S_ISDIR(st.st_mode) ? SDL_PATHTYPE_DIRECTORY : SDL_PATHTYPE_FILE;
        info->size = st.st_size;
        info->modify_time = (SDL_Time)st.st_mtime * 1000000000;
        info->access_time = info->modify_time; info->create_time = info->modify_time;
    }
    return true;
}
bool __wrap_SDL_CreateDirectory(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_CreateDirectory(path);
    if (match < 0) return false;
    for (char *p = name + 1; ; p++) {
        if (*p && *p != '/') continue;
        char save = *p; *p = 0;
        char check[PATH_MAX];
        if (mapped(name, check) == 1) {
            struct stat info;
            if (__wrap_stat(name, &info)) {
                if (__wrap_mkdir(name, 0700) && errno != EEXIST) { *p = save; return SDL_SetError("Cannot create game folder: %s", strerror(errno)); }
            } else if (!S_ISDIR(info.st_mode)) { *p = save; return SDL_SetError("Game path is not a directory."); }
        }
        *p = save; if (!save) break;
    }
    return true;
}
bool __wrap_SDL_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback callback, void *userdata) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_EnumerateDirectory(path, callback, userdata);
    if (match < 0) return false;
    DIR *dir = __wrap_opendir(path);
    if (!dir) return SDL_SetError("Cannot list game folder: %s", strerror(errno));
    size_t length = strlen(path);
    char prefix[PATH_MAX];
    if (snprintf(prefix, sizeof(prefix), "%s%s", path, length && path[length - 1] == '/' ? "" : "/") >= (int)sizeof(prefix)) {
        __wrap_closedir(dir); return SDL_SetError("Game path is too long.");
    }
    bool ok = true; struct dirent *entry;
    while ((entry = __wrap_readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        SDL_EnumerationResult result = callback(userdata, prefix, entry->d_name);
        if (result == SDL_ENUM_FAILURE) { ok = false; break; }
        if (result == SDL_ENUM_SUCCESS) break;
    }
    __wrap_closedir(dir); return ok;
}
bool __wrap_SDL_RenamePath(const char *from, const char *to) {
    char a[PATH_MAX], b[PATH_MAX]; int ma = mapped(from, a), mb = mapped(to, b);
    if (!ma && !mb) return SDL_RenamePath(from, to);
    return __wrap_rename(from, to) == 0 || SDL_SetError("Cannot rename game file: %s", strerror(errno));
}
bool __wrap_SDL_RemovePath(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_RemovePath(path);
    if (match < 0) return false;
    if (__wrap_remove(path) == 0 || errno == ENOENT) return true;
    return SDL_SetError("Cannot remove game path: %s", strerror(errno));
}
SDL_Surface *__wrap_SDL_LoadPNG(const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_LoadPNG(path);
    if (match < 0) return NULL;
    return SDL_LoadPNG_IO(__wrap_SDL_IOFromFile(path, "rb"), true);
}
bool __wrap_SDL_SavePNG(SDL_Surface *surface, const char *path) {
    char name[PATH_MAX]; int match = mapped(path, name);
    if (!match) return SDL_SavePNG(surface, path);
    if (match < 0) return false;
    return SDL_SavePNG_IO(surface, __wrap_SDL_IOFromFile(path, "wb"), true);
}
