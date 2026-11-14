#include "common.h"
#include "menu/menu_a.h"
#include "sys/pad.h"

/* The screen's work (overlay .data, 0x3B0EB8). */
Title *gTitle = NULL;

/*
 * Title: the title screen (progress mode 1), 0x336FC0..0x338020. A still picture for 300 frames, then the
 * "press start" movie and a one- or two-item menu (new game / continue) that runs the memory-card flow. After
 * 1800 idle frames it leaves for the attract demo (twice) or the opening movie (every third time).
 */

#define TITLE_VOICE_BASE 0x86F9

#ifdef PORT
#include "menu/port_menu_mouse.h"
extern void *memcpy(void *dst, const void *src, u32 size);
extern void Port_TitleLogoLoad(void *texture);
extern int Port_TitleLogoEnabled(void);
static void *sTitleLogoOriginal;
static int sTitleLogoChoice;
#define TITLE_LOGO_BYTES 132480
extern int gPortOverlayOpen; /* port/src/gs/gs_input.c: settings or quit dialog owns input */
extern int Ui_QuitAsk(void);
extern void PortInput_Swallow(void);
extern void PortInput_SwallowKeys(void);
extern void Port_FlashDrawMenuArt(void *movie, s32 screen, s32 titleMenuVisible);
#endif

/* What ends the screen: the movie plays out for 15 frames, then the fade starts. */
#define TITLE_LEAVE() \
    gTitle->flags |= TITLE_CHOSEN; \
    gTitle->flags |= TITLE_LEAVING; \
    gTitle->timer = 15

/* Counts an idle frame; at the limit leaves with result 1 (attract demo set up) or 2 (opening movie). */
void Title_Idle(s32 *result) {
    if (++gTitle->idle > TITLE_IDLE_FRAMES) {
        gTitle->idle = 0;
        TITLE_LEAVE();
        switch (gProgress->demoCount) {
        case 0:
        case 1:
            Demo_SetupBattle();
            *result = 1;
            gProgress->demoCount++;
            break;
        default:
            *result = 2;
            gProgress->demoCount = 0;
            break;
        }
    }
}

#define TT_RES(n) \
    res = (MTexRes *)MPACK_AT(gTitle->res, n); \
    Res_RelocateOffsets(&res, res, res)

/* Loads and unpacks the screen (section `section` of archive 1) and starts the dialog and card modules. */
void Title_Init(s32 section) {
    MTexRes *res = NULL;

    if (gMenuArc1 == NULL) {
        gMenuArc1 = File_LoadSync(gProgress->baseFile + 1, NULL, 0);
    }
    gTitle = Heap_Alloc(0xD0, 0x20, 0, 2);
    memset(gTitle, 0, 0xD0);
    gTitle->pack = (u32 *)MPACK_AT(gMenuArc1, section);
    gTitle->res = Sprite_Unpack(gTitle->pack, NULL, NULL);
    TT_RES(1);
    gTitle->bg = res;
    TT_RES(2);
    gTitle->tex[18] = MTEX(res, 0);
    gTitle->tex[20] = MTEX(res, 1);
    gTitle->tex[22] = MTEX(res, 2);
    gTitle->tex[21] = MTEX(res, 3);
    gTitle->tex[17] = MTEX(res, 4);
    gTitle->tex[19] = MTEX(res, 5);
    TT_RES(3);
    gTitle->tex[10] = MTEX(res, 0);
    gTitle->tex[11] = MTEX(res, 1);
    gTitle->tex[12] = MTEX(res, 2);
    gTitle->tex[13] = MTEX(res, 3);
    gTitle->tex[14] = MTEX(res, 4);
    gTitle->tex[15] = MTEX(res, 5);
    gTitle->tex[16] = MTEX(res, 6);
    TT_RES(14);
    gTitle->logo = res;
#ifdef PORT
    /* Preserve the raw US texture so either logo can be selected while this screen is open. */
    sTitleLogoOriginal = Heap_Alloc(TITLE_LOGO_BYTES, 0x20, 0, 2);
    memcpy(sTitleLogoOriginal, MPACK_AT(gTitle->res, 5), TITLE_LOGO_BYTES);
    sTitleLogoChoice = Port_TitleLogoEnabled();
    if (sTitleLogoChoice) {
        Port_TitleLogoLoad(MPACK_AT(gTitle->res, 5));
    }
#endif
    TT_RES(5);
    gTitle->tex[23] = MTEX(res, 0);
    TT_RES(6);
    gTitle->tex[4] = MTEX(res, 0);
    TT_RES(7);
    gTitle->tex[3] = MTEX(res, 0);
    TT_RES(8);
    gTitle->tex[6] = MTEX(res, 0);
    gTitle->tex[9] = MTEX(res, 1);
    TT_RES(9);
    gTitle->tex[5] = MTEX(res, 0);
    gTitle->tex[8] = MTEX(res, 1);
    gTitle->tex[7] = MTEX(res, 2);
    TT_RES(10);
    gTitle->tex[0] = MTEX(res, 0);
    gTitle->tex[1] = MTEX(res, 1);
    gTitle->tex[2] = MTEX(res, 2);
    Flash_Create(&gTitle->flash[0], MPACK_AT(gTitle->res, 11), gTitle->tex);
    Flash_Play(&gTitle->flash[0], 1);
    Dialog_Init(MPACK_AT(gTitle->res, 12), NULL, 0);
    McFlow_Init(1);
    if (gProgress->flags & MPROG_SAVE_LOADED) {
        gTitle->itemCount = 2;
        gTitle->cursor = 1;
    } else {
        gTitle->itemCount = 1;
        gTitle->cursor = 0;
    }
    gTitle->voice = Rand_Range(6);
}

