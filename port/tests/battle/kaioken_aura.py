#!/usr/bin/env python3
"""Production aura selection restores the ordinary palette when Kaioken ends."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/btl_capi_a.c').read_text()
a=s.index('s32 BtlCharApi_GetAuraType(')
b=s.index('\n/*',a)
fx=(root/'src/battle/btl_char_fx.c').read_text()
c=fx.index('void BtlFx_UpdateChargeFx(')
d=fx.index('/* Requests 4 and 5:',c)
charge=fx[c:d]
source=r'''
#include <assert.h>
#include <stddef.h>
#define PORT 1
typedef int s32;
typedef struct {int active,objId;} BtlCapiChr;
typedef BtlCapiChr FxChr;typedef BtlCapiChr BtlMemberChr;
typedef struct {int auraType;} Param;
typedef struct {struct {int flags;} u;Param *param;} BtlCapiObj;
static BtlCapiChr fighter;static BtlCapiObj object;static int zenkai=1;
int Port_ZenkaiEnabled(void) {return zenkai;}
BtlCapiChr *BtlChar_FindByObjId(int id) {return id==0 ? &fighter:NULL;}
BtlCapiObj *BtlChar_GetObj(BtlCapiChr *c) {return &object;}
BtlCapiObj *BtlObj_Get(int id) {return NULL;}
int BtlChar_TestFlag(BtlCapiChr *c,int flag) {return c->active && flag==0x98;}
int BtlParam_GetAuraKind(BtlCapiChr *c) {return 7;}
static int chargeActive,starts,bursts,stops;
int BtlChar_TestFxBit(FxChr *c,int bit) {return bit==7 || bit==8;}
int EftCharge_IsActive(int id) {return chargeActive;}
void EftCharge_Start(int *args) {starts++;chargeActive=1;}
void EftCharge_Stop(int id) {stops++;chargeActive=0;}
void EftCharge_Burst(int id) {bursts++;}
'''+s[a:b]+charge+r'''
int main() {
 assert(BtlCharApi_GetAuraType(0)==7);
 fighter.active=1;assert(BtlCharApi_GetAuraType(0)==7);
 zenkai=0;assert(BtlCharApi_GetAuraType(0)==7);
 object.u.flags=0x40000;assert(BtlCharApi_GetAuraType(0)==3);
 zenkai=1;fighter.active=0;object.u.flags=0;assert(BtlCharApi_GetAuraType(0)==7);
 assert(BtlCharApi_GetAuraType(1)==0);
 zenkai=1;fighter.active=1;BtlFx_UpdateChargeFx(&fighter);assert(starts==1 && bursts==1);
 zenkai=0;chargeActive=0;BtlFx_UpdateChargeFx(&fighter);assert(starts==2 && bursts==2);
 assert(!stops);

}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'aura.c';e=Path(tmp)/'aura';p.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=undefined',str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('Native Kaioken aura and charge-effect restoration checks passed')
