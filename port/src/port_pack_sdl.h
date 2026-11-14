#ifndef PORT_PACK_SDL_H
#define PORT_PACK_SDL_H
#include "port_pack.h"
#include <SDL3/SDL.h>
/* SDL's pathname image loader cannot see archive entries. Decode a bounded
 * entry in memory, preserving normal loose-file priority. */
static inline SDL_Surface *Port_PackLoadPNG(const char *path) {
    SDL_Surface *surface=SDL_LoadPNG(path);
    FILE *fp; int64_t base,size; void *bytes; SDL_IOStream *io;
    if (surface) return surface;
    fp=Port_PackOpen(path,&base,&size);
    if (!fp) return NULL;
    if (size<=0 || size>256*1024*1024) { fclose(fp); return NULL; }
    bytes=SDL_malloc((size_t)size);
    if (!bytes) { fclose(fp); return NULL; }
    if (fread(bytes,1,(size_t)size,fp)!=(size_t)size) { fclose(fp); SDL_free(bytes); return NULL; }
    fclose(fp);
    io=SDL_IOFromConstMem(bytes,(size_t)size);
    surface=io?SDL_LoadPNG_IO(io,true):NULL;
    SDL_free(bytes);
    return surface;
}
#endif
