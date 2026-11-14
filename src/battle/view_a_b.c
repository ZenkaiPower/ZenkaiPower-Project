#include "common.h"
#include "battle/view_a.h"
#include "sys/heap.h"

/*
 * MsgWin, 0x25CFC0..0x25D290: a message window that slides in from the left or the right and shows one line
 * of a text file ("mc_message_text"). Used all over the menu overlay. See battle/view_a.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Flash_Create(Flash *flash, void *data, void *tex);
extern void Flash_Destroy(Flash *flash);
extern void Flash_Advance(Flash *flash);
extern void Flash_Draw(Flash *flash);
extern void Flash_Play(Flash *flash, s32 arg);
extern void Flash_GotoLabel(Flash *flash, char *label, s32 arg);
extern void Flash_FindLabel(Flash *flash, char *parent, char *name, FlashRef *out);
extern void Font_FlushAll(void);
#ifdef PORT
extern s32 gPortMainMenuArtWindow;
extern void Port_GsMarker(s32 effect);
extern s32 Port_TitleMainChars43(void);
extern s32 Port_Menus43(void);
static s32 sPortMsgWinTextMarker;
static s32 sPortMsgWinKeepInset;
static void MsgWin_PortAttachInset(Flash *flash, FlashRef *ref);
#endif

/* text box module, after 0x2600B0 (not decompiled) */
extern void TextBox_AttachLine(Flash *flash, FlashRef *ref, s32 a, s32 b, s32 line, TextBox *box);
extern void TextBox_SetSpacing(TextBox *box, s32 a, s32 b);

/* Defined here: this object's .sdata (0x2FF0D8). */
MsgWin *gMsgWin = NULL;

/* Allocates the message window: movie and textures from the pack, text box preset 5 on `text`. */
void MsgWin_Init(u32 *pack, void *text, s32 side) {
    FlashTexRes *res = NULL;

    gMsgWin = Heap_Alloc(sizeof(MsgWin), 0x20, 0, HEAP_ANY);
    memset(gMsgWin, 0, sizeof(MsgWin));

    res = PACK_AT(pack, 1);
    Res_RelocateOffsets(&res, res, res);
    gMsgWin->tex[0] = res->tex;
    gMsgWin->tex[1] = res->tex + FLASH_TEX_SIZE;

    Flash_Create(gMsgWin->flash, PACK_AT(pack, 2), gMsgWin->tex);
    Flash_Play(gMsgWin->flash, 1);
    TextBox_Init(&gMsgWin->box, text, 5);
    gMsgWin->text = text;
    gMsgWin->side = side;
}

/* Destroys the movie and frees the window. */
void MsgWin_Term(void) {
    Flash_Destroy(gMsgWin->flash);
    if (gMsgWin != NULL) {
        Heap_Free(gMsgWin);
        gMsgWin = NULL;
    }
}

/* Steps and draws the window with one line of its text file. The first two arguments are not used. */
void MsgWin_Draw(s32 unused0, s32 unused1, s32 line) {
    FlashRef ref;
    Flash *flash;
    s32 i;
#ifdef PORT
    s32 textMarker = sPortMsgWinTextMarker ? sPortMsgWinTextMarker : (gPortMainMenuArtWindow ? 0x1A : 0);
#endif

    Flash_Advance(gMsgWin->flash);
    flash = gMsgWin->flash;
    if (gMsgWin->text != NULL) {
        Flash_FindLabel(flash, NULL, "mc_message_text", &ref);
        TextBox_AttachLine(flash, &ref, 0, 0, line, &gMsgWin->box);
#ifdef PORT
        if (sPortMsgWinKeepInset) MsgWin_PortAttachInset(flash, &ref);
#endif
    }
    for (i = 0; i < 1; i++) {
        Flash_Draw(&gMsgWin->flash[i]);
    }
#ifdef PORT
    if (textMarker) {
        Port_GsMarker(textMarker); /* scale the flushed glyphs about the window's chosen edge */
    }
#endif
    Font_FlushAll();
#ifdef PORT
    if (textMarker) {
        Port_GsMarker(0x13);
    }
#endif
}

#ifdef PORT
/* Opt in left-anchored dialogue text without changing other users of MsgWin.
   The movie's slide direction does not determine the text's layout anchor. */
