/* Single-owner save job. Payloads live outside rollback state and must be
   immutable until Poll completes. Only the worker publishes done/result. */
#ifndef PORT_ASYNC_SAVE_H
#define PORT_ASYNC_SAVE_H
#include <pthread.h>
typedef struct PortAsyncSave {
    pthread_t thread;
    int busy, done, result;
    int (*work)(void *);
    void *payload;
} PortAsyncSave;
static void *port_async_save_run(void *arg) {
    PortAsyncSave *job=(PortAsyncSave *)arg;
    job->result=job->work(job->payload);
    __atomic_store_n(&job->done,1,__ATOMIC_RELEASE);
    return NULL;
}
static int PortAsyncSave_Start(PortAsyncSave *job,int (*work)(void *),void *payload) {
    if(job->busy) return 0;
    job->work=work;job->payload=payload;job->done=0;job->busy=1;
    if(pthread_create(&job->thread,NULL,port_async_save_run,job)) {job->busy=0;return 0;}
    return 1;
}
/* -1 pending, otherwise the worker's nonnegative result. Never waits for I/O. */
static int PortAsyncSave_Poll(PortAsyncSave *job) {
    if(!job->busy) return 0;
    if(!__atomic_load_n(&job->done,__ATOMIC_ACQUIRE)) return -1;
    pthread_join(job->thread,NULL);
    job->busy=0;
    return job->result;
}
#endif
