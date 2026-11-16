#ifndef MENU_PORT_KEYBOARD_OPTIONS_H
#define MENU_PORT_KEYBOARD_OPTIONS_H
#include "port_pc_bindings.h"
extern int strcmp(const char *,const char *);
static s32 sOptionPcReset;
static s32 sOptionControllerFirst;
static s32 sOptionPcRow,sOptionPcFirst,sOptionPcWaiting,sOptionPcFooterSelected=-1;
static s32 sOptionPcClashPicker,sOptionPcClashClosing;
static s32 sOptionPcVirtualPicker;
static s32 Option_PcPickerValue(void) {
    return sOptionPcVirtualPicker ? Port_VirtualGamepad():Port_ClashSquare();
}
extern void Port_FlashOptionsCaptionMotion(s32 layer,f32 *x,f32 *y);
extern s32 Port_FlashOptionsCaptionStyle(s32 layer,u32 *tint);
extern f32 Port_FlashOptionsRowSlide(void *movie,s32 row);
static void Option_PcControllerSelect(s32 cursor) {
    if(cursor==gOption->cursor) return;
    if(gOption->cursor!=PORT_PLAYER1_CONTROLLER_ENTRY && gOption->cursor!=PORT_PLAYER2_CONTROLLER_ENTRY && gOption->cursor!=PORT_PC_CONTROLLER_ENTRY && gOption->cursor!=PORT_GAMEPAD_CONTROLLER_ENTRY &&
       gOption->cursor!=PORT_CLASH_CONTROLLER_ENTRY && gOption->cursor!=PORT_VIRTUAL_CONTROLLER_ENTRY)
        Option_PlateGoto(0,gOption->cursor==OPT_ITEM_CTRL_RESET ? OPT_CLIP_BOTTOM:OPT_CLIP_ROW_CTRL,"fl_off_start");
    gOption->cursor=cursor;
    if(cursor!=PORT_PLAYER1_CONTROLLER_ENTRY && cursor!=PORT_PLAYER2_CONTROLLER_ENTRY && cursor!=PORT_PC_CONTROLLER_ENTRY && cursor!=PORT_GAMEPAD_CONTROLLER_ENTRY &&
       cursor!=PORT_CLASH_CONTROLLER_ENTRY && cursor!=PORT_VIRTUAL_CONTROLLER_ENTRY) {
        Option_PlateGoto(0,cursor==OPT_ITEM_CTRL_RESET ? OPT_CLIP_BOTTOM:OPT_CLIP_ROW_CTRL,"fl_on_start");
        gOption->voiceLine=cursor==OPT_ITEM_KEYS ? 23:cursor==OPT_ITEM_VIB ? 24:10;
        OPT_VOICE(gOption->voiceLine);
    }
    Snd_PlaySe(1,0);
}
static const s32 sOptionControllerOrder[]={OPT_ITEM_VIB,
    PORT_CLASH_CONTROLLER_ENTRY,PORT_PLAYER1_CONTROLLER_ENTRY,PORT_PLAYER2_CONTROLLER_ENTRY,
#ifdef __ANDROID__
    PORT_VIRTUAL_CONTROLLER_ENTRY,
#endif
    OPT_ITEM_CTRL_RESET};
static const s32 sOptionPlayerControllerOrder[]={
#ifndef __ANDROID__
    PORT_PC_CONTROLLER_ENTRY,
#endif
    PORT_GAMEPAD_CONTROLLER_ENTRY};
