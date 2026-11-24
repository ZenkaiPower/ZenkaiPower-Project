"""Native element flags and exact texture recognition; optional installed HUD fixture."""
from pathlib import Path
import argparse
import subprocess
import tempfile
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'port/tools'))
import menu_font
parser = argparse.ArgumentParser()
parser.add_argument('--file', type=Path)
args = parser.parse_args()
source = r'''
#define _POSIX_C_SOURCE 200809L
#include "gs_native_elements.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    assert(ZenkaiNative_Enabled()==!!ZENKAIPOWER_NATIVE_ELEMENTS);
    assert(ZenkaiNative_HudShapes()==!!(ZENKAIPOWER_NATIVE_ELEMENTS && ZENKAIPOWER_NATIVE_HUD_SHAPES));
    uint32_t neutral[48];assert(ZenkaiNative_Palette(NULL,1,neutral)==1);
    assert(!ZenkaiNative_Identify(NULL,64,64,1));
    assert(!ZenkaiNative_TimerDigits(NULL,128,128,1));
    unsigned char pixels[256*64*4]={0};
    assert(!ZenkaiNative_Identify(pixels,64,64,1));
    if(argc>1) {
        FILE *fp=fopen(argv[1],"rb");assert(fp);
        for(int id=1;id<=7;id++) {
            int width=id==1 ? 64 : 256;
            size_t bytes=(size_t)width*64*4;
            assert(fread(pixels,1,bytes,fp)==bytes);
            assert(ZenkaiNative_Identify(pixels,width,64,1)==id);
            uint32_t palette[48];
            int entries=ZenkaiNative_Palette((const uint32_t *)pixels,id,palette);
            assert(entries==(id==1 ? 1 : 48));
            if(id==1) assert(palette[0]==0xffffffffu);
            else {
                assert(palette[0]==((const uint32_t *)pixels)[100]);
                assert(palette[18]==((const uint32_t *)pixels)[175]);
                assert(palette[24]==((const uint32_t *)pixels)[16*256+4]);
                assert(palette[47]==((const uint32_t *)pixels)[31*256+18]);
            }
            assert(!ZenkaiNative_Identify(pixels,width,64,0));
            assert(!ZenkaiNative_Identify(pixels,width,32,1));
            pixels[10]^=1;
            assert(!ZenkaiNative_Identify(pixels,width,64,1)); /* modified art falls back */
        }
        for(int variant=0;variant<3;variant++) {
            assert(fread(pixels,1,128*128*4,fp)==128*128*4);
            assert(ZenkaiNative_TimerDigits(pixels,128,128,1));
            assert(!ZenkaiNative_TimerDigits(pixels,128,128,0));
            pixels[0]^=1;assert(!ZenkaiNative_TimerDigits(pixels,128,128,1));
        }
        assert(fgetc(fp)==EOF);fclose(fp);
    }
    puts("Native elements: compiled master/individual switches and safe artwork recognition passed");
}
'''
with tempfile.TemporaryDirectory(prefix='native-elements-') as tmp:
    tmp = Path(tmp)
    c = tmp/'test.c'
    c.write_text(source)
    binary = tmp/'test'
    subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'port/src/gs'),str(c),'-o',str(binary)],check=True)
    command = [str(binary)]
    if args.file:
        bank = menu_font.section(args.file.read_bytes(),1)
        fixture = tmp/'hud.rgba'
        fixture.write_bytes(menu_font.texture(bank,3)+menu_font.texture(bank,8)+
                            b''.join(menu_font.texture(bank,8,p) for p in range(9,14))+
                            b''.join(menu_font.texture(bank,0,p) for p in range(3)))
        command.append(str(fixture))
    for master in range(2):
        for hud in range(2):
            subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',
                            '-DZENKAIPOWER_NATIVE_ELEMENTS='+str(master),
                            '-DZENKAIPOWER_NATIVE_HUD_SHAPES='+str(hud),
                            '-I'+str(root/'port/src/gs'),str(c),'-o',str(binary)],check=True)
            subprocess.run(command,check=True)
