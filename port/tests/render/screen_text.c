/* cc -std=c99 port/tests/render/screen_text.c -o /tmp/screen-text && /tmp/screen-text */
#include "../../src/gs/gs_screen_text.h"
#include <assert.h>
#include <stdio.h>
int gsScale = 1;
unsigned gsDrawCount, gsVertCount;
static GsDraw draws[4];
GsDraw *gsDraws=draws;
Vtx *gsVerts;
static uint32_t key(int index) { return index; }
int main(void) {
    gsDrawCount=4;
    gsDraws[0].vu=1;
    gsDraws[1].screenText=1;
    /* A translucent plate and fade after text must be replayed after it. */
    assert(GsScreenText_Start(0,key)==1);
    gsDraws[2].tex_is_target=1;
    assert(GsScreenText_Start(0,key)==-1);
    gsDraws[3].screenText=1;
    assert(GsScreenText_Start(0,key)==3);
    gsDraws[3].misc[1]=1;
    assert(GsScreenText_Start(0,key)==3);
    gsDraws[3].misc[1]=0;gsDraws[3].pipeline=1<<24;
    assert(GsScreenText_Start(0,key)==-1);
    gsDraws[3].pipeline=0;gsDraws[3].native=4;
    assert(GsScreenText_Start(0,key)==-1);
    gsDraws[3].native=0;gsDraws[3].target=1;
    assert(GsScreenText_Start(0,key)==-1);
    gsDraws[2].tex_is_target=0;gsDraws[3].target=0;
    gsDraws[2].native=5;gsDraws[3].misc[1]=1;
    assert(GsScreenText_Start(0,key)==1);
    int w,h,sc[4],full[4]={0,0,512,448};
    GsScreenText_Size(1920,1080,1778,&w,&h);
    assert(w==1920 && h==1080);
    GsScreenText_Size(1080,2400,1778,&w,&h);
    assert(w==1080 && h==607);
    GsScreenText_Size(2560,1080,1333,&w,&h);
    assert(w==1440 && h==1080);
    GsScreenText_Scissor(full,sc,1920,1080);
    assert(sc[0]==0 && sc[1]==0 && sc[2]==1920 && sc[3]==1080);
    int partial[4]={128,112,256,224};
    GsScreenText_Scissor(partial,sc,1920,1080);
    assert(sc[0]==480 && sc[1]==270 && sc[2]==960 && sc[3]==540);
    gsScale=2;for(int i=0;i<4;i++) partial[i]*=2;
    GsScreenText_Scissor(partial,sc,1920,1080);
    assert(sc[0]==480 && sc[1]==270 && sc[2]==960 && sc[3]==540);
    Vtx original[2]={{0}};original[1].x=512;original[1].y=448;
    original[1].z=.75f;original[1].s=.4f;original[1].q=1;original[1].r=123;
    gsVerts=original;gsVertCount=2;
    Vtx *native=GsScreenText_Vertices();assert(native);
    assert(native[1].x==1024 && native[1].y>1023.99f && native[1].y<1024.01f);
    assert(native[1].z==original[1].z && native[1].s==original[1].s && native[1].r==123);
    assert(original[1].x==512 && original[1].y==448);free(native);
    puts("Screen text: ordering, dependency fallback, letterboxing, scissor, and isolated vertices passed");
}