/* Frees the screen. */
void Title_Term(void) {
    s32 i;

#ifdef PORT
    if (sTitleLogoOriginal != NULL) {
        Heap_Free(sTitleLogoOriginal);
        sTitleLogoOriginal = NULL;
    }
#endif
    Dialog_Term();
    McFlow_Term();
    for (i = 0; i < TITLE_FLASH_NUM; i++) {
        Flash_Destroy(&gTitle->flash[i]);
    }
    if (gTitle->res != NULL) {
        Heap_Free(gTitle->res);
        gTitle->res = NULL;
    }
    if (gTitle != NULL) {
        Heap_Free(gTitle);
        gTitle = NULL;
    }
}

/* Draws the background, the two scrolling cloud layers, the menu plates and the movie. */
void Title_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 i;
    MFlash *flash;

#ifdef PORT
    {
        int choice = Port_TitleLogoEnabled();
        if (choice != sTitleLogoChoice && sTitleLogoOriginal != NULL) {
            MTexRes *logo = (MTexRes *)MPACK_AT(gTitle->res, 5);
            memcpy(logo, sTitleLogoOriginal, TITLE_LOGO_BYTES);
            if (choice) Port_TitleLogoLoad(logo);
            Res_RelocateOffsets(&logo, logo, logo);
            gTitle->tex[23] = MTEX(logo, 0);
            sTitleLogoChoice = choice;
        }
    }
