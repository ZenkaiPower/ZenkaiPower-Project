/*
 * Dragon Net Battle's letters (src/menu/port_tbo.c), written with the TrueType fonts of the font list
 * (menu_text.c, MenuText_Line*): its headings and buttons in the Menu font, its talk box in the Dialogue / UI font,
 * each on "Original" the first font of fonts.tsv for that use. A font added to fonts.tsv is a choice in F1 > UI and
 * so a look for these screens too; any character the font has can be written.
 *
 * The game's side asks for a line each frame it draws it (PortTbo_TextId, on the game's thread) and gets a number
 * for it and its measures; it then draws it as one of the screens' pictures (TBO_PSM, the number in CBP). The
 * renderer asks here for the picture of that number (TboText_Pixels) the first time and whenever the line or the
 * font changed (its version). The picture is the line twice: filled above, outlined below, white, so the game
 * tints and blends it as it did its letter sheet; a heading (kind 2) is one picture in its colours, the main
 * menu's heading style (MenuText_Heading*).
 *
 * Nothing here is game state: the screens are a menu of the PC's, before and outside an online match.
 */
#include "menu_text.h"
#include "menu_translation.h"
#include "../../../include/menu/tbo_art.h"
#include <SDL3/SDL.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEXT_MAX 128
#define IDLE_MS 1000 /* a line not asked for that long gives its place to another */

typedef struct {
    char text[TEXT_MAX];
    int kind;
    uint32_t generation; /* of the fonts it was measured with */
    uint32_t version;    /* changes whenever what the place holds changes (0: never used) */
    uint64_t askedMs;
    int advance, w, h, base, pad, cap, outline;
    int inkX, inkY, inkW, inkH;
} TboLine;

static TboLine sLines[TBO_TEXT_SLOTS];
static uint32_t sVersion;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

/* Whether the selected Menu font is active for text that actually has wording
   in the selected language. Port catalog flag 2 means a locale-specific row. */
int PortTbo_WiiUseFont(void) { return MenuText_GetFont() != 0; }
int PortTbo_WiiUseFontForArtwork(int textKind) {
    const char *locale = MenuTranslation_Locale();
    if (locale != NULL && strncmp(locale, "en", 2) != 0)
        return textKind == 2;
    return MenuText_GetFont() != 0 || textKind != 0;
}

/* The size the letters are drawn at, by kind (capitals, in pixels: the screens show them smaller, from smaller
   copies), and the outline's thickness: as the letter sheet had it, about a 16th of the capitals. */
static int line_cap(int kind) { return kind == 0 ? 80 : 48; }
static int line_outline(int kind) { return kind == 0 ? 5 : 3; }

static int line_measure(TboLine *l, uint32_t generation) {
    l->generation = generation;
    l->version = ++sVersion;
    if (sVersion == 0) {
        l->version = sVersion = 1;
    }
    if (l->kind == 3 || l->kind == 4) {
        /* Measure the actual styled caption, including its margin, outline
           and shadow. Generic line advance uses a different cap and cannot
           center the visible pixels of this fixed-size label texture. */
        int x,y,x1=0,y1=0;
        uint8_t *pixels=MenuText_RenderWiiLabel(l->text,l->kind-3,2,&l->w,&l->h);
        if(!pixels) return 0;
        l->cap=38;l->pad=10;l->base=48;l->outline=0;
        l->inkX=l->w;l->inkY=l->h;
        for(y=0;y<l->h;y++) for(x=0;x<l->w;x++) {
            if(!pixels[((size_t)y*l->w+x)*4+3]) continue;
            if(x<l->inkX) l->inkX=x;
            if(y<l->inkY) l->inkY=y;
            if(x+1>x1) x1=x+1;
            if(y+1>y1) y1=y+1;
        }
        free(pixels);
        l->inkW=x1-l->inkX;l->inkH=y1-l->inkY;l->advance=l->inkW;
        return l->inkW>0 && l->inkH>0;
    }
    if (l->kind >= 3 && l->kind <= 5) {
        int mw, mh, mb;
        l->cap = l->kind == 5 ? 21 : 16;
        l->pad = l->kind == 5 ? 8 : 4;
        l->outline = 0;
        l->w = 1024; l->h = l->kind == 5 ? 128 : 64;
        l->base = l->kind == 5 ? 44 : 22;
        l->advance = MenuText_LineMeasure(0, l->text, l->cap, l->pad, &mw, &mh, &mb);
        return l->advance > 0;
    }
    if (l->kind == 7) {
        int w,h;
        uint8_t *pixels=MenuText_RenderPauseHeading(l->text,&w,&h);
        if (!pixels) return 0;
        free(pixels);
        l->w=w;l->h=h;l->cap=72;l->base=92;l->pad=16;l->outline=0;
        l->advance=w;return 1;
    }
    if (l->kind == 6) {
        l->cap = 48; l->pad = 10; l->outline = 3;
        int x, y, x1 = 0, y1 = 0;
        uint8_t *coverage;
        l->advance = MenuText_LineMeasure(2, l->text, l->cap, l->pad, &l->w, &l->h, &l->base);
        if (l->advance <= 0) return 0;
        coverage = MenuText_LineRender(2, l->text, l->cap, l->pad, &l->w, &l->h, &l->base);
        if (!coverage) return 0;
        l->inkX = l->w; l->inkY = l->h;
        for (y = 0; y < l->h; y++) for (x = 0; x < l->w; x++) {
            if (!coverage[y * l->w + x]) continue;
            if (x < l->inkX) l->inkX = x;
            if (y < l->inkY) l->inkY = y;
            if (x + 1 > x1) x1 = x + 1;
            if (y + 1 > y1) y1 = y + 1;
        }
        free(coverage);
        l->inkW = x1 - l->inkX; l->inkH = y1 - l->inkY;
        return l->inkW > 0 && l->inkH > 0;
    }
    if (l->kind == 2) {
        l->outline = 0;
        l->advance = MenuText_HeadingMeasure(l->text, &l->cap, &l->w, &l->h, &l->base, &l->pad);
        return l->advance > 0;
    }
    l->cap = line_cap(l->kind);
    l->outline = line_outline(l->kind);
    l->pad = l->outline + 2;
    l->advance = MenuText_LineMeasure(l->kind, l->text, l->cap, l->pad, &l->w, &l->h, &l->base);
    return l->advance > 0;
}