static s32 Option_PcControllerCount(void) {
    return sOptionPcPlayerMenu ? sizeof(sOptionPlayerControllerOrder)/sizeof(s32):sizeof(sOptionControllerOrder)/sizeof(s32);
}
static s32 Option_PcControllerRow(s32 i) {
    return sOptionPcPlayerMenu ? sOptionPlayerControllerOrder[i]:sOptionControllerOrder[i];
}
static void Option_PcControllerEnsure(void) {
    s32 i;
    if(sOptionPcPlayerMenu) { sOptionControllerFirst=0;return; }
    for(i=0;i<Option_PcControllerCount();i++) if(Option_PcControllerRow(i)==gOption->cursor) break;
    if(i==Option_PcControllerCount()-1) return; /* native Default footer */
    if(i<sOptionControllerFirst) sOptionControllerFirst=i;
    if(i>=sOptionControllerFirst+4) sOptionControllerFirst=i-3;
}
static void Option_PcEnsure(void) {
    if(sOptionPcRow>=PORT_PC_ACTIONS) return;
    if(sOptionPcRow<sOptionPcFirst) sOptionPcFirst=sOptionPcRow;
    if(sOptionPcRow>=sOptionPcFirst+4) sOptionPcFirst=sOptionPcRow-3;
}
static s32 Option_PcInput(void) {
    s32 i,result;
    if(!Port_ZenkaiEnabled()) return 0;
    if(sOptionPcClashPicker && gOption->state==OPT_ITEM_CTRL) {
        if(gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT)) {
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");gOption->value^=1;
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");Snd_PlaySe(1,0);
        } else if(gPad[0].gamePressed&(PADG_CROSS|PADG_TRIANGLE)) {
            if(gPad[0].gamePressed&PADG_CROSS) {
                if(gOption->value!=Option_PcPickerValue()) {
                    if(sOptionPcVirtualPicker) Port_VirtualGamepadApply(gOption->value);
                    else Port_ClashSquareApply(gOption->value);
                    gOption->dirty=1;
                }
                Option_PlateGoto(0,OPT_CLIP_PICK,"fl_ok");
            } else {
                Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
                gOption->value=Option_PcPickerValue();Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");
            }
            sOptionPcClashPicker=0;sOptionPcClashClosing=1;
            Flash_GotoLabel(gOption->flash,"fl_value_out",1);
            Snd_PlaySe(1,(gPad[0].gamePressed&PADG_TRIANGLE) ? 2:1);
        }
        return 1;
    }
    if(sOptionPcClashClosing && gOption->state==OPT_ITEM_CTRL) { sOptionPcClashClosing=0;return 1; }
    if(gOption->state==OPT_ITEM_CTRL) {
        if(gOption->cursor==OPT_ITEM_KEYS) gOption->cursor=OPT_ITEM_VIB;
        if(sOptionPcPlayerMenu && (gPad[0].gamePressed&PADG_TRIANGLE)) {
            sOptionPcPlayerMenu=0;gOption->cursor=sOptionPcPlayer ? PORT_PLAYER2_CONTROLLER_ENTRY:PORT_PLAYER1_CONTROLLER_ENTRY;
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);Snd_PlaySe(1,2);return 1;
        }
        if(gPad[0].gameRepeat&(PADG_UP|PADG_DOWN)) {
            s32 count=Option_PcControllerCount();
            for(i=0;i<count;i++) if(gOption->cursor==Option_PcControllerRow(i)) break;
            Option_PcControllerSelect(Option_PcControllerRow((i+(gPad[0].gameRepeat&PADG_UP ? count-1:1))%count));
            Option_PcControllerEnsure();return 1;
        }
        if(!sOptionPcPlayerMenu && (gOption->cursor==PORT_PLAYER1_CONTROLLER_ENTRY || gOption->cursor==PORT_PLAYER2_CONTROLLER_ENTRY) && (gPad[0].gamePressed&PADG_CROSS)) {
            sOptionPcPlayer=gOption->cursor==PORT_PLAYER2_CONTROLLER_ENTRY;
            sOptionPcPlayerMenu=1;sOptionControllerFirst=0;gOption->cursor=Option_PcControllerRow(0);
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);Snd_PlaySe(1,1);return 1;
        }
        if((gOption->cursor==PORT_CLASH_CONTROLLER_ENTRY
#ifdef __ANDROID__
            || gOption->cursor==PORT_VIRTUAL_CONTROLLER_ENTRY
#endif
           ) && (gPad[0].gamePressed&PADG_CROSS)) {
            sOptionPcVirtualPicker=gOption->cursor==PORT_VIRTUAL_CONTROLLER_ENTRY;
            sOptionPcClashPicker=1;gOption->picker=3;
            gOption->value=0;Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
            gOption->value=1;Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
            gOption->value=Option_PcPickerValue();Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");
            Flash_GotoLabel(gOption->flash,"fl_value_in",1);Snd_PlaySe(1,1);return 1;
        }
        if((gOption->cursor==PORT_PC_CONTROLLER_ENTRY || gOption->cursor==PORT_GAMEPAD_CONTROLLER_ENTRY) && (gPad[0].gamePressed&PADG_CROSS)) {
            sOptionPcGamepad=gOption->cursor==PORT_GAMEPAD_CONTROLLER_ENTRY;
            gOption->state=PORT_PC_OPTIONS_PAGE;sOptionPcRow=sOptionPcFirst=sOptionPcWaiting=0;sOptionPcFooterSelected=-1;sOptionPcReset=0;
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);Snd_PlaySe(1,1);return 1;
        }
        return 0; /* existing native controller pickers and Back */
    }
    if(gOption->state!=PORT_PC_OPTIONS_PAGE) return 0;
    if(sOptionPcReset) { Option_UpdateReset();return 1; }
    if(sOptionPcWaiting) {
        result=PortInput_PcCapturePoll();
        if(result) { sOptionPcWaiting=0;if(result>0) gOption->dirty=1;Snd_PlaySe(1,result>0 ? 1:2); }
        return 1;
    }
    if(gPad[0].gamePressed&PADG_TRIANGLE) {
        gOption->state=OPT_ITEM_CTRL;gOption->cursor=sOptionPcGamepad ? PORT_GAMEPAD_CONTROLLER_ENTRY:PORT_PC_CONTROLLER_ENTRY;
        Flash_GotoLabel(gOption->flash,"fl_menu_change",1);
        Snd_PlaySe(1,2);
    } else if(sOptionPcRow==PORT_PC_ACTIONS && (gPad[0].gamePressed&PADG_CROSS)) {
        sOptionPcReset=1;gOption->resetStep=0;Snd_PlaySe(1,1);
    } else if(gPad[0].gameRepeat&(PADG_UP|PADG_DOWN)) {
        sOptionPcRow=(sOptionPcRow+(gPad[0].gameRepeat&PADG_UP ? PORT_PC_ACTIONS:1))%(PORT_PC_ACTIONS+1);
        Option_PcEnsure();Snd_PlaySe(1,0);
    } else if((gPad[0].gamePressed&PADG_CROSS) && sOptionPcRow<PORT_PC_ACTIONS) {
        if(sOptionPcGamepad) PortInput_GamepadCaptureStart(sOptionPcRow+PORT_PC_STORAGE_ACTIONS*sOptionPcPlayer);else PortInput_PcCaptureStart(sOptionPcRow+PORT_PC_STORAGE_ACTIONS*sOptionPcPlayer);sOptionPcWaiting=1;Snd_PlaySe(1,1);
    }
    return 1;
}
static void Option_PcMouse(PortMenuMouse *mouse) {
    s32 i;
    if(sOptionPcWaiting) return;
    if(sOptionPcClashPicker && gOption->state==OPT_ITEM_CTRL) { Option_PortPickerMouse(mouse);return; }
    if(sOptionPcClashClosing) return;
    if(mouse->cancel) { gPad[0].gamePressed|=PADG_TRIANGLE;return; }
    if((gOption->state==PORT_PC_OPTIONS_PAGE || !sOptionPcPlayerMenu) && Port_FlashMenuMouseHit(68,gOption->state==PORT_PC_OPTIONS_PAGE ? "mc_video_default":"mc_bottom_plate_1",512,32,0,mouse->x,mouse->y) &&
       (mouse->moved || mouse->confirm) && !mouse->wheel) {
        if(gOption->state==OPT_ITEM_CTRL) Option_PcControllerSelect(OPT_ITEM_CTRL_RESET);
        else {
            if(sOptionPcRow!=PORT_PC_ACTIONS) Snd_PlaySe(1,0);
            sOptionPcRow=PORT_PC_ACTIONS;
        }
        if(mouse->confirm) gPad[0].gamePressed|=PADG_CROSS;
        return;
    }
    if((gOption->state==PORT_PC_OPTIONS_PAGE || (!sOptionPcPlayerMenu && Option_PcControllerCount()>5)) && (mouse->moved || mouse->confirm) && !mouse->wheel) {
        extern s32 Port_PageAspectMilli(void),Port_TitleMainChars43(void),Port_Menus43(void);
        f32 scale=Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1333 ?
            1333.333f/Port_PageAspectMilli():1;
        f32 x=512+(272-512)*scale;
        s32 first=gOption->state==PORT_PC_OPTIONS_PAGE ? sOptionPcFirst:sOptionControllerFirst;
        s32 count=gOption->state==PORT_PC_OPTIONS_PAGE ? PORT_PC_ACTIONS:Option_PcControllerCount()-1;
        for(i=0;i<2;i++) {
            f32 y=i ? 252:64;
            if((i ? first<count-4:first>0) &&
               mouse->x>=x-16*scale && mouse->x<x+16*scale && mouse->y>=y-16 && mouse->y<y+16) {
                PortMenuMouse_SetHover(1);
                if(mouse->confirm) {
                    if(gOption->state==PORT_PC_OPTIONS_PAGE) sOptionPcRow=first+(i ? 3:0);
                    else gOption->cursor=Option_PcControllerRow(first+(i ? 3:0));
                    gPad[0].gameRepeat|=i ? PADG_DOWN:PADG_UP;
                }
                return;
            }
        }
    }
    for(i=0;i<4;i++) {
        if(gOption->state==PORT_PC_OPTIONS_PAGE && sOptionPcFirst+i>=PORT_PC_ACTIONS) break;
        /* These rows share the native Options plate's coordinates/pivot. */
        if(Option_PortHit(mouse,72+i*44,104+i*44)) {
            if(gOption->state==OPT_ITEM_CTRL) {
                if(sOptionControllerFirst+i>=Option_PcControllerCount()-(sOptionPcPlayerMenu ? 0:1)) break;
                Option_PcControllerSelect(Option_PcControllerRow(sOptionControllerFirst+i));
            } else {
                if(sOptionPcRow!=sOptionPcFirst+i) Snd_PlaySe(1,0);
                sOptionPcRow=sOptionPcFirst+i;
            }
            if(mouse->confirm) gPad[0].gamePressed|=PADG_CROSS;
            break;
        }
    }
    if(mouse->wheel) gPad[0].gameRepeat|=mouse->wheel<0 ? PADG_UP:PADG_DOWN;
}
static s32 Option_PcFooter(void) {
    MFlashRef ref;MFlashUv uv;
    if(!Port_ZenkaiEnabled()) return 0;
    if(gOption->state==OPT_ITEM_CTRL && (sOptionPcClashPicker || sOptionPcClashClosing)) {
        Port_FlashOptionsVideoPicker(sOptionPcVirtualPicker ? -3:-2);return 1;
    }
    if(gOption->state==OPT_ITEM_CTRL && sOptionPcPlayerMenu) { Port_FlashOptionsVideoFooter(3);return 1; }
    if(gOption->state!=PORT_PC_OPTIONS_PAGE) return 0;
    Port_FlashOptionsVideoPicker(-1);Port_FlashOptionsVideoFooter(1);
    uv.x0=uv.y0=0;uv.x1=256;uv.y1=32;
    Option_SetBottomText(gOption->flash,&ref,"mc_bottom_plate_1",uv);
    Option_SetBottomText(gOption->flash,&ref,"mc_bottom_plate_2",uv);
    if(sOptionPcFooterSelected!=(sOptionPcRow==PORT_PC_ACTIONS)) {
        sOptionPcFooterSelected=sOptionPcRow==PORT_PC_ACTIONS;
        Option_PlateGoto(0,OPT_CLIP_BOTTOM,sOptionPcFooterSelected ? "fl_on_start":"fl_off_start");
    }
    return 1;
}
static s32 Option_PcDraw(void) {
    s32 i;
    if(!Port_ZenkaiEnabled() || (gOption->page!=PORT_PC_CONTROLLER_PAGE && gOption->state!=PORT_PC_OPTIONS_PAGE)) return 0;
    Option_PortArtInit();Ps_SetShift(0);
    if(gOption->state!=PORT_PC_OPTIONS_PAGE) {
        Option_PcControllerEnsure();
        for(i=0;i<4 && sOptionControllerFirst+i<Option_PcControllerCount()-(sOptionPcPlayerMenu ? 0:1);i++) {
            s32 row=Option_PcControllerRow(sOptionControllerFirst+i);
            const char *id=row==OPT_ITEM_VIB ? "controls/vibration":row==PORT_PLAYER1_CONTROLLER_ENTRY ? "controls/player1/title":
                row==PORT_PLAYER2_CONTROLLER_ENTRY ? "controls/player2/title":row==PORT_PC_CONTROLLER_ENTRY ? "controls/column/mouse-keyboard":
                row==PORT_GAMEPAD_CONTROLLER_ENTRY ? "controls/column/gamepad":row==PORT_CLASH_CONTROLLER_ENTRY ? "controls/clash-mode/title":"controls/virtual-gamepad/title";
            const char *name=row==OPT_ITEM_VIB ? "Vibration":row==PORT_PLAYER1_CONTROLLER_ENTRY ? "1P Controller":
                row==PORT_PLAYER2_CONTROLLER_ENTRY ? "2P Controller":row==PORT_PC_CONTROLLER_ENTRY ? "Mouse/Keyboard":
                row==PORT_GAMEPAD_CONTROLLER_ENTRY ? "Gamepad":row==PORT_CLASH_CONTROLLER_ENTRY ? "Clash Input":"Virtual Gamepad";
            char label[128];
            MenuTranslation_PortTextCopy(id,name,label,sizeof(label));
            Option_PortButton(i,NULL,92-i*20+Port_FlashOptionsRowSlide(gOption->flash,i),72+i*44,label,gOption->cursor==row ? 1:0);
        }
        if(!sOptionPcPlayerMenu) {
            if(sOptionControllerFirst>0) Port_FlashOptionsChevron(gOption->flash,0,272,64);
            if(sOptionControllerFirst<Option_PcControllerCount()-5) Port_FlashOptionsChevron(gOption->flash,1,272,252);
        }
    } else {
        Option_PcEnsure();
        for(i=0;i<4 && sOptionPcFirst+i<PORT_PC_ACTIONS;i++) {
            s32 row=sOptionPcFirst+i,id;char binding[128],label[128];
            f32 slide=Port_FlashOptionsRowSlide(gOption->flash,i);
            f32 scale=19,x,y=88+i*44;
            static const char *const actionIds[PORT_PC_ACTIONS]={
                "controls/pc-action/ki-charge","controls/pc-action/blaster-wave","controls/pc-action/ki-blast",
                "controls/pc-action/fight","controls/pc-action/descend","controls/pc-action/ascend",
                "controls/pc-action/guard","controls/pc-action/dash","controls/pc-action/move-forward",
                "controls/pc-action/move-backward","controls/pc-action/move-left","controls/pc-action/move-right",
                "controls/pc-action/camera-left","controls/pc-action/camera-right","controls/pc-action/change-camera",
                "controls/pc-action/switch-character","controls/pc-action/transform-progress"
            };
            MenuTranslation_PortTextCopy(actionIds[row],sPortPcLabels[row],label,sizeof(label));
            Option_PortButton(i,NULL,92-i*20+slide,72+i*44,label,
                sOptionPcRow==row ? 1:0);
            if(sOptionPcGamepad) PortInput_GamepadBindingName(row+PORT_PC_STORAGE_ACTIONS*sOptionPcPlayer,binding,sizeof(binding));
            else PortInput_PcBindingName(row+PORT_PC_STORAGE_ACTIONS*sOptionPcPlayer,binding,sizeof(binding));
            {
                s32 layer,waiting=sOptionPcWaiting && row==sOptionPcRow;
                s32 disabled=!waiting && !strcmp(binding,"Unbound");
                const char *caption=waiting ? "...":binding;
                for(layer=0;layer<2;layer++) {
                    u32 tint; s32 alpha=Port_FlashOptionsCaptionStyle(layer,&tint);
                    if(!alpha) continue;
                    id=PortTbo_TextId(caption,layer ? 4:3);
                    if(id<0) continue;
                    scale=19;y=88+i*44;
                    if(PortTbo_TextInfo(id,7)*scale/1000.0f>138) scale=138000.0f/PortTbo_TextInfo(id,7);
                    x=432+slide-scale*(PortTbo_TextInfo(id,5)+0.5f*PortTbo_TextInfo(id,7))/1000.0f;
                    y-=scale*(PortTbo_TextInfo(id,6)+0.5f*PortTbo_TextInfo(id,8))/1000.0f;
                    if(waiting) {
                        f32 dx,dy;Port_FlashOptionsCaptionMotion(layer,&dx,&dy);x+=dx;y+=dy;
                    }
                    /* Native disabled text uses RGB 0x58, with unchanged opacity. */
                    if(disabled) {
                        u32 r=(tint&255)*88/128,g=((tint>>8)&255)*88/128,b=((tint>>16)&255)*88/128;
                        tint=r|(g<<8)|(b<<16);
                    }
                    Port_GsMarker(0x1B);
                    Ps_SpriteScreen(id,x,y,x+512*scale/19,y+32*scale/19,0,0,1,1,tint,alpha);
                    Port_GsMarker(0x13);
                }
            }
        }
        /* Hidden native Controller rows need not reach a resting frame.
           Scroll indicators belong to this page, independent of that timeline. */
        if(!sOptionPcWaiting) {
            if(sOptionPcFirst>0) Port_FlashOptionsChevron(gOption->flash,0,272,64);
            if(sOptionPcFirst<PORT_PC_ACTIONS-4) Port_FlashOptionsChevron(gOption->flash,1,272,252);
        }
    }
    return 1;
}
static s32 Option_PcHelp(void) {
    extern s32 MsgWin_PortLineRoom(s32 right);
    extern void Port_MsgWinDrawString43(u16 *text);
    static u16 text[2][200];static char previous[256],translated[256];static s32 current;const char *line,*id;
    if(!Port_ZenkaiEnabled()) return 0;
    if(gOption->state==PORT_PC_OPTIONS_PAGE) {
        if(sOptionPcGamepad) {
            id=sOptionPcWaiting ? "controls/help/gamepad/waiting":"controls/help/gamepad";
            line=sOptionPcWaiting ? "Press one or more gamepad inputs. Delete clears the binding.":"Select an action and press a gamepad input.";
        } else {
            id=sOptionPcWaiting ? "controls/help/keyboard/waiting":"controls/help/keyboard";
            line=sOptionPcWaiting ? "Press a key or mouse input. Esc cancels. Delete clears the binding.":"Select an action, then press a key or mouse input.";
        }
    } else if(gOption->state==OPT_ITEM_CTRL && (gOption->cursor==PORT_PLAYER1_CONTROLLER_ENTRY || gOption->cursor==PORT_PLAYER2_CONTROLLER_ENTRY)) {
        id=gOption->cursor==PORT_PLAYER1_CONTROLLER_ENTRY ? "controls/player1/help":"controls/player2/help";
        line=gOption->cursor==PORT_PLAYER1_CONTROLLER_ENTRY ? "Configure 1P controller.":"Configure 2P controller.";
    } else if(gOption->state==OPT_ITEM_CTRL && gOption->cursor==PORT_VIRTUAL_CONTROLLER_ENTRY) {
        id="controls/virtual-gamepad/help";
        line="Show or hide the virtual gamepad during battle.";
    } else if(gOption->state==OPT_ITEM_CTRL && (gOption->cursor==PORT_PC_CONTROLLER_ENTRY ||
               gOption->cursor==PORT_GAMEPAD_CONTROLLER_ENTRY || gOption->cursor==PORT_CLASH_CONTROLLER_ENTRY)) {
        id=gOption->cursor==PORT_CLASH_CONTROLLER_ENTRY ? "controls/help/gamepad/clash-mode":
            gOption->cursor==PORT_GAMEPAD_CONTROLLER_ENTRY ? "controls/help/controller/gamepad":"controls/help/controller/keyboard";
        line=gOption->cursor==PORT_CLASH_CONTROLLER_ENTRY ? "Choose how to struggle in a clash when using a gamepad.":
            gOption->cursor==PORT_GAMEPAD_CONTROLLER_ENTRY ? "Map battle actions, movement and camera controls to gamepad inputs.":
            "Map battle actions, movement and camera controls to keyboard and mouse inputs.";
    }
    else return 0;
    MenuTranslation_PortTextCopy(id,line,translated,sizeof(translated));
    if(strcmp(translated,previous)!=0) { current=1-current;strcpy(previous,translated); }
    Option_PortWrapHelp(translated,text[current],200,MsgWin_PortLineRoom(400));Port_MsgWinDrawString43(text[current]);return 1;
}
#endif
