/* Run from a release folder with fonts.pak. Checks the actual native text-texture path. */
#include "../../src/gs/menu_text.h"
#include "../../src/gs/menu_labels.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
extern int PortTbo_TextId(const char *, int);
extern int PortTbo_TextInfo(int, int);
extern uint32_t *TboText_Pixels(uint32_t, uint32_t *, uint32_t *, uint32_t *);
const char *Port_FileRoot(void) { return "gamedata"; }
int PortSongs_JapaneseSoundtrack(void) { return 0; }
int Port_NetSession(void) { return 0; }
int main(void) {
    int id = PortTbo_TextId("Zen", 6);
    assert(id >= 0);
    assert(PortTbo_TextId("Zen", 6) == id);
    assert(PortTbo_TextInfo(id, 0) > 0);
    uint32_t version, w, h;
    uint32_t *px = TboText_Pixels(id, &version, &w, &h);
    assert(px && version && w > 0 && h > 0 && h % 2 == 0);
    unsigned fill = 0, outline = 0;
    for (uint32_t i = 0; i < w*h/2; i++) {
        unsigned a = px[i] >> 24, b = px[i+w*h/2] >> 24;
        assert(a <= 128 && b <= 128 && b >= a);
        fill += a; outline += b;
    }
    assert(fill > 0 && outline > fill);
    unsigned x0 = w, y0 = h, x1 = 0, y1 = 0;
    for (unsigned y = 0; y < h/2; y++) for (unsigned x = 0; x < w; x++) {
        if (!(px[y*w+x] >> 24)) continue;
        if (x < x0) x0 = x;
        if (y < y0) y0 = y;
        if (x+1 > x1) x1 = x+1;
        if (y+1 > y1) y1 = y+1;
    }
    /* Centering must use the visible glyph bounds, including lowercase descenders, not the advance box. */
    assert(PortTbo_TextInfo(id, 5) == (int)(x0*1000/48));
    assert(PortTbo_TextInfo(id, 6) == (int)(y0*1000/48));
    assert(PortTbo_TextInfo(id, 7) == (int)((x1-x0)*1000/48));
    assert(PortTbo_TextInfo(id, 8) == (int)((y1-y0)*1000/48));
    free(px);
    MenuText_SetFont(0);
    id = PortTbo_TextId("SparkingBot", 6);
    px = TboText_Pixels(id, &version, &w, &h);
    assert(px); free(px);
    puts("PASS: native Anton names, GS alpha and visible glyph bounds");
}
