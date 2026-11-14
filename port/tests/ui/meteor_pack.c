/* cc -std=gnu99 port/tests/ui/meteor_pack.c -o /tmp/meteor-pack-test
 * /tmp/meteor-pack-test <directory containing only sparking-meteor.pak> */
#include "../../src/port_pack.h"
#include <assert.h>
int main(int argc,char **argv) {
    char pack[1200],path[2400],name[1200];
    unsigned char header[12],word[20],a[65536],b[65536];
    assert(argc==2);
    snprintf(pack,sizeof(pack),"%s/sparking-meteor.pak",argv[1]);
    FILE *index=fopen(pack,"rb"),*raw=fopen(pack,"rb");
    assert(index && raw && fread(header,1,12,index)==12);
    unsigned count=Port_PackU32(header+8); assert(count==31);
    for (unsigned i=0;i<count;i++) {
        assert(fread(word,1,20,index)==20);
        unsigned n=Port_PackU32(word); assert(n<sizeof(name));
        assert(fread(name,1,n,index)==n); name[n]=0;
        snprintf(path,sizeof(path),"%s/%s",argv[1],name);
        int64_t base,size; FILE *entry=Port_PackOpen(path,&base,&size);
        assert(entry && base==(int64_t)Port_PackU64(word+4) && size==(int64_t)Port_PackU64(word+12));
        fclose(entry);
        entry=Port_PackFopen(path,"rb"); assert(entry);
        assert(Port_PackSeek(raw,base,SEEK_SET)==0);
        int64_t left=size;
        while(left) {
            size_t take=left>(int64_t)sizeof(a)?sizeof(a):(size_t)left;
            assert(fread(a,1,take,raw)==take && fread(b,1,take,entry)==take && !memcmp(a,b,take));
            left-=take;
        }
        assert(fgetc(entry)==EOF); fclose(entry);
        struct stat st; assert(!Port_PackStat(path,&st) && st.st_size==size);
    }
    fclose(raw); fclose(index);
    puts("All 31 packaged assets verified through runtime reads");
}
