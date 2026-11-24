"""Exercise the production swipe-to-wheel path without Android or a display."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'port/src/gs/gs_input.c').read_text()
a=source.index('void PortMenuMouse_ReadScroll(')
b=source.index('\n}\n',a)+3
harness=r'''#include <assert.h>
#include <math.h>
#include <string.h>
#include "menu/port_menu_mouse.h"
static PortMenuMouse input;
void PortMenuMouse_Read(PortMenuMouse *out) {*out=input;}
'''+source[a:b]+r'''
static PortMenuMouse read(float x,float y) {
    PortMenuMouse out;input.x=x;input.y=y;input.moved=1;
    PortMenuMouse_ReadScroll(&out);input.dragStart=0;return out;
}
static void begin(void) {
    memset(&input,0,sizeof(input));input.dragX=100;input.dragY=100;
    input.dragStart=input.dragHeld=1;
}
int main(void) {
    PortMenuMouse out;
    input.confirm=1;out=read(100,100);assert(out.confirm && out.moved && !out.wheel);
    begin();out=read(100,90);assert(!out.wheel && !out.confirm && !out.moved);
    out=read(100,76);assert(out.wheel==1 && out.swipeAxis==2); /* finger up, same as wheel down */
    out=read(100,76);assert(!out.wheel); /* no auto-repeat while held */
    out=read(100,64);assert(!out.wheel);
    out=read(100,70);assert(!out.wheel); /* reversal drops old partial */
    out=read(100,88);assert(out.wheel==-1);
    input.dragHeld=0;input.dragEnd=1;out=read(100,112);assert(out.wheel==-1);
    input.dragEnd=0;out=read(100,112);assert(!out.wheel && out.moved);
    begin();out=read(76,98);assert(out.wheel==-1 && out.swipeAxis==1); /* left swipe moves focus left */
    out=read(76,20);assert(!out.wheel); /* direction stays locked */
    out=read(-200,20);assert(out.wheel==-1); /* one step per read, no queued burst */
    out=read(-170,20);assert(out.wheel==1); /* right swipe moves focus right */
    input.dragHeld=0;input.dragEnd=0;out=read(-170,20);assert(!out.wheel);
    begin();out=read(100,90);input.dragHeld=0;input.dragEnd=1;
    out=read(100,80);assert(!out.wheel); /* short drag never confirms */
    input.dragEnd=0;input.wheel=-1;out=read(100,80);assert(out.wheel==-1);
    memset(&input,0,sizeof(input));PortMenuMouse_ReadScroll(&out);assert(!out.wheel);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp);c=tmp/'scroll.c';c.write_text(harness);exe=tmp/'scroll'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-I'+str(root/'include'),str(c),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Touch menu scrolling: vertical/horizontal swipes, threshold, direction lock, reversal, release and mouse wheel passed')
