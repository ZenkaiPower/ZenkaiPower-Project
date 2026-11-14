/* Android archive indexes are shared by gameplay and verification. Each caller
 * owns a separate FILE/cursor; the SAF bridge reuses the underlying grant fd.
 * Indexes follow inode/size/mtime so replacement never reuses an old index. */
#if defined(__ANDROID__) || defined(PORT_ARCHIVE_TEST)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#define ZENKAIPOWER_ANDROID_INSTALLER 1 /* Use the raw header helpers in this implementation. */
#include "port_pack.h"
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

typedef struct ArchiveEntry {
    char *name;
    int64_t offset, size;
} ArchiveEntry;
typedef struct ArchiveIndex {
    char *path;
    struct stat stamp;
    ArchiveEntry *entries, **sorted;
    uint32_t count;
    unsigned long used;
    int named;
    FILE *source;
    uint64_t checked;
} ArchiveIndex;
static ArchiveIndex indexes[16];
static unsigned long clock_use;
static pthread_mutex_t index_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct ArchiveReader {
    struct ArchiveReader *next;
    FILE *stream;
    int fd;
    int64_t position,size;
} ArchiveReader;
static ArchiveReader *readers;
static pthread_mutex_t reader_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t now(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;
}
static ssize_t read_at(ArchiveReader *reader,void *out,size_t bytes) {
    ssize_t got;
    if (reader->position>=reader->size) return 0;
    if (bytes>(uint64_t)(reader->size-reader->position)) bytes=(size_t)(reader->size-reader->position);
    do { got=pread(reader->fd,out,bytes,(off_t)reader->position); } while (got<0 && errno==EINTR);
    if (got>0) reader->position+=got;
    return got;
}
static int64_t seek_to(ArchiveReader *reader,int64_t offset,int whence) {
    int64_t base=whence==SEEK_SET ? 0 : whence==SEEK_CUR ? reader->position : whence==SEEK_END ? reader->size : -1;
    if (base<0 || (offset>0 && base>INT64_MAX-offset) ||
        (offset<0 && (offset==INT64_MIN || base< -offset))) { errno=EINVAL; return -1; }
    return reader->position=base+offset;
}
static int reader_close(void *p) {
    ArchiveReader *reader=p,**link;
    pthread_mutex_lock(&reader_lock);
    for(link=&readers;*link && *link!=reader;link=&(*link)->next) { }
    if (*link) *link=reader->next;
    pthread_mutex_unlock(&reader_lock);
    int result=close(reader->fd); free(reader); return result;
}
#ifndef __BIONIC__
static ssize_t cookie_read(void *p,char *out,size_t n) { return read_at(p,out,n); }
static int cookie_seek(void *p,off64_t *at,int whence) {
    int64_t result=seek_to(p,*at,whence); if (result<0) return -1; *at=result; return 0;
}
#else
static int cookie_read(void *p,char *out,int n) { return (int)read_at(p,out,(size_t)n); }
static fpos_t cookie_seek(void *p,fpos_t at,int whence) { return (fpos_t)seek_to(p,(int64_t)at,whence); }
#endif
static FILE *reader_open(const ArchiveIndex *index) {
    ArchiveReader *reader=calloc(1,sizeof(*reader)); FILE *stream;
    if (!reader) return NULL;
    reader->fd=dup(fileno(index->source)); reader->size=index->stamp.st_size;
    if (reader->fd<0) { free(reader); return NULL; }
#ifndef __BIONIC__
    cookie_io_functions_t callbacks={cookie_read,NULL,cookie_seek,reader_close};
    stream=fopencookie(reader,"rb",callbacks);
#else
    stream=funopen(reader,cookie_read,NULL,cookie_seek,reader_close);
#endif
    if (!stream) { close(reader->fd); free(reader); return NULL; }
    reader->stream=stream;
    pthread_mutex_lock(&reader_lock); reader->next=readers; readers=reader; pthread_mutex_unlock(&reader_lock);
    return stream;
}
int Port_ArchiveFileStat(FILE *stream,struct stat *stamp) {
    ArchiveReader *reader; int result;
    pthread_mutex_lock(&reader_lock);
    for(reader=readers;reader && reader->stream!=stream;reader=reader->next) { }
    result=reader ? fstat(reader->fd,stamp) : fstat(fileno(stream),stamp);
    pthread_mutex_unlock(&reader_lock);
    return result;
}

