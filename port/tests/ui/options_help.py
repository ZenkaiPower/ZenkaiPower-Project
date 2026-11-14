#!/usr/bin/env python3
"""Check Options help wrapping against proportional font measurements."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'include/menu/port_options_page.h').read_text()
a=s.index('static void Option_PortWrapHelp(')
b=s.index('static s32 Option_PortHelp(',a)
source=r'''
#include <assert.h>
#include <string.h>
#include "menu/port_video_options.h"
typedef int s32;typedef unsigned int u32;typedef unsigned short u16;typedef unsigned char u8;
int Font_GetWidth(u16 *text) {
    int width=0;
    for(;*text;text++) width+=*text=='W' ? 12:*text=='i' ? 3:*text==' ' ? 4:7;
    return width;
}
'''+s[a:b]+r'''
static void check(const char *text,int room) {
    u16 out[160];Option_PortWrapHelp(text,out,160,room);
    assert(out[0]==0xFEFF);
    int start=1;
    for(int i=1;;i++) {
        if(out[i]=='\n' || !out[i]) {
            u16 end=out[i];out[i]=0;
            assert(Font_GetWidth(out+start)<=room);
            out[i]=end;if(!end) break;start=i+1;
        }
    }
    /* Wrapping preserves every non-whitespace character. */
    int at=1;
    for(int i=0;text[i];i++) if(text[i]!=' ' && text[i]!='\n') {
        while(out[at]==' ' || out[at]=='\n') at++;
        assert(out[at++]==(unsigned char)text[i]);
    }
}
int main(void) {
    u16 translated[160];
    Option_PortWrapHelp("R\xC3\xA9glages",translated,160,240);
    assert(translated[1]=='R' && translated[2]==0xE9 && translated[3]=='g');
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        check(Port_OptionsDescriptor(row)->help,240);check(Port_OptionsDescriptor(row)->help,300);
    }
    check("Wide WWWW words and thin iiiii words.",90);
    check("WWWWWWWWWWWWWWWWWWWW",48);
    check("Keep this break.\nWrap the next long line too.",100);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'help.cpp';exe=Path(tmp)/'help';src.write_text(source)
    subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'include'),str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Options help proportional-width wrapping checks passed')