/* The number of a line (TBO_TEXT_BASE and on), -1 if it cannot be written (no font, no text, no room). */
int PortTbo_TextId(const char *text, int kind) {
    uint32_t generation = MenuText_LineGeneration();
    uint64_t now = SDL_GetTicks();
    int i, spare = -1, id = -1;

    if (text == NULL || text[0] == '\0' || kind < 0 || kind > 7) {
        return -1;
    }
    pthread_mutex_lock(&sLock);
    for (i = 0; i < TBO_TEXT_SLOTS; i++) {
        TboLine *l = &sLines[i];
        if (l->version != 0 && l->kind == kind && strncmp(l->text, text, TEXT_MAX - 1) == 0) {
            l->askedMs = now;
            if (l->generation != generation && !line_measure(l, generation)) {
                break;
            }
            id = l->advance > 0 ? TBO_TEXT_BASE + i : -1;
            break;
        }
        if (spare < 0 && (l->version == 0 || now - l->askedMs >= IDLE_MS)) {
            spare = i;
        }
    }
    if (i == TBO_TEXT_SLOTS && spare >= 0) {
        TboLine *l = &sLines[spare];
        SDL_strlcpy(l->text, text, sizeof(l->text));
        l->kind = kind;
        l->askedMs = now;
        if (line_measure(l, generation)) {
            id = TBO_TEXT_BASE + spare;
        }
    }
    pthread_mutex_unlock(&sLock);
    return id;
}

/* How far the pen goes for a line, in thousandths of its capitals' height, without keeping it (for laying out:
   where to break a line). 0 without a font. */
int PortTbo_TextMeasure(const char *text, int kind) {
    int w, h, base, cap, adv;
    if (text == NULL || text[0] == '\0' || kind < 0 || kind > 1) {
        return 0;
    }
    cap = line_cap(kind);
    adv = MenuText_LineMeasure(kind, text, cap, 0, &w, &h, &base);
    return (int)((int64_t)adv * 1000 / cap);
}

/* A measure of a line, in thousandths of its capitals' height: 0 how far the pen goes, 1 the picture's width,
   2 its height (of one of its two halves; a heading's: all of it), 3 the baseline from its top, 4 the room left of
   the pen's start; HUD kind 6 also exposes 5/6 ink x/y and 7/8 ink width/height. */
int PortTbo_TextInfo(int id, int what) {
    int r = 0, v;
    pthread_mutex_lock(&sLock);
    if (id >= TBO_TEXT_BASE && id < TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        const TboLine *l = &sLines[id - TBO_TEXT_BASE];
        v = what == 0 ? l->advance : what == 1 ? l->w : what == 2 ? l->h : what == 3 ? l->base :
            what == 4 ? l->pad : what == 5 ? l->inkX : what == 6 ? l->inkY :
            what == 7 ? l->inkW : what == 8 ? l->inkH : 0;
        r = l->cap > 0 ? (int)((int64_t)v * 1000 / l->cap) : 0;
    }
    pthread_mutex_unlock(&sLock);
    return r;
}

