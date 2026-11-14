#ifndef SETUP_TITLE_TEXTURE_H
#define SETUP_TITLE_TEXTURE_H
#include "../src/gs/ps2_texture_tables.h"

// Same 8-bit GS upload/read layout as menu_labels.c; bounded to title textures.
static NativeTitleLogo title_pixels(View texture) {
    NativeTitleLogo out;
    size_t entry=(size_t)texture.word(4)*4;
    View e=texture.sub(entry,64);
    Uint64 tex0=(Uint64)e.word(48)|((Uint64)e.word(52)<<32);
    unsigned width=1u<<((tex0>>26)&15),height=1u<<((tex0>>30)&15);
    require(width<=512 && height<=512 && ((tex0>>20)&63)==19,"Unsupported title texture.");
    Uint32 blt=e.word(24),dbw=blt&63,tbw=(tex0>>14)&63;
    size_t pixels=(size_t)e.word(0)*4+96,palette=(size_t)e.word(4)*4+96;
    require(blt>>8==0 && pixels>=48,"Invalid title upload.");
    unsigned uw=texture.word(pixels-48)&4095,uh=texture.word(pixels-44)&4095;
    require(uw<=512 && uh<=512,"Invalid title upload dimensions.");
    std::vector<int> gs(512*512*4,-1);
    for(unsigned y=0;y<uh;y++) for(unsigned x=0;x<uw;x++) {
        unsigned a=(((y&~31u)*dbw+((x>>1)&~31u)+kBlock32[(y>>3)&3][(x>>3)&7])<<6)+kCol32[y&7][x&7];
        require(a<gs.size()/4,"Invalid title upload address.");
        for(unsigned c=0;c<4;c++) gs[a*4+c]=(y*uw+x)*4+c;
    }
    out.width=width; out.height=height; out.pixels.resize(width*height*4);
    for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) {
        unsigned col=(y&15)>>2,sx=x^((col&1)?4u:0u);
        unsigned a=((((y>>1)&~31u)*(tbw>>1)+((x>>2)&~31u)+kBlock32[(y>>4)&3][(x>>4)&7])<<8)+kCol8First[y&3][sx&15]+col*64;
        require(a<gs.size() && gs[a]>=0,"Invalid title pixel address.");
        unsigned index=texture.sub(pixels+(size_t)gs[a],1).p[0];
        size_t at=palette+((index&0xE7)|((index&8)<<1)|((index&16)>>1))*4;
        View color=texture.sub(at,4);
        unsigned char *p=&out.pixels[(y*width+x)*4];
        memcpy(p,color.p,4); p[3]=p[3]>=128 ? 255 : p[3]*2;
    }
    return out;
}
#endif
