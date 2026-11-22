/* cc -std=gnu99 -ffunction-sections -fdata-sections port/tests/menu_font/text_antialias.c
 * -Wl,--gc-sections -lm -pthread -o /tmp/text-antialias && /tmp/text-antialias */
#include "../../src/gs/menu_text.c"
#include <assert.h>
static int antialias = -1;
int Port_Setting(const char *name, int fallback) {
    if (!strcmp(name, "text_antialias")) {
        assert(fallback == 1);
        return antialias < 0 ? fallback : antialias;
    }
    return fallback;
}
int main(void) {
    Face font;
    int w,h,x,y,partial=0;
    assert(!face_open(&font,"port/third_party/fonts/anton/Anton-Regular.ttf"));
    int glyph=face_glyph(&font,'A');
    float scale=23.0f/font.capUnits;
    unsigned char *pixels=face_bitmap(&font,glyph,scale,scale,0,0,&w,&h,&x,&y);
    assert(pixels && MenuText_Antialias());
    for(int i=0;i<w*h;i++) partial+=pixels[i]>0 && pixels[i]<255;
    assert(partial); free(pixels);
    uint32_t key=MenuText_RasterKey();
    antialias=0;
    assert(MenuText_RasterKey()!=key);
    pixels=face_bitmap(&font,glyph,scale,scale,0,0,&w,&h,&x,&y);
    assert(pixels);
    for(int i=0;i<w*h;i++) assert(pixels[i]==0 || pixels[i]==255);
    free(pixels); face_close(&font);
    puts("Text antialiasing: default smooth edges, disabled coverage and cache key passed");
}
