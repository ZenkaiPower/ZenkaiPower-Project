/* gcc -std=gnu99 -g -fno-builtin -ffunction-sections -fdata-sections \
 *   -Iinclude -Iport/include -include port_compat.h \
 *   port/tests/ui/title_mouse_navigation.c -Wl,--gc-sections -o /tmp/title_mouse_navigation_test
 * /tmp/title_mouse_navigation_test */
#include "../../../src/menu/menu_a_c.c"
#include <stdio.h>
extern void abort(void);
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed at line %d\n", __LINE__); abort(); } } while (0)
Pad gPad[PAD_COUNT];
static MenuProgress progress;
MenuProgress *gProgress = &progress;
int gPortOverlayOpen;
static PortMenuMouse pending;
static int hitRow = -1, moves, confirms, quits, cardStarts, anyButton, swallowedPads, swallowedKeys;
int PortInput_AnyButtonPressed(void) { int pressed = anyButton; anyButton = 0; return pressed; }
void PortInput_Swallow(void) { swallowedPads++; }
void PortInput_SwallowKeys(void) { swallowedKeys++; }
void PortMenuMouse_Read(PortMenuMouse *mouse) { *mouse = pending; memset(&pending, 0, sizeof(pending)); }
int Port_FlashTitleMouseRow(float x, float y) { (void)x; (void)y; return hitRow; }
u32 Rand_Range(u32 n) { (void)n; return 0; }
void Flash_FindLabel(MFlash *f, char *p, char *n, MFlashRef *r) { (void)f; (void)p; (void)n; r->id = 0; r->more = 0; }
void Flash_ClipGotoLabel(MFlash *f, MFlashRef *r, char *n) { (void)f; (void)r; (void)n; }
void Flash_GotoLabel(MFlash *f, char *n, s32 start) { (void)f; (void)n; (void)start; }
void Voice_PlayWithSubtitle(void *text, s32 base, s32 line) { (void)text; (void)base; (void)line; }
s32 Snd_PlaySe(u32 mask, s32 id) { if (mask == 1 && id == 0) moves++; if (mask == 1 && id == 1) confirms++; return 0; }
int Ui_QuitAsk(void) { quits++; return 1; }
void Demo_SetupBattle(void) {}
void McFlow_Start(s32 mode) { CHECK(mode == 3); cardStarts++; }
void McFlow_SetDoneCb(s32 index, void *callback, void *arg) { (void)index; (void)callback; (void)arg; }

int main(void) {
    Title title = {0};
    s32 result = 1;
    gTitle = &title; title.itemCount = 2;
    title.flags=TITLE_STARTED;title.flash[0].flags=MFLASH_PAD;
    gPad[0].gamePressed=PADG_CROSS;Title_Input(&result);CHECK(title.state==1);
    gPad[0].gamePressed=PADG_CIRCLE;Title_Input(&result);CHECK(title.state==2);
    gPad[0].gamePressed=0;memset(&title,0,sizeof(title));title.itemCount=2;confirms=0;
    pending.confirm = 1;
    Title_Input(&result); CHECK(title.state == 1); /* intro skip does not need MFLASH_PAD */
    memset(&title, 0, sizeof(title)); title.itemCount = 2; title.state = 1;
    title.flags = TITLE_STARTED; title.flash[0].flags = MFLASH_PAD;
    swallowedPads = swallowedKeys = 0;
    anyButton = 1;
    Title_Input(&result); CHECK(title.state == 2 && !cardStarts);
    CHECK(swallowedPads == 1 && swallowedKeys == 1); /* prompt input cannot carry into the title menu */
    title.flash[0].flags = MFLASH_PAD;
    pending.confirm = 1;
    Title_Input(&result); CHECK(title.state == 2 && !cardStarts);
    pending.moved = 1; hitRow = 1;
    Title_Input(&result); CHECK(title.cursor == 1 && moves == 1);
    gPad[0].gameRepeat = 8;
    Title_Input(&result); CHECK(title.cursor == 0);
    gPad[0].gameRepeat = 0;
    Title_Input(&result); CHECK(title.cursor == 0); /* stationary pointer cannot override a key */
    pending.wheel = -1; Title_Input(&result); CHECK(title.cursor == 1); /* wraps */
    pending.wheel = 1; Title_Input(&result); CHECK(title.cursor == 0);
    pending.confirm = 1; hitRow = -1;
    Title_Input(&result); CHECK(!cardStarts && !(title.flags & TITLE_LEAVING));
    title.itemCount = 1; hitRow = 1; pending.confirm = 1;
    Title_Input(&result); CHECK(title.cursor == 0 && !(title.flags & TITLE_LEAVING)); /* disabled Continue */
    title.itemCount = 2; pending.cancel = 1;
    Title_Input(&result); CHECK(quits == 1);
    pending.confirm = 1; hitRow = 0; anyButton = 1;
    Title_Input(&result); CHECK(cardStarts == 1);
    gPortOverlayOpen = 1; pending.confirm = 1; hitRow = 1;
    Title_Input(&result); CHECK(!(title.flags & TITLE_LEAVING));
    gPortOverlayOpen = 0; title.flash[0].flags = 0; pending.confirm = 1;
    Title_Input(&result); CHECK(!(title.flags & TITLE_LEAVING)); /* animation gates choices */
    title.flash[0].flags = MFLASH_PAD; pending.confirm = 1;
    Title_Input(&result); CHECK(title.cursor == 1 && result == 0 && (title.flags & TITLE_LEAVING));
    return 0;
}
