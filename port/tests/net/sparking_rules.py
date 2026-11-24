"""Production source-save eligibility and shared online rules; no assets needed."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'port/src/gs/net.c').read_text()
names=['Port_NetSaveSparkingChanges','Port_NetSparkingChanges','sparking_rules','Port_NetRulesSet','Port_NetSpeedSet']
functions=[]
for name in names:
    m=re.search(r'^(?:static )?(?:int|void|uint32_t) '+name+r'\([^\n]*\) \{',source,re.M)
    assert m,name
    at=m.end();depth=1
    while depth:
        if source[at]=='{':depth+=1
        if source[at]=='}':depth-=1
        at+=1
    functions.append(source[m.start():at])
# Exercise the actual greeting handlers, including packet offsets and capability bits.
host=source.split('} else if (type == T_HELLO && sMode == 1) {',1)[1].split('} else if (type == T_HELLO_ACK && sMode == 2 && sFromLobby',1)[0]
join=source.split('} else if (type == T_HELLO_ACK && sMode == 2) {',1)[1].split('} else if (type == T_FPRINT)',1)[0]
harness=r'''#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "menu/port_build_flags.h"
#define RULE_SPARKING 0x2000u
#define RULE_FAST 0x1000u
#define RULE_SPEED_SHIFT 16
static int active,sConnected,sSourceZenkai,sSourceClassic,sWantSpeed=114;
static uint32_t sRules;
int Port_NetActive(void) {return active;}
static uint32_t speed_bits(int p) {return (uint32_t)p<<16 | (p==120 ? RULE_FAST:0);}
'''+ '\n'.join(functions)+r'''
enum { NAME_LEN=32, NET_UNLOCK_WORDS=4, MAGIC=123, T_HELLO_ACK=2, NET_VERSION=13 };
#define CAP_SPARKING 8u
#define CAP_MARK 0x43410000u
static const uint32_t kVersion=NET_VERSION;
static unsigned char pkt[256],reply[256];
static int n,replySize,from,sPeer,sDelay=2,sRollMax=8,sBotSide,sBotSetting;
static int sCfgDelay,sCfgRoll,sVersionBad,sCfgGot;
static uint32_t sSeed=27,sOpts[2],sUnlocks[2][NET_UNLOCK_WORDS];
static char sMyName[NAME_LEN]="test",sNames[2][NAME_LEN];
static int sPeerSparking;
char *getenv(const char *key) {return 0;}
static int bot_setting_env(void) {return 2;}
static void opts_take(int side,const void *p) {memcpy(&sOpts[side],p,4);}
static void net_send(const void *p,int size,const void *address) {memcpy(reply,p,size);replySize=size;}
static void host_greeting(void) {do {
'''+host+r'''
} while(0);}
static void join_greeting(void) {do {
'''+join+r'''
} while(0);}
int main(void) {
    for(int z=0;z<2;z++) for(int classic=0;classic<2;classic++) {
        sSourceZenkai=z;sSourceClassic=classic;
        assert(Port_NetSaveSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && z && !classic));
    }
    uint32_t other=114u<<16 | 3u<<8 | 2u<<4 | 1;
    for(int host=0;host<2;host++) for(int join=0;join<2;join++) {
        sRules=sparking_rules(other|RULE_SPARKING,host,join);
        assert((sRules&~RULE_SPARKING)==other);
        active=sConnected=1;assert(Port_NetSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && host && join));
        Port_NetRulesSet(1,2,3);assert(Port_NetSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && host && join));
        Port_NetSpeedSet(150);assert(Port_NetSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && host && join));
        sConnected=0;assert(!Port_NetSparkingChanges());
    }
    for(int hostSave=0;hostSave<2;hostSave++) for(int joinSave=0;joinSave<2;joinSave++) {
        sSourceZenkai=hostSave;sSourceClassic=0;sRules=other|RULE_SPARKING;
        memset(pkt,0,sizeof(pkt));n=(5+NAME_LEN/4+NET_UNLOCK_WORDS)*4;
        uint32_t cap=CAP_MARK|3|(joinSave?CAP_SPARKING:0);
        memcpy(pkt+16+NAME_LEN,&cap,4);
        host_greeting();
        assert(sConnected && Port_NetSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && hostSave && joinSave));
        sSourceZenkai=joinSave;sConnected=0;sPeerSparking=0;sVersionBad=0;sCfgGot=0;
        memcpy(pkt,reply,replySize);n=replySize;
        join_greeting();
        assert(!sVersionBad && sConnected && sCfgGot);
        assert(Port_NetSparkingChanges()==(ZENKAIPOWER_ONLINE_SPARKING_CHANGES && hostSave && joinSave));
        assert((sRules&~RULE_SPARKING)==other);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'rules.c';exe=Path(tmp)/'rules';c.write_text(harness)
    for enabled in (0,1):
        subprocess.run(['cc','-std=c99','-fsanitize=undefined','-I'+str(root/'include'),
                        '-DZENKAIPOWER_ONLINE_SPARKING_CHANGES='+str(enabled),str(c),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('Online rules: save eligibility, production greeting handlers for all peer combinations, rule/speed updates and disconnected state passed')
