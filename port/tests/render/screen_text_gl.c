/* Actual OpenGL composition helpers/shaders; screen_text_build.py --gpu. */
#include "../../src/gs/gs_gl.c"
#include <assert.h>
int gsScale=1;
unsigned gsVertCount;
Vtx *gsVerts;
GsTarget gsTargets[MAX_TARGETS];
const char *Port_GetEnv(const char *name) {(void)name;return NULL;}

static void quad(Vtx *out,float x0,float y0,float x1,float y1,float z,int r,int g,int b) {
    const float xy[6][2]={{x0,y0},{x1,y0},{x0,y1},{x0,y1},{x1,y0},{x1,y1}};
    for(int i=0;i<6;i++) {
        out[i]=(Vtx){0};out[i].x=xy[i][0];out[i].y=xy[i][1];out[i].z=z;
        out[i].r=r;out[i].g=g;out[i].b=b;out[i].a=128;out[i].q=1;
    }
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    sWindow=SDL_CreateWindow("screen text test",1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    assert(sWindow);sCtx=SDL_GL_CreateContext(sWindow);assert(sCtx && load_gl());
    sHasIndexed=glEnablei && glColorMaski;
    sMainProg[0]=make_program(kGsVertGlsl,kGsFragGlsl,"screen text",sGsSamplers,2);assert(sMainProg[0]);
    target_ensure(0);bind_target(0,1);
    float color[4]={.25f,.25f,.25f,1},aux[4]={.3f,0,0,1},depth=.8f;
    glClearBufferfv(GL_COLOR,0,color);glClearBufferfv(GL_COLOR,1,aux);glClearBufferfv(GL_DEPTH,0,&depth);
    assert(screen_target(1280,720));screen_capture(0);sScreenReplay=1;bind_target(0,0);
    Vtx original[18];quad(original,0,0,512,448,.7f,255,0,0);
    quad(original+6,100.2f,100,100.8f,200,.9f,255,255,255);
    quad(original+12,90,125,110,145,.95f,0,255,0);
    gsVerts=original;gsVertCount=18;Vtx *native=GsScreenText_Vertices();assert(native);
    glGenBuffers(1,&sVboGs);glBindBuffer(GL_ARRAY_BUFFER,sVboGs);
    glBufferData(GL_ARRAY_BUFFER,sizeof(original),native,GL_STREAM_DRAW);free(native);
    glGenVertexArrays(1,&sVaoGs);glBindVertexArray(sVaoGs);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void *)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vtx),(void *)12);
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void *)16);
    glGenBuffers(1,&sUboParams);glBindBuffer(GL_UNIFORM_BUFFER,sUboParams);
    glBufferData(GL_UNIFORM_BUFFER,96,NULL,GL_STREAM_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,1,sUboParams);
    struct {int mode[4];float misc[4],rect[4],orig[4],extra[4];} params={0};params.extra[0]=16777215;
    set_pipe(pipe_get((2u<<10)|(1u<<12)|(15u<<16)));set_params(&params,sizeof(params));
    int logical[4]={0,100,512,50},clip[4];GsScreenText_Scissor(logical,clip,1280,720);
    set_scissor(clip);glDrawArrays(GL_TRIANGLES,0,18);
    unsigned char *pixels=malloc(1280*720*4);assert(pixels);
    glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,1280,720,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    #define PIXEL(x,y,c) pixels[((y)*1280+(x))*4+(c)]
    assert(PIXEL(251,177,0)>250 && PIXEL(251,177,1)>250);
    assert(PIXEL(252,177,0)>60 && PIXEL(252,177,0)<70);
    assert(PIXEL(251,217,0)<5 && PIXEL(251,217,1)>250);
    assert(PIXEL(500,177,0)>60 && PIXEL(500,177,0)<70);
    assert(PIXEL(251,300,0)>60 && PIXEL(251,300,0)<70);
    assert(glGetError()==GL_NO_ERROR);free(pixels);
    SDL_GL_DestroyContext(sCtx);SDL_DestroyWindow(sWindow);SDL_Quit();
    puts("OpenGL screen text: native detail, scene depth, ordering and clipping passed");
}
