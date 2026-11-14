#include "../../setup/native.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cstdio>
int main(int argc,char **argv) {
    assert(argc==4);
    for(int i=0;i<2;i++) {
        NativeTitleLogo logo=Native_ReadTitleLogo(argv[1+i],i!=0);
        assert(logo.width==512 && logo.height==256 && logo.pixels.size()==512*256*4);
        SDL_Surface *surface=SDL_CreateSurfaceFrom(logo.width,logo.height,SDL_PIXELFORMAT_RGBA32,logo.pixels.data(),logo.width*4);
        assert(surface);
        std::string name=std::string(argv[3])+(i ? "/jp.png" : "/us.png");
        assert(SDL_SavePNG(surface,name.c_str())); SDL_DestroySurface(surface);
    }
    assert(Native_ReadTitleLogo("missing-image.iso",false).pixels.empty());
    puts("PASS: selected USA/Japanese ISO title previews without installation");
}
