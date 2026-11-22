"""Check production list submission captures the frame's game clock."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
core = (root / 'port/src/gs/gs_core.c').read_text()


def function(name):
    match = re.search(r'^static void ' + name + r'\([^\n]*\) \{.*?^\}', core, re.M | re.S)
    assert match, name
    return match[0]


code = r'''
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
unsigned gPortVBlanks, gGsFrameBlank;
static unsigned seenBlank;
static int sJob, sListDone, sJobTte;
static uint32_t sJobTadr;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sWake = PTHREAD_COND_INITIALIZER;
static void chain_block(void) {}
static void chain_end(void) {}
static void walk_chain(uint32_t tadr, int tte, void (*block)(void)) {
    (void)tadr; (void)tte; (void)block;
    seenBlank = gGsFrameBlank;
    gPortVBlanks += 9; /* The next game tick progresses while drawing. */
    assert(gGsFrameBlank == seenBlank);
}
''' + function('render_post') + '\n' + function('run_chain') + r'''
int main(void) {
    gPortVBlanks = 10;
    run_chain(123, 1);
    assert(seenBlank == 10 && gGsFrameBlank == 10 && gPortVBlanks == 19);
    gPortVBlanks = 11;
    render_post(2, 456, 0);
    assert(sJob == 2 && sJobTadr == 456 && !sJobTte && !sListDone);
    gPortVBlanks += 7; /* Worker starts after the submitting thread moves on. */
    walk_chain(sJobTadr, sJobTte, chain_block);
    assert(seenBlank == 11 && gGsFrameBlank == 11);
    puts("Frame clock checks passed: direct rendering and delayed threaded submission");
}
'''

with tempfile.TemporaryDirectory(prefix='interp-frame-clock-') as tmp:
    source, binary = Path(tmp) / 'test.c', Path(tmp) / 'test'
    source.write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-pthread',
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
