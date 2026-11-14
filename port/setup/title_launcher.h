#ifndef SETUP_TITLE_LAUNCHER_H
#define SETUP_TITLE_LAUNCHER_H
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

// The build embeds a small, disc-independent launcher as resource 200.
// Setup adds the player's extracted title artwork to a temporary copy.
static inline bool Setup_CreateTitleLauncher(const std::filesystem::path &root,bool japanese,DWORD *error=nullptr) {
    auto fail=[&](DWORD code) { if (error) *error=code; return false; };
    HMODULE module=GetModuleHandleW(nullptr);
    HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(200),MAKEINTRESOURCEW(10));
    if (!resource) return fail(GetLastError());
    HGLOBAL loaded=LoadResource(module,resource);
    const void *data=loaded ? LockResource(loaded) : nullptr;
    DWORD bytes=SizeofResource(module,resource);
    if (!data || !bytes) return fail(ERROR_RESOURCE_DATA_NOT_FOUND);
    std::ifstream source(root/(japanese ? L"sm.ico" : L"zenkaipower.ico"),std::ios::binary);
    std::vector<unsigned char> icon((std::istreambuf_iterator<char>(source)),{});
    auto word=[&](size_t at,int count) { uint32_t value=0; for(int i=0;i<count;i++) value|=(uint32_t)icon[at+i]<<(8*i); return value; };
    if (icon.size()<6 || word(0,2)!=0 || word(2,2)!=1) return fail(ERROR_INVALID_DATA);
    unsigned count=word(4,2);
    if (!count || count>256 || icon.size()<6+count*16) return fail(ERROR_INVALID_DATA);
    for(unsigned i=0;i<count;i++) {
        size_t entry=6+i*16; uint32_t length=word(entry+8,4),offset=word(entry+12,4);
        if (!length || offset>icon.size() || length>icon.size()-offset) return fail(ERROR_INVALID_DATA);
    }
    const auto target=root/(japanese ? L"sm.exe" : L"bt3.exe");
    const auto temp=root/(japanese ? L"sm.part.exe" : L"bt3.part.exe");
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return fail(GetLastError());
    DWORD written=0;
    bool ok=WriteFile(file,data,bytes,&written,nullptr) && written==bytes;
    DWORD code=ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!ok) { DeleteFileW(temp.c_str()); return fail(code); }
    HANDLE update=BeginUpdateResourceW(temp.c_str(),FALSE);
    if (!update) { code=GetLastError(); DeleteFileW(temp.c_str()); return fail(code); }
    std::vector<unsigned char> group(icon.begin(),icon.begin()+6);
    for(unsigned i=0;i<count && ok;i++) {
        size_t entry=6+i*16;
        group.insert(group.end(),icon.begin()+entry,icon.begin()+entry+12);
        group.push_back((unsigned char)(i+1)); group.push_back((unsigned char)((i+1)>>8));
        ok=UpdateResourceW(update,MAKEINTRESOURCEW(3),MAKEINTRESOURCEW(i+1),0,icon.data()+word(entry+12,4),word(entry+8,4))!=0;
    }
    if (ok) ok=UpdateResourceW(update,MAKEINTRESOURCEW(14),MAKEINTRESOURCEW(1),0,group.data(),(DWORD)group.size())!=0;
    code=ok ? ERROR_SUCCESS : GetLastError();
    if (!EndUpdateResourceW(update,!ok) && ok) { ok=false; code=GetLastError(); }
    if (ok && !MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { ok=false; code=GetLastError(); }
    if (!ok) DeleteFileW(temp.c_str());
    if (error) *error=code;
    return ok;
}
#endif
#endif
