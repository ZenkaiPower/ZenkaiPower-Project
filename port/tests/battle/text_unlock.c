/* Link with btl_seq.c built using port_compat.h and function sections,
 * then --gc-sections to isolate the script filters. */
#include "battle/btl_seq.h"
#include <assert.h>
#include <stdio.h>
static u64 save[0x4000/8];
u64 *gSaveData=save;
static BtlTextWork work;
static s32 memberFlag, checkedSide;
BtlTextWork *BtlMenu_GetWork(void) {return &work;}
s32 BtlCtrl_TestMemberUnk70(s32 side) {checkedSide=side;return memberFlag;}
int main(void) {
 u16 plain[]={'*','0',0}, condition[]={'&','0','7','7','*',0};
 u16 special[]={'&','0','8','6','*',0}, bad[]={'&','9','9','9','*',0};
 u16 mask[]={'!','F','*',0}, *p;
 s32 i;
 p=plain;assert(BtlText_CheckUnlock(&p)==1 && p==plain);
 p=condition;assert(!BtlText_CheckUnlock(&p) && p==condition+4);
 save[0xC10/8+77/64]|=1ULL<<(77%64);
 p=condition;assert(BtlText_CheckUnlock(&p)==1 && *p=='*');
 for(i=0;i<2;i++) {memberFlag=i;work.side=1;p=special;assert(BtlText_CheckUnlock(&p)==!i && checkedSide==1 && p==special+4);}
 p=bad;assert(!BtlText_CheckUnlock(&p) && p==bad+4);
 for(i=0;i<4;i++) {work.unk10=i;p=mask;assert(BtlText_CheckLineMask(&p,0)==1 && p==mask+2);}
 mask[1]='B';work.unk10=2;p=mask;assert(!BtlText_CheckLineMask(&p,1));
 mask[1]='3';work.unk10=0;p=mask;assert(!BtlText_CheckLineMask(&p,0));
 puts("Battle text checks passed: cursor advancement, locked/unlocked characters, member condition and line masks.");
 return 0;
}
