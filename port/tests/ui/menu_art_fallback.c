/* Standalone policy/presentation check; links the menu-aspect block from plat_stub.c. */
#include <assert.h>
#include <stdio.h>
#include "menu/port_menu_art_policy.h"

int gPortMenuMode = 1;
unsigned gGsFrame;
static int mode = 4;
static int optionState = 0;
int Port_Setting(const char *key, int fallback) { (void)key; return fallback; }
int Port_MenuArtScreenCovered(void) { return Port_MenuArtModeCovered(mode) && (mode != 62 || Port_MenuArtOptionsStateCovered(optionState)); }
extern int Port_Menus43(void), Port_Menus43Configured(void), Port_PageAspectMilli(void);
extern void Port_MenuAspectFrame(void);
extern void Port_Menus43Set(int on), Port_TitleMainChars43Set(int on);
extern void Port_Opening43Set(int on);
extern void Port_SetAspectMilli(int milli);

static void settle(void) {
    for (int i = 0; i < 20; i++) Port_MenuAspectFrame();
}

int main(void) {
    Port_SetAspectMilli(1778);
    Port_TitleMainChars43Set(1);
    Port_Menus43Set(0);
    settle();
    assert(!Port_Menus43() && Port_PageAspectMilli() == 1778);
    Port_Opening43Set(1);
    assert(Port_PageAspectMilli() == 1333);
    Port_Opening43Set(0);
    assert(Port_PageAspectMilli() == 1778);
    mode = 62;
    assert(!Port_Menus43()); /* Options is supported except screen calibration */
    optionState = 9; /* Screen Adjust */
    Port_MenuAspectFrame();
    int entering = Port_PageAspectMilli();
    assert(entering > 1333 && entering < 1778);
    assert(Port_PageAspectMilli() == entering); /* getters do not advance animation */
    /* Renderer compensation must track the displayed, not configured, aspect. */
    for (int i = 0; i < 10; i++) {
        double page = Port_PageAspectMilli();
        double displayedArtRatio = page * (1333.333 / page);
        assert(displayedArtRatio > 1333.332 && displayedArtRatio < 1333.334);
        Port_MenuAspectFrame();
    }
    settle();
    assert(Port_Menus43() && Port_PageAspectMilli() == 1333);
    assert(!Port_Menus43Configured()); /* automatic mode never overwrites manual preference */
    optionState = 11; /* calibration reset keeps the same fallback */
    assert(Port_Menus43());
    optionState = 2; /* return to supported Screen menu */
    assert(!Port_Menus43());
    mode = 80; /* Dragon Net */
    Port_MenuAspectFrame();
    assert(Port_PageAspectMilli() > 1333 && Port_PageAspectMilli() < 1778);
    settle();
    assert(!Port_Menus43() && Port_PageAspectMilli() == 1778);
    mode = 41; /* separate Versus team selector */
    assert(Port_Menus43());
    mode = 999; /* future modes default safely to the fallback */
    assert(Port_Menus43());
    gPortMenuMode = 0; /* a fight must not inherit a menu fallback */
    assert(Port_PageAspectMilli() == 1778);
    gPortMenuMode = 1;
    Port_TitleMainChars43Set(0);
    settle();
    assert(!Port_Menus43() && Port_PageAspectMilli() == 1778);
    Port_Opening43Set(1);
    assert(Port_PageAspectMilli() == 1778);
    Port_Opening43Set(0);
    Port_Menus43Set(1);
    settle();
    assert(Port_Menus43() && Port_PageAspectMilli() == 1333);
    Port_TitleMainChars43Set(1);
    mode = 35;
    assert(!Port_Menus43()); /* covered screen wins over the saved manual setting */
    Port_TitleMainChars43Set(0);
    assert(Port_Menus43()); /* disabling automatic mode restores the manual preference */
    puts("Menu art automatic fallback checks passed");
    return 0;
}
