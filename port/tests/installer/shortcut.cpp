// Synthetic icon encoding and Windows shell-link round trip; run in a scratch directory.
#include "../../src/gs/title_icon.h"
#include "../../setup/shortcut.h"
#include "../../setup/title_launcher.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
static unsigned read(const std::vector<unsigned char> &b,size_t at,int n) {
    unsigned v=0; for(int i=0;i<n;i++) v|=(unsigned)b.at(at+i)<<(8*i); return v;
}
int main(int argc, char **argv) {
#ifdef _WIN32
    if (argc == 3 && std::string(argv[1]) == "--create") {
        HRESULT error=S_OK;
        bool ok=Setup_CreateShortcut(std::filesystem::u8path(argv[2]),false,&error);
        printf("Shortcut creation: %s (0x%08lX)\n",ok ? "success" : "failed",(unsigned long)error);
        return ok ? 0 : 1;
    }
#endif
    unsigned char rgba[8*4*4]={0};
    for(int y=1;y<3;y++) for(int x=1;x<7;x++) {
        rgba[(y*8+x)*4]=255; rgba[(y*8+x)*4+3]=255;
    }
    auto icon=TitleIcon_Encode(rgba,8,4);
    assert(!icon.empty() && read(icon,0,2)==0 && read(icon,2,2)==1 && read(icon,4,2)==6);
    for(int i=0;i<6;i++) {
        size_t e=6+i*16,offset=read(icon,e+12,4),size=read(icon,e+8,4);
        assert(offset+size<=icon.size() && read(icon,offset,4)==40);
        unsigned n=icon[e]?icon[e]:256;
        assert(read(icon,offset+4,4)==n && read(icon,offset+8,4)==n*2);
    }
    assert(TitleIcon_Encode(nullptr,8,4).empty());
    unsigned char blank[16]={0}; assert(TitleIcon_Encode(blank,2,2).empty());
#ifdef _WIN32
    auto root=std::filesystem::current_path();
    FILE *f=fopen("zenkaipower.ico","wb"); assert(f);
    assert(fwrite(icon.data(),1,icon.size(),f)==icon.size()); assert(!fclose(f));
    HICON image=(HICON)LoadImageW(nullptr,(root/L"zenkaipower.ico").c_str(),IMAGE_ICON,32,32,LR_LOADFROMFILE);
    assert(image); DestroyIcon(image);
    DWORD launcherError=0;
    // Windows treats ZenkaiPower.exe and zenkaipower.exe as the same path.
    // Creating the title launcher must preserve the actual game executable.
    const std::string gameMarker="game executable must survive title launcher creation";
    { std::ofstream game(root/L"ZenkaiPower.exe",std::ios::binary); game<<gameMarker; }
    assert(Setup_CreateTitleLauncher(root,false,&launcherError));
    { std::ifstream game(root/L"ZenkaiPower.exe",std::ios::binary);
      assert(std::string((std::istreambuf_iterator<char>(game)),{})==gameMarker); }
    HMODULE launcher=LoadLibraryExW((root/L"bt3.exe").c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE);
    assert(launcher);
    assert(FindResourceW(launcher,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(14)));
    FreeLibrary(launcher);
    assert(Setup_CreateShortcut(root));
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    IShellLinkW *link=nullptr; IPersistFile *file=nullptr;
    assert(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void **)&link)));
    assert(SUCCEEDED(link->QueryInterface(IID_IPersistFile,(void **)&file)));
    assert(SUCCEEDED(file->Load((root/L"Budokai Tenkaichi 3.lnk").c_str(),STGM_READ)));
    wchar_t path[32768]; int index=-1;
    assert(SUCCEEDED(link->GetIconLocation(path,32768,&index)) && index==0);
    assert(std::filesystem::path(path)==root/L"bt3.exe");
    assert(SUCCEEDED(link->GetWorkingDirectory(path,32768)));
    assert(std::filesystem::path(path)==root);
    WIN32_FIND_DATAW data;
    assert(SUCCEEDED(link->GetPath(path,32768,&data,SLGP_RAWPATH)));
    assert(std::filesystem::equivalent(std::filesystem::path(path),root/L"bt3.exe"));
    file->Release(); link->Release(); CoUninitialize();
    std::filesystem::copy_file(root/L"zenkaipower.ico",root/L"sm.ico");
    assert(Setup_CreateTitleLauncher(root,true,&launcherError));
    assert(Setup_CreateShortcut(root,true));
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    assert(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void **)&link)));
    assert(SUCCEEDED(link->QueryInterface(IID_IPersistFile,(void **)&file)));
    assert(SUCCEEDED(file->Load((root/L"Sparking! METEOR.lnk").c_str(),STGM_READ)));
    assert(SUCCEEDED(link->GetIconLocation(path,32768,&index)) && index==0);
    assert(std::filesystem::path(path)==root/L"sm.exe");
    file->Release(); link->Release(); CoUninitialize();
#endif
#ifdef _WIN32
    /* SDL's worker may already have an MTA apartment. Do not require STA or
       uninitialize the caller's apartment when creating the shell link. */
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)));
    HRESULT error=E_FAIL;
    assert(Setup_CreateShortcut(std::filesystem::current_path(),false,&error));
    assert(SUCCEEDED(error));
    assert(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)==RPC_E_CHANGED_MODE);
    assert(!Setup_CreateShortcut(std::filesystem::current_path()/L"missing-folder",false,&error));
    assert(FAILED(error));
    CoUninitialize();
#endif
    puts("PASS: multi-size transparent ICO; Windows icon loading, shortcuts, existing COM apartment, and failure reporting");
}