void Port_MsgWinDraw43(s32 line) {
    s32 old = sPortMsgWinTextMarker;
    sPortMsgWinTextMarker = Port_TitleMainChars43() && !Port_Menus43()
        ? 0x1A : 0;
    MsgWin_Draw(0, 0, line);
    sPortMsgWinTextMarker = old;
}

/* Ultimate Battle uses a wide shaped backing: preserve its animated text inset. */
void Port_MsgWinDrawInset43(s32 line) {
    s32 oldInset = sPortMsgWinKeepInset;
    sPortMsgWinKeepInset = Port_TitleMainChars43() && !Port_Menus43();
    Port_MsgWinDraw43(line);
    sPortMsgWinKeepInset = oldInset;
}
#endif

/* Starts the slide-in animation of the window's side. */
void MsgWin_Open(void) {
    if (gMsgWin->side == 0) {
        Flash_GotoLabel(gMsgWin->flash, "fl_l_in", 1);
    } else {
        Flash_GotoLabel(gMsgWin->flash, "fl_r_in", 1);
    }
}

/* Starts the slide-out animation of the window's side. */
void MsgWin_Close(void) {
    if (gMsgWin->side == 0) {
        Flash_GotoLabel(gMsgWin->flash, "fl_l_out", 1);
    } else {
        Flash_GotoLabel(gMsgWin->flash, "fl_r_out", 1);
    }
}

/* Changes the text file of the window (NULL: no text). */
void MsgWin_SetText(void *text) {
    gMsgWin->text = text;
    gMsgWin->box.text = text;
}

/* Chooses the side the window slides in from. */
void MsgWin_SetSide(s32 side) {
    gMsgWin->side = side;
}

#ifdef PORT
/* PC build, for screens of the PC's own (menu/port_tbo.c): the window with a line of theirs in place of a line of the
   text file, written by the game's text box in the game's font, where the game writes its own (and read out by the
   screen reader as the game's lines are: it reads the box's line, port/src/a11y_game.c). */
#include "battle/view_b.h"

extern void Flash_ClipSetCallbackC(Flash *flash, FlashRef *ref, void *fn, void *arg);
extern void TextBox_AttachString(Flash *flash, FlashRef *ref, s32 x, s32 y, u16 *str, TextBox *box);
extern s32 Font_GetGlyphHeight(void);

extern s32 Port_PageAspectMilli(void);
static f32 sMsgWinTextX = -1.0f; /* where the line was last drawn (the screen's 512 x 448), -1 before */

/* The text box's own drawing, and where it drew (it follows the window's slide-in). */
static void MsgWin_PortDrawClip(void *draw, f32 *prop) {
    sMsgWinTextX = prop[14]; /* the matrix's translation (FlashProp 0x38: sys/gfxm_c.h) */
    if (sPortMsgWinKeepInset && sPortMsgWinTextMarker && Port_PageAspectMilli() > 1340) {
        TextBoxClipProp adjusted = *(TextBoxClipProp *)prop;
        TextBoxDraw adjustedDraw = *(TextBoxDraw *)draw;
        f32 narrow = 1333.333f / (f32)Port_PageAspectMilli();
        f32 inset = (f32)gMsgWin->box.unkC;
        /* Narrow glyphs about the animated text inset, not the screen's x=0. */
        adjusted.x = (adjusted.x + inset) / narrow - inset;
        /* Font clipping happens before GS narrowing. Express the wide window's
           horizontal bounds in those same pre-narrowing coordinates, otherwise
           the inset shift cuts the end of a line at the old right boundary. */
        adjustedDraw.clip[0] = (s32)((f32)adjustedDraw.clip[0] / narrow);
        adjustedDraw.clip[2] = (s32)((f32)adjustedDraw.clip[2] / narrow + 0.999f);
        TextBox_DrawClip(&adjustedDraw, &adjusted);
    } else TextBox_DrawClip((TextBoxDraw *)draw, (TextBoxClipProp *)prop);
}

static void MsgWin_PortAttachInset(Flash *flash, FlashRef *ref) {
    Flash_ClipSetCallbackC(flash, ref, MsgWin_PortDrawClip,
                          &((TextBoxFull *)&gMsgWin->box)->draw);
}

