/* gcc -std=gnu11 -pthread port/tests/files/archive_cache.c -o /tmp/archive-cache
 * Run in an empty temporary directory. */
#define PORT_ARCHIVE_TEST 1
#include "../../src/port_archive.c"
#include <assert.h>

static void word(FILE *f,uint64_t n,int bytes) {
    while(bytes--) { assert(fputc((unsigned char)n,f)!=EOF); n>>=8; }
}
static void afs(const char *name,int value) {
    FILE *f=fopen(name,"wb"); assert(f);
    fwrite("AFS\0",1,4,f); word(f,2,4);
    word(f,24,4); word(f,2,4); word(f,26,4); word(f,2,4);
    fputc(value,f); fputc(value+1,f); fputc(value+2,f); fputc(value+3,f); fclose(f);
}
static void named(const char *name) {
    FILE *f=fopen(name,"wb"); assert(f);
    fwrite("BT3TEXT1",1,8,f); word(f,2,4);
    word(f,7,4); word(f,66,8); word(f,1,8); fwrite("dir/z\x2ej",1,7,f);
    word(f,7,4); word(f,67,8); word(f,1,8); fwrite("dir/a\x2ej",1,7,f);
    fputc('z',f); fputc('a',f); fclose(f);
}
static void *reader(void *unused) {
    int i; (void)unused;
    for(i=0;i<500;i++) {
        int64_t base,size;
        FILE *f=Port_ArchiveLookup("test.afs",NULL,i%2,&base,&size,-1,NULL,0);
        assert(f && size==2 && fgetc(f)==10+(i%2)*2); fclose(f);
    }
    return NULL;
}
int main(void) {
    int64_t base,size; FILE *a,*b; char listed[30]; pthread_t threads[4]; int i;
    afs("test.afs",10);
    a=Port_ArchiveLookup("test.afs",NULL,0,&base,&size,-1,NULL,0);
    b=Port_ArchiveLookup("test.afs",NULL,1,&base,&size,-1,NULL,0);
    assert(a && b && fgetc(a)==10 && fgetc(b)==12 && fgetc(a)==11);
    struct stat stamp;
    assert(!Port_ArchiveFileStat(a,&stamp) && stamp.st_size==28);
    fclose(a); fclose(b);
    for(i=0;i<4;i++) assert(!pthread_create(&threads[i],NULL,reader,NULL));
    for(i=0;i<4;i++) pthread_join(threads[i],NULL);
    b=Port_ArchiveLookup("test.afs",NULL,0,&base,&size,-1,NULL,0);
    afs("replacement.afs",20); assert(!rename("replacement.afs","test.afs"));
    for(i=0;i<16;i++) indexes[i].checked=0; // Expire the one-second snapshot.
    a=Port_ArchiveLookup("test.afs",NULL,0,&base,&size,-1,NULL,0);
    assert(a && fgetc(a)==20); fclose(a);
    assert(b && fgetc(b)==10); fclose(b); // Existing readers survive replacement/eviction.
    assert(!Port_ArchiveLookup("test.afs",NULL,2,&base,&size,-1,NULL,0));
    named("test.pak");
    a=Port_ArchiveLookup("test.pak","dir/a.j",0,&base,&size,-1,NULL,0);
    assert(a && size==1 && fgetc(a)=='a'); fclose(a);
    a=Port_ArchiveLookup("test.pak","dir",0,&base,&size,0,listed,sizeof(listed));
    assert(a && !strcmp(listed,"z.j")); fclose(a); // Preserve original index order.
    assert(!Port_ArchiveLookup("test.pak","absent",0,&base,&size,-1,NULL,0));
    a=fopen("test.pak","wb"); fwrite("BT3TEXT1",1,8,a); word(a,100000,4); fclose(a);
    for(i=0;i<16;i++) indexes[i].checked=0;
    assert(!Port_ArchiveLookup("test.pak","dir/a.j",0,&base,&size,-1,NULL,0));
    a=fopen("sparse.afs","wb"); assert(a);
    fwrite("AFS\0",1,4,a); word(a,3,4);
    word(a,0,4); word(a,0,4); // Omitted entry zero.
    word(a,32,4); word(a,1,4); // Retained selector/resource entry.
    word(a,0,4); word(a,0,4); // Omitted trailing entry.
    fputc('E',a); fclose(a);
    a=Port_ArchiveLookup("sparse.afs",NULL,1,&base,&size,-1,NULL,0);
    assert(a && size==1 && fgetc(a)=='E'); fclose(a);
    assert(!Port_ArchiveLookup("sparse.afs",NULL,0,&base,&size,-1,NULL,0));
    assert(!Port_ArchiveLookup("sparse.afs",NULL,2,&base,&size,-1,NULL,0));
    remove("sparse.afs");
    remove("test.afs"); remove("test.pak");
    puts("PASS: archive indexes, independent/concurrent readers, replacement, bounds, listing and malformed packages");
    return 0;
}
