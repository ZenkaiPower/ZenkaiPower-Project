"""Check that online reporting retains the source save's Classic Mode."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
text = (root / 'port/src/gs/net.c').read_text()
start = text.index('int Port_NetClassicMode(void)')
function = text[start:text.index('\n}', start) + 2]
source = r'''
#include "sys/port_zenkai_pack.h"
#include <assert.h>
static unsigned char save[0x4000];
static void *gSaveData = save;
static int active, isolated, sSourceClassic;
int Port_ZenkaiEnabled(void) { return active; }
char *getenv(const char *name) { return isolated ? "net_session" : 0; }
''' + function + r'''
int main(void) {
    active = 1;
    assert(!Port_NetClassicMode()); /* Legacy save. */
    Port_ZenkaiPackWriteFlags(save, PORT_ZENKAI_PACK_DISABLED);
    assert(Port_NetClassicMode());
    active = 0;
    assert(!Port_NetClassicMode()); /* Original-mode save. */
    active = 1;
    sSourceClassic = Port_NetClassicMode();
    isolated = 1; active = 0;
    Port_ZenkaiPackWriteFlags(save, 0); /* Temporary online save. */
    assert(Port_NetClassicMode());
    sSourceClassic = 0;
    Port_ZenkaiPackWriteFlags(save, PORT_ZENKAI_PACK_DISABLED);
    assert(!Port_NetClassicMode());
    isolated = 0;
    assert(!Port_NetClassicMode());
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='classic-report-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(source)
    subprocess.run(['gcc', '-std=c99', '-fsanitize=undefined', '-Iinclude',
                    str(path / 'test.c'), '-o', str(path / 'test')], cwd=root, check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Classic Mode reporting checks passed')
