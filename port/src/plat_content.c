/* Compose built-in visual content with installed original gameplay resources.
 * Manifests describe asset targets, base resources and replacement ranges. */
#include <stdlib.h>
#include "port_pack.h"
#include "plat_content.h"
#include "../../include/sys/port_zenkai_features.h"

#if PORT_ZENKAI_EXTRA_OUTFITS
extern int Port_ZenkaiPackEnabled(void);
int PortContent_Enabled(void) { return Port_ZenkaiPackEnabled(); }
typedef struct ContentAsset {
    const char *target, *base;
    const unsigned char *data;
    unsigned size, first, last;
} ContentAsset;
typedef struct ContentCostume { int character, count; } ContentCostume;
#include "../build/generated/content_assets.inc"
extern FILE *Port_FileOpenHost(const char *, char *, size_t);
static const ContentAsset *content_find(const char *path) {
    unsigned i;
    if (!PortContent_Enabled()) return NULL;
    for (i=0;i<sizeof(content_assets)/sizeof(content_assets[0]);i++)
        if (!strcmp(path,content_assets[i].target)) return &content_assets[i];
    return NULL;
}
int PortContent_Has(const char *path) { return content_find(path)!=NULL; }
int PortContent_CostumeCount(int character, int fallback) {
    unsigned i;
    if (!PortContent_Enabled()) return fallback;
    for (i=0;i<sizeof(content_costumes)/sizeof(content_costumes[0]);i++)
        if (content_costumes[i].character==character) return content_costumes[i].count;
    return fallback;
}

static uint32_t content_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void content_put32(unsigned char *p, uint32_t value) {
    p[0]=value; p[1]=value>>8; p[2]=value>>16; p[3]=value>>24;
}
FILE *PortContent_Open(const char *rel) {
    char base[128], path[512];
    FILE *original, *result=NULL;
    unsigned char *data=NULL, *output=NULL;
    const unsigned char *visual;
    long length;
    uint32_t count, i, cursor, capacity, visualSize;
    const ContentAsset *asset=content_find(rel);
    if (!asset) return NULL;
    visual=asset->data; visualSize=asset->size;
    snprintf(base,sizeof(base),"%s",asset->base);
    original=Port_FileOpenHost(base,path,sizeof(path));
    if (!original) return NULL;
    if (fseek(original,0,SEEK_END) || (length=ftell(original))<12 || length>16*1024*1024 ||
        fseek(original,0,SEEK_SET)) goto done;
    data=(unsigned char *)malloc((size_t)length);
    if (!data || fread(data,1,(size_t)length,original)!=(size_t)length) goto done;
    count=content_u32(data);
    if (count<asset->last || count>4096 || (count+2)*4>(uint32_t)length) goto done;
    capacity=(uint32_t)length+visualSize+count*16+2048;
    output=(unsigned char *)calloc(capacity,1);
    if (!output) goto done;
    content_put32(output,count);
    cursor=(count+2)*4;
    for (i=1;i<=count;i++) {
        const unsigned char *source=(i>=asset->first && i<=asset->last) ? visual : data;
        uint32_t size=source==visual ? visualSize : (uint32_t)length;
        uint32_t first=content_u32(source+i*4), last=content_u32(source+(i+1)*4);
        uint32_t start=first&~3u, end=last&~3u;
        if (start>end || end>size) goto done;
        cursor=(cursor+15)&~15u;
        if (end-start>capacity-cursor) goto done;
        content_put32(output+i*4,cursor|(first&3));
        memcpy(output+cursor,source+start,end-start);
        cursor+=end-start;
    }
    content_put32(output+(count+1)*4,cursor);
    cursor=(cursor+2047)&~2047u;
    result=Port_PackTempFile();
    if (result && (fwrite(output,1,cursor,result)!=cursor || fseek(result,0,SEEK_SET))) {
        fclose(result); result=NULL;
    }
done:
    fclose(original);
    free(data);
    free(output);
    return result;
}

#else
int PortContent_Enabled(void) { return 0; }
int PortContent_Has(const char *path) { (void)path; return 0; }
int PortContent_CostumeCount(int character, int fallback) { (void)character; return fallback; }
FILE *PortContent_Open(const char *rel) { (void)rel; return NULL; }
#endif
