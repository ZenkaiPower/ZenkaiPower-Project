"""Check directional HUD poses; optionally compare native/Python extraction with --hud FILE."""
from pathlib import Path
import argparse
import io
import re
import struct
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[3]
view=(root/'android/app/src/main/java/com/zenkaipower/android/VirtualPadView.java').read_text()
method=re.search(r'    private static int analogFrame\(.*?\n    \}',view,re.S)[0]
code='class AnalogTest {\n'+method+r'''
    static void check(int frame,float x,float y) {
        if(analogFrame(x,y)!=frame) throw new AssertionError(x+","+y);
    }
    public static void main(String[] args) {
        check(0,0,0);check(0,.05f,-.05f);
        check(1,0,-1);check(2,1,-1);check(3,1,0);check(4,1,1);check(5,0,1);
        check(4,-1,1);check(3,-1,0);check(2,-1,-1);
        check(0,0,0); // release returns to neutral
        System.out.println("Analog pose checks passed: neutral, deadzone and eight directions");
    }
}
'''
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hud',type=Path)
args=parser.parse_args()
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp);java=tmp/'AnalogTest.java';java.write_text(code)
    subprocess.run(['java',str(java)],check=True)
    if args.hud:
        from PIL import Image
        sys.path.insert(0,str(root/'port/tools'))
        from android_buttons import analog_images
        header=(root/'android/app/src/main/cpp/installer_buttons.h').read_text().split('void android_buttons(')[0]
        cpp=r'''
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <vector>
#include "native.h"
using Uint64=uint64_t;using Uint32=uint32_t;using Bytes=std::vector<unsigned char>;
static void require(bool value,const char *why) {if(!value) throw std::runtime_error(why);}
struct View {
 const unsigned char *p;size_t n;
 explicit View(const Bytes &v):p(v.data()),n(v.size()) {}
 View(const unsigned char *v,size_t size):p(v),n(size) {}
 View sub(size_t at,size_t size) const {require(at<=n && size<=n-at,"bounds");return View(p+at,size);}
 uint32_t word(size_t at) const {uint32_t v;memcpy(&v,sub(at,4).p,4);return v;}
 unsigned char byte(size_t at) const {return *sub(at,1).p;}
};
#include "title_texture.h"
'''+header+r'''
int main(int argc,char **argv) {
 require(argc==2,"path");std::ifstream f(argv[1],std::ios::binary);
 Bytes bytes((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
 View hud(bytes);size_t at=hud.word(8)&~3u;View prompts=hud.sub(at,hud.n-at);
 for(int frame=0;frame<6;frame++) {
  NativeTitleLogo image=android_icon(prompts,frame+1);
  fwrite(&image.width,4,1,stdout);fwrite(&image.height,4,1,stdout);
  fwrite(image.pixels.data(),1,image.pixels.size(),stdout);
 }
}
'''
        source=tmp/'decode.cpp';exe=tmp/'decode';source.write_text(cpp)
        subprocess.run(['c++','-std=c++17','-I'+str(root/'port/setup'),str(source),'-o',str(exe)],check=True)
        decoded=subprocess.check_output([str(exe),str(args.hud.resolve())]);at=0
        for frame,(name,png) in enumerate(analog_images(args.hud.read_bytes())):
            width,height=struct.unpack_from('<ii',decoded,at);at+=8
            size=width*height*4
            with Image.open(io.BytesIO(png)) as image:
                assert image.size==(width,height)==(64,64)
                assert image.convert('RGBA').tobytes()==decoded[at:at+size],name
            at+=size
        assert at==len(decoded)
        print('All six analog artwork frames match byte-for-byte between native and Python extractors')
