#ifndef TITLE_ICON_H
#define TITLE_ICON_H
#include <SDL3/SDL.h>
#include <vector>
#include <algorithm>
/* Standard ICO with uncompressed 32-bit DIB images and transparency masks. */
static inline std::vector<unsigned char> TitleIcon_Encode(const unsigned char *rgba, int width, int height) {
    std::vector<unsigned char> out;
    if (!rgba || width<=0 || height<=0 || width>512 || height>512) return out;
    int x0=width,y0=height,x1=0,y1=0;
    for (int y=0;y<height;y++) for (int x=0;x<width;x++) if (rgba[(y*width+x)*4+3]) {
        x0=std::min(x0,x); y0=std::min(y0,y); x1=std::max(x1,x+1); y1=std::max(y1,y+1);
    }
    if (x0>=x1 || y0>=y1) return out;
    const int sizes[]={16,32,48,64,128,256};
    auto word=[&](unsigned v,int n) { for(int i=0;i<n;i++) out.push_back((unsigned char)(v>>(i*8))); };
    word(0,2); word(1,2); word(6,2);
    unsigned offset=6+6*16;
    for(int n:sizes) {
        unsigned bytes=40+n*n*4+((n+31)/32)*4*n;
        word(n==256?0:n,1); word(n==256?0:n,1); word(0,2); word(1,2); word(32,2); word(bytes,4); word(offset,4);
        offset+=bytes;
    }
    for(int n:sizes) {
        SDL_Surface *source=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,(void *)rgba,width*4);
        SDL_Surface *target=SDL_CreateSurface(n,n,SDL_PIXELFORMAT_RGBA32);
        if (!source || !target) { SDL_DestroySurface(source); SDL_DestroySurface(target); return {}; }
        SDL_ClearSurface(target,0,0,0,0);
        int w=x1-x0,h=y1-y0;
        SDL_Rect crop={x0,y0,w,h};
        int dw=w>=h?n:std::max(1,n*w/h), dh=h>=w?n:std::max(1,n*h/w);
        SDL_Rect dest={(n-dw)/2,(n-dh)/2,dw,dh};
        SDL_SetSurfaceBlendMode(source,SDL_BLENDMODE_NONE);
        bool ok=SDL_BlitSurfaceScaled(source,&crop,target,&dest,SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(source);
        if (!ok) { SDL_DestroySurface(target); return {}; }
        word(40,4); word(n,4); word(n*2,4); word(1,2); word(32,2); word(0,4); word(n*n*4,4);
        word(0,4); word(0,4); word(0,4); word(0,4);
        for(int y=n-1;y>=0;y--) for(int x=0;x<n;x++) {
            const unsigned char *p=(const unsigned char *)target->pixels+y*target->pitch+x*4;
            out.push_back(p[2]); out.push_back(p[1]); out.push_back(p[0]); out.push_back(p[3]);
        }
        int stride=((n+31)/32)*4;
        for(int y=n-1;y>=0;y--) for(int b=0;b<stride;b++) {
            unsigned char mask=0;
            for(int bit=0;bit<8;bit++) {
                int x=b*8+bit;
                if(x<n && !((unsigned char *)target->pixels)[y*target->pitch+x*4+3]) mask|=128>>bit;
            }
            out.push_back(mask);
        }
        SDL_DestroySurface(target);
    }
    return out;
}
#endif
