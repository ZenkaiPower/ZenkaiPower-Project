/* Offscreen comparison using the game's OpenGL backend and actual GS shader.
   Built by compare_native_hud.py; no game process or Android device required. */
#include "../src/gs/gs_gl.c"
#include <assert.h>
#include "../src/gs/gs_native_elements.h"
int gsScale=1;
unsigned gsVertCount;
Vtx *gsVerts;
GsTarget gsTargets[MAX_TARGETS];
const char *Port_GetEnv(const char *name) { (void)name; return NULL; }

static void save_pixels(const char *folder, int kind, const char *label, int w, int h) {
    char path[4096];
    assert(snprintf(path,sizeof(path),"%s/%02d-%s.rgba",folder,kind,label)<(int)sizeof(path));
    size_t bytes=(size_t)w*h*4;
    unsigned char *pixels=malloc(bytes);assert(pixels);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    /* The backend's translated GS vertex shader makes row zero the logical
       top of the picture in this framebuffer readback (same as its tests). */
    FILE *fp=fopen(path,"wb");assert(fp);
    assert(fwrite(pixels,1,bytes,fp)==bytes);assert(!fclose(fp));free(pixels);
}
int main(int argc,char **argv) {
    if(argc!=5) { fprintf(stderr,"usage: capture input.rgba output-folder scale linear|nearest\n");return 2; }
    int scale=atoi(argv[3]);assert(scale>=1 && scale<=8);
    int linear=!strcmp(argv[4],"linear");
    FILE *input=fopen(argv[1],"rb");assert(input);
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    sWindow=SDL_CreateWindow("Native HUD comparison",64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(sWindow);
    sCtx=SDL_GL_CreateContext(sWindow);assert(sCtx && load_gl());
    typedef const unsigned char *(*CaptureGetString)(GLenum);
    CaptureGetString getString=(CaptureGetString)SDL_GL_GetProcAddress("glGetString");
    if(getString) printf("OpenGL renderer: %s\nOpenGL version: %s\n",getString(0x1F01),getString(0x1F02));
    sHasIndexed=glEnablei && glColorMaski;
    sMainProg[0]=make_program(kGsVertGlsl,kGsFragGlsl,"native HUD comparison",sGsSamplers,2);assert(sMainProg[0]);
    Vtx vertices[6];
    const float xy[6][2]={{0,0},{1,0},{0,1},{0,1},{1,0},{1,1}};
    for(int i=0;i<6;i++) {
        vertices[i]=(Vtx){0};vertices[i].x=xy[i][0]*1024;vertices[i].y=xy[i][1]*1024;
        vertices[i].z=.9f;vertices[i].r=vertices[i].g=vertices[i].b=vertices[i].a=128;
        vertices[i].s=xy[i][0];vertices[i].t=xy[i][1];vertices[i].q=1;
    }
    glGenBuffers(1,&sVboGs);glBindBuffer(GL_ARRAY_BUFFER,sVboGs);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);
    glGenVertexArrays(1,&sVaoGs);glBindVertexArray(sVaoGs);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void *)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_UNSIGNED_BYTE,GL_TRUE,sizeof(Vtx),(void *)12);
    glEnableVertexAttribArray(2);glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void *)16);
    glGenBuffers(1,&sUboParams);glBindBuffer(GL_UNIFORM_BUFFER,sUboParams);
    glBufferData(GL_UNIFORM_BUFFER,96,NULL,GL_STREAM_DRAW);glBindBufferBase(GL_UNIFORM_BUFFER,1,sUboParams);
    for(int kind=1;kind<=7;kind++) {
        uint32_t header[3];assert(fread(header,sizeof(header),1,input)==1);
        int w=header[1],h=header[2];
        assert(header[0]==(unsigned)kind && w==(kind==1 ? 64 : 256) && h==64);
        size_t bytes=(size_t)w*h*4;
        unsigned char *rgba=malloc(bytes);assert(rgba && fread(rgba,1,bytes,input)==bytes);
        uint32_t *palette=malloc(48*sizeof(uint32_t));assert(palette);
        int paletteSize=ZenkaiNative_Palette((const uint32_t *)rgba,kind,palette);
        GsTex nativePalette=tex_create(paletteSize,1,GS_TEXFMT_RGBA8,1);
        tex_upload(nativePalette,0,paletteSize,1,GS_TEXFMT_RGBA8,palette,paletteSize*4);
        GsTex original=tex_create(w,h,GS_TEXFMT_RGBA8,1);
        tex_upload(original,0,w,h,GS_TEXFMT_RGBA8,rgba,bytes); /* owns rgba */
        glBindTexture(GL_TEXTURE_2D,(GLuint)original);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        assert(screen_target(w*scale,h*scale));sScreenReplay=1;bind_target(0,1);
        set_pipe(pipe_get((1u<<10)|(15u<<16))); /* unblended RGBA, depth always */
        int full[4]={0,0,w*scale,h*scale};set_scissor(full);
        /* Bind the source image explicitly; the procedural draw must not sample it. */
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,(GLuint)original);
        struct { int mode[4];float misc[4],rect[4],orig[4],extra[4]; } params={0};
        params.mode[1]=1;params.mode[2]=1; /* decal + texture alpha: neutral tint */
        params.orig[0]=w;params.orig[1]=h;params.orig[2]=kind;params.extra[0]=16777215;
        for(int native=0;native<=1;native++) {
            params.mode[0]=native ? 4 : 1;
            set_pipe(pipe_get((1u<<10)|(15u<<16)|(native ? GS_PIPE_NATIVE_HUD : 0)));
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,(GLuint)(native ? nativePalette : original));
            set_params(&params,sizeof(params));glDrawArrays(GL_TRIANGLES,0,6);
            save_pixels(argv[2],kind,native ? "native" : "original",w*scale,h*scale);
        }
        tex_destroy(nativePalette);tex_destroy(original);assert(glGetError()==GL_NO_ERROR);
    }
    assert(fgetc(input)==EOF);fclose(input);
    SDL_GL_DestroyContext(sCtx);SDL_DestroyWindow(sWindow);SDL_Quit();
    return 0;
}
