#ifndef SETUP_SHORTCUT_H
#define SETUP_SHORTCUT_H
#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <propkey.h>
#include <filesystem>
/* Worker-thread COM lifetime; all paths are Unicode and fully qualified. */
static inline bool Setup_CreateShortcut(const std::filesystem::path &folder, bool japanese=false, HRESULT *error=nullptr, bool desktop=false) {
    const auto root=std::filesystem::absolute(folder);
    const wchar_t *name=japanese ? L"Sparking! METEOR" : L"Budokai Tenkaichi 3";
    const auto launcher=root/(japanese ? L"sm.exe" : L"bt3.exe");
    HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    // SDL may have initialized this worker's COM apartment already. Shell links
    // can use that apartment; only balance initialization that we own.
    if (FAILED(init) && init!=RPC_E_CHANGED_MODE) {
        if (error) *error=init;
        return false;
    }
    IShellLinkW *link=nullptr; IPersistFile *file=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void **)&link);
    if (SUCCEEDED(hr)) hr=link->SetPath(launcher.c_str());
    if (SUCCEEDED(hr)) hr=link->SetWorkingDirectory(root.c_str());
    if (SUCCEEDED(hr)) hr=link->SetIconLocation(launcher.c_str(),0);
    if (SUCCEEDED(hr)) hr=link->SetDescription(name);
    if (SUCCEEDED(hr)) {
        IPropertyStore *store=nullptr;
        hr=link->QueryInterface(IID_IPropertyStore,(void **)&store);
        if (SUCCEEDED(hr)) {
            PROPVARIANT value={}; value.vt=VT_LPWSTR;
            value.pwszVal=const_cast<wchar_t *>(japanese ? L"ZenkaiPower.SparkingMeteor" : L"ZenkaiPower.BudokaiTenkaichi3");
            hr=store->SetValue(PKEY_AppUserModel_ID,value);
            if (SUCCEEDED(hr)) hr=store->Commit();
            store->Release();
        }
    }
    if (SUCCEEDED(hr)) hr=link->QueryInterface(IID_IPersistFile,(void **)&file);
    auto destination=root;
    if (SUCCEEDED(hr) && desktop) {
        wchar_t desktopPath[MAX_PATH]={};
        hr=SHGetFolderPathW(nullptr,CSIDL_DESKTOPDIRECTORY,nullptr,SHGFP_TYPE_CURRENT,desktopPath);
        if (SUCCEEDED(hr)) destination=desktopPath;
    }
    if (SUCCEEDED(hr)) hr=file->Save((destination/(std::wstring(name)+L".lnk")).c_str(),TRUE);
    if (file) file->Release();
    if (link) link->Release();
    if (SUCCEEDED(init)) CoUninitialize();
    if (error) *error=hr;
    return SUCCEEDED(hr);
}
#endif
#endif
