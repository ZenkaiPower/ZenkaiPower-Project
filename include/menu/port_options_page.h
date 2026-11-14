#ifndef MENU_PORT_OPTIONS_PAGE_H
#define MENU_PORT_OPTIONS_PAGE_H
#include "menu/port_screen.h"
#include "menu/port_video_options.h"
extern int Port_OptionsVideoGet(int row);
extern void Port_OptionsVideoChange(int row,int direction);
extern void Port_OptionsVideoDefaults(void);
extern void Port_OptionsBattleEffectsDefaults(void);
extern void Port_OptionsCategoryDefaults(int category);
extern int Port_OptionsAvailable(int row);
extern int Port_OptionsSaveSync(void *save);
extern void Port_FlashOptionsPlateDim(s32 disabled);
extern void Port_FlashOptionsCaptionMotion(s32 layer,f32 *x,f32 *y);
extern s32 Port_FlashOptionsCaptionStyle(s32 layer,u32 *tint);
extern void Port_GsMarker(s32 effect);
extern s32 Port_ZenkaiEnabled(void);
static s32 sOptionVideoRow,sOptionVideoFirst;
static s32 sOptionPickerClosing;
static s32 sOptionVideoEditing,sOptionVideoOriginal;
static s32 sOptionSettingsCategory; /* Index into the shared category descriptors. */
static s32 sOptionPortArtReady;
static MFlash sOptionPlateMovies[4];
static s32 sOptionPlateSelected[4];
static s32 sOptionFooterSelected;
static s32 sOptionVideoPhase,sOptionVideoClosing;
static s32 sOptionTopLeaving;
static s32 sOptionReturningTop;
static s32 sOptionFromPage;
static s32 sOptionTopFirst;
static f32 sOptionTopScroll;
static const s32 sOptionTopOrder[]={1,2,3,4,PORT_OPTION_MORE,5};
extern void Port_FlashOptionsViewport(s32 enabled);
extern void Port_FlashOptionsChevronFrame(s32 page);
extern void Port_FlashOptionsChevron(void *movie,s32 down,f32 x,f32 y);
static s32 Option_PortSettingCount(void) {
    return sPortOptionCounts[sOptionSettingsCategory];
}
static s32 Option_PortSettingRow(s32 row) {
    return Port_OptionsCategoryRow(sOptionSettingsCategory,row);
}
static const PortVideoOption *Option_PortDescriptor(s32 row) {
    return Port_OptionsDescriptor(Option_PortSettingRow(row));
}
static s32 Option_PortSettingGet(s32 row) {
    return Port_OptionsVideoGet(Option_PortSettingRow(row));
}
static void Option_PortSettingChange(s32 row,s32 direction) {
    Port_OptionsVideoChange(Option_PortSettingRow(row),direction);
}
static void Option_PortCancelEdit(void) {
    if(sOptionVideoEditing) {
        s32 count=Option_PortDescriptor(sOptionVideoRow)->count;
        s32 steps=(sOptionVideoOriginal-Option_PortSettingGet(sOptionVideoRow)+count)%count;
        while(steps-->0) Option_PortSettingChange(sOptionVideoRow,1);
        sOptionVideoEditing=0;
    }
}
static void Option_PortDefaults(void) {
    Port_OptionsCategoryDefaults(sOptionSettingsCategory);
}
/* Both save policies defer persistence to the native Options exit flow. */
static s32 Option_PortSaveChanges(void) {
    if(Port_OptionsSaveSync(gSaveData)) gOption->dirty=1;
    return 0;
}
static void Option_PortTopEnsure(void) {
    s32 i,old=sOptionTopFirst;
    for(i=0;i<6;i++) if(sOptionTopOrder[i]==gOption->cursor) break;
    if(i>=5) return; /* EXIT stays in the native footer. */
    if(i<sOptionTopFirst) sOptionTopFirst=i;
    if(i>=sOptionTopFirst+4) sOptionTopFirst=i-3;
    if(sOptionTopFirst!=old) sOptionTopScroll=44.0f*(sOptionTopFirst-old);
}
static s32 Option_PortTopMouse(PortMenuMouse *mouse) {
    extern s32 Port_PageAspectMilli(void);
    extern s32 Port_TitleMainChars43(void);
    extern s32 Port_Menus43(void);
    f32 narrow=Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1333 ?
        1333.333f/Port_PageAspectMilli():1.0f;
    s32 i;
    Option_PortTopEnsure();
    if(mouse->cancel) gPad[0].gamePressed|=PADG_TRIANGLE;
    if(mouse->wheel) gPad[0].gameRepeat|=mouse->wheel<0 ? PADG_UP:PADG_DOWN;
    if((mouse->moved || mouse->confirm) && !mouse->wheel &&
       Port_FlashMenuMouseHit(68,"mc_bottom_plate_1",512,32,0,mouse->x,mouse->y)) return 5;
    if(sOptionTopScroll>0.5f || sOptionTopScroll< -0.5f) return -1;
    if(!(mouse->moved || mouse->confirm) || mouse->wheel) return -1;
    for(i=0;i<2;i++) {
        /* Center of the visible list bounds: x=32..512. */
        f32 cx=512+(272-512)*narrow,cy=i ? 252:64;
        if((i ? sOptionTopFirst<1:sOptionTopFirst>0) &&
           mouse->x>=cx-16*narrow && mouse->x<cx+16*narrow && mouse->y>=cy-16 && mouse->y<cy+16) {
            PortMenuMouse_SetHover(1);
            if(mouse->confirm) {
                gOption->cursor=sOptionTopOrder[i ? sOptionTopFirst+3:sOptionTopFirst];
                gPad[0].gameRepeat|=i ? PADG_DOWN:PADG_UP;
            }
            return -1;
        }
    }
    for(i=0;i<4;i++) {
        f32 left=512+(92-i*20-512)*narrow,y=72+i*44;
        if(mouse->x>=left && mouse->x<512 && mouse->y>=y && mouse->y<y+32) {
            PortMenuMouse_SetHover(1);
            return sOptionTopOrder[sOptionTopFirst+i];
        }
    }
    return -1;
}
extern void Port_FlashOptionsPlate(void *movie,void *reference,f32 x,f32 y,const char *label);
extern void Port_FlashOptionsVideoFooter(s32 enabled);
extern void Port_FlashOptionsVideoPicker(s32 row);
extern void Port_FlashOptionsHeading(const char *text);
extern int MenuTranslation_PortTextCopy(const char *id,const char *fallback,char *out,s32 capacity);
extern int MenuTranslation_Ensure(void);
extern const char *MenuTranslation_Text(const char *id);
static void Option_PortDraw(void);
extern s32 PortTbo_TextId(const char *text,s32 kind);
extern s32 PortTbo_TextInfo(s32 id,s32 what);
static void Option_PortArtInit(void) {
    MFlashRef ref;
    s32 variant,frame;
    if(sOptionPortArtReady || !Port_ZenkaiEnabled()) return;
    Port_FlashOptionsChevronFrame(-1);
    sOptionFooterSelected=0;sOptionVideoEditing=0;
    sOptionPortArtReady=1;sOptionTopLeaving=0;sOptionVideoPhase=sOptionVideoClosing=0;sOptionFromPage=OPT_ST_TOP;sOptionTopFirst=0;sOptionTopScroll=0;
    for(variant=0;variant<4;variant++) {
        memset(&sOptionPlateMovies[variant],0,sizeof(MFlash));
        Flash_Create(&sOptionPlateMovies[variant],MPACK_AT(gOption->res,8),gOption->tex);
        Flash_Play(&sOptionPlateMovies[variant],1);
        for(frame=0;frame<120;frame++) Flash_Advance(&sOptionPlateMovies[variant]);
        Flash_FindLabel(&sOptionPlateMovies[variant],NULL,"mc_menu_plate_4",&ref);
        Flash_ClipGotoLabel(&sOptionPlateMovies[variant],&ref,"fl_off_start");
        for(frame=0;frame<24;frame++) Flash_Advance(&sOptionPlateMovies[variant]);
        sOptionPlateSelected[variant]=0;
        {
            MFlashUv uv;
            memset(&uv,0,sizeof(uv));uv.x1=512;uv.y1=32;
            Flash_FindLabel(&sOptionPlateMovies[variant],"mc_menu_plate_4","mc_menu_text_off",&ref);
            Flash_ClipSetUv(&sOptionPlateMovies[variant],&ref,&uv);
            Flash_FindLabel(&sOptionPlateMovies[variant],"mc_menu_plate_4","mc_menu_text_on",&ref);
            Flash_ClipSetUv(&sOptionPlateMovies[variant],&ref,&uv);
        }
    }

}
static void Option_PortArtTerm(void) {
    if(!sOptionPortArtReady) return;
    sOptionPortArtReady=0;
    {
        s32 i;
        for(i=0;i<4;i++) Flash_Destroy(&sOptionPlateMovies[i]);
    }
}

