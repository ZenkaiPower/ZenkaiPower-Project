/* Zenkai native elements: optional procedural replacements shared by renderers.
   New collection members must honor the master switch through this helper. */
#ifndef GS_NATIVE_ELEMENTS_H
#define GS_NATIVE_ELEMENTS_H
#include <stdint.h>
#include "../../../include/menu/port_build_flags.h"
static int ZenkaiNative_Enabled(void) {
    return !!ZENKAIPOWER_NATIVE_ELEMENTS;
}
static int ZenkaiNative_HudShapes(void) {
    return ZENKAIPOWER_NATIVE_ELEMENTS && ZENKAIPOWER_NATIVE_HUD_SHAPES;
}
/* Exact decoded identities, not VRAM addresses: shared texture slots are reused
   by menus and stages. Unknown art, palettes and texture packs stay original.
   1: timer panels; 2: shadow atlas; 3..7: green, red, gold, yellow, blue gauges. */
static int ZenkaiNative_Identify(const uint8_t *rgba, int w, int h, int indexed) {
    static const uint64_t hashes[] = {
        UINT64_C(0xd25f5a832e24cdb4),
        UINT64_C(0x848cd90da26338c2),
        UINT64_C(0xdbeb7a2dbb58cd14),
        UINT64_C(0x8336a844bbc62ba9),
        UINT64_C(0x1e587af79de2852f),
        UINT64_C(0x890141fe59cd89b7),
        UINT64_C(0x6d6f718a4e3ade86),
    };
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!rgba || !indexed || h != 64 || (w != 64 && w != 256)) return 0;
    for (unsigned i = 0; i < (unsigned)w*h*4; i++)
        hash = (hash ^ rgba[i]) * UINT64_C(1099511628211);
    for (unsigned i = w == 64 ? 0 : 1; i < (w == 64 ? 1 : 7); i++)
        if (hash == hashes[i]) return (int)i+1;
    return 0;
}
/* Timer digits (white, yellow, red/infinity) retain their selected font,
   but share the native timer panel's game-resolution rendering domain. */
static int ZenkaiNative_TimerDigits(const uint8_t *rgba, int w, int h, int indexed) {
    static const uint64_t hashes[3]={
        UINT64_C(0xbf31c51ea53c9536),
        UINT64_C(0x0b87f838c8da4542),
        UINT64_C(0xe2e09efc6b7a6d32),
    };
    if(!rgba || !indexed || w!=128 || h!=128) return 0;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned i=0;i<128*128*4;i++) hash=(hash^rgba[i])*UINT64_C(1099511628211);
    return hash==hashes[0] || hash==hashes[1] || hash==hashes[2];
}
/* Extract only the gradient colors into a 48x1 palette. Geometry remains
   procedural. Keeping dynamic palette indexing out of GLSL constant arrays
   avoids large per-pipeline compiler work and fragment register pressure. */
static int ZenkaiNative_Palette(const uint32_t *source, int kind, uint32_t colors[48]) {
    int at=0;
    if(kind==1) { colors[0]=0xffffffffu; return 1; }
    for(int y=0;y<12;y++) colors[at++]=source[y*256+100];
    for(int y=6;y<12;y++) colors[at++]=source[y*256+40];
    for(int y=0;y<6;y++) colors[at++]=source[y*256+175];
    for(int y=16;y<24;y++) colors[at++]=source[y*256+4];
    for(int y=16;y<32;y++) colors[at++]=source[y*256+25-(y-16)/2];
    return at;
}
#endif
