"""Check resolution choices, render factors and screenshot sample bounds."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
source = r'''
#include <cassert>
#include "menu/port_video_options.h"
int main() {
 const PortVideoOption &option=sPortVideoOptions[0];
 assert(Port_OptionsCategoryRow(3,0)==21);
 assert(Port_OptionsCategoryRow(3,1)==19);
 assert(Port_OptionsCategoryRow(3,2)==20);
 for(int row=0;row<8;row++) assert(Port_OptionsCategoryRow(1,row)==8+row);
#ifdef __ANDROID__
 assert(sPortOptionCounts[0]==7);
 for(int row=0;row<7;row++) assert(Port_OptionsCategoryRow(0,row)==(row<6 ? row:7));
 assert(sPortVideoOptions[6].defaultValue==0);
 assert(option.count==6 && option.defaultValue==150);
 for(int i=0;i<6;i++) {
  float scale=Port_RenderScaleFactor(option.values[i]);
  assert(scale==(i+1)*0.5f);
  assert(Port_RenderScaleEncode(scale)==option.values[i]);
  int w=int(512*scale), h=int(448*scale);
  assert(w==256*(i+1) && h==224*(i+1));
  for(int y=0;y<448;y++) for(int x=0;x<512;x++) {
   int sx=int((x+0.5f)*scale), sy=int((y+0.5f)*scale);
   assert(sx>=0 && sx<w && sy>=0 && sy<h);
  }
 }
 assert(Port_RenderScaleFactor(0)==1);
 assert(Port_RenderScaleFactor(8)==3);
#else
 assert(sPortOptionCounts[0]==8);
 for(int row=0;row<8;row++) assert(Port_OptionsCategoryRow(0,row)==row);
 assert(option.count==9 && option.defaultValue==0);
 for(int i=0;i<9;i++) assert(option.values[i]==i);
#endif
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)
 (path/'test.cpp').write_text(source)
 for android in (False,True):
  command=['g++','-std=c++17','-fsanitize=undefined','-I'+str(root/'include')]
  if android: command+=['-D__ANDROID__']
  subprocess.run(command+[str(path/'test.cpp'),'-o',str(path/'test')],check=True)
  subprocess.run([str(path/'test')],check=True)
print('Desktop and Android resolution checks passed')
