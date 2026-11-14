#include "common.h"
#include "menu/menu_a.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "menu/port_screen.h"

#ifdef PORT
/*
 * PC only: the progress mode of the port's own screens (include/menu/port_screen.h, docs/port/menus.md). The game
 * has no mode to spare for a new screen, so the screens run in one that Progress_Main (menu_a_b.c) hands to
 * PortScreen_Main; gPortScreen says which. A screen reads what it needs itself: the windows of the game's menus come
 * from the versus mode's archive (gMenuArc5, as Duel_Main reads it).
 */

extern s32 PortTbo_Run(void); /* Wii Dragon Net Battle. */

s32 gPortScreen;

/* Shared page, sprite, and text drawing for Dragon Net Battle and language selection. */
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"

extern f32 cosf(f32 x);
extern f32 sinf(f32 x);

extern u64 *Dma_BeginDirect(void);
extern void Dma_EndDirect(u64 *end);

/* A colour for the shapes and the letters' tints: 0..255 a channel. */
u32 Ps_Rgba(s32 r, s32 g, s32 b, s32 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

/* Draw a four-cornered shape in screen coordinates. */
void Ps_ScreenQuad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3) {
    u64 *p = Dma_BeginDirect();

    p[0] = 4 | (1ULL << 15) | (1ULL << 60);
    p[1] = 0xE;
    p += 2;
    p[0] = 0x01BF000001FF0000ULL;
    p[1] = 0x40;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42;
    p += 2;
    p[0] = 0x30000;
    p[1] = 0x47;
    p += 2;
    p[0] = 0x4C;
    p[1] = 0x00;
    p += 2;
    p[0] = 4 | (1ULL << 15) | (1ULL << 58) | (2ULL << 60);
    p[1] = 0x51;
    p += 2;
    p[0] = (u64)c0 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c1 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c2 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x2 * 16.0f) + 0x7000) | ((u64)((s32)(y2 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)c3 | (0x3F800000ULL << 32);
    p[1] = (u64)((s32)(x3 * 16.0f) + 0x7000) | ((u64)((s32)(y3 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

void Ps_Copy(char *dst, const char *src, s32 max) {
    s32 i;

    for (i = 0; i < max - 1 && src[i] != 0; i++) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

/* `src` after what `dst` (`max` bytes) holds. (No sprintf "%s" in this file: the game's code has 32-bit pointers,
   and one passed through sprintf's "..." can reach the C library with garbage in its upper half.) */
void Ps_Cat(char *dst, const char *src, s32 max) {
    s32 n = 0;

    while (n < max - 1 && dst[n] != 0) {
        n++;
    }
    Ps_Copy(dst + n, src, max - n);
}

s32 Ps_Len(const char *s) {
    s32 n = 0;

    while (s[n] != 0) {
        n++;
    }
    return n;
}

s32 Ps_Move(s32 *cur, s32 count) {
    Pad *pad = &gPad[0];
    s32 was = *cur;

    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        *cur = (*cur + 1) % count;
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        *cur = (*cur + count - 1) % count;
    }
    if (*cur != was) {
        Snd_PlaySe(1, 0);
    }
    return *cur != was;
}

/* ---------------------------------------------------------------------------------------------- the pictures */
/* Extracted Wii artwork is drawn on a centered 4:3 page. */
extern int Port_PageAspectMilli(void); /* port/src/plat_stub.c: the shape the menus are shown in (4:3 with "4:3 in menus", else the window's), width / height * 1000 */

static f32 sPsPx, sPsPy, sPsPw = 512.0f, sPsPh = 448.0f; /* the page on the screen, set once a frame */
static s32 sPsAlign;                                      /* 0 a line starts at its x, 1 is centred on it, 2 ends there */

/* Dragon Net foregrounds mix raw screen art and page/font coordinates. While scoped,
   make page conversion identity and let one renderer marker transform the whole group. */
static s32 sPsForeground43, sPsForegroundIdentity;
extern s32 Port_TitleMainChars43(void);
extern s32 Port_Menus43(void);
extern void Port_GsMarker(s32 effect);
void Ps_Foreground43Begin(void) {
    sPsForeground43 = Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli() > 1340;
    sPsForegroundIdentity = sPsForeground43;
    Ps_PageSet();
    if (sPsForeground43) Port_GsMarker(0x19);
}
void Ps_ForegroundStretchBegin(void) {
    sPsForeground43 = 0;
    sPsForegroundIdentity = 1;
    Ps_PageSet();
}
void Ps_Foreground43Anchor(s32 edge) {
    if (sPsForeground43) Port_GsMarker(edge == 1 ? 0x1A : edge == 2 ? 0x1B : 0x19);
}
void Ps_Foreground43End(void) {
    if (sPsForeground43) {
        Font_FlushAll();
        Port_GsMarker(0x13);
    }
    sPsForeground43 = sPsForegroundIdentity = 0;
    Ps_PageSet();
}

void Ps_PageSet(void) {
    if (sPsForegroundIdentity) {
        sPsPx = sPsPy = 0.0f;
        sPsPw = 512.0f; sPsPh = 448.0f;
        return;
    }

    f32 aspect = (f32)Port_PageAspectMilli() / 1000.0f;
    f32 page = 4.0f / 3.0f;

    if (aspect >= page) { /* wider: the whole height, the grounds at the sides */
        sPsPh = 448.0f;
        sPsPw = 512.0f * page / aspect;
    } else {
        sPsPw = 512.0f;
        sPsPh = 448.0f * aspect / page;
    }
    sPsPx = (512.0f - sPsPw) * 0.5f;
    sPsPy = (448.0f - sPsPh) * 0.5f;
}

/* Horizontal page entrance offset. */
static f32 sPsShift;

void Ps_SetShift(f32 x) {
    sPsShift = x;
}

f32 Ps_X(f32 x) {
    return sPsPx + (x + sPsShift) * sPsPw / 512.0f;
}

/* Where the window's left edge is, in the page's units (0 on a 4:3 window, less on a wider one). */
f32 Ps_Left(void) {
    return -sPsPx * 512.0f / sPsPw;
}

f32 Ps_Y(f32 y) {
    return sPsPy + y * sPsPh / 448.0f;
}

/* 0..1 of the picture as a UV coordinate: 1024 texels in 1/16ths, held to the register's 14 bits */
u32 Ps_Uv(f32 t) {
    s32 v = (s32)(t * 1024.0f * 16.0f);

    return v < 0 ? 0 : v > 0x3FFF ? 0x3FFF : (u32)v;
}

/* A picture's part (u, v in 0..1) over a box of the SCREEN, tinted (0x80 per channel = as it is). */
void Ps_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    u64 *p;

    if (alpha <= 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    if (alpha > 0x80) {
        alpha = 0x80;
    }
    p = Dma_BeginDirect();
    p[0] = 4 | (1ULL << 15) | (1ULL << 60); /* A+D x 4 */
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1: blended by the source's alpha */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1: bilinear */
    p += 2;
    p[0] = id == TBO_HUD_NAME_FRAME ? 0x3001B : 0x30000; /* Skip transparent frame texels in its alpha-mask pass. */
    p[1] = 0x47; /* TEST_1: no alpha test, depth always passes */
    p += 2;
    p[0] = 5;
    p[1] = 0x08; /* CLAMP_1: clamped both ways */
    p += 2;
    p[0] = 1 | (1ULL << 15) | (1ULL << 58) | (8ULL << 60);
    p[1] = 0x535306E1; /* RGBAQ, (A+D: nothing), TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
    p += 2;
    p[0] = (u64)(tint & 0xFFFFFF) | ((u64)alpha << 24);
    p[1] = 0;
    p += 2;
    /* TEX0: base 1, width 1, format TBO_PSM, 1024 x 1024, the texture's alpha, modulated, the picture's number where
       the palette address goes */
    p[0] = 1ULL | (1ULL << 14) | ((u64)TBO_PSM << 20) | (10ULL << 26) | (10ULL << 30) | (1ULL << 34) | ((u64)(id & 0x3FFF) << 37);
    p[1] = 0x156; /* PRIM: sprite, textured, blended, UV */
    p += 2;
    p[0] = (u64)Ps_Uv(u0) | ((u64)Ps_Uv(v0) << 16);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)Ps_Uv(u1) | ((u64)Ps_Uv(v1) << 16);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

/* The same over a box of the page. */
void Ps_Sprite(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    Ps_SpriteScreen(id, Ps_X(x0), Ps_Y(y0), Ps_X(x1), Ps_Y(y1), u0, v0, u1, v1, tint, alpha);
}

f32 Ps_PicW(s32 id) {
    return (f32)sTboSize[id][0];
}

f32 Ps_PicH(s32 id) {
    return (f32)sTboSize[id][1];
}

/* Draw shapes in page coordinates. */
void Ps_Quad(f32 x0, f32 y0, f32 x1, f32 y1, f32 x2, f32 y2, f32 x3, f32 y3, u32 c0, u32 c1, u32 c2, u32 c3) {
    Ps_ScreenQuad(Ps_X(x0), Ps_Y(y0), Ps_X(x1), Ps_Y(y1), Ps_X(x2), Ps_Y(y2), Ps_X(x3), Ps_Y(y3), c0, c1, c2, c3);
}

void Ps_Rect(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Ps_Quad(x0, y0, x1, y0, x0, y1, x1, y1, top, top, bottom, bottom);
}

/* ---- the letters: lines written with the TrueType fonts of the font list (port/src/gs/tbo_text.c): the headings
   and buttons in the Menu font, the hosts' talk in the Dialogue / UI font (F1 > UI; on "Original" the first font of
   fonts.tsv for each) ---- */
extern int PortTbo_TextId(const char *text, int kind);      /* a line's number for Ps_Sprite, -1: not written */
extern int PortTbo_TextInfo(int id, int what);              /* its measures, in thousandths of its capitals */
extern int PortTbo_TextMeasure(const char *text, int kind); /* how far the pen goes, without keeping it */

void Ps_Align(s32 align) {
    sPsAlign = align;
}

f32 Ps_TextWidth(char *str, f32 scale) {
    return (f32)PortTbo_TextMeasure(str, 0) / 1000.0f * PS_CELL_UNITS * scale * PS_CAP_OF_CELL * PS_UNIT_X;
}

/* Where a line of `scale` starts (its y) for its capitals to be centred on `middle`. */
f32 Ps_TextTop(f32 middle, f32 scale) {
    f32 cell = PS_CELL_UNITS * scale;

    return middle - cell * (PS_BASE_OF_CELL - PS_CAP_OF_CELL * 0.5f);
}

/* Colour of Ps_Rgba (0..255 per channel) as a tint of the white letters (0x80 = as they are). */
u32 Ps_Tint(u32 c) {
    return ((c & 0xFF) >> 1) | ((((c >> 8) & 0xFF) >> 1) << 8) | ((((c >> 16) & 0xFF) >> 1) << 16);
}

u32 Ps_Mix(u32 a, u32 b, f32 t) {
    s32 k;
    u32 out = 0;

    for (k = 0; k < 24; k += 8) {
        f32 ca = (f32)((a >> k) & 0xFF);
        f32 cb = (f32)((b >> k) & 0xFF);

        out |= (u32)(ca + (cb - ca) * t) << k;
    }
    return out;
}

/* A line's picture (the line filled above, outlined below) over the page: the pen starting at x, the baseline at
   `base`, capitals `cap` units high, squeezed across by `fit`. bands 0: the outline in `line`; else the fill from
   `top` to `bottom` in that many bands. */
void Ps_Line(s32 id, f32 x, f32 base, f32 cap, f32 fit, u32 top, u32 bottom, u32 line, s32 bands, s32 alpha) {
    f32 ky = cap / 1000.0f;
    f32 kx = ky * fit * PS_UNIT_X;
    f32 x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    f32 x1 = x0 + (f32)PortTbo_TextInfo(id, 1) * kx;
    f32 y0 = base - (f32)PortTbo_TextInfo(id, 3) * ky;
    f32 h = (f32)PortTbo_TextInfo(id, 2) * ky;
    s32 band;

    if (bands == 0) {
        Ps_Sprite(id, x0, y0, x1, y0 + h, 0.0f, 0.5f, 1.0f, 1.0f, Ps_Tint(line), alpha);
        return;
    }
    for (band = 0; band < bands; band++) {
        f32 t0 = (f32)band / (f32)bands;
        f32 t1 = (f32)(band + 1) / (f32)bands;

        Ps_Sprite(id, x0, y0 + h * t0, x1, y0 + h * t1, 0.0f, t0 * 0.5f, 1.0f, t1 * 0.5f,
                  Ps_Tint(Ps_Mix(top, bottom, ((f32)band + 0.5f) / (f32)bands)), alpha);
    }
}

/* A line of letters with independent horizontal and vertical size, and an optional outer border. */
void Ps_TextExOuterXY(f32 x, f32 y, char *str, f32 scaleX, f32 scaleY, f32 width,
                      u32 top, u32 bottom, u32 line, u32 outer, f32 outerWidth) {
    f32 cell = PS_CELL_UNITS * scaleY;
    f32 cap = cell * PS_CAP_OF_CELL;
    f32 fit = 1.0f;
    f32 lineFit;
    f32 w;
    s32 alpha = (s32)((top >> 24) & 0xFF);
    s32 id;

    if (str[0] == 0 || (id = PortTbo_TextId(str, 0)) < 0) {
        return;
    }
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * PS_CELL_UNITS * scaleX * PS_CAP_OF_CELL * PS_UNIT_X;
    if (width > 0.0f && w > width) {
        fit = width / w;
        if (fit < 0.6f) {
            fit = 0.6f;
        }
        w *= fit;
    }
    if (sPsAlign == 1) {
        x -= w * 0.5f;
    } else if (sPsAlign == 2) {
        x -= w;
    }
    lineFit = scaleX == scaleY ? fit : fit * scaleX / scaleY;
    if (outerWidth > 0.0f) {
        static const f32 offset[8][2] = {
            {-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f},
            {-0.707f, -0.707f}, {0.707f, -0.707f}, {-0.707f, 0.707f}, {0.707f, 0.707f}
        };
        s32 i;
        for (i = 0; i < 8; i++) {
            Ps_Line(id, x + offset[i][0] * outerWidth,
                    y + cell * PS_BASE_OF_CELL + offset[i][1] * outerWidth,
                    cap, lineFit, top, bottom, outer, 0, alpha);
        }
    }
    Ps_Line(id, x, y + cell * PS_BASE_OF_CELL, cap, lineFit, top, bottom, line, 0, alpha); /* the outline under the fill */
    Ps_Line(id, x, y + cell * PS_BASE_OF_CELL, cap, lineFit, top, bottom, line, 3, alpha);
}

/* A line of letters with an optional second border outside its dark outline. */
void Ps_TextExOuter(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom,
                    u32 line, u32 outer, f32 outerWidth) {
    Ps_TextExOuterXY(x, y, str, scale, scale, width, top, bottom, line, outer, outerWidth);
}

/* A line of letters: filled from `top` to `bottom` (a gradient), outlined in `line`; squeezed to `width` if it is
   wider (0: no limit). The alpha is the top colour's. */
void Ps_TextEx(f32 x, f32 y, char *str, f32 scale, f32 width, u32 top, u32 bottom, u32 line) {
    Ps_TextExOuter(x, y, str, scale, width, top, bottom, line, 0, 0.0f);
}

/* A heading in the main menu's heading style (gold to red, a brown bevelled outline, a light rim: one picture in
   its own colours), its capitals as Ps_TextEx's of that scale; squeezed to `width` if it is wider (0: no limit). */
void Ps_Heading(f32 x, f32 y, char *str, f32 scale, f32 width, s32 alpha) {
    f32 cell = PS_CELL_UNITS * scale;
    f32 cap = cell * PS_CAP_OF_CELL;
    f32 ky = cap / 1000.0f;
    f32 kx;
    f32 fit = 1.0f;
    f32 w;
    f32 x0;
    f32 y0;
    s32 id;

    if (str[0] == 0 || (id = PortTbo_TextId(str, 2)) < 0) {
        return;
    }
    w = (f32)PortTbo_TextInfo(id, 0) / 1000.0f * cap * PS_UNIT_X;
    if (width > 0.0f && w > width) {
        fit = width / w;
        if (fit < 0.6f) {
            fit = 0.6f;
        }
        w *= fit;
    }
    if (sPsAlign == 1) {
        x -= w * 0.5f;
    } else if (sPsAlign == 2) {
        x -= w;
    }
    kx = ky * fit * PS_UNIT_X;
    x0 = x - (f32)PortTbo_TextInfo(id, 4) * kx;
    y0 = y + cell * PS_BASE_OF_CELL - (f32)PortTbo_TextInfo(id, 3) * ky;
    Ps_Sprite(id, x0, y0, x0 + (f32)PortTbo_TextInfo(id, 1) * kx, y0 + (f32)PortTbo_TextInfo(id, 2) * ky, 0.0f, 0.0f, 1.0f, 1.0f,
              Ps_Tint(0xFFFFFF), alpha);
}

/* ---- the talk box's letters: capitals 11.5 units high, the baseline 18.25 down (about the size of the main
   menu's own), white with a thin dark edge ---- */

void Ps_Text(f32 x, f32 y, char *s, f32 scale, f32 width, s32 style) {
    switch (style) {
    case 1: /* gold: under the cursor */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xEC, 0xB8, 0x80), Ps_Rgba(0xFF, 0x94, 0x30, 0x80), Ps_Rgba(0x5C, 0x24, 0x08, 0x80));
        break;
    case 2: /* a heading: as the lines (gold is the cursor's alone, as in the game's menus) */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80), Ps_Rgba(0xE4, 0xD8, 0xFF, 0x80), Ps_Rgba(0x40, 0x14, 0x64, 0x80));
        break;
    case 3: /* cyan: a value */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xD0, 0xFC, 0xFF, 0x80), Ps_Rgba(0x70, 0xE0, 0xFF, 0x80), Ps_Rgba(0x08, 0x28, 0x50, 0x80));
        break;
    case 4: /* grey: not open yet */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xC8, 0xC8, 0xD4, 0x80), Ps_Rgba(0x98, 0x98, 0xAC, 0x80), Ps_Rgba(0x30, 0x30, 0x44, 0x80));
        break;
    case 5: /* green: online */
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xD8, 0xFF, 0xC0, 0x80), Ps_Rgba(0x60, 0xE8, 0x50, 0x80), Ps_Rgba(0x08, 0x40, 0x10, 0x80));
        break;
    default:
        Ps_TextEx(x, y, s, scale, width, Ps_Rgba(0xFF, 0xFF, 0xFF, 0x80), Ps_Rgba(0xE4, 0xD8, 0xFF, 0x80), Ps_Rgba(0x40, 0x14, 0x64, 0x80));
        break;
    }
}

s32 PortScreen_Main(void) {
    s32 screen = gPortScreen;
    s32 result = 0;

    gPortScreen = PORT_SCREEN_NONE;
    if (gMenuArc5 == NULL) {
        gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    }
    switch (screen) {
    case PORT_SCREEN_DRAGON_NET:
        Bgm_Play(0x10B16 + 1);
        result = PortTbo_Run();
        Adx_StopAll();
        gProgress->mode = 4;
        break;
    default:
        gProgress->mode = 4;
        break;
    }
    return result;
}
#endif
