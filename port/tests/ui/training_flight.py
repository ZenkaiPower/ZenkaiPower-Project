#!/usr/bin/env python3
"""Check the production child-timeline tick gate without rebuilding the game."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/sys/gfxm_c.c').read_text()
start = source.index('void FlashSlot_Advance(FlashSlot *slot, u32 count) {')
end = source.index('\n}\n', start) + 3
snapshot_start = source.index('static struct {\n    FlashShape *shape;')
snapshot_end = source.index('static s32 sTrainingFlightPreviousCount;', snapshot_start) + len('static s32 sTrainingFlightPreviousCount;')
helpers_start = source.index('static void FlashTraining_SaveFlight(')
helpers_end = source.index('\n#endif', helpers_start)
code = r'''
#include <assert.h>
#include <string.h>
#include "sys/gfxm_c.h"
static s32 sTrainingSlowFlight;
static int sTrainingFlightTick;
static float sTrainingFlightPhase;
static int sTrainingGotenTree;
int Port_PageAspectMilli(void) { return 1778; }
''' + source[snapshot_start:snapshot_end] + source[helpers_start:helpers_end] + r'''
static unsigned gotenTicks, otherTicks;
static FlashClip goten, videl;
void FlashTl_Advance(FlashTl *tl, u32 count, u8 children, u8 force) {
 if (tl==&goten.tl) gotenTicks+=count;
 else otherTicks+=count;
}
''' + source[start:end] + r'''
int main(void) {
 FlashSlot g={0}, v={0}, child={0};FlashShape flying={0};FlashMtx drawn;int i;
 strcpy(goten.name,"mc_ch_goten");strcpy(videl.name,"mc_guide_bidel");
 g.clip=&goten;v.clip=&videl;sTrainingSlowFlight=1;
 for(i=0;i<120;i++) { sTrainingFlightTick=(i%4)!=0;FlashSlot_Advance(&g,1);FlashSlot_Advance(&v,1); }
 assert(gotenTicks==90&&otherTicks==120);
 /* Pending timeline jumps are processed even on a skipped decorative tick. */
 sTrainingFlightTick=0;goten.tl.state=FLASH_TL_GOTO;FlashSlot_Advance(&g,1);assert(gotenTicks==91);
 goten.tl.state=0;sTrainingSlowFlight=0;
 for(i=0;i<120;i++) { sTrainingFlightTick=(i%4)!=0;FlashSlot_Advance(&g,1); }
 assert(gotenTicks==211); /* native pace outside the scoped widescreen movie */
 /* A skipped timeline tick still renders a new fractional pose each frame. */
 goten.tl.slots=&child;goten.tl.depths=1;child.shape=&flying;
 flying.id=1;flying.mtx.m[2]=20;flying.mtx.m[5]=40;
 FlashTraining_SaveFlight(&goten);
 flying.mtx.m[2]=30;flying.mtx.m[5]=44;sTrainingGotenTree=1;
 for(i=0;i<4;i++) {
  sTrainingFlightPhase=i*0.25f;drawn=FlashTraining_FlightMatrix(&flying);
  assert(drawn.m[2]==20+i*2.5f);assert(drawn.m[5]==40+i);
  assert(flying.mtx.m[2]==30); /* rendering never changes the timeline */
 }
 flying.mtx.m[2]=-300;drawn=FlashTraining_FlightMatrix(&flying);
 assert(drawn.m[2]==-300); /* loop reset never sweeps through the page */
 goten.tl.state=FLASH_TL_GOTO;FlashTraining_SaveFlight(&goten);
 assert(sTrainingFlightPreviousCount==0);
 sTrainingGotenTree=0;drawn=FlashTraining_FlightMatrix(&flying);
 assert(drawn.m[2]==flying.mtx.m[2]);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='training-flight-') as tmp:
    tmp = Path(tmp)
    (tmp / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c99', '-DPORT', '-Iinclude', '-fsanitize=undefined',
                    str(tmp / 'test.c'), '-o', str(tmp / 'test')], cwd=root, check=True)
    subprocess.run([str(tmp / 'test')], check=True)
print('Training flight checks passed: scoped Goten pacing, normal guide timing, immediate seeks, interpolated poses and safe loop resets.')
