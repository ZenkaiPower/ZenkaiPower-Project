#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
/* Run in an empty directory. Uses sparse archives, including offsets near 4 GiB.
 * cc -std=gnu99 -D_FILE_OFFSET_BITS=64 port/tests/files/afs.c port/src/plat_fcache.c -o /tmp/afs-test
 * Also builds with x86_64-w64-mingw32-gcc for execution on Windows. */
#include "../../src/port_pack.h"
#include <assert.h>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

const char *Port_FileRoot(void) { return "."; }
int PortStages_Alias(const char *r, char *o, unsigned n) { (void)r; (void)o; (void)n; return 0; }
int PortSongs_Alias(const char *r, char *o, unsigned n) { (void)r; (void)o; (void)n; return 0; }
int Port_StageFits(const char *r, long n) { (void)r; (void)n; return 1; }
int Port_StageBufSize(void) { return 0; }
void Port_StateTouch(void *p, unsigned long n) { (void)p; (void)n; }
extern long Port_FileSize(const char *);
extern size_t Port_FileReadAt(const char *, long, void *, size_t);

static void word(FILE *f, uint32_t v) {
    unsigned char b[4] = {v, v >> 8, v >> 16, v >> 24};
    assert(fwrite(b, 1, 4, f) == 4);
}
static void archive(const char *name, uint32_t offset, int malformed) {
    FILE *f = fopen(name, "wb"); assert(f);
#ifdef _WIN32
    DWORD returned;
    assert(DeviceIoControl((HANDLE)_get_osfhandle(_fileno(f)), FSCTL_SET_SPARSE,
                           NULL, 0, NULL, 0, &returned, NULL));
#endif
    assert(fwrite("AFS\0", 1, 4, f) == 4);
    word(f, 2); word(f, offset); word(f, malformed ? 100 : 8);
    word(f, 24); word(f, 4);
    assert(fwrite("HEAD", 1, 4, f) == 4);
    assert(Port_PackSeek(f, offset, SEEK_SET) == 0);
    assert(fwrite("PAYLOAD!", 1, 8, f) == 8);
    assert(fclose(f) == 0);
}
int main(void) {
#ifdef _WIN32
    assert(CreateDirectoryW(L"archive temp \x00e9",NULL));
    assert(!_wputenv_s(L"TMP",L".\\archive temp \x00e9"));
    FILE *temporary=Port_PackTempFile(); assert(temporary);
    wchar_t temporaryPath[32768];
    DWORD pathLength=GetFinalPathNameByHandleW((HANDLE)_get_osfhandle(_fileno(temporary)),
                                             temporaryPath,32768,FILE_NAME_NORMALIZED);
    assert(pathLength>0 && pathLength<32768);
    assert(wcsstr(temporaryPath,L"archive temp \x00e9")!=NULL);
    assert(fwrite("test",1,4,temporary)==4);
    assert(!fclose(temporary));
    assert(GetFileAttributesW(temporaryPath)==INVALID_FILE_ATTRIBUTES);
#endif
    const uint32_t offsets[] = {32, 0x7ffffffcU, 0x80000010U, 0xfffffff0U};
    for (unsigned i = 0; i < sizeof(offsets)/sizeof(offsets[0]); i++) {
        char name[64], path[64], rel[64], bytes[16] = {0};
        int64_t base, size; FILE *f;
        snprintf(name, sizeof(name), "test%u.afs", i);
        snprintf(path, sizeof(path), "./test%u/00000.bin", i);
        snprintf(rel, sizeof(rel), "test%u/00000.bin", i);
        archive(name, offsets[i], 0);
        f = Port_PackOpen(path, &base, &size);
        assert(f && base == offsets[i] && size == 8);
        assert(fread(bytes, 1, 8, f) == 8 && !memcmp(bytes, "PAYLOAD!", 8)); fclose(f);
        struct stat st;
        assert(Port_PackStat(path, &st) == 0 && st.st_size == 8);
        f = Port_PackFopen(path, "rb"); assert(f);
        assert(fread(bytes, 1, sizeof(bytes), f) == 8 && fgetc(f) == EOF); fclose(f);
        assert(Port_FileSize(rel) == 8);
        assert(Port_FileReadAt(rel, 2, bytes, sizeof(bytes)) == 6 && !memcmp(bytes, "YLOAD!", 6));
        assert(Port_FileReadAt(rel, -1, bytes, 1) == 0);
        assert(Port_FileReadAt(rel, 8, bytes, 1) == 0);
        assert(Port_FileReadAt(rel, 0, bytes, 4) == 4 && !memcmp(bytes, "PAYL", 4));
        snprintf(path, sizeof(path), "./test%u/00001.bin", i);
        f = Port_PackOpen(path, &base, &size);
        assert(f && base == 24 && size == 4);
        assert(fread(bytes, 1, 4, f) == 4 && !memcmp(bytes, "HEAD", 4)); fclose(f);
        snprintf(path, sizeof(path), "./test%u/00002.bin", i);
        assert(!Port_PackOpen(path, &base, &size));
    }
    archive("bad.afs", 32, 1);
    int64_t base, size;
    assert(!Port_PackOpen("./bad/00000.bin", &base, &size));
    /* Older Windows packers emitted backslashes in virtual resource names. */
    FILE *named = fopen("disc.pak", "wb"); assert(named);
    assert(fwrite("BT3TEXT1", 1, 8, named) == 8);
    word(named, 1); word(named, 12);
    word(named, 44); word(named, 0); word(named, 4); word(named, 0);
    assert(fwrite("BIN\\DBZP.BIN", 1, 12, named) == 12);
    assert(fwrite("MENU", 1, 4, named) == 4); fclose(named);
    named = Port_PackOpen("./disc/BIN/DBZP.BIN", &base, &size);
    assert(named && base == 44 && size == 4);
    char menu[4];
    assert(fread(menu, 1, 4, named) == 4 && !memcmp(menu, "MENU", 4)); fclose(named);
    puts("PASS: AFS reads and cache seeks across 2 GiB and near 4 GiB; bounds and invalid entries");
    return 0;
}
