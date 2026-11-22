"""Exercise adaptive interpolation with display-paced and overloaded presents."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
draw = (ROOT / 'port/src/gs/gs_draw.c').read_text()
match = re.search(r'^static void interp_cost\(uint64_t ns\) \{.*?^\}', draw, re.M | re.S)
assert match
due = re.search(r'^static uint64_t interp_due\(void\) \{.*?^\}', draw, re.M | re.S)
assert due
limit = re.search(r'^static void interp_limit_pending\(void\) \{.*?^\}', draw, re.M | re.S)
assert limit
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint64_t sInterpCostNs;
static int sInterpCap=8, paced;
static int interp_steps(void) { return 8; }
static int interp_present_paced(void) { return paced; }
static uint64_t Port_BlankNs(void) { return 16683350; }
static uint64_t sInterpBlendShown,sInterpBlendTook,sInterpRealTook,sInterpTickNs;
static int sInterpNext,sInterpSteps;
static unsigned sInterpBlanks=2;
''' + match[0] + '\n' + limit[0] + '\n' + due[0] + r'''
int main(int argc,char **argv) {
    assert(argc==2);
    int scenario=atoi(argv[1]);
    /* Protection starts with the very first slow frame, not 300 presents later. */
    interp_cost(80000000);
    assert(sInterpCap==1);
    for(int i=0;i<10;i++) interp_cost(1000000);
    assert(sInterpCap==1); /* Cheap real frames cannot immediately re-enable blends. */
    sInterpCap=8;
    /* At 60 FPS, the real frame needs its own render cost, not blend synthesis time. */
    sInterpBlendShown=100000000;sInterpTickNs=33366700;
    sInterpSteps=sInterpNext=2;sInterpBlendTook=4000000;sInterpRealTook=1000000;
    assert(interp_due()+sInterpRealTook==sInterpBlendShown+sInterpTickNs/2);
    /* A cap drop changes the pending schedule before the pump starts waiting. */
    sInterpSteps=4;sInterpNext=2;sInterpCap=2;
    interp_due();assert(sInterpNext==4);
    sInterpBlanks=1;sInterpSteps=2;sInterpNext=2;sInterpCap=1;
    interp_due();assert(sInterpNext==2);
    sInterpCap=8;sInterpBlanks=2;
    /* Earlier intermediate frames still account for their reconstruction work. */
    sInterpSteps=4;sInterpNext=2;
    assert(interp_due()+sInterpBlendTook==sInterpBlendShown+sInterpTickNs/4);
    /* Slow renders must retain their real cost in scheduling at 45/32 FPS. */
    sInterpSteps=sInterpNext=2;
    sInterpRealTook=22222222;
    assert(interp_due()+sInterpRealTook==sInterpBlendShown+sInterpTickNs/2);
    sInterpRealTook=31250000;
    assert(interp_due()+sInterpRealTook==sInterpBlendShown+sInterpTickNs/2);
    paced=scenario!=0;
    uint64_t cost=scenario==2 ? 8333333 : scenario==3 ? 4166667 : 16666667;
    int expected=scenario==2 ? 4 : scenario==3 ? 8 : scenario==0 ? 1 : 2;
    for(int i=0;i<500;i++) interp_cost(cost);
    assert(sInterpCap==expected);
    if(paced) {
        /* Genuine slow rendering must still reduce the output rate. */
        interp_cost(40000000);
        assert(sInterpCap==1);
        for(int i=0;i<500;i++) interp_cost(cost);
        assert(sInterpCap==expected);
    }
    printf("PASS: paced=%d, present=%.2fms, cap=%d, overload/recovery\n",paced,cost/1e6,sInterpCap);
}
'''
with tempfile.TemporaryDirectory(prefix='interp-pacing-') as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(code)
    subprocess.run(['cc', '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-function', str(source), '-o', str(binary)], check=True)
    for scenario in range(4):
        subprocess.run([str(binary), str(scenario)], check=True)
