#ifndef PORT_ONLINE_PRESENCE_H
#define PORT_ONLINE_PRESENCE_H
#include "port_pack.h"

/* One index read per AFS, rather than 68,601 archive opens on shared storage.
   Missing/invalid packed entries still use the normal loose/mod lookup. */
static int Port_OnlineRequiredPresent(const char *root, const int counts[3],
                                      const char *const *loose, int looseCount) {
    char path[1600], relative[128];
    for (int part=0;part<3;part++) {
        unsigned char header[8], *index=NULL;
        uint32_t count=0;
        int64_t total=0;
        snprintf(path,sizeof(path),"%s/pzs3us%d.afs",root,part);
        FILE *fp=fopen(path,"rb");
        if(fp) {
            if(fread(header,1,8,fp)==8 && !memcmp(header,"AFS\0",4)) {
                count=Port_PackU32(header+4);
                if(count<=1000000 && !Port_PackSeek(fp,0,SEEK_END)) total=Port_PackTell(fp);
                if(count && count<=1000000 && total>=8+(int64_t)count*8 && !Port_PackSeek(fp,8,SEEK_SET)) {
                    index=malloc((size_t)count*8);
                    if(index && fread(index,8,count,fp)!=count) {free(index);index=NULL;}
                }
            }
            fclose(fp);
        }
        for(int id=0;id<counts[part];id++) {
            if(index && (uint32_t)id<count) {
                uint32_t offset=Port_PackU32(index+(size_t)id*8), bytes=Port_PackU32(index+(size_t)id*8+4);
                if(offset>=8+(uint64_t)count*8 && (uint64_t)offset+bytes<=(uint64_t)total) continue;
            }
            snprintf(relative,sizeof(relative),"pzs3us%d/%05d.bin",part,id);
            snprintf(path,sizeof(path),"%s/mods/%s",root,relative);
            if(Port_PackExists(path)) continue;
            snprintf(path,sizeof(path),"%s/%s",root,relative);
            if(!Port_PackExists(path)) {
                fprintf(stderr,"zenkaipower: online data: missing required file %s\n",relative);
                free(index);return 0;
            }
        }
        free(index);
    }
    for(int i=0;i<looseCount;i++) {
        snprintf(path,sizeof(path),"%s/mods/%s",root,loose[i]);
        if(Port_PackExists(path)) continue;
        snprintf(path,sizeof(path),"%s/%s",root,loose[i]);
        if(!Port_PackExists(path)) {
            fprintf(stderr,"zenkaipower: online data: missing required file %s\n",loose[i]);return 0;
        }
    }
    return 1;
}
#endif
