#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif
#include <SDL3/SDL.h>
#include "ui.h"
#if defined(__linux__)
#include "menu_labels.h"
#include <string>
#include <unistd.h>
extern "C" const char *Port_FileRoot(void);
extern "C" int Port_TitleLogoEnabled(void);

static std::string desktop_value(const std::string &text,bool command=false) {
    std::string result;
    for(char c:text) {
        if (c=='\n') result+="\\n";
        else if (c=='\r') result+="\\r";
        else if (c=='\t') result+="\\t";
        else if (c=='\\') result+=command ? "\\\\\\\\" : "\\\\";
        else if (command && (c=='\"' || c=='$' || c=='`')) { result+="\\\\"; result+=c; }
        else if (command && c=='%') result+="%%";
        else result+=c;
    }
    return result;
}

static void linux_desktop_icon(SDL_Surface *icon) {
    const char *xdg=SDL_getenv("XDG_DATA_HOME"),*home=SDL_getenv("HOME");
    if ((!xdg || xdg[0]!='/') && (!home || home[0]!='/')) return;
    std::string data=xdg && xdg[0]=='/' ? xdg : std::string(home)+"/.local/share";
    std::string apps=data+"/applications",icons=data+"/icons/hicolor/256x256/apps";
    if (!SDL_CreateDirectory(apps.c_str()) || !SDL_CreateDirectory(icons.c_str())) return;
    std::string png=icons+"/org.zenkaipower.Game.png";
    if (!SDL_SavePNG(icon,(png+".part").c_str()) || !SDL_RenamePath((png+".part").c_str(),png.c_str())) return;
    char exe[4096];
    ssize_t length=readlink("/proc/self/exe",exe,sizeof(exe)-1);
    if (length<=0 || length>=(ssize_t)sizeof(exe)-1) return;
    exe[length]=0;
    std::string program=exe;
    std::string entry="[Desktop Entry]\nType=Application\nName="+desktop_value(Ui_GameTitle())+"\nExec=\""+
        desktop_value(program,true)+"\"\nPath="+desktop_value(program.substr(0,program.find_last_of('/')))+
        "\nIcon="+desktop_value(png)+"\nStartupWMClass=org.zenkaipower.Game\nTerminal=false\nCategories=Game;\n";
    std::string target=apps+"/org.zenkaipower.Game.desktop",temp=target+".part";
    SDL_IOStream *file=SDL_IOFromFile(temp.c_str(),"wb");
    if (!file) return;
    bool written=SDL_WriteIO(file,entry.data(),entry.size())==entry.size();
    bool closed=SDL_CloseIO(file);
    if (!written || !closed || !SDL_RenamePath(temp.c_str(),target.c_str())) SDL_RemovePath(temp.c_str());
}
#endif

extern "C" void Port_TitleTaskbarPrepare(void) {
    SDL_SetAppMetadata(Ui_GameTitle(),nullptr,"org.zenkaipower.Game");
#if defined(__linux__)
    SDL_SetHint(SDL_HINT_APP_ID,"org.zenkaipower.Game");
#endif
}
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <string>
#include <vector>

static void taskbar_string(IPropertyStore *store, REFPROPERTYKEY key, const wchar_t *text) {
    PROPVARIANT value={}; value.vt=VT_LPWSTR; value.pwszVal=const_cast<wchar_t *>(text);
    store->SetValue(key,value);
}
#endif

extern "C" void Port_TitleTaskbar(SDL_Window *window) {
#ifdef _WIN32
    const char *title=SDL_getenv("ZENKAIPOWER_TITLE_LAUNCHER");
    if (!window || !title || (SDL_strcmp(title,"zenkaipower") && SDL_strcmp(title,"sm"))) return;
    bool japanese=SDL_strcmp(title,"sm")==0;
    std::vector<wchar_t> path(32768);
    DWORD size=GetModuleFileNameW(nullptr,path.data(),(DWORD)path.size());
    if (!size || size>=path.size()) return;
    std::wstring launcher(path.data(),size);
    launcher=launcher.substr(0,launcher.find_last_of(L"\\/")+1)+(japanese ? L"sm.exe" : L"bt3.exe");
    const wchar_t *identity=japanese ? L"ZenkaiPower.SparkingMeteor" : L"ZenkaiPower.BudokaiTenkaichi3";
    SetCurrentProcessExplicitAppUserModelID(identity);
    HWND hwnd=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
    if (!hwnd) return;
    IPropertyStore *store=nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(hwnd,IID_IPropertyStore,(void **)&store))) {
        std::wstring command=L"\""+launcher+L"\"";
        taskbar_string(store,PKEY_AppUserModel_RelaunchCommand,command.c_str());
        const char *display=Ui_GameTitle();
        int chars=MultiByteToWideChar(CP_UTF8,0,display,-1,nullptr,0);
        std::vector<wchar_t> displayName(chars>0 ? chars:1);
        if(chars>0 && MultiByteToWideChar(CP_UTF8,0,display,-1,displayName.data(),chars))
            taskbar_string(store,PKEY_AppUserModel_RelaunchDisplayNameResource,displayName.data());
        taskbar_string(store,PKEY_AppUserModel_RelaunchIconResource,(launcher+L",0").c_str());
        taskbar_string(store,PKEY_AppUserModel_ID,identity);
        store->Commit(); store->Release();
    }
    // Keep the handles alive for the lifetime of the window.
    static HICON large=nullptr,small=nullptr;
    HICON nextLarge=nullptr,nextSmall=nullptr;
    if (ExtractIconExW(launcher.c_str(),0,&nextLarge,&nextSmall,1)) {
        SendMessageW(hwnd,WM_SETICON,ICON_BIG,(LPARAM)nextLarge);
        SendMessageW(hwnd,WM_SETICON,ICON_SMALL,(LPARAM)nextSmall);
        if (large) DestroyIcon(large);
        if (small) DestroyIcon(small);
        large=nextLarge; small=nextSmall;
    }
#elif defined(__linux__)
    if (!window) return;
    int width=0,height=0;
    unsigned char *rgba=Port_TitleLogoEnabled() ? MenuLabels_MeteorLogo(Port_FileRoot(),&width,&height) : nullptr;
    if (!rgba) rgba=MenuLabels_TitleLogo(Port_FileRoot(),&width,&height);
    if (!rgba) return;
    SDL_Surface *source=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,rgba,width*4);
    SDL_Surface *icon=SDL_CreateSurface(256,256,SDL_PIXELFORMAT_RGBA32);
    if (source && icon) {
        SDL_ClearSurface(icon,0,0,0,0);
        int w=width>=height ? 256 : SDL_max(1,256*width/height);
        int h=height>=width ? 256 : SDL_max(1,256*height/width);
        SDL_Rect destination={(256-w)/2,(256-h)/2,w,h};
        SDL_SetSurfaceBlendMode(source,SDL_BLENDMODE_NONE);
        if (SDL_BlitSurfaceScaled(source,nullptr,icon,&destination,SDL_SCALEMODE_LINEAR)) {
            SDL_SetWindowIcon(window,icon);
            linux_desktop_icon(icon);
        }
    }
    SDL_DestroySurface(icon); SDL_DestroySurface(source); SDL_free(rgba);
#else
    (void)window;
#endif
}
