"""Local/custom setup locations persist and preserve the flat save layout."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
CODE = r'''
#include "port/setup/locations.h"
#include "port/src/port_save.h"
#include <cassert>
#include <cstdlib>
static int zenkai;
extern "C" int Port_ZenkaiSelected(void) { return zenkai; }
int main(int argc,char **argv) {
    namespace fs=std::filesystem;
    std::string root=std::string(argv[1])+"/",error;
    std::string data=root+"custom data",saves=root+"custom saves";
    assert(Setup_DataRoot(root)==root+"gamedata/");
    assert(!strcmp(Port_SaveDirectory(),"savegame"));
    assert(Setup_SaveLocations(root,data,saves,error));
    assert(Setup_DataRoot(root)==data+"/");
    char value[2048],path[2048];
    assert(Port_LocationRead(root.c_str(),"saves",value,sizeof(value)) && value==saves);
    setenv("ZENKAIPOWER_SAVE_DIR",value,1);
    Port_SaveCardPath(path,sizeof(path),"BASLUS-21678DBZT3/BASLUS-21678DBZT3");
    assert(path==saves+"/game.sav");
    zenkai=1;
    Port_SaveCardPath(path,sizeof(path),"BASLUS-21678DBZT3/BASLUS-21678DBZT3");
    assert(path==saves+"/zenkaipower.sav");
    assert(Port_SaveUserFile("net_account.dat","savegame/net_account.dat")==saves+"/net_account.dat");
    setenv("ZENKAIPOWER_SAVES","legacy",1);
    Port_SaveCardPath(path,sizeof(path),"BASLUS-21678DBZT3/BASLUS-21678DBZT3");
    assert(!strcmp(path,"legacy/card1/BASLUS-21678DBZT3/game.sav"));
    unsetenv("ZENKAIPOWER_SAVES"); unsetenv("ZENKAIPOWER_SAVE_DIR");
    assert(!Setup_SaveLocations(root,"relative",saves,error));
    assert(Setup_DataRoot(root)==data+"/");
    assert(!Setup_SaveLocations(root,data+"\ninvalid",saves,error));
    assert(Setup_SaveLocations(root,"","",error));
    assert(Setup_DataRoot(root)==root+"gamedata/");
    Port_SaveCardPath(path,sizeof(path),"BASLUS-21678DBZT3/BASLUS-21678DBZT3");
    assert(!strcmp(path,"savegame/zenkaipower.sav"));
}
'''
with tempfile.TemporaryDirectory(prefix='storage-locations-') as tmp:
    directory = Path(tmp)
    source, executable = directory / 'test.cpp', directory / 'test'
    source.write_text(CODE)
    subprocess.run(['g++', '-std=c++17', '-fsanitize=address,undefined',
                    '-I' + str(ROOT), str(source), '-lSDL3', '-o', str(executable)], check=True)
    subprocess.run([str(executable), str(directory)], cwd=directory, check=True)
print('PASS: default/custom data, flat saves, persistence, legacy override, invalid paths, and return to Local')