/* As MsgWin_Draw, with `str` (16-bit, 0xFEFF first; NULL: no line). A changed line is another buffer: the screen
   reader tells lines apart by where they are. */
void MsgWin_PortDrawString(u16 *str) {
    FlashRef ref;
    Flash *flash;

    Flash_Advance(gMsgWin->flash);
    flash = gMsgWin->flash;
    gMsgWin->text = str;
    Flash_FindLabel(flash, NULL, "mc_message_text", &ref);
    if (str != NULL) {
        TextBox_AttachString(flash, &ref, 0, 0, str, &gMsgWin->box);
        Flash_ClipSetCallbackC(flash, &ref, MsgWin_PortDrawClip, &((TextBoxFull *)&gMsgWin->box)->draw);
    } else {
        Flash_ClipSetCallbackC(flash, &ref, NULL, NULL);
    }
    Flash_Draw(flash); /* Backing remains wide; glyphs are deferred until the flush. */
    if (sPortMsgWinTextMarker) Port_GsMarker(sPortMsgWinTextMarker);
    Font_FlushAll();
    if (sPortMsgWinTextMarker) Port_GsMarker(0x13);
}

/* Dynamic Dragon Net dialogue follows the existing Dragon Story MsgWin rule. */
void Port_MsgWinDrawString43(u16 *str) {
    s32 old = sPortMsgWinTextMarker, oldInset = sPortMsgWinKeepInset;
    sPortMsgWinTextMarker = Port_TitleMainChars43() && !Port_Menus43() ? 0x1A : 0;
    sPortMsgWinKeepInset = sPortMsgWinTextMarker != 0;
    MsgWin_PortDrawString(str);
    sPortMsgWinTextMarker = old;
    sPortMsgWinKeepInset = oldInset;
}

/* Results do not draw the message box, but its native text style still needs the
   movie's opening animation and text-clip attachment before names can use it. */
void MsgWin_PortPrepareNativeNames(void) {
    static u16 blank[] = {0xFEFF, ' ', 0};
    FlashRef ref;
    Flash_Advance(gMsgWin->flash);
    Flash_FindLabel(gMsgWin->flash, NULL, "mc_message_text", &ref);
    TextBox_AttachString(gMsgWin->flash, &ref, 0, 0, blank, &gMsgWin->box);
}

/* Draw a short player name elsewhere on the screen with the message window's
   native font, colors, shadow and scale. The window itself remains unchanged. */
void MsgWin_PortDrawNativeName(const char *name, s32 x0, s32 y0, s32 x1, s32 y1) {
    TextBoxDraw draw;
    u16 text[34];
    s32 i, height;

    if (name == NULL || name[0] == 0) return;
    draw = ((TextBoxFull *)&gMsgWin->box)->draw;
    text[0] = 0xFEFF;
    for (i = 0; i < 32 && name[i] != 0; i++) text[i + 1] = (u8)name[i];
    text[i + 1] = 0;
    draw.str = text;
    draw.align = 1;
    height = (s32)((f32)Font_GetGlyphHeight() * draw.scaleY);
    draw.x = (x0 + x1) / 2;
    draw.y = y0 + ((y1 - y0) - height) / 2;
    draw.clip[0] = 0;
    draw.clip[1] = 0;
    draw.clip[2] = 512;
    draw.clip[3] = 448;
    draw.noFlush = 0;
    TextBox_DrawClip(&draw, NULL);
}

/* How wide a line may be, in the font's pixels: from where the line starts up to `right` (the screen's 512). */
s32 MsgWin_PortLineRoom(s32 right) {
    f32 x = sMsgWinTextX < 0.0f ? 40.0f : sMsgWinTextX + (f32)gMsgWin->box.unkC;

    return (s32)((f32)right - x);
}

/* One or two lines start at the same height (the two lines' place): screens whose line changes with the cursor do not
   have it jump up and down. Three start higher, as the game's. */
void MsgWin_PortSameTop(void) {
    TextBox_SetLineOffsets((struct TextBox *)&gMsgWin->box, 0x14, 0x14, 0xA, 0, 0);
}
#endif

/* Passes two values to TextBox_SetSpacing for the window's text box. */
void MsgWin_SetBoxParam(s32 a, s32 b) {
    TextBox_SetSpacing(&gMsgWin->box, a, b);
}
