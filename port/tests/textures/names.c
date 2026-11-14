/* gcc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined
 * port/tests/textures/names.c -Wl,--gc-sections -lSDL3 -o /tmp/texture_names_test
 * /tmp/texture_names_test */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "../../src/gs/gs_texpack.c"
const char *Port_GetEnv(const char *name) { return NULL; }
static uint8_t block[256];
const uint8_t *Gs_BlockPtr(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e) { return block; }
uint32_t Gs_VramRead(uint32_t a,uint32_t b,uint32_t c,uint32_t x,uint32_t y) { return 0x80000000u | (x + y*16)*0x010101u; }
int main(void) {
    char name[64], expected[64];
    uint32_t alpha, clut[256], bits;
    uint64_t tex;
    for (int i=0;i<256;i++) block[i]=(uint8_t)(i%7+2);
    tex=XXH3_64bits(block,256);
    bits=0x13 | 4<<6 | 4<<10 | 1<<14;
    for(int i=0;i<256;i++) {
        uint32_t k=(i&0xE7)|((i&8)<<1)|((i&0x10)>>1);
        clut[i]=Gs_VramRead(0,1,0,k&15,k>>4);
    }
    snprintf(expected,64,"%llx-%llx-%08x",(unsigned long long)tex,(unsigned long long)XXH3_64bits(&clut[2],7*4),bits);
    TexPack_Name(name,0,1,0x13,16,16,1,0,0);
    assert(!strcmp(name,expected));
    assert(TexPack_Lookup(0,1,0x13,16,16,1,0,0,&alpha)==-1);
    /* A pack using the full palette must display its actual filename. */
    Entry entry={0}; uint32_t buckets[1024]={0}; char path[128];
    entry.tex=tex;entry.clut=XXH3_64bits(clut,sizeof(clut));entry.bits=bits;entry.pal=1;
    snprintf(expected,64,"%llx-%llx-%08x",(unsigned long long)tex,(unsigned long long)entry.clut,bits);
    snprintf(path,sizeof(path),"textures/test/%s.png",expected);entry.path=path;
    sEntries=&entry;sCount=1;sBucket=buckets;sBuckets=1024;
    buckets[bucket_of(entry.tex,entry.clut,bits)&1023]=1;
    TexPack_Name(name,0,1,0x13,16,16,1,0,0);
    assert(!strcmp(name,expected));
    assert(TexPack_Lookup(0,1,0x13,16,16,1,0,0,&alpha)==0);
    sEntries=NULL;sCount=sBuckets=0;sBucket=NULL;
    snprintf(expected,64,"%llx-%08x",(unsigned long long)tex,3<<6|3<<10);
    TexPack_Name(name,0,1,0,8,8,0,0,0);assert(!strcmp(name,expected));
    TexPack_Name(name,0,1,0x14,32,16,1,0,0);assert(name[0]);
    TexPack_Name(name,0,1,0x13,8,8,1,0,0);assert(!name[0]);
    TexPack_Name(name,0,1,2,16,16,1,0,0);assert(!name[0]);
    /* Wii pixels use a distinct namespace and still pass the pack filename parser. */
    {
        const uint8_t pixels[] = {255, 10, 20, 128, 30, 40, 50, 128};
        char other[64], file[72];
        assert(TexPack_WiiLookup(name, pixels, 2, 1) == -1);
        snprintf(expected, sizeof(expected), "%llx-%08x",
                 (unsigned long long)XXH3_64bits(pixels, sizeof(pixels)), 0x80000001u);
        assert(!strcmp(name, expected));
        TexPack_WiiLookup(other, pixels, 1, 2);
        assert(strcmp(name, other)); /* Same bytes, different dimensions. */
        snprintf(file, sizeof(file), "%s.png", name);
        add("textures/", file);
        assert(sCount == 1 && sEntries[0].bits == 0x80000001u && !sEntries[0].pal);
        sBucket = buckets;
        memset(buckets, 0, sizeof(buckets));
        sBuckets = 1024;
        buckets[bucket_of(sEntries[0].tex, 0, sEntries[0].bits) & 1023] = 1;
        assert(TexPack_WiiLookup(other, pixels, 2, 1) == 0);
        assert(!strcmp(name, other));
        free(sEntries[0].path);
        free(sEntries);
        sEntries = NULL; sCount = sCap = sBuckets = 0; sBucket = NULL;
        assert(TexPack_WiiLookup(name, pixels, 0, 1) == -1 && !name[0]);
    }
    /* PNGs keep the original bytes, including GS alpha; existing files survive. */
    {
        char dir[] = "/tmp/zenkai-export-XXXXXX";
        char *cwd = getcwd(NULL, 0);
        const uint8_t pixels[] = {255, 10, 20, 128, 30, 40, 50, 249};
        const uint8_t other[] = {0, 0, 0, 0, 0, 0, 0, 0};
        SDL_Surface *loaded, *rgba;
        assert(cwd && mkdtemp(dir) && chdir(dir) == 0);
        assert(TexPack_Export("test", pixels, 2, 1) == 1);
        assert(TexPack_Export("test", other, 2, 1) == 0);
        loaded = SDL_LoadPNG("export/test.png");
        assert(loaded && loaded->w == 2 && loaded->h == 1);
        rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
        assert(rgba && memcmp(rgba->pixels, pixels, sizeof(pixels)) == 0);
        SDL_DestroySurface(rgba);
        SDL_DestroySurface(loaded);
        assert(TexPack_Export("test", NULL, 2, 1) == -1);
        assert(unlink("export/test.png") == 0 && rmdir("export") == 0);
        assert(chdir(cwd) == 0 && rmdir(dir) == 0);
        free(cwd);
    }
    puts("Texture filename and PNG export checks passed (original pixels, alpha, no overwrite).");
}
