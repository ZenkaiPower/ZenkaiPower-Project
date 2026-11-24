/* Controlled slow storage: polling must keep returning so frames can draw. */
#include "../../src/plat_mc.c"
#include <assert.h>
#include <time.h>
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition=PTHREAD_COND_INITIALIZER;
static int started,released,failWrite,online;
int Port_NetActive(void) {return online;}
void Port_StateTouch(void *p,unsigned long n) {(void)p;(void)n;}
int Port_ZenkaiEnabled(void) {return 1;}
int Port_ZenkaiSelected(void) {return 1;}
void Port_ZenkaiExistingSaveLoaded(void) {}
FILE *Port_ZenkaiOpen(const char *path,int mode) {(void)path;(void)mode;return tmpfile();}
int Port_ZenkaiFlush(const char *path,FILE *file) {
    (void)path;(void)file;
    pthread_mutex_lock(&gate);started=1;pthread_cond_broadcast(&condition);
    while(!released) pthread_cond_wait(&condition,&gate);
    pthread_mutex_unlock(&gate);
    return !failWrite;
}
static void release_storage(void) {
    pthread_mutex_lock(&gate);released=1;pthread_cond_broadcast(&condition);pthread_mutex_unlock(&gate);
}
static int finish(int command) {
    int result,cmd=-1,ready;
    struct timespec pause={0,1000000};
    for(int i=0;i<5000;i++) {
        ready=func_002A2498(1,&cmd,&result);
        if(ready) {assert(ready==1 && cmd==command);return result;}
        nanosleep(&pause,NULL);
    }
    assert(!"save worker timed out");return -99;
}
int main(void) {
    char dir[]="/tmp/async-card-XXXXXX";assert(mkdtemp(dir));
    assert(!chdir(dir));unsetenv("ZENKAIPOWER_SAVES");
    assert(func_002A1E58(0,0,"zenkaipower.sav",2|0x200)==0);
    int fd=finish(2);assert(fd>=0 && sMc[fd].open);
    unsigned char bytes[4]={1,2,3,4};
    assert(!func_002A2320(fd,bytes,sizeof(bytes)));
    memset(bytes,9,sizeof(bytes)); /* worker owns an immutable copy */
    pthread_mutex_lock(&gate);
    while(!started) pthread_cond_wait(&condition,&gate);
    pthread_mutex_unlock(&gate);
    int frames=0,cmd=77,result=88;
    for(int i=0;i<120;i++) {
        assert(func_002A2498(1,&cmd,&result)==0);
        assert(cmd==77 && result==88 && sMc[fd].pos==0);
        frames++; /* presentation can continue while storage is blocked */
    }
    assert(frames==120 && func_002A1FB8(fd)==-200);
    release_storage();assert(finish(6)==4 && sMc[fd].pos==4);
    rewind(sHost[fd]);assert(fgetc(sHost[fd])==1 && fgetc(sHost[fd])==2);
    failWrite=1;assert(!func_002A2320(fd,bytes,sizeof(bytes)));
    assert(finish(6)==-4 && sMc[fd].pos==4); /* failure remains visible to native flow */
    assert(!func_002A1FB8(fd));assert(finish(3)==0 && !sMc[fd].open && !sHost[fd]);
    assert(func_002A2498(1,NULL,NULL)==-1);
    online=1;failWrite=0;
    assert(!func_002A1E58(0,0,"zenkaipower.sav",2|0x200) && !sMcIoJob.busy);
    fd=finish(2);assert(fd>=0);
    assert(!func_002A2320(fd,bytes,sizeof(bytes)) && !sMcIoJob.busy);
    assert(finish(6)==4);assert(!func_002A1FB8(fd) && finish(3)==0);

    puts("Async card: responsive polling, serialized requests, immutable bytes and write errors passed");
    return 0;
}
