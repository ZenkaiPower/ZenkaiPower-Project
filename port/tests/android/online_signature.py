#!/usr/bin/env python3
"""Compare APK approval with the production Android runtime signature."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import zipfile

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'port/tools'))
from data_package import pack_directory
from make_online_signature import build_signature

with tempfile.TemporaryDirectory() as tmp:
    work=Path(tmp)
    libs=work/'lib'; libs.mkdir()
    engine=libs/'libzenkaipower.so'; engine.write_bytes(b'Android engine fixture')
    (libs/'libSDL3.so').write_bytes(b'Android SDL fixture')
    metadata=work/'engine'; metadata.mkdir()
    for name in ('mem','dat'):
        (metadata/f'libzenkaipower.so.{name}').write_bytes(name.encode())
    pack_directory(metadata,keep_loose=True)
    apk=work/'game.apk'
    with zipfile.ZipFile(apk,'w') as z:
        for p in libs.iterdir(): z.write(p,'lib/arm64-v8a/'+p.name)
        z.write(work/'engine.pak','assets/engine/setup/engine.pak')
    src=(root/'port/src/plat_verify.c').read_text()
    a=src.index('int Port_BuildSignature(')
    b=src.index('\nint Port_DataSignature(',a)
    c=work/'signature.c'
    c.write_text('''#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "gs/net_sign.h"
#define FILES 68605
static const char *Port_FileRoot(void) { return "."; }
static void rel_of(int n,char *out,size_t size) { snprintf(out,size,"fixture"); }
static int Port_PackExists(const char *path) { return 1; }
static int kVerifyCount[3];
static const char *kVerifyLoose[4];
static int Port_OnlineRequiredPresent(const char *root,const int *counts,const char *const *loose,int n) { return 1; }
'''+src[a:b]+'''\nint main(int argc,char **argv) {
char out[65]; if(!Port_BuildSignature(argv[1],out)) return 1; puts(out);return 0;
}\n''')
    exe=work/'test'
    subprocess.run(['cc','-D__ANDROID__','-I'+str(root/'port/src'),str(c),str(root/'port/src/gs/net_sign.c'),'-o',str(exe)],check=True)
    env=dict(os.environ,ZENKAIPOWER_ANDROID_METADATA=str(metadata)); env.pop('ZENKAIPOWER_FILE_ALIAS',None)
    result=subprocess.run([str(exe),str(engine)],env=env,capture_output=True,text=True,check=True)
    assert result.stdout.strip()==build_signature(apk)
    with zipfile.ZipFile(apk,'a') as z: z.writestr('unrelated.txt','APK metadata does not alter engine approval')
    assert result.stdout.strip()==build_signature(apk)
print('Android APK and runtime signatures match')
