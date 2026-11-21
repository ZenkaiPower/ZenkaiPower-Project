#!/usr/bin/env python3
"""Exercise production charge ownership across transformations and tag switches."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/btl_char_member.c').read_text()
a=s.index('static s32 sSpiritCharge[');b=s.index('\n#endif',a)
source=r'''
#include <assert.h>
#include <string.h>
#include "battle/btl_char_member.h"
#include "sys/port_zenkai_features.h"
static BtlMemberChr chars[2];
static int pack=1;
int Port_ZenkaiPackEnabled(void) {return pack;}
BtlMemberChr *BtlChar_Get(int side) {return &chars[side];}
BtlMember *BtlMember_GetActive(BtlMemberChr *chr) {return &chr->members[chr->active];}
static void setCharge(BtlMemberChr *chr,int n) {*(int *)((unsigned char *)chr+0xE5C)=n;}
static int getCharge(BtlMemberChr *chr) {return *(int *)((unsigned char *)chr+0xE5C);}
'''+s[a:b]+r'''
int main(void) {
 BtlMemberChr *g=&chars[0];g->side=0;g->memberCount=2;
 g->members[0].chara=1;g->members[0].gauge.health=10000;
 g->members[1].chara=25;g->members[1].gauge.health=10000;
 chars[1].side=1;chars[1].memberCount=1;chars[1].members[0].chara=1;
 chars[1].members[0].gauge.health=10000;
 Port_SpiritBombReset();setCharge(g,3);Port_SpiritBombRemember(g);
 assert(Port_SpiritBombStageCharge(0)==3);
 g->active=1;Port_SpiritBombRestore(g);
 assert(getCharge(g)==0 && Port_SpiritBombStageCharge(0)==3);
 g->active=0;g->members[0].chara=2;Port_SpiritBombRestore(g);
 assert(getCharge(g)==3 && Port_SpiritBombStageCharge(0)==3);
 g->members[0].chara=1;Port_SpiritBombRestore(g);assert(getCharge(g)==3);
 assert(Port_SpiritBombStageCharge(1)==0);
 setCharge(&chars[1],2);Port_SpiritBombRemember(&chars[1]);
 Port_SpiritBombConsume(g);assert(Port_SpiritBombStageCharge(0)==0);
 assert(Port_SpiritBombStageCharge(1)==2);
 setCharge(g,1);Port_SpiritBombRemember(g);g->members[0].gauge.health=0;
 assert(Port_SpiritBombStageCharge(0)==0);
 pack=0;assert(Port_SpiritBombStageCharge(1)==0);pack=1;
 Port_SpiritBombReset();Port_SpiritBombRestore(&chars[1]);assert(getCharge(&chars[1])==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'charges.c';p.write_text(source);exe=Path(tmp)/'charges'
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Spirit Bomb ownership, tag switches, transformations, consumption, KO and battle reset checks passed')
