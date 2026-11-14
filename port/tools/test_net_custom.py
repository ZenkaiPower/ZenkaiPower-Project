"""Verify personal netplay profiles and chunk recovery without a game/server."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NetCustomTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("gcc"), "requires gcc")
    def test_profiles_chunks_and_relaunch(self):
        source = r'''
#include "SOURCE"
#undef getenv
#include <assert.h>
const char *Port_GetEnv(const char *name) { return getenv(name); }
int Port_ZenkaiEnabled(void) { return 0; }
int Port_ZenkaiSelected(void) { return 0; }
int Port_ZenkaiMetadata(const char *path, char id[33], char hash[65]) { return 0; }
unsigned char *gSaveData;
static void u16(unsigned char *p, unsigned value) { uint16_t n=value;memcpy(p,&n,2); }
static void i32(unsigned char *p, int value) { int32_t n=value;memcpy(p,&n,4); }
static void make_save(unsigned char *save,int item,int chara) {
    memset(save,0,0x4000);
    i32(save+0x20C,10);
    u16(save+0x1808,item);u16(save+0x1808+0x34,2);
    for(int i=0;i<14;i++) i32(save+0x2D40+i*0x1C+0x18,-1);
    u16(save+0x2D40,item+1);u16(save+0x2D40+0x14,3);
    i32(save+0x2D40+0x18,chara);save[0x2EC8+item-1]=3;
    i32(save+0x3028,12345);save[0x1608]=7;save[0xC10]=0xAA;
}
int main(int argc, char **argv) {
    unsigned char host[0x4000],join[0x4000],work[0x4000];
    uint8_t profiles[2][NET_CUSTOM_BYTES];
    make_save(host,10,35);make_save(join,20,42);
    if(argc>1) {
        /* Bot-only inventory unlocks never mutate the source player's save. */
        unsigned char before[sizeof(host)];memcpy(before,host,sizeof(host));
        gSaveData=host;setenv("ZENKAIPOWER_NET_BOT","1",1);capture_custom(0);
        for(int i=0;i<350;i++) assert(sCustom[0][4+0x2EC8-NET_CUSTOM_OFFSET+i]==3);
        assert(!memcmp(before,host,sizeof(host)));
        return 0;
    }
    gSaveData=host;capture_unlocks(0);memcpy(profiles[0],sCustom[0],NET_CUSTOM_BYTES);
    gSaveData=join;capture_unlocks(1);memcpy(profiles[1],sCustom[1],NET_CUSTOM_BYTES);
    memcpy(sCustom[0],profiles[0],NET_CUSTOM_BYTES);sCustomReady[0]=1;
    sCustomReady[1]=0;sCustomReceived=0;sMe=0;sSession=1;
    memcpy(work,host,sizeof(work));gSaveData=work;
    Port_NetCustomApply(1);assert(!memcmp(work,host,sizeof(work)));
    assert(!custom_store_chunk(NET_CUSTOM_CHUNKS,profiles[1],1));
    assert(!custom_store_chunk(0,profiles[1],NET_CUSTOM_CHUNK-1));
    /* Out-of-order chunks, a duplicate, and a missing packet. */
    for(int i=NET_CUSTOM_CHUNKS-1;i>0;i--) {
        unsigned at=i*NET_CUSTOM_CHUNK,n=NET_CUSTOM_BYTES-at;
        if(n>NET_CUSTOM_CHUNK)n=NET_CUSTOM_CHUNK;
        assert(custom_store_chunk(i,profiles[1]+at,n));
        assert(custom_store_chunk(i,profiles[1]+at,n));
    }
    assert(!sCustomReady[1]);
    assert(custom_store_chunk(0,profiles[1],NET_CUSTOM_CHUNK));
    assert(sCustomReady[1] && sCustomReceived==NET_CUSTOM_ALL);
    assert(!memcmp(sCustom[0],profiles[0],NET_CUSTOM_BYTES));
    /* A late duplicate cannot mutate an accepted peer profile. */
    uint8_t bad[NET_CUSTOM_CHUNK];memset(bad,255,sizeof(bad));
    assert(custom_store_chunk(0,bad,sizeof(bad)));
    assert(!memcmp(sCustom[1],profiles[1],NET_CUSTOM_BYTES));
    Port_NetCustomApply(1);
    assert(!memcmp(work+0x1808,join+0x1808,NET_CUSTOM_SAVE_BYTES));
    assert(!memcmp(work+0x3028,host+0x3028,4));
    assert(work[0x1608]==7 && work[0xC10]==0xAA);
    Port_NetCustomApply(0);
    assert(!memcmp(work+0x1808,host+0x1808,NET_CUSTOM_SAVE_BYTES));
    sSession=0;Port_NetCustomApply(1);
    assert(!memcmp(work+0x1808,host+0x1808,NET_CUSTOM_SAVE_BYTES));
    char encoded[NET_CUSTOM_BYTES*2+1];
    for(int i=0;i<NET_CUSTOM_BYTES;i++)snprintf(encoded+i*2,3,"%02x",profiles[1][i]);
    setenv("ZENKAIPOWER_NET_CUSTOM",encoded,1);restore_custom(0);
    assert(!memcmp(sCustom[0],profiles[1],NET_CUSTOM_BYTES));
    char *cached=getenv("ZENKAIPOWER_NET_CUSTOM");cached[0]='z';restore_custom(0);
    assert(!memcmp(sCustom[0],profiles[1],NET_CUSTOM_BYTES));
    /* Malformed save entries cannot index past the game's item/character tables. */
    memset(bad,0,sizeof(bad));
    uint8_t invalid[NET_CUSTOM_BYTES];memset(invalid,255,sizeof(invalid));
    custom_sanitize(invalid);
    uint16_t item,level;int32_t chara;
    memcpy(&item,invalid+4,2);memcpy(&level,invalid+4+0x34,2);
    memcpy(&chara,invalid+4+97*0x38+0x18,4);
    assert(item==0 && level==6 && chara==-1);
    gSaveData=NULL;capture_custom(0);
    memcpy(&chara,sCustom[0]+4+97*0x38+0x18,4);assert(chara==-1);
    return 0;
}
'''.replace("SOURCE", str(ROOT / "port/src/gs/net.c"))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "custom.c"
            exe = Path(directory) / "custom"
            path.write_text(source)
            subprocess.run(["gcc", "-w", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                            "-ffunction-sections", "-fdata-sections", "-I" + str(ROOT / "port/build/gen/gs"),
                            str(path), "-Wl,--gc-sections", "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
            subprocess.run([str(exe), "bot"], check=True)


if __name__ == "__main__":
    unittest.main()
