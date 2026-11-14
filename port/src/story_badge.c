/* Derive difficulty badges from the loaded indexed texture. No external images.
   GS upload mapping matches menu_font.py; original ring and CLUT stay unchanged. */
#include <stdint.h>
#include <string.h>
#include "port_addr.h"
#include "port_host.h"
static const unsigned char block32[4][8] = {
    {0,1,4,5,16,17,20,21},
    {2,3,6,7,18,19,22,23},
    {8,9,12,13,24,25,28,29},
    {10,11,14,15,26,27,30,31}
};
static const unsigned char col32[8][8] = {
    {0,1,4,5,8,9,12,13},
    {2,3,6,7,10,11,14,15},
    {16,17,20,21,24,25,28,29},
    {18,19,22,23,26,27,30,31},
    {32,33,36,37,40,41,44,45},
    {34,35,38,39,42,43,46,47},
    {48,49,52,53,56,57,60,61},
    {50,51,54,55,58,59,62,63}
};
static const unsigned char col8[4][16] = {
    {0,4,16,20,32,36,48,52,2,6,18,22,34,38,50,54},
    {8,12,24,28,40,44,56,60,10,14,26,30,42,46,58,62},
    {33,37,49,53,1,5,17,21,35,39,51,55,3,7,19,23},
    {41,45,57,61,9,13,25,29,43,47,59,63,11,15,27,31}
};
static const unsigned char block4[8][4] = {
    {0,2,8,10},
    {1,3,9,11},
    {4,6,12,14},
    {5,7,13,15},
    {16,18,24,26},
    {17,19,25,27},
    {20,22,28,30},
    {21,23,29,31}
};
static const unsigned char col4[4][32] = {
    {0,8,32,40,64,72,96,104,2,10,34,42,66,74,98,106,4,12,36,44,68,76,100,108,6,14,38,46,70,78,102,110},
    {16,24,48,56,80,88,112,120,18,26,50,58,82,90,114,122,20,28,52,60,84,92,116,124,22,30,54,62,86,94,118,126},
    {65,73,97,105,1,9,33,41,67,75,99,107,3,11,35,43,69,77,101,109,5,13,37,45,71,79,103,111,7,15,39,47},
    {81,89,113,121,17,25,49,57,83,91,115,123,19,27,51,59,85,93,117,125,21,29,53,61,87,95,119,127,23,31,55,63}
};

static PORT_HOST uint8_t variants[3][4096];
static PORT_HOST uint16_t offsets[4096];
static PORT_HOST uint8_t shifts[4096];
static PORT_HOST int valid, payloadSize, pixelBits, selected;

