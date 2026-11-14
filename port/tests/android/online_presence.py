#!/usr/bin/env python3
"""Required-file checks cover packed, loose, overridden and broken AFS entries."""
from pathlib import Path
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as tmp:
    work=Path(tmp)
    source=work/'check.c'
    source.write_text('''#include "port_online_presence.h"
int main(int argc,char **argv) {
int counts[3]={1,2,3}; const char *loose[]={"disc/stream.bin"};
return Port_OnlineRequiredPresent(argv[1],counts,loose,1) ? 0:1;
}
''')
    exe=work/'check'
    subprocess.run(['cc','-fsanitize=undefined','-I'+str(root/'port/src'),str(source),'-o',str(exe)],check=True)
    for part,count in enumerate((1,2,3)):
        offset=8+count*8
        (work/f'pzs3us{part}.afs').write_bytes(b'AFS\0'+struct.pack('<I',count)+b''.join(struct.pack('<II',offset+i,1) for i in range(count))+bytes(count))
    (work/'disc').mkdir();(work/'disc/stream.bin').write_bytes(b'stream')
    def check(expected):
        assert subprocess.run([str(exe),str(work)],capture_output=True).returncode==expected
    check(0)
    packed=work/'pzs3us2.afs'; original=packed.read_bytes()
    packed.write_bytes(original[:-1]);check(1)
    override=work/'mods/pzs3us2/00002.bin';override.parent.mkdir(parents=True);override.write_bytes(b'override');check(0)
    override.unlink();check(1)
    loose=work/'pzs3us2/00002.bin';loose.parent.mkdir();loose.write_bytes(b'loose');check(0)
    loose.unlink();packed.write_bytes(original);check(0)
    (work/'disc/stream.bin').unlink();check(1)
print('Bulk online presence checks passed')
