#ifndef SETUP_ARTWORK_H
#define SETUP_ARTWORK_H
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#include "../third_party/stb/stb_image.h"

// Optional local JPEG/PNG artwork in the selected game folder.
static inline SDL_Surface *Setup_LoadArtwork(const std::string &root, const char *name,
                                            int maxWidth=4096, int maxHeight=4096) {
    if(root.empty()) return nullptr;
    std::string path=root + (root.back()=='/' || root.back()=='\\' ? "" : "/") + "setup/" + name;
    SDL_IOStream *input=SDL_IOFromFile(path.c_str(),"rb");
    if(!input) return nullptr;
    Sint64 bytes=SDL_GetIOSize(input);
    if(bytes<1 || bytes>16*1024*1024) { SDL_CloseIO(input); return nullptr; }
    std::vector<unsigned char> encoded((size_t)bytes);
    bool complete=SDL_ReadIO(input,encoded.data(),encoded.size())==encoded.size();
    SDL_CloseIO(input);
    if(!complete) return nullptr;
    int width,height,channels;
    if(!stbi_info_from_memory(encoded.data(),(int)encoded.size(),&width,&height,&channels) ||
       width<1 || height<1 || width>4096 || height>4096) return nullptr;
    unsigned char *pixels=stbi_load_from_memory(encoded.data(),(int)encoded.size(),&width,&height,&channels,4);
    if(!pixels) return nullptr;
    SDL_Surface *decoded=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,pixels,width*4);
    SDL_Surface *surface=decoded ? SDL_ConvertSurface(decoded,SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(decoded);
    stbi_image_free(pixels);
    if(surface && (surface->w>maxWidth || surface->h>maxHeight)) {
        float scale=std::min((float)maxWidth/surface->w,(float)maxHeight/surface->h);
        SDL_Surface *small=SDL_ScaleSurface(surface,std::max(1,(int)(surface->w*scale)),
                                           std::max(1,(int)(surface->h*scale)),SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(surface); surface=small;
    }
    return surface;
}
#endif