#endif
    Sprite_DrawPicture(gTitle->bg, 0, 0, 0x80);
    flash = &gTitle->flash[0];
    for (i = 0; i < 2; i++) {
        uv.y0 = 0;
        uv.y1 = 0x100;
        uv.x0 = 0;
        uv.x1 = 0x200;
        sprintf(name, "mc_kumo_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        FlashAnim_Scroll(flash, &ref, &uv, &gTitle->cloud[i], NULL, i ? -1.0666667f : -0.71111111f, 0.0f);
    }
    for (i = 0; i < 2; i++) {
        uv.y0 = i * 0x20;
        uv.y1 = uv.y0 + 0x20;
        uv.x0 = 0;
        uv.x1 = 0x200;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetColor(flash, &ref, i < gTitle->itemCount ? 1.0f : 0.5f);
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    for (i = 0; i < TITLE_FLASH_NUM; i++) {
#ifdef PORT
        Port_FlashDrawMenuArt(&gTitle->flash[i], 1, gTitle->state >= 2);
#else
        Flash_Draw(&gTitle->flash[i]);
#endif
    }
}

/* Sends a clip of movie `idx` to a label: kind 1 the "press start" plate, kind 2 the plate under the cursor. */
void Title_ClipGoto(s32 idx, s32 kind, char *label) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gTitle->flash[idx];

    switch (kind) {
    case 1:
        sprintf(name, "mc_press_start_plate");
        break;
    case 2:
        sprintf(name, "mc_menu_plate_%d", gTitle->cursor + 1);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Steps the movie and plays the sound it asks for. */
void Title_Update(void) {
    s32 i;

    for (i = 0; i < TITLE_FLASH_NUM; i++) {
        Flash_Advance(&gTitle->flash[i]);
    }
    if (gTitle->flash[0].se & 1) {
        Snd_PlaySe(2, 0x37);
    }
}

/* Card flow callback 0 (done): the save exists now; leave with result 0. */
void Title_OnCardDone(s32 *result) {
    if (result != NULL) {
        *result = 0;
    }
    TITLE_LEAVE();
}

/* Card flow callback 1 (not done): back to the menu. */
void Title_OnCardCancel(void) {
    Title_ClipGoto(0, 2, "fl_on_start");
}

/* Pad 0: start skips the intro and opens the menu; up / down, confirm, or quit from the menu on PC. */
void Title_Input(s32 *result) {
    u32 pressed = gPad[0].gamePressed, repeat = gPad[0].gameRepeat;
#ifdef PORT
    PortMenuMouse mouse;
    s32 row;
    s32 anyButton = PortInput_AnyButtonPressed();
    PortMenuMouse_Read(&mouse);
    if (gPortOverlayOpen) {
        return;
    }
    if (!(pressed || repeat)) {
        if (gTitle->state < 2) {
            if (mouse.confirm) pressed = 0x1000; /* click advances intro / Press START */
        } else {
            row = (mouse.moved || mouse.confirm) && !mouse.wheel ? Port_FlashTitleMouseRow(mouse.x, mouse.y) : -1;
            if (row >= 0 && row < gTitle->itemCount) {
                if (row != gTitle->cursor && (gTitle->flash[0].flags & MFLASH_PAD)) {
                    Title_ClipGoto(0, 2, "fl_off_start");
                    gTitle->cursor = row;
                    Title_ClipGoto(0, 2, "fl_on_start");
                    Snd_PlaySe(1, 0);
                }
                if (mouse.confirm) pressed = 0x200;
            }
            if (mouse.cancel) pressed = PADG_TRIANGLE;
            else if (mouse.wheel) repeat = mouse.wheel < 0 ? 8 : 4;
        }
        if (mouse.moved || mouse.confirm || mouse.cancel || mouse.wheel) gTitle->idle = 0;
    }
    if (gTitle->state < 2 && anyButton) pressed |= PADG_START;
    if (gTitle->state < 2 && pressed) {
        PortInput_Swallow();
        PortInput_SwallowKeys();
    }
#endif
    if (gTitle->state == 0) {
        if (pressed) {
            Flash_GotoLabel(&gTitle->flash[0], "fl_skip", 1);
            gTitle->state = 1;
            Snd_PlaySe(1, 1);
            return;
        }
    }
    if (!(gTitle->flash[0].flags & MFLASH_PAD)) {
        return;
    }
    if (!(gTitle->flags & TITLE_STARTED)) {
        Title_ClipGoto(0, 1, "fl_on_start");
        gTitle->state = 1;
        gTitle->flags |= TITLE_STARTED;
        if (Rand_Range(7) != 0) {
            Voice_PlayWithSubtitle(NULL, TITLE_VOICE_BASE, gTitle->voice);
        } else {
            Voice_PlayWithSubtitle(NULL, TITLE_VOICE_BASE, 0x40);
        }
    }
    switch (gTitle->state) {
    case 1:
        if (pressed) {
            gTitle->idle = 0;
            Flash_GotoLabel(&gTitle->flash[0], "fl_menu_in", 1);
            Title_ClipGoto(0, 1, "fl_ok");
            Title_ClipGoto(0, 2, "fl_on_start");
            gTitle->state = 2;
            Voice_PlayWithSubtitle(NULL, TITLE_VOICE_BASE, gTitle->voice + 6);
            Snd_PlaySe(2, 0x38);
        } else {
            Title_Idle(result);
        }
        break;
    case 2:
#ifdef PORT
        if (pressed & PADG_TRIANGLE) {
            gTitle->idle = 0;
            Ui_QuitAsk();
            break;
        }
#endif
        if (repeat & 8) {
            gTitle->idle = 0;
            if (gTitle->itemCount >= 2) {
                Title_ClipGoto(0, 2, "fl_off_start");
                gTitle->cursor--;
                if (gTitle->cursor < 0) {
                    gTitle->cursor = gTitle->itemCount - 1;
                }
                Title_ClipGoto(0, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (repeat & 4) {
            gTitle->idle = 0;
            if (gTitle->itemCount >= 2) {
                Title_ClipGoto(0, 2, "fl_off_start");
                gTitle->cursor++;
                if (gTitle->cursor >= gTitle->itemCount) {
                    gTitle->cursor = 0;
                }
                Title_ClipGoto(0, 2, "fl_on_start");
                Snd_PlaySe(1, 0);
            }
        } else if (pressed & 0x200) {
            gTitle->idle = 0;
            Title_ClipGoto(0, 2, "fl_ok");
            switch (gTitle->cursor) {
            case 0:
                McFlow_Start(3);
                McFlow_SetDoneCb(0, Title_OnCardDone, result);
                McFlow_SetDoneCb(1, Title_OnCardCancel, NULL);
                break;
            case 1:
                *result = 0;
                TITLE_LEAVE();
                break;
            }
            Snd_PlaySe(1, 1);
        } else {
            Title_Idle(result);
        }
        break;
    }
}

#ifdef PORT
extern int Port_Setting(const char *name, int def);
extern int Port_NetFixedStart(void); /* port/src/gs/net.c */
extern void Port_Opening43Set(s32 on);
#endif

/* The title screen's own frame loops. Returns 0 to go on to the main menu, 1 when the attract demo was set up
   (the overlay returns and the battle runs), 2 to play the opening movie. */
s32 Title_Run(s32 section) {
    s32 result = 1;
    s32 wait = 0;

    Title_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
#ifdef PORT
    Port_Opening43Set(1);
    /* The copyright page is always skipped. Keep the network guard for fixed-start sessions. */
    while (Port_NetFixedStart()) {
#else
    while (1) {
#endif
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        Sprite_DrawPicture(gTitle->logo, 0, 0, 0x80);
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            gTitle->flags |= TITLE_FADED_IN;
        }
        if (ColorFade_IsFadingOut()) {
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTitle->flags & TITLE_FADED_IN) {
            wait++;
            if (wait > 300) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
    }
#ifdef PORT
    Port_Opening43Set(0);
#endif
    ColorFade_StartIn(0, 0, 0, 0x14);
#ifdef PORT
    PortMenuMouse_Enable(1);
#endif
    for (;;) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (!(gProgress->flags & MPROG_FREEZE)) {
            Title_Update();
        }
        Title_Draw();
        gTitle->mcState = McFlow_Update();
        ColorFade_Draw();
#ifdef PORT
        if(!gTitle->mcState) PortMenuMouse_Ready((gTitle->state == 0 || (gTitle->flash[0].flags & MFLASH_PAD)) &&
            !gTitle->mcState && !(gTitle->flags & TITLE_LEAVING) && !(gProgress->flags & MPROG_FREEZE) &&
            ColorFade_IsInDone() && !ColorFade_IsFadingOut());
#endif
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsFadingOut()) {
            Voice_FadeOutStep(0);
            Bgm_FadeOutStep();
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gTitle->flags & TITLE_LEAVING) {
            if (--gTitle->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gTitle->mcState == 0) {
            Title_Input(&result);
        }
    }
    if (result == 0) {
        gProgress->flags |= MPROG_SAVE_LOADED;
    }
    sceGsSyncPath(0, 0);
    Title_Term();
#ifdef PORT
    PortMenuMouse_Enable(0);
#endif
    Dma_ResetBuffers();
    return result;
}