static uint32_t word(const void *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t quad(const void *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static unsigned get(const uint8_t *data, int i) {
    return (data[offsets[i]] >> shifts[i]) & ((1u << pixelBits) - 1);
}
static void put(uint8_t *data, int i, unsigned value) {
    unsigned mask = ((1u << pixelBits) - 1) << shifts[i];
    data[offsets[i]] = (data[offsets[i]] & ~mask) | (value << shifts[i]);
}

void Port_StoryBadge(void *entry, int level, int reset) {
    uint8_t *e = entry;
    uint8_t *packet = Port_Ptr(word(e + 56));
    uint8_t *palette = Port_Ptr(word(e + 60));
    int index = level == 1 ? 0 : level == 2 ? 1 : 2;
    if (reset) {
        int32_t map[16384];
        uint8_t mask[4096] = {0};
        uint16_t queue[4096];
        uint8_t blank[4096];
        uint64_t tex = quad(e + 48), reg;
        unsigned psm = (tex >> 20) & 63, tbw = (tex >> 14) & 63;
        unsigned blt = word(e + 24), dbw = blt & 63, uw, uh, x, y;
        int head = 0, tail = 0, minx = 64, miny = 64, maxx = 0, maxy = 0;
        int i, v, empty = -1;
        valid = 0;
        if (!packet || !palette || ((tex >> 26) & 15) != 6 || ((tex >> 30) & 15) != 6 ||
            (psm != 19 && psm != 20) || (blt >> 8) != 0) return;
        pixelBits = psm == 19 ? 8 : 4;
        reg = quad(packet + 48); uw = reg & 4095; uh = (reg >> 32) & 4095;
        if (!uw || !uh || uw * uh > 1024 || word(e + 8) < 96 + uw * uh * 4) return;
        payloadSize = uw * uh * 4;
        memset(map, -1, sizeof(map));
        for (y = 0; y < uh; y++) for (x = 0; x < uw; x++) {
            unsigned a = (((y & ~31u) * dbw + ((x >> 1) & ~31u) +
                block32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + col32[y & 7][x & 7];
            if (a * 4 + 3 >= 16384) return;
            for (i = 0; i < 4; i++) map[a * 4 + i] = (y * uw + x) * 4 + i;
        }
        for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
            unsigned col = (y & 15) >> 2, sx = x ^ ((col & 1) ? 4 : 0), a, address;
            if (psm == 19) {
                a = ((((y >> 1) & ~31u) * (tbw >> 1) + ((x >> 2) & ~31u) +
                    block32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + col8[y & 3][sx & 15] + col * 64;
                address = a; shifts[y * 64 + x] = 0;
            } else {
                a = ((((y >> 2) & ~31u) * (tbw >> 1) + ((x >> 2) & ~31u) +
                    block4[(y >> 4) & 7][(x >> 5) & 3]) << 9) + col4[y & 3][sx & 31] + col * 128;
                address = a >> 1; shifts[y * 64 + x] = (a & 1) * 4;
            }
            if (address >= 16384 || map[address] < 0 || map[address] >= payloadSize) return;
            offsets[y * 64 + x] = map[address];
        }
        memcpy(variants[2], packet + 96, payloadSize);
        /* Flood only the central star, which is disconnected from the ring. */
        queue[tail++] = 25 * 64 + 32; mask[queue[0]] = 1;
        while (head < tail) {
            int at = queue[head++], px = at % 64, py = at / 64;
            int neighbors[4] = {at - 1, at + 1, at - 64, at + 64};
            if (px < minx) minx = px;
            if (px > maxx) maxx = px;
            if (py < miny) miny = py;
            if (py > maxy) maxy = py;
            for (i = 0; i < 4; i++) {
                int n = neighbors[i]; unsigned color;
                if (n < 0 || n >= 4096 || mask[n] || (i < 2 && n / 64 != py)) continue;
                v = get(variants[2], n);
                color = psm == 19 ? (v & 0xE7) | ((v & 8) << 1) | ((v & 16) >> 1) : v;
                if (palette[96 + color * 4 + 3]) { mask[n] = 1; queue[tail++] = n; }
            }
        }
        /* Fail closed if a replacement texture has different artwork. */
        if (minx < 15 || maxx > 48 || miny < 10 || maxy > 50 || tail < 100) return;
        for (i = 0; i < 4096; i++) if (!mask[i] && i % 64 > minx && i % 64 < maxx &&
            i / 64 > miny && i / 64 < maxy) {
            unsigned color;
            v = get(variants[2], i);
            color = psm == 19 ? (v & 0xE7) | ((v & 8) << 1) | ((v & 16) >> 1) : v;
            if (!palette[96 + color * 4 + 3]) { empty = v; break; }
        }
        if (empty < 0) return;
        memcpy(blank, variants[2], payloadSize);
        for (i = 0; i < 4096; i++) if (mask[i]) put(blank, i, empty);
        for (v = 0; v < 2; v++) {
            int w = v ? 22 : 16, h = v ? 28 : 20, ox = (64 - w) / 2, oy = (64 - h) / 2 - 1;
            memcpy(variants[v], blank, payloadSize);
            for (y = 0; y < (unsigned)h; y++) for (x = 0; x < (unsigned)w; x++) {
                int sx = minx + x * (maxx - minx + 1) / w;
                int sy = miny + y * (maxy - miny + 1) / h;
                int from = sy * 64 + sx;
                if (mask[from]) put(variants[v], (oy + y) * 64 + ox + x, get(variants[2], from));
            }
        }
        selected = 2; valid = 1;
    }
    if (valid && packet && index != selected) {
        memcpy(packet + 96, variants[index], payloadSize);
        selected = index;
    }
}
