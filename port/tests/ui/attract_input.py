"""Check the attract battle's title-style any-button handling."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'src/battle/btl_seq.c').read_text()
start = text.index('static s32 BtlSeq_AttractSkip(void)')
function = text[start:text.index('\n}', start) + 2]
source = r'''
#include <assert.h>
typedef int s32;
struct { unsigned gamePressed; } gPad[2];
static int mode, any, reads, swallowed, keys;
int gPortOverlayOpen;
int Battle_GetMode(void) { return mode; }
int PortInput_AnyButtonPressed(void) { int value = any; any = 0; reads++; return value; }
void PortInput_Swallow(void) { swallowed++; }
void PortInput_SwallowKeys(void) { keys++; }
''' + function + r'''
int main(void) {
    mode = 0; any = 1;
    assert(!BtlSeq_AttractSkip() && !reads && any);
    mode = 7;
    assert(BtlSeq_AttractSkip() && swallowed == 1 && keys == 1);
    assert(!BtlSeq_AttractSkip());
    for (int side = 0; side < 2; side++) {
        gPad[side].gamePressed = 0x8000;
        assert(BtlSeq_AttractSkip());
        gPad[side].gamePressed = 0;
    }
    any = 1; gPortOverlayOpen = 1;
    assert(!BtlSeq_AttractSkip() && !any && swallowed == 3);
    gPortOverlayOpen = 0;
    assert(!BtlSeq_AttractSkip());
    for (mode = 0; mode < 9; mode++) {
        if (mode == 7) continue;
        any = 1; gPad[0].gamePressed = 0xffff;
        assert(!BtlSeq_AttractSkip() && any && swallowed == 3);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='attract-input-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(source)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Werror', '-fsanitize=undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Attract input checks passed: any key/click, both pads, overlays and mode isolation')
