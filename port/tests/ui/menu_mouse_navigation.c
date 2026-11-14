/* gcc -std=gnu99 -g -fno-builtin -ffunction-sections -fdata-sections \
 *   -Iinclude -Iport/include -include port_compat.h \
 *   port/tests/ui/menu_mouse_navigation.c -Wl,--gc-sections -o /tmp/menu_mouse_navigation_test
 * /tmp/menu_mouse_navigation_test */
#include "../../../src/menu/menu_a.c"
extern void abort(void);
#define CHECK(condition) do { if (!(condition)) abort(); } while (0)
Pad gPad[PAD_COUNT];
static MenuProgress progress;
MenuProgress *gProgress = &progress;
static SaveData save;
SaveData *gSaveData = &save;
s32 gPortScreen;
static PortMenuMouse pending;
static int hitRow = -1, hitArrow, moves, confirms, cancels;
void PortMenuMouse_Read(PortMenuMouse *mouse) { *mouse = pending; memset(&pending, 0, sizeof(pending)); }
int Port_FlashMainMouseRow(float x, float y) { (void)x; (void)y; return hitRow; }
int Port_FlashMainMouseArrow(float x, float y) { (void)x; (void)y; return hitArrow; }
u32 Rand_Range(u32 n) { (void)n; return 0; }
void Flash_FindLabel(MFlash *f, char *p, char *n, MFlashRef *r) { (void)f; (void)p; (void)n; r->id = 0; r->more = 0; }
void Flash_ClipGotoLabel(MFlash *f, MFlashRef *r, char *n) { (void)f; (void)r; (void)n; }
void Flash_GotoLabel(MFlash *f, char *n, s32 start) { (void)f; (void)n; (void)start; }
void Voice_PlayWithSubtitle(void *text, s32 base, s32 line) { (void)text; (void)base; (void)line; }
s32 Snd_PlaySe(u32 mask, s32 id) { (void)mask; if (id == 0) moves++; if (id == 1) confirms++; if (id == 2) cancels++; return 0; }
void IconWin_Close(void) {}
void MsgWin_Close(void) {}
void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames) { (void)r; (void)g; (void)b; (void)frames; }
int Port_WiiArtAvailable(void) { return 1; }

int main(void) {
    MainMenu menu = {0};
    s32 result = 1, i;
    gMainMenu = &menu; menu.itemCount = 11; menu.flags = MAINMENU_STARTED;
    menu.flash[0].flags = MFLASH_PAD;
    for (i = 0; i < menu.itemCount; i++) menu.items[i] = i;
    pending.moved = 1; hitRow = 2;
    MainMenu_Input(&result); CHECK(menu.cursor == 2 && moves == 1 && !confirms);
    hitRow = 0; /* A stationary pointer must not override a key. */
    gPad[0].gameRepeat = 4; MainMenu_Input(&result); CHECK(menu.cursor == 3);
    gPad[0].gameRepeat = 0; MainMenu_Input(&result); CHECK(menu.cursor == 3);
    pending.wheel = 1; MainMenu_Input(&result); CHECK(menu.cursor == 3 && menu.top == 1);
    menu.cursor = 0; pending.wheel = -1;
    MainMenu_Input(&result); CHECK(menu.cursor == 0 && menu.top == 0);
    hitArrow = -1; pending.confirm = 1;
    MainMenu_Input(&result); CHECK(menu.cursor == 0 && menu.top == 10 && !confirms);
    hitArrow = 1; menu.cursor = 3; pending.confirm = 1;
    MainMenu_Input(&result); CHECK(menu.cursor == 3 && menu.top == 0 && !confirms);
    hitArrow = 0;
    hitRow=4;menu.cursor=2;pending.confirm=1;
    MainMenu_Input(&result); CHECK(menu.cursor==0 && menu.top==10 && !confirms);
    hitRow=5;pending.confirm=1;
    MainMenu_Input(&result); CHECK(menu.cursor==3 && menu.top==0 && !confirms);
    pending.confirm = 1; hitRow = -1;
    MainMenu_Input(&result); CHECK(!confirms && !(menu.flags & MAINMENU_LEAVING));
    pending.confirm = 1; hitRow = 1; /* Select and enter the clicked row in one frame. */
    MainMenu_Input(&result); CHECK(menu.cursor == 1 && confirms == 1 && progress.mode == 33);
    CHECK(menu.flags & MAINMENU_LEAVING);
    menu.flags = MAINMENU_STARTED; pending.cancel = 1;
    MainMenu_Input(&result); CHECK(result == 0 && cancels == 1);
    menu.flash[0].flags = 0; pending.confirm = 1;
    MainMenu_Input(&result); CHECK(confirms == 1); /* Opening/scroll animation gates input. */
    return 0;
}
