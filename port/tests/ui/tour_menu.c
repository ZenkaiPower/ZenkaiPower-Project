#include "common.h"
#include "menu/menu_j.h"
#include "menu/port_tour_rules.h"
#include <assert.h>
#include <stdio.h>

static SaveData save;
SaveData *gSaveData = &save;
static TourMenu menu;
static int zenkai=1;
int Port_ZenkaiEnabled(void) { return zenkai; }
u32 Rand_Range(u32 n) { assert(n); return n-1; }
extern void TourMenu_CheckInvite(void);

int main(void) {
    int hour,cell,tour;
    gTourMenu=&menu;
    for(zenkai=0;zenkai<=1;zenkai++) for(cell=0;cell<2;cell++) for(hour=0;hour<24;hour++) {
        memset(&save,0,sizeof(save));memset(&menu,0,sizeof(menu));
        save.unkA0C=hour;save.unkA08=TOUR_SAVE_STARTED;
        save.slot[2].val[1]=cell?127:63;
        tour=hour>=7 && hour<=12?0:hour>=13 && hour<=18?1:hour>=19?2:hour<=4?3:4;
        TourMenu_CheckInvite();
        if(zenkai && !cell && tour!=0 && tour!=4) {
            assert(!(menu.flags & TOURMENU_INVITE));
            assert(!(save.unkA08 & TOUR_SAVE_INVITE_MASK));
        } else {
            assert(menu.flags & TOURMENU_INVITE);
            assert(menu.invite==tour && (save.unkA08 & TOUR_SAVE_INVITE_MASK)==(1<<tour));
        }
        assert(save.unkA0C==hour);
    }
    /* A save carried over from a previous build cannot retain a gated invitation. */
    zenkai=1;memset(&save,0,sizeof(save));memset(&menu,0,sizeof(menu));
    save.unkA08=TOUR_SAVE_STARTED|(1<<2);save.unkA0C=20;
    TourMenu_CheckInvite();assert(!(save.unkA08 & TOUR_SAVE_INVITE_MASK));
    puts("Tournament invitations: every hour, both modes, Cell gating and stale invitations passed");
}
