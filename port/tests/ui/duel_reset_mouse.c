/* Build with the same section-GC flags as menu_mouse_navigation.c. */
#include "../../../src/menu/menu_g_c.c"
extern void abort(void);
extern int strcmp(const char *, const char *);
#define CHECK(x) do { if (!(x)) abort(); } while (0)
Pad gPad[PAD_COUNT];
static DuelMenu menu;
static PortMenuMouse pending;
static int drawn, cursor, saved, hoverChanges;
void Port_DialogDraw43(s32 visible) { CHECK(visible); drawn = 1; }
void PortMenuMouse_Read(PortMenuMouse *out) { CHECK(drawn); *out = pending; }
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) {
    CHECK(drawn && scope == 4 && w == 128 && h == 64 && pivot == 256);
    return strcmp(name, x < 256 ? "mc_menu_plate_1" : "mc_menu_plate_2") == 0;
}
void Dialog_SetLayout(s32 n) {}
void Dialog_Start(s32 n) {}
void Dialog_SetCursor(s32 n) { cursor=n; }
void Port_DialogMouseSelect(int n) {
    if (cursor != n) { cursor=n; hoverChanges++; }
}
void Dialog_SetChoices(s32 n) {}
void Dialog_SetMsg(s32 n) {}
s32 Dialog_IsClosed(void) { return 0; }
s32 Dialog_Input(s32 cancel) {
    if (gPad[0].gamePressed & PADG_TRIANGLE) return -1;
    if (gPad[0].gamePressed & PADG_CROSS) return cursor == 0 ? 1 : -1;
    return 0;
}
void Save_ResetRules(void) { saved++; }
void Flash_FindLabel(MFlash *f,char *parent,char *name,MFlashRef *ref) {}
void Flash_ClipGotoLabel(MFlash *f,MFlashRef *ref,char *label) {}
int main(void) {
    gDuelMenu=&menu;
    /* Input starts with no captured dialog. Draw must precede its hit query. */
    menu.reset.state=1;
    pending.confirm=1; pending.x=100;
    DuelMenu_UpdateReset(); CHECK(menu.reset.answer==1);
    /* Moving within Yes keeps its selected animation running. */
    memset(&menu,0,sizeof(menu)); memset(gPad,0,sizeof(gPad)); drawn=0;
    menu.reset.state=1; pending.confirm=0; pending.moved=1;
    DuelMenu_UpdateReset(); CHECK(menu.reset.answer==0);
    { int changes=hoverChanges;
      pending.x=110; drawn=0;
      DuelMenu_UpdateReset(); CHECK(hoverChanges==changes);
    }
    pending.confirm=1;
    memset(&menu,0,sizeof(menu)); memset(gPad,0,sizeof(gPad)); drawn=0;
    menu.reset.state=1; pending.x=400;
    DuelMenu_UpdateReset(); CHECK(menu.reset.answer==-1 && saved==0);
    memset(&menu,0,sizeof(menu)); memset(gPad,0,sizeof(gPad)); drawn=0;
    menu.reset.state=1; pending.confirm=0; pending.cancel=1;
    DuelMenu_UpdateReset(); CHECK(menu.reset.answer==-1 && saved==0);
    return 0;
}
