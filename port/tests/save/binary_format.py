"""Check Python/C binary save compatibility without player files or game data."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'port/tools'))
import generate_zenkai_save as generator

with tempfile.TemporaryDirectory(prefix='binary-save-') as temporary:
    directory = Path(temporary)
    save, _ = generator.fresh_save(None)
    envelope = generator.seal_save(bytes(save))
    assert envelope[:8] == b'ZENKAI\x00\x03'
    assert len(envelope) == len(save) + 88
    assert envelope[88:] == bytes(save)
    assert generator.verify_envelope(envelope) == save
    for offset in (7, 10, 24, 56, 100):
        damaged = bytearray(envelope)
        damaged[offset] ^= 1
        try:
            generator.verify_envelope(bytes(damaged))
        except ValueError:
            pass
        else:
            raise AssertionError('damaged envelope accepted')
    target = directory / 'zenkaipower.sav'
    target.write_bytes(envelope)
    source = directory / 'verify.c'
    binary = directory / 'verify'
    source.write_text(r'''
#include "zenkai_crypto.h"
#include <assert.h>
#include <string.h>
int main(int argc, char **argv) {
    unsigned char payload[0x4000]; char id[33],hash[65];
    assert(argc==2 && Port_ZenkaiMetadata(argv[1],id,hash));
    FILE *f=Port_ZenkaiOpen(argv[1],1); assert(f);
    assert(fread(payload,1,sizeof(payload),f)==sizeof(payload) && fgetc(f)==EOF);
    assert(Port_ZenkaiFlush(argv[1],f)); fclose(f);
    return 0;
}
''')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-Iport/src',
                    str(source), 'port/src/zenkai_crypto.c', '-o', str(binary)], cwd=root, check=True)
    subprocess.run([str(binary), str(target)], check=True)
    assert target.read_bytes() == envelope
    # The checked-in starter uses the same format and keeps the intended roster.
    starter = (root / 'misc/startsave/zenkaipower.sav').read_bytes()
    generator.verify_envelope(starter)
    assert hashlib.blake2b(starter[88:], digest_size=32).digest() == starter[24:56]
print('Binary saves: Python/C format agreement, checksums, stable rewrites and starter roster passed')
