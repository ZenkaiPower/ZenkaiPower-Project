/* cc -std=gnu99 -ffunction-sections -fdata-sections port/tests/ui/net_selection_mouse.c
 * -Wl,--gc-sections -lSDL3 -pthread -lm -o /tmp/net-selection-mouse */
#include <assert.h>
#include "../../../port/src/gs/gs_input.c"
static int enabled=1,current,phase=1,target=4,ready=1;
int Port_Setting(const char *name,int fallback) {return enabled;}
static void selector(PortMenuMouse *mouse,PortMouseNetNav *nav) {
    nav->ready=ready;nav->phase=phase;nav->current=current;
    nav->target=(mouse->moved || mouse->confirm) ? target:-1;
    nav->negative=0x80;nav->positive=0x20;
    if(mouse->wheel) {nav->target=-1;nav->action=mouse->wheel<0 ? 0x10:0x40;}
}
static unsigned sample(void) {
    unsigned char pad[18]={255,255,128,128,128,128};
    PortMenuMouse_NetSample(pad);
    return (~(pad[0]|pad[1]<<8))&65535;
}
static void motion(int click) {sMenuMouse.moved=1;sMenuMouse.confirm=click;}
int main(void) {
    int i;
    PortMenuMouse_Enable(1);PortMenuMouse_Ready(1);PortMenuMouse_NetSelector(selector);
    motion(1);assert(sample()==0x20); /* click far away: move, don't confirm */
    for(i=0;i<8;i++) assert(!sample()); /* input delay: don't overshoot */
    current=1;assert(sample()==0x20);assert(!sample());
    current=2;assert(sample()==0x20);assert(!sample());
    current=3;assert(sample()==0x20);assert(!sample());
    current=4;assert(sample()==0x4000);assert(!sample());assert(!sample());
    /* Both copies replay the same ordinary pad packets, no pointer payload. */
    phase++;current=0;motion(0);assert(sample()==0x20);assert(!sample());
    phase++;assert(!sample()); /* transition cancels stale targets */
    motion(1);assert(sample()==0x20);
    enabled=0;assert(!sample());enabled=1;assert(!sample());
    /* Session restoration restores the original menu owner and flushes clicks. */
    PortMenuMouse_SessionSave();assert(!PortMenuMouse_Enabled() && !sNetMouseSelector);
    PortMenuMouse_Enable(1);PortMenuMouse_NetSelector(selector);motion(1);
    PortMenuMouse_Enable(0);PortMenuMouse_SessionRestore();
    assert(PortMenuMouse_Enabled() && sNetMouseSelector==selector && !sNetMouse.active && !sMenuMouse.confirm);
    assert(sGameMouseMode==PORT_MOUSE_GAME_OFF);
    PortMenuMouse_Ready(1);sMenuMouse.cancel=1;assert(sample()==0x1000);
    assert(!sample());sMenuMouse.wheel=1;assert(sample()==0x40);
    /* Physical input overrides a pending mouse navigation sequence. */
    assert(!sample());phase++;current=0;motion(1);assert(sample()==0x20);
    { unsigned char pad[18]={255,191,128,128,128,128};
      PortMenuMouse_NetSample(pad);assert(pad[1]==191 && !sNetMouse.active); }
    PortMenuMouse_NetSelector(NULL);motion(1);assert(!sample());
    /* A restored selector loop repairs host ownership after another screen
       disabled it. Ordinary frames must retain an in-flight click target. */
    for(i=0;i<2;i++) {
        PortMenuMouse_Enable(0);PortMenuMouse_NetSelector(NULL);
        PortMenuMouse_NetEnsureSelector(selector);
        assert(PortMenuMouse_Enabled() && sNetMouseSelector==selector);
        PortMenuMouse_Ready(1);phase++;current=0;motion(1);
        assert(sample()==0x20);
        PortMenuMouse_NetEnsureSelector(selector);
        assert(sNetMouse.active && sNetMouse.confirm==0x4000);
        assert(!sample());current=target;assert(sample()==0x4000);
    }
    return 0;
}
