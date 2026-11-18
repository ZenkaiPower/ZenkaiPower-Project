"""Check production dash-vector adapters without building the Android app."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
s=(ROOT/'src/battle/eft_l_d.c').read_text()
a=s.index('#ifdef PORT /* Match the callee')
b=s.index('extern void *gEftSpdLine;',a)
caller=r'''
#include "battle/eft_l.h"
#include "battle/port_effect_vector.h"
#include <assert.h>
'''+s[a:b]+r'''
typedef void (*TrailCall)(s32,PortEffectVector,PortEffectVector);
typedef void (*StreakCall)(s32,PortEffectVector,PortEffectVector,PortEffectVector,f32,f32,f32);
_Static_assert(__builtin_types_compatible_p(__typeof__(&EftSpdLine_AddTrail),TrailCall),"Trail argument alignment differs from adapter");
_Static_assert(__builtin_types_compatible_p(__typeof__(&EftSpdLine_AddStreak),StreakCall),"Streak argument alignment differs from adapter");
#ifdef __ANDROID__
_Static_assert(__alignof__(PortEffectVector)==16,"ARM64 vectors must use aligned register pairs");
#else
_Static_assert(__alignof__(PortEffectVector)==__alignof__(Vec4),"Desktop ABI must stay unchanged");
#endif
extern int checks;
int main(void) {
    struct { int pad;Vec4 value; } input={0,{123,-45,67,1}};
    Vec4 tail={-10,20,30,1},dir={0,0,-1,0};
    EftSpdLine_AddTrail(7,&input.value,&dir);
    EftSpdLine_AddStreak(9,&input.value,&tail,&dir,.25f,2,64);
    assert(checks==2);
    return 0;
}
'''
callee=r'''
#include "battle/eft_m.h"
#include <assert.h>
int checks;
void EftSpdLine_AddTrail(s32 id,EftVec pos,EftVec dir) {
    assert(id==7 && pos.x==123 && pos.y==-45 && pos.z==67 && pos.w==1);
    assert(dir.x==0 && dir.y==0 && dir.z==-1 && dir.w==0);checks++;
}
void EftSpdLine_AddStreak(s32 id,EftVec pos,EftVec tail,EftVec dir,f32 speed,f32 life,f32 width) {
    assert(id==9 && pos.x==123 && pos.y==-45 && pos.z==67 && pos.w==1);
    assert(tail.x==-10 && tail.y==20 && tail.z==30 && tail.w==1);
    assert(dir.x==0 && dir.y==0 && dir.z==-1 && dir.w==0);
    assert(speed==.25f && life==2 && width==64);checks++;
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory);(tmp/'caller.c').write_text(caller);(tmp/'callee.c').write_text(callee)
    for android in (False,True):
        subprocess.run(['gcc','-std=gnu11','-fno-builtin','-fsanitize=undefined','-Iinclude','-Iport/include','-include','port_compat.h']+
            (['-D__ANDROID__=1'] if android else [])+[str(tmp/'caller.c'),str(tmp/'callee.c'),'-o',str(tmp/'test')],cwd=ROOT,check=True)
        subprocess.run([str(tmp/'test')],check=True)
print('Dash adapter checks passed: Android alignment, desktop ABI, position/direction and streak parameters.')