static void discard(ArchiveIndex *index) {
    uint32_t i;
    for (i=0;i<index->count;i++) free(index->entries[i].name);
    free(index->entries); free(index->sorted); free(index->path);
    if (index->source) fclose(index->source);
    memset(index,0,sizeof(*index));
}
static int compare(const void *a,const void *b) {
    return strcmp((*(ArchiveEntry *const *)a)->name,(*(ArchiveEntry *const *)b)->name);
}
static int same(const ArchiveIndex *index,const struct stat *stamp) {
    return index->stamp.st_dev==stamp->st_dev && index->stamp.st_ino==stamp->st_ino &&
        index->stamp.st_size==stamp->st_size && index->stamp.st_mtim.tv_sec==stamp->st_mtim.tv_sec &&
        index->stamp.st_mtim.tv_nsec==stamp->st_mtim.tv_nsec;
}
static int load(ArchiveIndex *index,FILE *file,const char *path,const struct stat *stamp,int named) {
    unsigned char header[12],record[20];
    uint32_t i,count; size_t names=0;
    int headerSize=named ? 12 : 8;
    if (stamp->st_size<headerSize || fread(header,1,headerSize,file)!=(size_t)headerSize ||
        memcmp(header,named ? "BT3TEXT1" : "AFS\0",named ? 8 : 4)) return 0;
    count=Port_PackU32(header+(named ? 8 : 4));
    if (count>(named ? 100000u : 1000000u) || (uint64_t)headerSize+(uint64_t)count*(named ? 20 : 8)>(uint64_t)stamp->st_size) return 0;
    index->entries=calloc(count ? count : 1,sizeof(ArchiveEntry));
    index->path=strdup(path); index->stamp=*stamp; index->named=named;
    if (!index->entries || !index->path) return 0;
    index->count=count;
    for (i=0;i<count;i++) {
        ArchiveEntry *entry=&index->entries[i];
        uint64_t offset,size;
        if (fread(record,1,named ? 20 : 8,file)!=(size_t)(named ? 20 : 8)) return 0;
        offset=named ? Port_PackU64(record+4) : Port_PackU32(record);
        size=named ? Port_PackU64(record+12) : Port_PackU32(record+4);
        if (offset>(uint64_t)stamp->st_size || size>(uint64_t)stamp->st_size-offset ||
            (!named && offset<8+(uint64_t)count*8)) return 0;
        entry->offset=(int64_t)offset; entry->size=(int64_t)size;
        if (named) {
            uint32_t n=Port_PackU32(record),j;
            if (!n || n>=1200 || (names+=n+1)>64u*1024*1024) return 0;
            entry->name=malloc(n+1);
            if (!entry->name || fread(entry->name,1,n,file)!=n || memchr(entry->name,0,n)) return 0;
            entry->name[n]=0;
            for (j=0;j<n;j++) if (entry->name[j]=='\\') entry->name[j]='/';
        }
    }
    if (named) {
        index->sorted=malloc((count ? count : 1)*sizeof(*index->sorted));
        if (!index->sorted) return 0;
        for (i=0;i<count;i++) index->sorted[i]=&index->entries[i];
        qsort(index->sorted,count,sizeof(*index->sorted),compare);
    }
    index->source=file;
    index->checked=now();
    return 1;
}
FILE *Port_ArchiveLookup(const char *archive,const char *relative,unsigned long id,
                        int64_t *base,int64_t *length,int listIndex,char *listed,size_t capacity) {
    FILE *file=NULL,*opened=NULL;
    struct stat stamp; ArchiveIndex *index=NULL,*old=&indexes[0];
    ArchiveEntry *entry=NULL; unsigned i; int found=0;
    pthread_mutex_lock(&index_lock);
    for (i=0;i<16;i++) {
        ArchiveIndex *candidate=&indexes[i];
        if (candidate->path && !strcmp(candidate->path,archive) && candidate->named==(relative!=NULL)) {
            index=candidate; break;
        }
        if (!candidate->path || (old->path && candidate->used<old->used)) old=candidate;
    }
    if (!index) index=old;
    // In-place edits are visible immediately without querying the provider.
    // Replacements at the same path are detected by the periodic reopen below.
    if (index->source && (fstat(fileno(index->source),&stamp) || !same(index,&stamp))) index->checked=0;
    if (!index->path || strcmp(index->path,archive) || index->named!=(relative!=NULL) || now()-index->checked>=1000000000) {
        opened=fopen(archive,"rb");
        if (!opened || fstat(fileno(opened),&stamp)) goto done;
        if (index->path && !strcmp(index->path,archive) && index->named==(relative!=NULL) && same(index,&stamp)) {
            index->checked=now();
        } else {
            discard(index);
            if (!load(index,opened,archive,&stamp,relative!=NULL)) { discard(index); goto done; }
            opened=NULL; // The index owns the retained source stream.
        }
    }
    index->used=++clock_use;
    if (!relative) {
        if (id<index->count) entry=&index->entries[id];
    } else if (listIndex>=0) {
        size_t n=strlen(relative);
        for (i=0;i<index->count;i++) {
            const char *name=index->entries[i].name;
            if (!strncmp(name,relative,n) && name[n]=='/' && !strchr(name+n+1,'/') && listIndex--==0) {
                snprintf(listed,capacity,"%s",name+n+1); found=1; break;
            }
        }
    } else {
        ArchiveEntry key={(char *)relative,0,0},*pointer=&key;
        ArchiveEntry **match=bsearch(&pointer,index->sorted,index->count,sizeof(*index->sorted),compare);
        if (match) entry=*match;
    }
    if (entry) { *base=entry->offset; *length=entry->size; found=1; }
done:
    if (found) file=reader_open(index);
    if (opened) fclose(opened);
    pthread_mutex_unlock(&index_lock);
    if (file && (!entry || !Port_PackSeek(file,*base,SEEK_SET))) return file;
    if (file) fclose(file);
    return NULL;
}
#endif
