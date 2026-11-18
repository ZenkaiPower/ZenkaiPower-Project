/* Resource-free checks against the production RNG, pool, file and sound code. */
#include "common.h"
#include "sys/port_original_fixes.h"
#include "sys/rand.h"
#include "sys/file.h"
#include "sys/snd.h"
#include "sys/heap.h"
#include "battle/btl_pool.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

static int zenkai, liveBlocks, reads, ticks, sentCount;
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_ZenkaiPackEnabled(void) { abort(); } /* Classic must not gate fixes. */
u32 gRandState[RAND_N], gRandMag01[2] = {0, 0x9908b0df};
s32 gRandIndex = RAND_N + 1;
BtlPool *gBtlPool;
s32 gBtlPoolInitArg;
FileReqQueue gFileReq;
static SndRpc rpc;
SndRpc *gSndRpc = &rpc;
SndRpcBuf gSndRpcBuf;
u16 gSndSerial;
static SndCmd sent[256];

void *Heap_Alloc(s32 size, u32 align, s32 tail, s32 heap) {
    void *p = NULL;
    assert(posix_memalign(&p, align, size) == 0);
    liveBlocks++;
    return p;
}
void Heap_Free(void *p) { if (p) { liveBlocks--; free(p); } }
void FlushCache(s32 mode) {}
s32 sceSifCallRpc(SndSifClient *client, s32 cmd, s32 mode, void *data,
                 s32 size, void *recv, s32 recvSize, void *end, void *arg) {
    if (cmd == SND_RPC_TICK) ticks++;
    else {
        SndQueue *q = data;
        int i;
        assert(cmd == SND_RPC_QUEUE && size == sizeof(*q));
        assert(q->count <= SND_QUEUE_MAX);
        for (i = 0; i < q->count; i++) sent[sentCount++] = q->cmd[i];
    }
    return 0;
}
ADXF ADXF_OpenAfs(s32 partition, s32 id) { return (void *)(intptr_t)(id + 1); }
void ADXF_Close(ADXF f) {}
s32 ADXF_GetFsizeSct(ADXF f) { return 1; }
s32 ADXF_ReadNw(ADXF f, s32 sectors, void *buf) {
    *(int *)buf = (int)(intptr_t)f;
    reads++;
    return sectors;
}
s32 ADXF_GetStat(ADXF f) { return ADXF_STAT_READEND; }

static void checkRng(int fixed) {
    unsigned long long hash = 1469598103934665603ULL;
    int i;
    Rand_Seed(5489);
    for (i = 0; i < 4000; i++) hash = (hash ^ Rand_Next()) * 1099511628211ULL;
    assert(hash == (fixed ? EXPECT_FIXED : EXPECT_ORIGINAL));
}
static void checkPool(int fixed) {
    unsigned char *first, *spill;
    int before, i;
    BtlPool_Init(0);
    first = BtlPool_Alloc(0, gBtlPool->slots[0].size);
    assert(first == gBtlPool->slots[0].base);
    before = liveBlocks;
    if (fixed) {
        unsigned char *neighbor = gBtlPool->slots[1].base;
        for (i = 0; i < 32; i++) neighbor[i] = 0xa5;
        spill = BtlPool_Alloc(0, 1);
        assert(liveBlocks == before + 1 && ((uintptr_t)spill % 32) == 0);
        for (i = 0; i < 32; i++) spill[i] = 0x42;
        for (i = 0; i < 32; i++) assert(neighbor[i] == 0xa5);
        assert(gBtlPool->slots[0].used == gBtlPool->slots[0].size);
        BtlPool_Free(0, spill);
        assert(liveBlocks == before);
        BtlPool_Alloc(0, 64);
        BtlPool_Reset(0);
        assert(liveBlocks == before && gBtlPool->slots[0].used == 0);
        BtlPool_Alloc(0, gBtlPool->slots[0].size + 32);
        assert(liveBlocks == before + 1);
    }
    BtlPool_Term();
    assert(liveBlocks == 0);
}
static void checkSound(int fixed) {
    int i;
    ticks = sentCount = 0;
    gSndSerial = 0;
    for (i = 0; i < 96; i++) {
        int result = Snd_QueuePlay(1, i, 64, 64, 0, 0);
        assert(result == (fixed || i < 32 ? i : -1));
    }
    assert(ticks == 0);
    Snd_QueueStopId(1, 1);
    Snd_QueueStopHandle(5);
    Snd_RpcFlush();
    assert(ticks == 1 && sentCount == (fixed ? 98 : 32));
    for (i = 0; i < (fixed ? 96 : 32); i++) {
        assert(sent[i].id == i && sent[i].handle == i && sent[i].type == SND_CMD_PLAY);
    }
    if (fixed) {
        assert(sent[96].type == SND_CMD_STOP_ID && sent[96].id == 1);
        assert(sent[97].type == SND_CMD_STOP_HANDLE && sent[97].handle == 5);
    }
    assert(Snd_QueuePlay(1, 256, 64, 64, 0, 0) == -1);
    assert(gSndRpc->queue.count == 0);
}
static void checkFiles(int fixed) {
    int buffers[33], i;
    reads = 0;
    File_InitRequests();
    for (i = 0; i < (fixed ? 33 : 32); i++) {
        buffers[i] = -1;
        assert(File_Request(i + 1, &buffers[i]) == &buffers[i]);
    }
    assert(reads == (fixed ? 32 : 0));
    while (!File_UpdateRequests()) {}
    for (i = 0; i < (fixed ? 33 : 32); i++) assert(buffers[i] == i + 1);
    assert(File_GetPendingCount() == 0 && List_GetCount(&gFileReq.free) == 32);
}
int main(void) {
    for (zenkai = 0; zenkai < 2; zenkai++) {
        int fixed = zenkai && PORT_ZENKAI_ORIGINAL_FIXES;
        assert(Port_OriginalFixesEnabled() == fixed);
        checkRng(fixed);
        checkPool(fixed);
        checkSound(fixed);
        checkFiles(fixed);
    }
    return 0;
}
