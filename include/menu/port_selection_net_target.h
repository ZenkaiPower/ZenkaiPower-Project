#ifndef PORT_SELECTION_NET_TARGET_H
#define PORT_SELECTION_NET_TARGET_H
#include "menu/port_net_selection_mouse.h"
#include "menu/port_selection_mouse.h"
/* Normalize the two native versus selectors without changing either state.
   0 grid, 1 forms, 2 sets, 3 items, 4 help, 5 colors, 6 saved, 7 team,
   8 done, 9 stage, 10 music. Buttons below are raw DualShock bits. */
typedef struct PortMouseNetView {
    s32 state,side,row,current,count,member;
    const s32 *chips;
} PortMouseNetView;
static void Port_SelectionNetTarget(PortMenuMouse *mouse,PortMouseNetNav *nav,PortMouseNetView *view) {
    s32 state=view->state,side=view->side,scope=side ? 12:11,hit=-1;
    f32 pivot=side ? 512:0;
    nav->phase=state+16*side+256*(view->row+1)+65536*view->member;
    nav->current=view->current;nav->target=-1;
    nav->negative=0x80;nav->positive=0x20;
    if(state==8) return; /* native Back may reopen a finished local side */
    if(state==0 || state==1 || state==6) {
        hit=Port_SelectionMouseHit(mouse,scope,"mc_chara_chip_%03d",0,7,32,32,pivot);
        if(hit>=0 && view->chips && view->chips[hit]>=0xA2 && view->chips[hit]!=0xA3) hit=-1;
    } else if(state==2 || state==5) {
        hit=Port_SelectionMouseHit(mouse,scope,state==5 ? "mc_color_plate_%d":"mc_custom_plate_%d",
            1,view->count,state==5 ? 64:256,32,pivot);
        if(hit>=1) hit--;else hit=-1;
        if(state==2) {
            s32 arrow=Port_SelectionMouseHit(mouse,scope,"mc_custom_plate_%d/mc_yajirusi",1,view->count,32,32,pivot);
            nav->negative=0x10;nav->positive=0x40;
            if(arrow>1) {hit=arrow-1;nav->action=side ? 0x80:0x20;}
        }
    } else if(state==7) {
        nav->negative=0x10;nav->positive=0x40;
        hit=Port_SelectionMouseHit(mouse,scope,"mc_team_%d",0,view->count,64,32,pivot);
        if(Port_SelectionMouseHit(mouse,scope,"mc_menu_plate",0,1,256,32,pivot)>=0) hit=5;
    } else if(state==3) {
        extern void Port_ItemPanelNetTarget(s32,PortMenuMouse *,PortMouseNetNav *);
        Port_ItemPanelNetTarget(side,mouse,nav);
#ifdef __ANDROID__
        if(mouse->confirm && !mouse->wheel && nav->target<0 && !nav->action) nav->action=0x4000;
#endif
        return;
    } else if(state==4) {if(mouse->confirm) nav->action=0x4000;}
    else if(state==9 || state==10) {
        scope=10;pivot=256;
        if(state==9) {
            hit=Port_SelectionMouseHit(mouse,10,"mc_map_chip_%02d",0,6,64,64,256);
            if(hit<0 && Port_SelectionMusicHit(mouse,1)) hit=6;
        } else {
            nav->negative=0x10;nav->positive=0x40;
            if(Port_SelectionMusicHit(mouse,0)) hit=view->current;
        }
    }
    nav->target=hit;
    if(mouse->wheel) nav->action=mouse->wheel<0 ? ((state==1 || state==5) ? 0x80:0x10):((state==1 || state==5) ? 0x20:0x40);
    if(hit<0 && mouse->confirm) {
        s32 up=Port_SelectionMouseHit(mouse,scope,"mc_yajirusi_up",0,1,32,32,pivot);
        s32 down=Port_SelectionMouseHit(mouse,scope,"mc_yajirusi_down",0,1,32,32,pivot);
        if(up>=0 || down>=0) nav->action=up>=0 ? 0x10:0x40;
    }
#ifdef __ANDROID__
    if(hit<0 && mouse->confirm && !mouse->wheel && !nav->action) nav->action=0x4000;
#endif
}
#endif
