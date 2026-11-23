#ifndef GS_SCREEN_TEXT_H
#define GS_SCREEN_TEXT_H
#include "gs_draw.h"
#include <stdlib.h>
#include <string.h>

/* A suffix, rather than text moved to the end: subsequent plates, translucent
   dialogs and fades must remain above the lettering. Destination alpha masks
   are replayed together. Buffer reads, shadows and native effects retain their
   original passes. */
static int GsScreenText_Start(int target, uint32_t (*pipeKey)(int)) {
    int start = -1, first = -1, masked = 0;
    for (unsigned n = gsDrawCount; n-- > 0;) {
        const GsDraw *d = &gsDraws[n];
        if ((d->native && d->native != 5) || d->vu || d->target != target || d->tex_is_target ||
            (pipeKey(d->pipeline) & ((1u << 24) | (1u << 22)))) break;
        first = (int)n;
        if (d->native == 5 || d->misc[1] != 0) masked = 1;
        if (d->screenText) start = (int)n;
    }
    /* Reveal masks are built before the word: replay the complete safe UI
       suffix so their alpha writes and DATE snapshots have display precision. */
    return start >= 0 && masked ? first : start;
}

static void GsScreenText_Size(int sw, int sh, int aspect, int *w, int *h) {
    *w = *h = 0;
    if (sw <= 0 || sh <= 0 || aspect <= 0) return;
    if ((long long)sw * 1000 > (long long)sh * aspect) {
        *h = sh; *w = (int)(((long long)sh * aspect + 500) / 1000);
    } else {
        *w = sw; *h = (int)(((long long)sw * 1000 + aspect / 2) / aspect);
    }
}

/* The ordinary GS shader spans 1024x1024; the displayed rectangle is 512x448.
   Moving its vertices into that viewport keeps glyph UVs, colors and Z intact. */
static Vtx *GsScreenText_Vertices(void) {
    Vtx *vertices = malloc((size_t)gsVertCount * sizeof(Vtx));
    if (!vertices) return NULL;
    memcpy(vertices, gsVerts, (size_t)gsVertCount * sizeof(Vtx));
    for (unsigned i = 0; i < gsVertCount; i++) {
        vertices[i].x *= (float)GS_W / 512;
        vertices[i].y *= (float)GS_H / 448;
    }
    return vertices;
}

static void GsScreenText_Scissor(const int *in, int *out, int w, int h) {
    int extent[2] = {512 * SCALE, 448 * SCALE}, size[2] = {w, h};
    for (int axis = 0; axis < 2; axis++) {
        long long begin = (long long)in[axis] * size[axis] / extent[axis];
        long long end = ((long long)(in[axis] + in[axis+2]) * size[axis] + extent[axis]-1) / extent[axis];
        if (begin < 0) begin = 0;
        if (begin > size[axis]) begin = size[axis];
        if (end < begin) end = begin;
        if (end > size[axis]) end = size[axis];
        out[axis] = (int)begin; out[axis+2] = (int)(end-begin);
    }
}
#endif
