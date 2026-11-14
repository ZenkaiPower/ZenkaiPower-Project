#include <windows.h>
#include <string>
#include <vector>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    std::vector<wchar_t> path(32768);
    DWORD size=GetModuleFileNameW(nullptr,path.data(),(DWORD)path.size());
    if (!size || size>=path.size()) return 1;
    std::wstring folder(path.data(),size);
    folder=folder.substr(0,folder.find_last_of(L"\\/"));
    std::wstring launcher(path.data(),size);
    size_t slash=launcher.find_last_of(L"\\/");
    bool japanese=_wcsicmp(launcher.c_str()+slash+1,L"sm.exe")==0;
    SetEnvironmentVariableW(L"ZENKAIPOWER_TITLE_LAUNCHER",japanese ? L"sm" : L"zenkaipower");
    std::wstring program=folder+L"\\ZenkaiPower.exe";
    std::wstring command=L"\""+program+L"\"";
    if (arguments && *arguments) command+=L" "+std::wstring(arguments);
    STARTUPINFOW startup={}; startup.cb=sizeof(startup);
    PROCESS_INFORMATION process={};
    if (!CreateProcessW(program.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,
                        folder.c_str(),&startup,&process)) {
        MessageBoxW(nullptr,L"Could not start ZenkaiPower.exe. Keep the launcher beside the game executable.",
                    L"ZenkaiPower",MB_OK|MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return 0;
}
