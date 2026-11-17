#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *root;
static int packEnabled = 1;
int Port_ZenkaiPackEnabled(void) { return packEnabled; }
const char *Port_FileRoot(void) { return root; }
int PortStages_Alias(const char *r,char *o,unsigned n) { return 0; }
int PortSongs_Alias(const char *r,char *o,unsigned n) { return 0; }
void Port_StateTouch(void *p,unsigned long n) {}
int Port_StageFits(const char *r,long n) { return 1; }
int Port_StageBufSize(void) { return 0; }
#include "plat_fcache.c"
#include "plat_content.c"
int main(int argc,char **argv) {
    const int indices[]={3015,3019,3025,3029,3016,3020,3026,3030};
    unsigned char *data,*expected;
    char rel[64],path[256];
    FILE *f;
    long n;
    root=argv[1];
    assert(PortContent_Enabled()==PORT_ZENKAI_EXTRA_OUTFITS);
    if (!PORT_ZENKAI_EXTRA_OUTFITS) {
        assert(PortContent_CostumeCount(159,2)==2);
        assert(PortContent_CostumeCount(160,2)==2);
        assert(!PortContent_Has("pzs3us1/03016.bin"));
        assert(!PortContent_Open("pzs3us1/03016.bin"));
        puts("PASS: compiled-out outfits retain original costume counts and resources.");
        return 0;
    }
    assert(PortContent_CostumeCount(159,2)==4);
    assert(PortContent_CostumeCount(160,2)==4);
    assert(PortContent_CostumeCount(0,2)==2);
    for(int i=0;i<8;i++) {
        snprintf(rel,sizeof(rel),"pzs3us1/%05d.bin",indices[i]);
        n=Port_FileSize(rel); assert(n>0);
        assert(PortContent_Has(rel));
        f=Port_FileOpenHost(rel,path,sizeof(path)); assert(f);
        assert(!strncmp(path,"builtin:",8));
        struct stat metadata;
        assert(!fstat(fileno(f),&metadata) && metadata.st_size==n);
        fclose(f);
        data=malloc(n); expected=malloc(n);
        assert(Port_FileReadAt(rel,0,data,n)==n);
        snprintf(path,sizeof(path),"%s/%05d.bin",argv[2],indices[i]);
        f=fopen(path,"rb");assert(f);
        assert(fread(expected,1,n,f)==n && fgetc(f)==EOF);fclose(f);
        assert(!memcmp(data,expected,n));
        assert(Port_FileReadAt(rel,n-1,data,2)==1);
        assert(Port_FileReadAt(rel,n,data,1)==0);
        free(data);free(expected);
    }
    /* A cached embedded stream must not leak into Classic Mode, or back. */
    for (int mode=0;mode<3;mode++) {
        packEnabled = mode==1;
        assert(PortContent_CostumeCount(159,2)==(packEnabled ? 4 : 2));
        assert(PortContent_CostumeCount(160,2)==(packEnabled ? 4 : 2));
        assert(PortContent_Has("pzs3us1/03016.bin")==packEnabled);
        if (!packEnabled) assert(!PortContent_Open("pzs3us1/03016.bin"));
        unsigned char header[4];
        assert(Port_FileReadAt("pzs3us1/03016.bin",0,header,4)==4);
        assert(header[0]==(packEnabled ? 52 : 0x7b));
        f=Port_FileOpenHost("pzs3us1/03016.bin",path,sizeof(path)); assert(f);
        assert((!strncmp(path,"builtin:",8))==packEnabled);
        fclose(f);
    }
    puts("PASS: eight compiled outfit states preserve original portraits and gameplay; bounded reads and Classic Mode transitions pass.");
}