/* Page coordinates are converted once, identically for drawing and hit tests. */
static s32 Option_PortHit(PortMenuMouse *mouse,f32 y0,f32 y1) {
    Ps_PageSet();
    if(!(mouse->moved || mouse->confirm) || mouse->wheel) return 0;
    {
        extern s32 Port_PageAspectMilli(void);
        extern s32 Port_TitleMainChars43(void);
        extern s32 Port_Menus43(void);
        f32 scale=Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1333 ?
            1333.333f/Port_PageAspectMilli():1.0f;
        f32 left=512.0f+(((gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_MORE_PAGE || gOption->state==OPT_ITEM_CTRL || gOption->state==PORT_PC_OPTIONS_PAGE) ? 92.0f-20.0f*((y0-72.0f)/44.0f):gOption->state==OPT_ST_TOP ? 18.0f:98.0f)-512.0f)*scale;
        if(mouse->x<left || mouse->x>=512 || mouse->y<y0 || mouse->y>=y1) return 0;
    }
    PortMenuMouse_SetHover(1);
    return 1;
}
static void Option_PortPlates(s32 visible) {
    MFlashRef ref;
    char name[40];
    s32 i;
    for(i=1;i<=4;i++) {
        sprintf(name,"mc_menu_plate_%d",i);
        Flash_FindLabel(gOption->flash,NULL,name,&ref);
        Flash_ClipSetFlags(gOption->flash,&ref,2,visible);
    }
    /* The native footer keeps its authored page-transition timeline. */
}
static void Option_PortVideoEnsure(void) {
    if(sOptionVideoRow>=Option_PortSettingCount()) return;
    if(sOptionVideoRow<sOptionVideoFirst) sOptionVideoFirst=sOptionVideoRow;
    if(sOptionVideoRow>=sOptionVideoFirst+PORT_VIDEO_VISIBLE_ROWS)
        sOptionVideoFirst=sOptionVideoRow-PORT_VIDEO_VISIBLE_ROWS+1;
}
static void Option_PortVideoMouse(PortMenuMouse *mouse) {
    s32 i;
    if(!Port_ZenkaiEnabled() || sOptionVideoClosing || sOptionPickerClosing || sOptionVideoPhase<16) return;
    if(mouse->cancel) { gPad[0].gamePressed|=PADG_TRIANGLE;return; }
    if(!sOptionVideoEditing && (mouse->moved || mouse->confirm) && !mouse->wheel &&
       Port_FlashMenuMouseHit(68,"mc_video_default",512,32,0,mouse->x,mouse->y)) {
        if(!sOptionVideoEditing || mouse->confirm) {
            sOptionVideoEditing=0;
            s32 footerRow=gOption->state==PORT_OPTION_MORE_PAGE ? PORT_OPTION_CATEGORY_COUNT:Option_PortSettingCount();
            if(sOptionVideoRow!=footerRow) Snd_PlaySe(1,0);
            sOptionVideoRow=footerRow;
            if(mouse->confirm) gPad[0].gamePressed|=PADG_CROSS;
        }
        return;
    }
    if(gOption->state==PORT_OPTION_VIDEO_PAGE && !sOptionVideoEditing) {
        extern s32 Port_PageAspectMilli(void);
        extern s32 Port_TitleMainChars43(void);
        extern s32 Port_Menus43(void);
        f32 narrow=Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1333 ? 1333.333f/Port_PageAspectMilli():1;
        f32 cx=512+(272-512)*narrow;
        Option_PortVideoEnsure();
        for(i=0;i<2;i++) {
            f32 cy=i ? 252:64;
            if((i ? sOptionVideoFirst<Option_PortSettingCount()-PORT_VIDEO_VISIBLE_ROWS:sOptionVideoFirst>0) &&
               (mouse->moved || mouse->confirm) && !mouse->wheel &&
               mouse->x>=cx-16*narrow && mouse->x<cx+16*narrow && mouse->y>=cy-16 && mouse->y<cy+16) {
                PortMenuMouse_SetHover(1);
                if(mouse->confirm) {
                    sOptionVideoEditing=0;
                    sOptionVideoRow=i ? sOptionVideoFirst+PORT_VIDEO_VISIBLE_ROWS-1:sOptionVideoFirst;
                    gPad[0].gameRepeat|=i ? PADG_DOWN:PADG_UP;
                }
                return;
            }
        }
    }
    for(i=0;i<(gOption->state==PORT_OPTION_MORE_PAGE ? PORT_OPTION_CATEGORY_COUNT:PORT_VIDEO_VISIBLE_ROWS);i++) {
        f32 y=72+i*44;
        if(gOption->state!=PORT_OPTION_MORE_PAGE && sOptionVideoFirst+i>=Option_PortSettingCount()) break;
        if(!Option_PortHit(mouse,y,y+32)) continue;
        {
            s32 row=gOption->state==PORT_OPTION_MORE_PAGE ? i:sOptionVideoFirst+i;
            if(sOptionVideoEditing && row!=sOptionVideoRow) break;
            if(row!=sOptionVideoRow) {
                sOptionVideoEditing=0;
                Snd_PlaySe(1,0);
            }
            sOptionVideoRow=row;
        }
        if(mouse->confirm) {
            extern s32 Port_PageAspectMilli(void);
            extern s32 Port_TitleMainChars43(void);
            extern s32 Port_Menus43(void);
            f32 scale=Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1333 ?
                1333.333f/Port_PageAspectMilli():1.0f;
            f32 localX=512+(mouse->x-512)/scale;
            if(sOptionVideoEditing && localX>=328 && localX<392)
                gPad[0].gameRepeat|=PADG_LEFT;
            else if(sOptionVideoEditing && localX>=448 && localX<512)
                gPad[0].gameRepeat|=PADG_RIGHT;
            else gPad[0].gamePressed|=PADG_CROSS;
        }
        break;
    }
    if(mouse->cancel) gPad[0].gamePressed|=PADG_TRIANGLE;
    if(mouse->wheel) gPad[0].gameRepeat|=sOptionVideoEditing ?
        (mouse->wheel<0 ? PADG_LEFT:PADG_RIGHT):(mouse->wheel<0 ? PADG_UP:PADG_DOWN);
}
static void Option_PortPickerMouse(PortMenuMouse *mouse) {
    s32 hit;
    if(sOptionPickerClosing) return;
    hit=Port_SelectionMouseHit(mouse,68,"mc_select_plate_%d",1,2,256,32,256);
    if(hit>0) {
        hit--;
        if(hit!=gOption->value) {
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
            gOption->value=hit;
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");Snd_PlaySe(1,0);
        }
        if(mouse->confirm) gPad[0].gamePressed|=PADG_CROSS;
    }
    if(mouse->cancel) gPad[0].gamePressed|=PADG_TRIANGLE;
    if(mouse->wheel) gPad[0].gameRepeat|=mouse->wheel<0 ? PADG_LEFT:PADG_RIGHT;
}
static s32 Option_PortVideoEditing(s32 row) {
    return gOption->state==PORT_OPTION_VIDEO_PAGE && sOptionVideoEditing && sOptionVideoRow==row &&
        !sOptionVideoClosing && !sOptionPickerClosing;
}
static s32 Option_PortVideoFocus(s32 row) {
    return sOptionVideoRow==row ? (gOption->state==PORT_OPTION_VIDEO_PICKER || Option_PortVideoEditing(row) ? 2:1):0;
}
/* Return 1 when the PC page consumed this frame's native input. */
static s32 Option_PortInput(void) {
    s32 i,next;
    if(Option_PcInput()) return 1;
    if(!Port_ZenkaiEnabled()) {
        Option_PortCancelEdit();
        if(gOption->state==PORT_OPTION_MORE_PAGE || gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_VIDEO_RESET || gOption->state==PORT_OPTION_VIDEO_PICKER) {
            gOption->state=OPT_ST_TOP;gOption->cursor=4;
            Option_PortPlates(1);MsgWin_Open();
            Option_PlateGoto(0,OPT_CLIP_ROW,"fl_on_start");
            return 1;
        }
        if(gOption->state==OPT_ST_TOP && gOption->cursor==PORT_OPTION_MORE) {
            gOption->cursor=4;
            Option_PlateGoto(0,OPT_CLIP_ROW,"fl_on_start");
        }
        return 0;
    }
    if(gOption->state==PORT_OPTION_MORE_PAGE) {
        if(sOptionVideoClosing || sOptionVideoPhase<16) return 1;
        if((gPad[0].gamePressed&PADG_TRIANGLE) ||
           (sOptionVideoRow==PORT_OPTION_CATEGORY_COUNT && (gPad[0].gamePressed&PADG_CROSS))) {
            sOptionVideoClosing=1;
            gOption->fromPage=1;gOption->page=0;
            Option_PlateGoto(0,OPT_CLIP_BOTTOM,"fl_off_start");
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);
            Snd_PlaySe(1,(gPad[0].gamePressed&PADG_TRIANGLE) ? 2:1);
        } else if(gPad[0].gameRepeat&(PADG_UP|PADG_DOWN)) {
            sOptionVideoRow=(sOptionVideoRow+((gPad[0].gameRepeat&PADG_UP) ? PORT_OPTION_CATEGORY_COUNT:1))%(PORT_OPTION_CATEGORY_COUNT+1);Snd_PlaySe(1,0);
        } else if(gPad[0].gamePressed&PADG_CROSS) {
            sOptionVideoEditing=0;
            sOptionSettingsCategory=sOptionVideoRow;
            sOptionFromPage=PORT_OPTION_MORE_PAGE;
            gOption->state=PORT_OPTION_VIDEO_PAGE;sOptionVideoRow=0;sOptionVideoFirst=0;sOptionVideoPhase=-16;
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);
            Option_PlateGoto(0,OPT_CLIP_BOTTOM,"fl_off_start");
            sOptionFooterSelected=0;Snd_PlaySe(1,1);
        }
        return 1;
    }
    if(gOption->state==PORT_OPTION_VIDEO_PICKER) {
        if(gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT)) {
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");gOption->value^=1;
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");Snd_PlaySe(1,0);
        } else if(gPad[0].gamePressed&(PADG_CROSS|PADG_TRIANGLE)) {
            if((gPad[0].gamePressed&PADG_CROSS) && gOption->value!=Option_PortSettingGet(sOptionVideoRow))
                Option_PortSettingChange(sOptionVideoRow,1);
            if(gPad[0].gamePressed&PADG_TRIANGLE) {
                Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
                gOption->value=Option_PortSettingGet(sOptionVideoRow);
                Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");
            } else Option_PlateGoto(0,OPT_CLIP_PICK,"fl_ok");
            /* Native Stereo/Mono restores its parent row while the picker exits. */
            gOption->state=PORT_OPTION_VIDEO_PAGE;
            Flash_GotoLabel(gOption->flash,"fl_value_out",1);sOptionPickerClosing=1;
            Snd_PlaySe(1,(gPad[0].gamePressed&PADG_TRIANGLE) ? 2:1);
        }
        return 1;
    }
    if(gOption->state==PORT_OPTION_VIDEO_RESET) { Option_UpdateReset();return 1; }
    if(gOption->state==PORT_OPTION_VIDEO_PAGE) {
        if(sOptionPickerClosing) { sOptionPickerClosing=0;return 1; } /* Native PAD gate waited for exit. */
        if(sOptionVideoClosing || sOptionVideoPhase<16) return 1;
        if(gPad[0].gamePressed&PADG_TRIANGLE) {
            if(sOptionVideoEditing) { Option_PortCancelEdit();Snd_PlaySe(1,2);return 1; }
            sOptionVideoClosing=1;
            Flash_GotoLabel(gOption->flash,"fl_menu_change",1);Snd_PlaySe(1,2);
        } else if(sOptionVideoRow==Option_PortSettingCount() && (gPad[0].gamePressed&PADG_CROSS)) {
            gOption->state=PORT_OPTION_VIDEO_RESET;gOption->resetStep=0;
            Snd_PlaySe(1,1);
        } else if(!sOptionVideoEditing && (gPad[0].gameRepeat&(PADG_UP|PADG_DOWN))) {
            sOptionVideoRow=(sOptionVideoRow+((gPad[0].gameRepeat&PADG_UP) ? Option_PortSettingCount():1))%(Option_PortSettingCount()+1);
            Option_PortVideoEnsure();Snd_PlaySe(1,0);
        } else if(sOptionVideoRow<Option_PortSettingCount() && !Port_OptionsAvailable(Option_PortSettingRow(sOptionVideoRow))) {
            /* Focus and feedback remain native; only activation is blocked. */
            if(gPad[0].gamePressed&PADG_CROSS) Snd_PlaySe(1,1);
            else if(gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT)) Snd_PlaySe(1,0);
        } else if(sOptionVideoRow<Option_PortSettingCount() && Option_PortDescriptor(sOptionVideoRow)->count==2 &&
                  (gPad[0].gamePressed&PADG_CROSS)) {
            /* Stereo/Mono mode hides the controller-only third/fourth buttons. */
            gOption->state=PORT_OPTION_VIDEO_PICKER;sOptionPickerClosing=0;gOption->picker=3;
            gOption->value=0;Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
            gOption->value=1;Option_PlateGoto(0,OPT_CLIP_PICK,"fl_off_start");
            gOption->value=Option_PortSettingGet(sOptionVideoRow);
            Option_PlateGoto(0,OPT_CLIP_PICK,"fl_on_start");
            Flash_GotoLabel(gOption->flash,"fl_value_in",1);Snd_PlaySe(1,1);
        } else if(sOptionVideoRow<Option_PortSettingCount() && Option_PortDescriptor(sOptionVideoRow)->count!=2) {
            if(gPad[0].gamePressed&PADG_CROSS) {
                if(!sOptionVideoEditing) sOptionVideoOriginal=Option_PortSettingGet(sOptionVideoRow);
                sOptionVideoEditing=!sOptionVideoEditing;
                Snd_PlaySe(1,1);
            } else if(sOptionVideoEditing && (gPad[0].gameRepeat&(PADG_LEFT|PADG_RIGHT))) {
                Option_PortSettingChange(sOptionVideoRow,(gPad[0].gameRepeat&PADG_LEFT) ? -1:1);
                Option_PortVideoEnsure();Snd_PlaySe(1,0);
            }
        }
        return 1;
    }
    if(gOption->state!=OPT_ST_TOP) return 0;
    if(gPad[0].gameRepeat&(PADG_UP|PADG_DOWN)) {
        for(i=0;i<6;i++) if(sOptionTopOrder[i]==gOption->cursor) break;
        if(i==6) i=0;
        next=sOptionTopOrder[(i+((gPad[0].gameRepeat&PADG_UP) ? 5:1))%6];
        Option_PlateGoto(0,gOption->cursor==5 ? OPT_CLIP_BOTTOM:OPT_CLIP_ROW,"fl_off_start");
        gOption->cursor=next;
        Option_PortTopEnsure();
        Option_PlateGoto(0,next==5 ? OPT_CLIP_BOTTOM:OPT_CLIP_ROW,"fl_on_start");
        if(next!=PORT_OPTION_MORE) {
            gOption->voiceLine=next;OPT_VOICE(next);
        }
        gOption->greeted=1;Snd_PlaySe(1,0);
        return 1;
    }
    if(gOption->cursor==PORT_OPTION_MORE && (gPad[0].gamePressed&PADG_CROSS)) {
        sOptionFromPage=OPT_ST_TOP;
        gOption->fromPage=0;gOption->page=1;
        gOption->state=PORT_OPTION_MORE_PAGE;sOptionVideoRow=0;sOptionVideoPhase=-16;sOptionVideoClosing=0;
        Option_PortPlates(1);
        Flash_GotoLabel(gOption->flash,"fl_menu_change",1);
        Option_PlateGoto(0,OPT_CLIP_BOTTOM,"fl_off_start");
        /* The help window is already open; only its text changes. */
        sOptionFooterSelected=0;Snd_PlaySe(1,1);
        return 1;
    }
    return 0;
}
static void Option_PortPrepareFooter(void);
/* Returning to Options waits for the custom rows to leave the page. */
static s32 Option_PortAdvance(void) {
    /* First remove the outgoing top-level rows, then reveal the new page. */
    if(sOptionVideoPhase<0) {
        sOptionVideoPhase++;
        if(sOptionVideoPhase==0) Option_PortPlates(0);
        return 1;
    }
    if(sOptionVideoClosing) {
        if(sOptionVideoPhase>0) sOptionVideoPhase--;
        if(sOptionVideoPhase==0) {
            if(gOption->state==PORT_OPTION_VIDEO_PAGE) {
                gOption->state=PORT_OPTION_MORE_PAGE;sOptionVideoRow=sOptionSettingsCategory;sOptionVideoPhase=0;
            } else {
                gOption->state=OPT_ST_TOP;gOption->cursor=PORT_OPTION_MORE;
                gOption->page=0;gOption->fromPage=1;
                sOptionReturningTop=1;
                Option_PortPlates(1);
                Option_PlateGoto(0,OPT_CLIP_ROW,"fl_on_start");
                /* The native return timeline already started on Back.
                   Restarting here flashes its outgoing footer a second time. */
            }
            sOptionVideoClosing=0;
            /* This runs inside Flash_Draw, after the normal footer setup.
               Update its caption mode now, before the footer depth draws. */
            Option_PortPrepareFooter();
            return 0;
        }
    } else if(sOptionVideoPhase<16) sOptionVideoPhase++;
    return 1;
}
/* Wrap by measured glyph width, using the same right margin as native help. */
static void Option_PortWrapHelp(const char *line,u16 *out,s32 capacity,s32 room) {
    extern s32 Font_GetWidth(u16 *text);
    const u8 *p=(const u8 *)line;
    s32 n=1,start=1,space=-1;
    out[0]=0xFEFF;
    while(*p && n<capacity-2) {
        u32 code=*p++;
        u32 minimum=0;
        s32 extra=0,i,valid=1;
        if(code>=0xC2 && code<=0xDF) { code&=0x1F;minimum=0x80;extra=1; }
        else if(code>=0xE0 && code<=0xEF) { code&=0x0F;minimum=0x800;extra=2; }
        else if(code>=0xF0 && code<=0xF4) { code&=0x07;minimum=0x10000;extra=3; }
        else if(code>=0x80) valid=0;
        for(i=0;valid && i<extra;i++) {
            if((p[i]&0xC0)!=0x80) valid=0;
            else code=(code<<6)|(p[i]&0x3F);
        }
        if(valid && extra) p+=extra;
        if(!valid || (extra && code<minimum) || code>0xFFFF || (code>=0xD800 && code<=0xDFFF)) code='?';
        u16 ch=(u16)code;
        if(ch==' ' && n==start) continue;
        out[n++]=ch;out[n]=0;
        if(ch=='\n') { start=n;space=-1;continue; }
        if(ch==' ') space=n-1;
        if(Font_GetWidth(out+start)>room && n-start>1) {
            if(space>=start) { out[space]='\n';start=space+1; }
            else { out[n-1]='\n';out[n++]=ch;start=n-1;out[n]=0; }
            space=-1;
        }
    }
    out[n]=0;
}
static s32 Option_PortHelp(void) {
    extern void Port_MsgWinDrawString43(u16 *text);
    extern s32 MsgWin_PortLineRoom(s32 right);
    static u16 text[2][160];
    static char translated[1024],previous[1024];
    static s32 current;
    const char *line,*id;
    s32 room;
    if(Option_PcHelp()) return 1;
    if(!Port_ZenkaiEnabled()) return 0;
    if(gOption->state==PORT_OPTION_MORE_PAGE && sOptionVideoPhase>=0) {
        static const char *help[]={"Adjust resolution and display settings.","Configure battle visuals and stage effects.","Configure startup screens.","Configure automatic saving, game edition and network mode.","Return to Options."};
        static const char *ids[]={"options/help/category-video","options/help/category-battle-effects","options/help/category-visual","options/help/category-general","options/help/return"};
        line=help[sOptionVideoRow];
        id=ids[sOptionVideoRow];
    } else if((gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_VIDEO_RESET || gOption->state==PORT_OPTION_VIDEO_PICKER) && sOptionVideoPhase>=0) {
        if(sOptionVideoRow<Option_PortSettingCount()) {
            const PortVideoOption *option=Option_PortDescriptor(sOptionVideoRow);
            if(Option_PortSettingRow(sOptionVideoRow)==PORT_NETWORK_OPTION_OFFSET &&
               !Port_OptionsAvailable(PORT_NETWORK_OPTION_OFFSET)) {
                line="Extract the Wii game files to enable Network Mode.";
                id="options/help/network-files";
            } else {
                line=option->help;
                id=option->helpId;
            }
        } else {
            line="Restore the defaults for this category.";
            id="options/help/default";
        }
    } else if(gOption->cursor==PORT_OPTION_MORE) {
        line="View additional game options.";
        id="options/help/more";
    } else return 0;
    MenuTranslation_PortTextCopy(id,line,translated,sizeof(translated));
    if(strcmp(translated,previous)!=0) { current=1-current;strcpy(previous,translated); }
    room=MsgWin_PortLineRoom(400);
    if(room<40) room=40;
    Option_PortWrapHelp(translated,text[current],160,room);
    Port_MsgWinDrawString43(text[current]);
    return 1;
}
static const char *Option_PortHeading(void) {
    static char translated[128];
    const char *id,*fallback;
    if(!Port_ZenkaiEnabled()) return NULL;
    if(gOption->state==PORT_PC_OPTIONS_PAGE) {
        id=sOptionPcGamepad ? "controls/column/gamepad":"controls/column/mouse-keyboard";
        fallback=sOptionPcGamepad ? "Gamepad":"Mouse/Keyboard";
        MenuTranslation_PortTextCopy(id,fallback,translated,sizeof(translated));
        return translated;
    }
    if(gOption->state==PORT_OPTION_MORE_PAGE) { id="options/more/title";fallback="More Options"; }
    else if(gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_VIDEO_PICKER || gOption->state==PORT_OPTION_VIDEO_RESET) {
        id=sPortOptionCategoryIds[sOptionSettingsCategory];fallback=sPortOptionCategories[sOptionSettingsCategory];
    } else return NULL;
    MenuTranslation_PortTextCopy(id,fallback,translated,sizeof(translated));
    return translated;
}
/* Use the original footer, including its authored movement and foreground order. */
static void Option_PortPrepareFooter(void) {
    if(Option_PcFooter()) return;
    s32 active=Port_ZenkaiEnabled() &&
        (gOption->state==PORT_OPTION_MORE_PAGE || gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_VIDEO_RESET || gOption->state==PORT_OPTION_VIDEO_PICKER);
    s32 page=gOption->state==PORT_OPTION_MORE_PAGE ? PORT_OPTION_MORE_PAGE:
        (sOptionVideoPhase<0 ? sOptionFromPage:gOption->state);
    Port_FlashOptionsVideoPicker(gOption->state==PORT_OPTION_VIDEO_PICKER || sOptionPickerClosing ? Option_PortSettingRow(sOptionVideoRow):-1);
    /* Both footer plates belong to the native page-change timeline. */
    Port_FlashOptionsVideoFooter(sOptionReturningTop ? 2:
        (active && page!=OPT_ST_TOP ? (page==PORT_OPTION_MORE_PAGE ? 2:1):0));
    if((active && page==PORT_OPTION_MORE_PAGE) || sOptionReturningTop) {
        MFlashRef ref;
        MFlashUv uv;
        /* The native page may still prepare Default's atlas row this frame.
           Restore EXIT before drawing, including the frame Advance returns
           to the top page from inside the row draw hook. */
        uv.x0=0;uv.y0=0;uv.x1=0x100;uv.y1=0x20;
        Option_SetBottomText(gOption->flash,&ref,"mc_bottom_plate_1",uv);
        /* fl_menu_change also draws the outgoing footer on plate 2.
           More Options has EXIT on both sides of this transition. */
        Option_SetBottomText(gOption->flash,&ref,"mc_bottom_plate_2",uv);
    }
    if(active && sOptionVideoPhase>=0) {
        s32 footerRow=gOption->state==PORT_OPTION_MORE_PAGE ? PORT_OPTION_CATEGORY_COUNT:Option_PortSettingCount();
        s32 selected=sOptionVideoRow==footerRow ? (gOption->state==PORT_OPTION_VIDEO_RESET ? 2:1):0;
        if(selected!=sOptionFooterSelected) {
            Option_PlateGoto(0,OPT_CLIP_BOTTOM,selected==2 ? "fl_ok":selected ? "fl_on_start":"fl_off_start");
            sOptionFooterSelected=selected;
        }
    }
}
/* Same native blue/gold decoration, diagonal stagger and caption styles as
   the authored Options rows. Coordinates are native foreground coordinates. */
