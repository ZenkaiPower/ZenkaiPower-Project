#!/usr/bin/env python3
"""Run the real frame scheduling functions with a mock backend; no GPU needed.
python3 port/tests/render/interp_resource_lifetime.py
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'port/src/gs/gs_draw.c').read_text()

def function(name):
    match = re.search(r'^(?:static )?(?:void|int) ' + name + r'\([^\n]*\) \{.*?^\}', source, re.M | re.S)
    assert match, name
    return match.group(0)

prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_TARGETS 4
#define MAX_VU_UNIFORMS 4
#define MAX_VERTS 4
#define INTERP_MAX_PRIMS 4
#define GS_NO_PICTURE 0
 typedef int Vu0Uniform, Vtx, InterpPrim;
static int sInGlowChain, gPortResim, gsSkipFrame, gsInterp = 2, gsAnchor;
static unsigned gsVertCount, gsVuVertCount, gsVuIdxCount, gsVuUniCount, gsDrawCount, gsSkipped, gsNative;
static int gsTargetCount = 1;
static struct { unsigned draws; } gsTargets[MAX_TARGETS];
static struct { uint32_t verts, vuVerts, vuIdx, vuUni, draws; int anchor; unsigned skipped, native; unsigned tdraws[MAX_TARGETS]; } sInterpSaved;
static unsigned sInterpPrevCount, sInterpPrevPrimCount, sInterpPrimCount, sInterpLastBlank;
static unsigned gInterpTicks, gInterpShown, gInterpSkipped, gInterpPaired, gInterpUnpaired, gInterpCamera;
static unsigned gInterpTris, gInterpTrisBlended, gInterpWhy[5], gInterpTrisCamera, gInterpStatic, gInterpThrough, gInterpCuts;
static int sInterpNext, sInterpRepeat, sInterpSteps, sInterpCap = 8;
static uint64_t sInterpTickNs, sInterpBlendShown, sInterpBlendTook, sInterpRealTook, sInterpPrepareTook, gGpuEndNs;
static Vu0Uniform *sInterpPrev, *sInterpBlend, *gsVuUni;
static uint32_t *sInterpPrevSig, *sInterpSig;
static Vtx *sInterpPrevVerts, *sInterpBlendVerts, *gsVerts;
static InterpPrim *sInterpPrevPrims, *sInterpPrims;
static void *sInterpPrimUsed;
unsigned gPortVBlanks, gGsFrame;
static int scalePending, scaleCalls, labelCalls, presents, canBlend;
static int resourcesLive = 1;
static unsigned tickBlanks = 2;
static uint64_t fakeNow, lastCost, prepareCost = 5000000;
static void render(void) {
    assert(resourcesLive); /* A retained draw may never reference released targets. */
    presents++;
    fakeNow += 1000000;
    gsVertCount = gsVuVertCount = gsVuIdxCount = gsVuUniCount = gsDrawCount = 0;
}
static struct { void (*frameEnd)(void); } backend = {render}, *sBackend = &backend;
static void scale_apply(void) { if (scalePending) { scalePending = 0; scaleCalls++; resourcesLive = 0; gsTargetCount = 0; } }
static void menu_labels_apply(void) { labelCalls++; }
static uint64_t gpu_now(void) { return ++fakeNow; }
static uint64_t Port_BlankNs(void) { return 16683350; }
static int Port_Setting(const char *s, int d) { (void)s; return d; }
static int interp_steps(void) { return gsInterp ? 4 : 1; }
static void interp_signatures(uint32_t *s) { (void)s; }
static unsigned interp_prims(InterpPrim *p) { (void)p; return 0; }
static int interp_build(float t, int first) { (void)t; (void)first; fakeNow += prepareCost; return canBlend; }
static void interp_build_verts(float t) { (void)t; fakeNow += 10000000; }
static void interp_wall(float t) { (void)t; }
static void interp_wall_real(void) {}
static void interp_wall_restore(void) {}
static void interp_remember(void) {}
static void interp_cost(uint64_t t) { lastCost=t; }
static void interp_present_log(char c) { (void)c; }
static void skip_decide(void) {}
static void GsGpu_InterpPump(uint64_t ahead, int all) { (void)ahead; (void)all; abort(); }
'''
suffix = r'''
static void frame(int blend, int interpolation) {
    canBlend = blend; gsInterp = interpolation;
    resourcesLive = 1; gsTargetCount = 1;
    gsDrawCount = gsVertCount = 1;
    scalePending = 1; scaleCalls = labelCalls = presents = 0;
    sInterpNext = 0; gPortVBlanks += tickBlanks;
    GsGpu_FrameEnd();
}
int main(void) {
    /* Startup/camera cut: its real picture is retained for three more presents. */
    frame(0, 2);
    assert(presents == 1 && sInterpRepeat && sInterpNext == 2);
    assert(scaleCalls == 0 && labelCalls == 0);
    while (sInterpNext) interp_show_next();
    assert(presents == 4 && !sInterpRepeat && scaleCalls == 1 && labelCalls == 1);
    /* Ordinary interpolation has the same resource lifetime. */
    frame(1, 2);
    assert(presents == 1 && !sInterpRepeat && scaleCalls == 0);
    assert(lastCost >= 16000000); /* initial camera/pairing + synthesis + render */
    while (sInterpNext) interp_show_next();
    assert(presents == 4 && scaleCalls == 1 && labelCalls == 1);
    /* A long stall drops overdue blends but must present the real frame and
       release retained render targets exactly once. */
    frame(1,2);
    assert(presents==1 && sInterpNext==2 && resourcesLive);
    fakeNow+=100000000;
    assert(interp_show_next());
    assert(presents==2 && !sInterpNext && scaleCalls==1 && labelCalls==1);
    /* Expensive reconstruction shows one untouched real frame, with no
       synthetic model or duplicate replay, even with interpolation requested. */
    prepareCost=40000000;
    frame(1,2);
    assert(presents==1 && sInterpSteps==1 && !sInterpNext && !sInterpRepeat);
    assert(lastCost>=41000000 && scaleCalls==1 && labelCalls==1);
    prepareCost=5000000;
    /* With smoothing disabled, changes apply immediately after the picture. */
    frame(0, 0);
    assert(presents == 1 && !sInterpNext && scaleCalls == 1 && labelCalls == 1);
    /* A 60 Hz scene at the minimum adaptive cap still has one real picture. */
    tickBlanks=1;sInterpCap=1;
    frame(0,1);
    assert(sInterpSteps==1 && presents==1 && !sInterpNext);
    puts("Interpolation resource lifetime checks passed");
}
'''
with tempfile.TemporaryDirectory(prefix='zenkaipower-interp-lifetime-') as tmp:
    c = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    c.write_text(prefix + '\n' + '\n'.join(function(name) for name in
        ('interp_counts_restore', 'interp_show', 'interp_show_next', 'GsGpu_FrameEnd')) + suffix)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
