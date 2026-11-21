#!/usr/bin/env python3
"""Production manual unlock policy and close-range reacquisition gates."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/btl_char_flag.c').read_text()
a=s.index('#define PORT_LOCK_MANUAL')
b=s.index('\n#endif',a)
policy=s[a:b]
a=s.index('void BtlChar_UpdateLockOn(')
b=s.index('\n}',a)+2
update=s[a:b]
action=(root/'src/battle/btl_act_c.c').read_text()
a=action.index('s32 BtlAct_SearchHandler(')
b=action.index('\n#endif',a)+len('\n#endif')
search=action[a:b]+'\nreturn 1;\n}\n'
decide=(root/'src/battle/btl_act_h_b.c').read_text()
a=decide.index('    if (mask & 0x200000) {\n#ifdef PORT')
b=decide.index('\n#endif',a)+len('\n#endif')
search_gate='int CanAutoSearch(BtlFlagChr *chr) { int mask=0x200000;\n'+decide[a:b]+'\n} return 1; }\n'

source=r'''
#include <assert.h>
#include <string.h>
#define PORT 1
#define PORT_ZENKAI_MANUAL_LOCK 1
#include "battle/btl_char_flag.h"
static int pack=1,cpu,held,frames,model;
int Port_ZenkaiPackEnabled(void) { return pack; }
int BtlInput_IsHeld(void *p,unsigned mask) { assert(mask==0x4000);return held; }
int BtlInput_GetHeldFrames(void *p,int bit) { assert(bit==14);return frames; }
int BattleSide_IsCpu(int side) { return cpu; }
int BtlCharApi_IsModelNew(int id) { return model; }
int BtlChar_TestFlag(BtlFlagChr *c,unsigned n) {return !!(c->held[n/8]&(1<<(n%8)));}
void BtlChar_SetHeldFlag(BtlFlagChr *c,unsigned n) {c->held[n/8]|=1<<(n%8);}
void BtlChar_ClearFlag(BtlFlagChr *c,unsigned n) {c->held[n/8]&=~(1<<(n%8));}
float BtlChar_GetSpacing(BtlFlagChr *c,int a) {return 100;}
float BtlOpp_GetDistance(BtlFlagChr *c) {return 1;}
'''+policy+update+r'''
typedef BtlFlagChr Chr;
static int requested;
void BtlAct_Request(Chr *chr,int action) { requested=action; }
'''+search+search_gate+r'''
int main(void) {
 BtlFlagChr c={0},snapshot;
 BtlChar_SetHeldFlag(&c,3);BtlChar_SetHeldFlag(&c,5);
 held=1;frames=14;BtlChar_UpdateLockOn(&c);assert(BtlChar_TestFlag(&c,5));
 frames=15;BtlChar_UpdateLockOn(&c);assert(!BtlChar_TestFlag(&c,5) && !BtlChar_TestFlag(&c,0x13));
 assert(Port_BtlManualUnlocked(&c));
 assert(!CanAutoSearch(&c));
 assert(BtlAct_SearchHandler(&c,1)==0 && requested==0xB);
 requested=0;assert(BtlAct_SearchHandler(&c,2)==0 && requested==0xB);
 assert(BtlAct_SearchHandler(&c,3)==1); /* original exit cleanup still runs */
 frames=100;for(int i=0;i<100;i++) BtlChar_UpdateLockOn(&c);
 assert(Port_BtlManualUnlocked(&c) && !BtlChar_TestFlag(&c,5));
 held=0;frames=0;BtlChar_UpdateLockOn(&c);snapshot=c;
 assert(Port_BtlManualUnlocked(&c));
 held=1;frames=14;BtlChar_UpdateLockOn(&c);assert(Port_BtlManualUnlocked(&c));
 frames=15;BtlChar_UpdateLockOn(&c);assert(!Port_BtlManualUnlocked(&c) && BtlChar_TestFlag(&c,5));
 frames=100;BtlChar_UpdateLockOn(&c);assert(BtlChar_TestFlag(&c,5));
 c=snapshot;assert(Port_BtlManualUnlocked(&c)); /* snapshot owns suppression */
 held=0;BtlChar_UpdateLockOn(&c);pack=0;BtlChar_UpdateLockOn(&c);
 assert(!Port_BtlManualUnlocked(&c) && BtlChar_TestFlag(&c,5));
 pack=1;held=0;BtlChar_UpdateLockOn(&c);assert(CanAutoSearch(&c));cpu=1;held=1;frames=20;BtlChar_UpdateLockOn(&c);
 assert(!Port_BtlManualUnlocked(&c) && BtlChar_TestFlag(&c,5));
 cpu=0;held=0;BtlChar_UpdateLockOn(&c);held=1;frames=15;BtlChar_UpdateLockOn(&c);
 assert(Port_BtlManualUnlocked(&c));model=1;BtlChar_UpdateLockOn(&c);
 assert(!Port_BtlManualUnlocked(&c));model=0;BtlChar_UpdateLockOn(&c);
 assert(!Port_BtlManualUnlocked(&c)); /* held on new model cannot toggle until released */
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'lock.c';p.write_text(source);exe=Path(tmp)/'lock'
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Manual unlock hold, release, reacquisition, CPU, Classic Mode and reset checks passed')
