/* gcc -std=gnu99 -g -fno-builtin -ffunction-sections -fdata-sections \
 *   -Iinclude -Iport/include -include port_compat.h \
 *   port/tests/ui/quit_mouse.c -Wl,--gc-sections -o /tmp/quit_mouse_test
 * /tmp/quit_mouse_test */
#include "../../src/quit_dialog.c"
#include <assert.h>
#include <string.h>
Dialog *gDialog;
Progress *gProgress;
static Dialog dialog, other;
static int moves, accepted, cancelled, mouseSupport = 1;
static int online, loaded, closed;
int gPortResim;
HeapBlock *gHeapStart[HEAP_COUNT], *gHeapEnd[HEAP_COUNT];
s32 gHeapSize[HEAP_COUNT];
static u32 archive[64], resource[64];
void Heap_InitFreeBlock(HeapBlock *block, s32 size) { assert(block == (HeapBlock *)sQuitHeap); }
void *Heap_Alloc(s32 size, u32 align, s32 tail, s32 heap) {
    assert(sQuitPrivate && gHeapStart[0] == (HeapBlock *)sQuitHeap);
    return archive;
}
int PortQuit_ReadArchive(int partition, int index, void *dst, int capacity) {
    assert(sQuitPrivate && gHeapStart[0] == (HeapBlock *)sQuitHeap);
    return sizeof(archive);
}
int Port_Setting(const char *name, int fallback) { (void)name; (void)fallback; return mouseSupport; }
int Port_NetActive(void) { return online; }
const char *MenuTranslation_PortText(const char *id) { (void)id; return "Exit?"; }
s32 File_IsPartitionLoaded(s32 partition) { (void)partition; return loaded; }
void *File_LoadSync(s32 id, void *buf, s32 unused) { (void)id; (void)buf; (void)unused; return NULL; }
void *Sprite_Unpack(void *src, void *dst, s32 *size) { (void)src; (void)dst; (void)size; return loaded ? resource : NULL; }
void Heap_Free(void *p) { (void)p; }
void Dialog_Init(void *file, void *text, s32 size) { (void)file; (void)text; (void)size; gDialog = &dialog; }
void Dialog_Term(void) { gDialog = NULL; }
void Dialog_SetLayout(s32 layout) { (void)layout; }
void Dialog_SetMsg(s32 message) { (void)message; }
void Dialog_SetChoices(s32 choices) { (void)choices; }
void Dialog_SetCursor(s32 cursor) { gDialog->cursor = cursor; }
void Dialog_Start(s32 command) { if (command == DIALOG_CMD_CLOSE) cancelled++; else gDialog->flash[0].flags = 2; }
void Dialog_PlayCursorPlate(s32 flash, const char *label) { (void)flash; (void)label; accepted++; }
s32 Dialog_IsClosed(void) { return closed; }
s32 Snd_PlaySe(u32 mask, s32 id) { (void)mask; if (id == 0) moves++; return 0; }
s32 sceGsSyncPath(s32 mode, u16 timeout) { (void)mode; (void)timeout; return 0; }
void Port_DialogDraw43(s32 visible) { (void)visible; }
void Port_FlashQuitMouseCapture(int on) { (void)on; }
int Port_FlashQuitMouseRow(int x16, int y16) {
    if (y16 < 280 * 16 || y16 >= 344 * 16) return -1;
    if (x16 >= 88 * 16 && x16 < 216 * 16) return 0;
    if (x16 >= 308 * 16 && x16 < 436 * 16) return 1;
    return -1;
}
static void ready(void) {
    memset(&dialog, 0, sizeof(dialog));
    dialog.cursor = 1; dialog.flash[0].flags = 2;
    sQuitDialog = &dialog; gDialog = &other;
    sQuitState = 2; sQuitAction = 0;
    sQuitMouseHover = sQuitMouseClick = 0;
    moves = accepted = cancelled = 0;
}
int main(void) {
    ready();
    PortQuit_Mouse(150 * 16, 300 * 16, 0); PortQuit_Draw(1);
    assert(dialog.cursor == 0 && moves == 1 && !accepted && gDialog == &other);
    PortQuit_Action(PORT_QUIT_MOVE); PortQuit_Draw(1);
    assert(dialog.cursor == 1); PortQuit_Draw(1); assert(dialog.cursor == 1);
    PortQuit_Mouse(150 * 16, 300 * 16, 1);
    PortQuit_Mouse(350 * 16, 300 * 16, 0); /* motion cannot relocate an earlier click */
    PortQuit_Draw(1); assert(sQuitState == 4 && accepted == 1 && dialog.cursor == 0);
    ready(); PortQuit_Mouse(350 * 16, 300 * 16, 1); PortQuit_Draw(1);
    assert(sQuitState == 3 && cancelled == 1 && !accepted);
    ready(); PortQuit_Mouse(256 * 16, 300 * 16, 1); PortQuit_Draw(1);
    assert(sQuitState == 2 && !accepted && !cancelled); /* gap */
    PortQuit_Mouse(150 * 16, 300 * 16, 2); PortQuit_Draw(1);
    assert(sQuitState == 3 && !accepted && cancelled == 1);
    ready(); dialog.flash[0].flags = 0;
    PortQuit_Mouse(150 * 16, 300 * 16, 1);
    dialog.flash[0].flags = 2; PortQuit_Draw(1);
    assert(sQuitState == 2 && !accepted); /* clicks during opening are discarded */
    PortQuit_Mouse(150 * 16, 300 * 16, 1); PortQuit_Action(0); PortQuit_Draw(1);
    assert(sQuitState == 2 && !accepted); /* focus loss discards pending input */
    ready(); PortQuit_Mouse(150 * 16, 300 * 16, 1); mouseSupport = 0;
    PortQuit_Draw(1); assert(sQuitState == 2 && !accepted);
    PortQuit_Mouse(150 * 16, 300 * 16, 1); PortQuit_Draw(1);
    assert(sQuitState == 2 && !accepted);
    mouseSupport = 1; PortQuit_Draw(1); assert(sQuitState == 2 && !accepted);
    PortQuit_Mouse(150 * 16, 300 * 16, 1); PortQuit_Draw(1);
    assert(sQuitState == 4 && accepted == 1);
    /* Online uses the native dialog with a private heap, restoring the game's
       heap and dialog after every draw, including cancellation and cleanup. */
    Progress progress = {0};
    HeapBlock original[4];
    progress.unk0[1] = 1; gProgress = &progress;
    for (int i = 0; i < HEAP_COUNT; i++) {
        gHeapStart[i] = &original[i]; gHeapEnd[i] = &original[i+2]; gHeapSize[i] = 123+i;
    }
    online = loaded = 1; sQuitState = 0; sQuitDialog = NULL;
    PortQuit_Request(); PortQuit_Draw(1);
    assert(PortQuit_Ready() && !PortQuit_Failed() && sQuitPrivate && gDialog == &other);
    for (int i = 0; i < HEAP_COUNT; i++) {
        assert(gHeapStart[i] == &original[i] && gHeapEnd[i] == &original[i+2] && gHeapSize[i] == 123+i);
    }
    gPortResim = 1; PortQuit_Action(PORT_QUIT_CANCEL); PortQuit_Draw(1);
    assert(sQuitState == 2); /* rollback replays must not advance local UI */
    gPortResim = 0; PortQuit_Draw(1); assert(sQuitState == 3);
    closed = 1; PortQuit_Draw(1); assert(sQuitState == 7);
    PortQuit_Draw(1); assert(sQuitState == 0 && !sQuitPrivate && gDialog == &other);
    for (int i = 0; i < HEAP_COUNT; i++) {
        assert(gHeapStart[i] == &original[i] && gHeapEnd[i] == &original[i+2] && gHeapSize[i] == 123+i);
    }
    return 0;
}