/* Pause headings share the native battle UI's widescreen correction.
   Other generated text belongs to screens which already lay out their own page. */
int TboText_IsPauseHeading(uint32_t id) {
    int result=0;
    pthread_mutex_lock(&sLock);
    if(id>=TBO_TEXT_BASE && id<TBO_TEXT_BASE+TBO_TEXT_SLOTS) {
        const TboLine *line=&sLines[id-TBO_TEXT_BASE];
        result=line->version!=0 && line->kind==7;
    }
    pthread_mutex_unlock(&sLock);
    return result;
}

/* The version of a line's place (0: nothing there). */
uint32_t TboText_Version(uint32_t id) {
    uint32_t r = 0;
    pthread_mutex_lock(&sLock);
    if (id >= TBO_TEXT_BASE && id < TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        r = sLines[id - TBO_TEXT_BASE].version;
    }
    pthread_mutex_unlock(&sLock);
    return r ? (r * 16777619u ^ MenuText_RasterKey()) : 0;
}

/* The picture of a line for the renderer: *w x *h RGBA (the GS's alpha, 0x80 opaque), the filled line in the upper
   half and its outline in the lower (a heading: itself); NULL if there is none. free() it. *version: what it
   shows. */
uint32_t *TboText_Pixels(uint32_t id, uint32_t *version, uint32_t *w, uint32_t *h) {
    TboLine l;
    uint8_t *cov;
    uint32_t *px;
    int lw, lh, lb, x, y, dx, dy, r;

    if (id < TBO_TEXT_BASE || id >= TBO_TEXT_BASE + TBO_TEXT_SLOTS) {
        return NULL;
    }
    pthread_mutex_lock(&sLock);
    l = sLines[id - TBO_TEXT_BASE];
    pthread_mutex_unlock(&sLock);
    *version = l.version ? (l.version * 16777619u ^ MenuText_RasterKey()) : 0;
    if (l.version == 0 || l.advance <= 0) {
        return NULL;
    }
    if (l.kind >= 3 && l.kind <= 5) {
        int scale = MenuText_RasterScale(2);
        px = (uint32_t *)(void *)MenuText_RenderWiiLabel(l.text, l.kind - 3, scale, &lw, &lh);
        if (px == NULL) return NULL;
        /* Layout stays measured at 2x; normalized UVs allow the actual caption
           texture to follow the screen without moving or resizing Options rows. */
        if (lw * 2 != l.w * scale || lh * 2 != l.h * scale) { free(px); return NULL; }
        *w = (uint32_t)lw; *h = (uint32_t)lh;
        return px;
    }
    if (l.kind == 7) {
        px=(uint32_t *)(void *)MenuText_RenderPauseHeading(l.text,&lw,&lh);
        if (!px) return NULL;
        *w=lw;*h=lh;return px;
    }
    if (l.kind == 2) {
        px = (uint32_t *)(void *)MenuText_HeadingRender(l.text, &lw, &lh);
        if (px != NULL && (lw != l.w || lh != l.h)) {
            free(px);
            px = NULL;
        }
        *w = (uint32_t)lw;
        *h = (uint32_t)lh;
        return px;
    }
    cov = MenuText_LineRender(l.kind == 6 ? 2 : l.kind, l.text, l.cap, l.pad, &lw, &lh, &lb);
    if (cov == NULL) {
        return NULL;
    }
    if (lw != l.w || lh != l.h) { /* (the font changed since it was measured: it is measured again next frame) */
        free(cov);
        return NULL;
    }
    px = malloc((size_t)lw * lh * 2 * 4);
    if (px == NULL) {
        free(cov);
        return NULL;
    }
    r = l.outline;
    for (y = 0; y < lh; y++) {
        for (x = 0; x < lw; x++) {
            int fill = cov[(size_t)y * lw + x], line = 0;
            for (dy = -r; dy <= r && line < 255; dy++) {
                int yy = y + dy;
                if (yy < 0 || yy >= lh) {
                    continue;
                }
                for (dx = -r; dx <= r; dx++) {
                    int xx = x + dx, c;
                    if (xx < 0 || xx >= lw || dx * dx + dy * dy > r * r + r) {
                        continue;
                    }
                    c = cov[(size_t)yy * lw + xx];
                    if (c > line) {
                        line = c;
                    }
                }
            }
            px[(size_t)y * lw + x] = 0xFFFFFFu | (uint32_t)((fill * 128 + 127) / 255) << 24;
            px[(size_t)(y + lh) * lw + x] = 0xFFFFFFu | (uint32_t)((line * 128 + 127) / 255) << 24;
        }
    }
    free(cov);
    *w = (uint32_t)lw;
    *h = (uint32_t)lh * 2;
    return px;
}
