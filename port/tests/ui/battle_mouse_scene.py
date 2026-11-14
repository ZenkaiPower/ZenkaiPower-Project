#!/usr/bin/env python3
"""Exercise the production intro/winner update functions with no skip callback."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'src/battle/btl_seq.c').read_text()
def function(signature):
    start = text.index(signature)
    return text[start:text.index('\n}\n', start) + 3]

source = r'''
#include <assert.h>
#include <stddef.h>
typedef int s32;
typedef unsigned u32;
#define PADG_CROSS 0x200
#define PADG_START 0x1000
struct { u32 gamePressed; } gPad[2];
typedef struct { int step, side[2], skip, timer; int (*poll)(void); } BtlSeqTalkCtx;
static int stopped, fades, aborted;
int BtlCtrl_IsPoseReached(int side) { return 0; }
int Voice_IsStopped(int side) { return 0; }
int Ramp_Step(void *timer) { return 0; }
void ScrXfade_RequestCapture(void) { fades++; }
void ScrXfade_Start(int direction,float seconds) {}
int BtlScript_IsEventRunning(void) { return 1; }
void BtlScript_AbortEvents(void) { aborted++; }
void BtlSeq_StopTalk(void) { stopped++; }
'''
source += function('static s32 BtlSeq_MouseSkip(void)')
source += function('s32 BtlSeqIntroTalk_Update(BtlSeqTalkCtx *ctx)')
source += function('s32 BtlSeqWinTalk_Update(BtlSeqTalkCtx *ctx)')
source += r'''
int main(void) {
    BtlSeqTalkCtx scene={0};
    assert(BtlSeqIntroTalk_Update(&scene)==1 && !stopped);
    gPad[0].gamePressed=PADG_CROSS|PADG_START;
    assert(BtlSeqIntroTalk_Update(&scene)==2 && stopped==1 && aborted==1 && fades==1);
    scene.step=99; scene.skip=0;
    assert(BtlSeqIntroTalk_Update(&scene)==2); /* story mode's wait for skip */
    assert(BtlSeqWinTalk_Update(&scene)==6);
    gPad[0].gamePressed=0; gPad[1].gamePressed=PADG_CROSS|PADG_START;
    scene.step=0;
    assert(BtlSeqIntroTalk_Update(&scene)==2); /* online/second pad */
    assert(BtlSeqWinTalk_Update(&scene)==6);
    gPad[1].gamePressed=PADG_CROSS;
    assert(BtlSeqIntroTalk_Update(&scene)==1); /* ordinary confirm is not a scene skip */
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'scene.c'
    exe = Path(tmp) / 'scene'
    src.write_text(source)
    subprocess.run(['cc', '-std=c99', '-DPORT=1', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Battle mouse scene checks passed')