static void Option_PortButton(s32 index,void *reference,f32 x,f32 y,char *label,s32 selected) {
    MFlashRef ref;
    s32 focus=selected==-2 ? 1:selected;
    if(selected!=sOptionPlateSelected[index]) {
        Flash_FindLabel(&sOptionPlateMovies[index],NULL,"mc_menu_plate_4",&ref);
        Flash_ClipGotoLabel(&sOptionPlateMovies[index],&ref,focus==2 ? "fl_ok" : focus>0 ? "fl_on_start":"fl_off_start");
        sOptionPlateSelected[index]=selected;
    }
    Flash_Advance(&sOptionPlateMovies[index]);
    Port_FlashOptionsPlateDim(selected<0);
    Port_FlashOptionsPlate(&sOptionPlateMovies[index],reference,x,y,label);
    Port_FlashOptionsPlateDim(0);
}
static void Option_PortDrawTop(f32 shift,void *reference) {
    s32 i;
    static char *labels[]={"Save/Load","Screen","Sound","Controller","More Options"};
    static const char *labelIds[]={"options-normal/0/0","options-normal/0/1","options-normal/0/2","options-normal/0/3"};
    Option_PortTopEnsure();
    Sprite_SetScissor(0,511,52,278);
    for(i=0;i<4;i++) {
        s32 entry=sOptionTopFirst+i;
        char translated[128];
        char *label=labels[entry];
        if(entry<4 && MenuTranslation_Ensure()) {
            const char *text=MenuTranslation_Text(labelIds[entry]);
            if(text!=NULL) { snprintf(translated,sizeof(translated),"%s",text);label=translated; }
        } else if(entry==4) { MenuTranslation_PortTextCopy("options/more/title",labels[entry],translated,sizeof(translated));label=translated; }
        Option_PortButton(i,reference,92-i*20+shift,72+i*44+sOptionTopScroll,label,
                         gOption->cursor==sOptionTopOrder[entry] ? (gOption->state>OPT_ST_TOP ? 2:1):0);
    }
    Sprite_SetScissor(0,511,0,447);
    if(shift==0 && gOption->state==OPT_ST_TOP) {
        if(sOptionTopFirst>0) Port_FlashOptionsChevron(gOption->flash,0,272,64);
        if(sOptionTopFirst<1) Port_FlashOptionsChevron(gOption->flash,1,272,252);
    }
    sOptionTopScroll*=0.6f;
    if(sOptionTopScroll<0.5f && sOptionTopScroll> -0.5f) sOptionTopScroll=0;
}
static void Option_PortDrawMore(f32 shift,s32 confirmed) {
    s32 i,selected=confirmed ? sOptionSettingsCategory:sOptionVideoRow;
    for(i=0;i<PORT_OPTION_CATEGORY_COUNT;i++) {
        char label[128];
        MenuTranslation_PortTextCopy(sPortOptionCategoryIds[i],sPortOptionCategories[i],label,sizeof(label));
        Option_PortButton(i,NULL,92-i*20+shift,72+i*44,label,
            selected==i ? (confirmed ? 2:1):0);
    }
}
/* Called at the original menu-row depth inside the Options movie. */
void Port_OptionsTopDraw(void) {
    if(!Port_ZenkaiEnabled()) return;
    Port_FlashOptionsChevronFrame(gOption->state+(gOption->state==PORT_PC_OPTIONS_PAGE && sOptionPcGamepad ? 100:0));
    if(Option_PcDraw()) return;
    if(gOption->state==OPT_ST_TOP && (gOption->flash[0].flags&MFLASH_PAD))
        sOptionReturningTop=0;
    if(gOption->state==PORT_OPTION_MORE_PAGE || gOption->state==PORT_OPTION_VIDEO_PAGE || gOption->state==PORT_OPTION_VIDEO_RESET || gOption->state==PORT_OPTION_VIDEO_PICKER) {
        Option_PortDraw();return;
    }
    Option_PortArtInit();Ps_SetShift(0);
    Option_PortDrawTop(0,gOption->flash);
}
static void Option_PortDraw(void) {
    s32 i;
    if(!Port_ZenkaiEnabled()) return;
    Option_PortArtInit();
    if(gOption->page==0) sOptionTopLeaving=1;
    else if((gOption->state!=PORT_OPTION_MORE_PAGE && gOption->state!=PORT_OPTION_VIDEO_PAGE && gOption->state!=PORT_OPTION_VIDEO_RESET && gOption->state!=PORT_OPTION_VIDEO_PICKER)) {
        if(!sOptionTopLeaving) return;
        if(gOption->flash[0].flags&MFLASH_PAD) { sOptionTopLeaving=0;return; }
    }
    Ps_SetShift(0);
    if((gOption->state!=PORT_OPTION_MORE_PAGE && gOption->state!=PORT_OPTION_VIDEO_PAGE && gOption->state!=PORT_OPTION_VIDEO_RESET && gOption->state!=PORT_OPTION_VIDEO_PICKER)) return;
    else {
        f32 shift;
        if(sOptionVideoPhase<0) {
            f32 progress;
            Option_PortAdvance();
            progress=1.0f+(f32)sOptionVideoPhase/16.0f;
            if(sOptionFromPage==PORT_OPTION_MORE_PAGE) Option_PortDrawMore(600.0f*progress*progress,1);
            else Option_PortDrawTop(600.0f*progress*progress,NULL);
            return;
        }
        if(!Option_PortAdvance()) return;
        shift=1.0f-(f32)sOptionVideoPhase/16.0f;
        shift=600.0f*shift*shift;
        if(gOption->state==PORT_OPTION_MORE_PAGE) {
            Option_PortDrawMore(shift,0);return;
        }
        Option_PortVideoEnsure();
        for(i=0;i<PORT_VIDEO_VISIBLE_ROWS;i++) {
            s32 row=sOptionVideoFirst+i;
            const PortVideoOption *option;
            char valueText[128],labelText[128];
            if(row>=Option_PortSettingCount()) break;
            option=Option_PortDescriptor(row);
            s32 selected=Port_OptionsAvailable(Option_PortSettingRow(row)) ? Option_PortVideoFocus(row):
                (Option_PortVideoFocus(row) ? -2:-1);
            s32 value=Option_PortSettingGet(row);
            MenuTranslation_PortTextCopy(option->nameIds[value],option->names[value],valueText,sizeof(valueText));
            MenuTranslation_PortTextCopy(option->labelId,option->label,labelText,sizeof(labelText));
            s32 id=PortTbo_TextId(valueText,selected ? 4:3);
            f32 y=72+i*44;
            Option_PortButton(i,NULL,92-i*20+shift,y,labelText,Option_PortVideoEditing(row) ? 1:selected);
            if(option->count==2 || selected<0) continue;
            /* Keep the current value visible; activate the row to reveal arrows. */
            if(id>=0) {
                /* Both axes use the styled texture's actual ink bounds. */
                f32 x=420+shift-19.0f*((f32)PortTbo_TextInfo(id,5)+0.5f*PortTbo_TextInfo(id,7))/1000.0f;
                f32 top=y+16-19.0f*((f32)PortTbo_TextInfo(id,6)+0.5f*PortTbo_TextInfo(id,8))/1000.0f;
                u32 tint=PS_PLAIN;s32 alpha=128;
                if(Option_PortVideoEditing(row)) {
                    f32 dx,dy;
                    Port_FlashOptionsCaptionMotion(1,&dx,&dy);x+=dx;top+=dy;
                    alpha=Port_FlashOptionsCaptionStyle(1,&tint);
                }
                Port_GsMarker(0x1B);
                Ps_SpriteScreen(id,x,top,x+512,top+32,0,0,1,1,tint,alpha);
                Port_GsMarker(0x13);
            }
            if(!Option_PortVideoEditing(row)) continue;
            Port_FlashOptionsChevron(gOption->flash,2,360+shift,y+16);
            Port_FlashOptionsChevron(gOption->flash,3,480+shift,y+16);
        }
        if(!sOptionVideoClosing && !sOptionVideoEditing && gOption->state==PORT_OPTION_VIDEO_PAGE) {
            if(sOptionVideoFirst>0) Port_FlashOptionsChevron(gOption->flash,0,272,64);
            if(sOptionVideoFirst<Option_PortSettingCount()-PORT_VIDEO_VISIBLE_ROWS) Port_FlashOptionsChevron(gOption->flash,1,272,252);
        }
    }
}
#endif
