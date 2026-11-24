#include "native_hud_palette_fixture.h"
/* Shared pixel checks for the real OpenGL and Vulkan fragment shader. */
static const unsigned char *hud_pixel(const unsigned char *pixels,int x,int y,int width) {
    return pixels+((y*720/64)*1280+x*1280/width)*4;
}
static void native_hud_check(const unsigned char *pixels,int kind) {
    const unsigned char *p;
    int width=kind==1 ? 64 : 256;
    assert(hud_pixel(pixels,width-2,40,width)[3]==0); /* unused atlas space */
    if(kind==1) {
        p=hud_pixel(pixels,32,10,width);
        assert(p[3]>140 && p[3]<190); /* translucent timer panel */
        assert(hud_pixel(pixels,32,40,width)[3]>140); /* lower panel */
        assert(hud_pixel(pixels,2,28,width)[3]==0);
        /* Bases taper away at the corners, with a darker horizontal lip. */
        assert(hud_pixel(pixels,5,21,width)[3]<20);
        assert(hud_pixel(pixels,59,21,width)[3]<20);
        assert(hud_pixel(pixels,6,60,width)[3]<20);
        assert(hud_pixel(pixels,59,60,width)[3]<20);
        assert(hud_pixel(pixels,32,21,width)[3]>190);
        assert(hud_pixel(pixels,32,60,width)[3]>190);
        assert(hud_pixel(pixels,32,23,width)[3]==0);
        assert(hud_pixel(pixels,32,62,width)[3]==0);
    } else {
        p=hud_pixel(pixels,100,6,width);
        if(kind==2) assert(p[0]==0 && p[1]==0 && p[2]==0 && p[3]>80 && p[3]<110);
        else {
            assert(p[3]>240);
            if(kind==3) assert(p[1]>p[0]*4 && p[1]>p[2]*4);
            if(kind==4) assert(p[0]>p[1]*4 && p[0]>p[2]*4);
            if(kind==5) assert(p[0]>p[1] && p[1]>p[2]*4);
            if(kind==6) assert(p[0]>p[2]*4 && p[1]>p[2]*4);
            if(kind==7) assert(p[2]>p[1] && p[1]>p[0]*4);
        }
        /* Reference palette colors at the long, bright upper band. */
        static const unsigned char top[6][3]={
            {0,0,0},{153,254,170},{217,22,22},
            {254,220,153},{237,254,153},{153,186,254}
        };
        p=hud_pixel(pixels,100,1,width);
        for(int channel=0;channel<3;channel++) {
            int error=(int)p[channel]-top[kind-2][channel];
            assert(error>=-1 && error<=1);
        }
        /* Both bright upper bands turn up before their slanted right tips. */
        if(kind>=3) {
            const unsigned char *middle=hud_pixel(pixels,180,1,width);
            const unsigned char *tip=hud_pixel(pixels,185,1,width);
            assert(middle[3]>240 && tip[3]>240);
            assert(middle[0]+middle[1]+middle[2]>tip[0]+tip[1]+tip[2]+40);
            p=hud_pixel(pixels,158,1,width);
            if(kind==3 || kind==7) assert(p[0]<70);
            if(kind==5 || kind==6) assert(p[2]<70);
        }
        /* The bright bevel also follows the rising edge of the step. */
        if(kind==3 || kind>=5) {
            p=hud_pixel(pixels,63,4,width);
            assert(p[0]>135 && p[1]>135 && p[2]>135 && p[3]>230);
        }
        /* Check sub-atlas-pixel neighbors across the old x=63 shading cut. */
        if(kind>=3) {
            int y=9*720/64, x=63*1280/width;
            const unsigned char *left=pixels+(y*1280+x-1)*4;
            const unsigned char *right=left+4;
            assert(left[3]>240 && right[3]>240);
            for(int channel=0;channel<3;channel++) {
                int change=(int)left[channel]-(int)right[channel];
                assert(change>=-4 && change<=4);
            }
        }
        p=hud_pixel(pixels,100,54,width); /* backing */
        assert(p[0]==0 && p[1]==0 && p[2]==0 && p[3]>240);
        assert(hud_pixel(pixels,35,20,width)[3]>240); /* Ki backing */
        assert(hud_pixel(pixels,22,25,width)[3]>80); /* slanted Ki fill */
        assert(hud_pixel(pixels,175,3,width)[3]>80); /* remaining-bar marker */
        assert(hud_pixel(pixels,200,3,width)[3]>240);
        assert(hud_pixel(pixels,3,17,width)[3]>80); /* stock chevron */
        /* The chevron's upper and lower faces join without an internal rim. */
        for(int x=3;x<=6;x++) {
            p=hud_pixel(pixels,x,20,width);
            if(kind>=3) assert(p[3]>240);
            else assert(p[3]>80 && p[3]<110);
        }
        if(kind==7) {
            p=hud_pixel(pixels,4,20,width);
            assert(p[0]<40 && p[1]<130 && p[2]>150); /* blue shading, no gray stripe */
        }
        p=hud_pixel(pixels,100,48,width); /* filtered upper edge of backing */
        if(kind>=3) {
            assert(p[0]>40 && p[0]<200 && p[0]==p[1] && p[1]==p[2]);
            assert(p[3]>80 && p[3]<220); /* gray rim with partial coverage */
            p=hud_pixel(pixels,32,20,width); /* vertical Ki backing border */
            assert(p[0]>40 && p[0]==p[1] && p[1]==p[2]);
        } else assert(p[0]==0 && p[1]==0 && p[2]==0); /* shadow has no white halo */
        p=hud_pixel(pixels,100,60,width);
        assert(p[0]==0 && p[1]==0 && p[2]==0); /* bottom padding stays black */
    }
}

/* Smooth silhouettes have fractional coverage; sharp mode has only full atlas
   alpha or zero, and preserves constant samples within each source texel. */
static void native_hud_filter_check(const unsigned char *pixels,int kind,int sharp) {
    if(kind<3) return;
    unsigned partial=0;
    for(int y=0;y<140;y++) for(int x=0;x<1280;x++) {
        int a=pixels[(y*1280+x)*4+3];
        if(a>1 && a<251) partial++;
    }
    assert(sharp ? partial==0 : partial>100);
    if(sharp) {
        const unsigned char *a=pixels+(70*1280+501)*4;
        const unsigned char *b=pixels+(70*1280+503)*4;
        for(int c=0;c<4;c++) assert(a[c]==b[c]);
    }
}
