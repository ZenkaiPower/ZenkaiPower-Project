#ifndef PORT_REMOVE_EMPTY_DIRS_H
#define PORT_REMOVE_EMPTY_DIRS_H
/* Setup-only cleanup. Never follow directory links or remove ordinary files. */
#include <SDL3/SDL.h>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif
static inline bool Port_RealDirectory(const std::string &path) {
#ifdef _WIN32
    int size=MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,NULL,0);
    if (!size) return false;
    std::wstring wide((size_t)size,L'\0');
    if (!MultiByteToWideChar(CP_UTF8,0,path.c_str(),-1,&wide[0],size)) return false;
    DWORD attr=GetFileAttributesW(wide.c_str());
    return attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY) && !(attr&FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat st;
    return lstat(path.c_str(),&st)==0 && S_ISDIR(st.st_mode);
#endif
}
static inline void Port_RemoveEmptyChildren(const std::string &root);
static inline SDL_EnumerationResult Port_EmptyDirVisit(void *,const char *dir,const char *name) {
    std::string path=std::string(dir)+name;
    if (Port_RealDirectory(path)) {
        Port_RemoveEmptyChildren(path);
        // RemovePath fails harmlessly when any file or directory remains.
        SDL_RemovePath(path.c_str());
    }
    return SDL_ENUM_CONTINUE;
}
static inline void Port_RemoveEmptyChildren(const std::string &root) {
    if (Port_RealDirectory(root)) SDL_EnumerateDirectory(root.c_str(),Port_EmptyDirVisit,NULL);
}
#endif
