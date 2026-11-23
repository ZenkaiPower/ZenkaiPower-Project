/* Actual SDL GPU shaders/pipelines; run through screen_text_build.py --gpu. */
#include "../../src/gs/gs_gpu.c"
#include <assert.h>
int gsScale=1;
uint64_t gGpuPipeNs;
unsigned gGpuNewPipes;
const char *Port_GetEnv(const char *name) { (void)name; return NULL; }
unsigned gsVertCount;
Vtx *gsVerts;

static void quad(Vtx *out,float x0,float y0,float x1,float y1,float z,int r,int g,int b) {
    const float xy[6][2]={{x0,y0},{x1,y0},{x0,y1},{x0,y1},{x1,y0},{x1,y1}};
    for(int i=0;i<6;i++) {
        out[i]=(Vtx){0};out[i].x=xy[i][0];out[i].y=xy[i][1];out[i].z=z;
        out[i].r=r;out[i].g=g;out[i].b=b;out[i].a=128;out[i].q=1;
    }
}
int main(void) {
    assert(SDL_Init(SDL_INIT_VIDEO));
    sDev=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV,true,"vulkan");
    if(!sDev) {fprintf(stderr,"GPU: %s\n",SDL_GetError());return 1;}
    sVs=shader(SHADER_CODE(kGsVert),SDL_GPU_SHADERSTAGE_VERTEX,0,0);
    sFs=shader(SHADER_CODE(kGsFrag),SDL_GPU_SHADERSTAGE_FRAGMENT,2,1);
    sFxVs=shader(SHADER_CODE(kFxVert),SDL_GPU_SHADERSTAGE_VERTEX,0,0);
    assert(sVs && sFs && sFxVs && screen_pipeline_create());
    for(int i=6;i<=7;i++) {
        SDL_GPUSamplerCreateInfo ci={0};
        ci.min_filter=ci.mag_filter=i==7 ? SDL_GPU_FILTER_LINEAR:SDL_GPU_FILTER_NEAREST;
        ci.address_mode_u=ci.address_mode_v=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sSamplers[i]=SDL_CreateGPUSampler(sDev,&ci);assert(sSamplers[i]);
    }
    target_ensure(0);assert(screen_target(1280,720));
    SDL_GPUCommandBuffer *cmd=SDL_AcquireGPUCommandBuffer(sDev);
    SDL_GPUColorTargetInfo ct[2]={{0}};
    SDL_GPUDepthStencilTargetInfo dt={0};
    ct[0].texture=sTgCol[0];ct[1].texture=sTgAux[0];
    ct[0].clear_color=(SDL_FColor){.25f,.25f,.25f,1};
    ct[1].clear_color=(SDL_FColor){.3f,0,0,1};
    for(int i=0;i<2;i++) {ct[i].load_op=SDL_GPU_LOADOP_CLEAR;ct[i].store_op=SDL_GPU_STOREOP_STORE;}
    dt.texture=sTgDep[0];dt.clear_depth=.8f;dt.load_op=SDL_GPU_LOADOP_CLEAR;dt.store_op=SDL_GPU_STOREOP_STORE;
    dt.stencil_load_op=SDL_GPU_LOADOP_DONT_CARE;dt.stencil_store_op=SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(cmd,ct,2,&dt);SDL_EndGPURenderPass(pass);
    screen_capture(cmd,0);
    Vtx original[18];
    quad(original,0,0,512,448,.7f,255,0,0); /* behind scene depth: must disappear */
    quad(original+6,100.2f,100,100.8f,200,.9f,255,255,255); /* sub-GS-pixel letter stroke */
    quad(original+12,90,125,110,145,.95f,0,255,0); /* later panel: must cover the stroke */
    gsVerts=original;gsVertCount=18;
    Vtx *native=GsScreenText_Vertices();assert(native);
    SDL_GPUTransferBufferCreateInfo ti={0};ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;ti.size=sizeof(original);
    SDL_GPUTransferBuffer *upload=SDL_CreateGPUTransferBuffer(sDev,&ti);
    void *memory=SDL_MapGPUTransferBuffer(sDev,upload,false);memcpy(memory,native,sizeof(original));
    SDL_UnmapGPUTransferBuffer(sDev,upload);free(native);
    SDL_GPUCopyPass *copy=SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation location={upload,0};
    SDL_GPUBufferRegion region={sScreenVbuf,0,sizeof(original)};
    SDL_UploadToGPUBuffer(copy,&location,&region,false);SDL_EndGPUCopyPass(copy);
    ct[0].texture=sScreenCol;ct[1].texture=sScreenAux;dt.texture=sScreenDep;
    for(int i=0;i<2;i++) ct[i].load_op=SDL_GPU_LOADOP_LOAD;
    dt.load_op=SDL_GPU_LOADOP_LOAD;
    pass=SDL_BeginGPURenderPass(cmd,ct,2,&dt);
    int pipeline=pipe_get((2u<<10)|(1u<<12)|(15u<<16));
    SDL_BindGPUGraphicsPipeline(pass,sPipes[pipeline].p);
    SDL_GPUBufferBinding binding={sScreenVbuf,0};SDL_BindGPUVertexBuffers(pass,0,&binding,1);
    SDL_GPUTextureSamplerBinding samplers[2]={{sTgCol[0],sSamplers[6]},{sTgAux[0],sSamplers[6]}};
    SDL_BindGPUFragmentSamplers(pass,0,samplers,2);
    struct {int mode[4];float misc[4],rect[4],orig[4],extra[4];} params={0};
    params.extra[0]=16777215;
    SDL_PushGPUFragmentUniformData(cmd,0,&params,sizeof(params));
    int logical[4]={0,100,512,50},clipped[4];
    GsScreenText_Scissor(logical,clipped,1280,720);
    SDL_Rect sc={clipped[0],clipped[1],clipped[2],clipped[3]};SDL_SetGPUScissor(pass,&sc);
    SDL_DrawGPUPrimitives(pass,18,1,0,0);SDL_EndGPURenderPass(pass);
    ti.usage=SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;ti.size=1280*720*4;
    SDL_GPUTransferBuffer *download=SDL_CreateGPUTransferBuffer(sDev,&ti);
    copy=SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion source={0};source.texture=sScreenCol;source.w=1280;source.h=720;source.d=1;
    SDL_GPUTextureTransferInfo destination={0};destination.transfer_buffer=download;
    SDL_DownloadFromGPUTexture(copy,&source,&destination);SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence=SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    assert(fence && SDL_WaitForGPUFences(sDev,true,&fence,1));
    unsigned char *pixels=SDL_MapGPUTransferBuffer(sDev,download,false);assert(pixels);
    #define PIXEL(x,y,c) pixels[((y)*1280+(x))*4+(c)]
    assert(PIXEL(251,177,0)>250 && PIXEL(251,177,1)>250); /* native stroke */
    assert(PIXEL(252,177,0)>60 && PIXEL(252,177,0)<70); /* finer than enlarged 1x pixels */
    assert(PIXEL(251,217,0)<5 && PIXEL(251,217,1)>250); /* original ordering */
    assert(PIXEL(500,177,0)>60 && PIXEL(500,177,0)<70); /* copied depth hid red */
    assert(PIXEL(251,300,0)>60 && PIXEL(251,300,0)<70); /* scissor */
    SDL_UnmapGPUTransferBuffer(sDev,download);
    SDL_ReleaseGPUFence(sDev,fence);SDL_DestroyGPUDevice(sDev);SDL_Quit();
    puts("GPU screen text: native subpixel detail, scene depth, panel ordering and clipping passed");
}
