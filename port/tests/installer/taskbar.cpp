// Run beside setup-generated bt3.exe and sm.exe; windows stay hidden.
#include "../../src/gs/title_taskbar.cpp"
#include <cassert>
#include <cstdio>
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO));
    for (bool japanese : {false,true}) {
        SDL_setenv_unsafe("ZENKAIPOWER_TITLE_LAUNCHER",japanese ? "sm" : "zenkaipower",1);
        SDL_Window *window=SDL_CreateWindow("Taskbar identity test",32,32,SDL_WINDOW_HIDDEN);
        assert(window);
        Port_TitleTaskbar(window);
        HWND hwnd=(HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
        assert(SendMessageW(hwnd,WM_GETICON,ICON_BIG,0));
        assert(SendMessageW(hwnd,WM_GETICON,ICON_SMALL,0));
        IPropertyStore *store=nullptr;
        assert(SUCCEEDED(SHGetPropertyStoreForWindow(hwnd,IID_IPropertyStore,(void **)&store)));
        PROPVARIANT value={};
        assert(SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID,&value)));
        assert(value.vt==VT_LPWSTR);
        assert(wcscmp(value.pwszVal,japanese ? L"ZenkaiPower.SparkingMeteor" : L"ZenkaiPower.BudokaiTenkaichi3")==0);
        PropVariantClear(&value);
        assert(SUCCEEDED(store->GetValue(PKEY_AppUserModel_RelaunchCommand,&value)));
        assert(value.vt==VT_LPWSTR && std::wstring(value.pwszVal).find(japanese ? L"sm.exe" : L"bt3.exe")!=std::wstring::npos);
        PropVariantClear(&value);
        store->Release(); SDL_DestroyWindow(window);
    }
    SDL_Quit();
    puts("PASS: both launcher taskbar identities, relaunch paths, and window icons");
}
