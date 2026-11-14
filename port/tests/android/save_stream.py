"""Build an Android save round-trip regression for running with run-as.

Package the executables temporarily as libzenkai_save_test.so and
libzenkai_save_old.so in the debug APK native-library directory; run them with
run-as from the installed nativeLibraryDir (Android blocks execution from /tmp).
Remove the test libraries before building distributable APKs.

The fixed binary must exit 0 in the app sandbox. The control uses libc tmpfile
and must fail there, demonstrating why ordinary storage permissions cannot fix
versioned save streams. Pass the app cache directory and a writable test parent.
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'port/tools'))
import android

out = root/'port/build/android/save-stream'
out.mkdir(parents=True, exist_ok=True)
source = r'''
#include "zenkai_crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
int main(int argc,char **argv) {
    char directory[1200],path[1280],options[1280],id[33],hash[65];
    unsigned char original[0x4000],readback[0x4000];
    FILE *file; int i;
    if(argc!=3 || setenv("ZENKAIPOWER_TMPDIR",argv[1],1)) return 2;
    snprintf(directory,sizeof(directory),"%s/.zenkai-save-test-XXXXXX",argv[2]);
    if(!mkdtemp(directory)) { perror("save test directory"); return 2; }
    snprintf(path,sizeof(path),"%s/zenkaipower.sav",directory);
    snprintf(options,sizeof(options),"%s/zenkaipower.sav.options",directory);
    for(i=0;i<sizeof(original);i++) original[i]=(unsigned char)(i*31+7);
    file=Port_ZenkaiOpen(path,0x202);
    if(!file) { perror("temporary save stream"); rmdir(directory); return 3; }
    if(fwrite(original,1,sizeof(original),file)!=sizeof(original) || !Port_ZenkaiFlush(path,file)) return 4;
    fclose(file);
    if(!Port_ZenkaiMetadata(path,id,hash)) return 5;
    file=Port_ZenkaiOpen(path,1);
    if(!file || fread(readback,1,sizeof(readback),file)!=sizeof(readback) || memcmp(original,readback,sizeof(original))) return 6;
    fclose(file);
    file=Port_ZenkaiOpen(options,0x202);
    if(!file || fwrite(original,1,512,file)!=512 || !Port_ZenkaiFlush(options,file)) return 7;
    fclose(file);
    file=Port_ZenkaiOpen(options,1);
    if(!file || fread(readback,1,512,file)!=512 || memcmp(original,readback,512)) return 8;
    fclose(file);
    unlink(path); unlink(options); rmdir(directory);
    puts("Unencrypted binary main save and Options companion: write/load/checksum passed");
    return 0;
}
'''
main = out/'test.c'
main.write_text(source)
production = root/'port/src/zenkai_crypto.c'
control = out/'libc-tmpfile.c'
control.write_text(production.read_text().replace('Port_PackTempFile()', 'tmpfile()'))
for name, crypto in [('fixed',production),('libc-tmpfile',control)]:
    subprocess.run([android.CLANG]+android.TARGET+['-O2','-I'+str(root/'port/src'),
                    str(main),str(crypto),'-o',str(out/name)],check=True)
    print(out/name)
