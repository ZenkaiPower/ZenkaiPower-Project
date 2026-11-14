#ifndef PORT_PROCESS_H
#define PORT_PROCESS_H
#include <SDL3/SDL.h>
#ifdef _WIN32
#include <windows.h>
/* SDL 3.4 hides Windows console windows only for background processes, whose
 * public exit code is always zero. Keep a process handle for the real status. */
#define PORT_PROCESS_HANDLE "ZenkaiPower.process.handle"
static inline void SDLCALL Port_CloseProcessHandle(void *unused, void *handle) {
    (void)unused;
    CloseHandle((HANDLE)handle);
}
#endif
static inline SDL_Process *Port_CreateHiddenProcess(SDL_PropertiesID props) {
#ifdef _WIN32
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,true);
#endif
    SDL_Process *process=SDL_CreateProcessWithProperties(props);
#ifdef _WIN32
    if (process) {
        SDL_PropertiesID state=SDL_GetProcessProperties(process);
        DWORD pid=(DWORD)SDL_GetNumberProperty(state,SDL_PROP_PROCESS_PID_NUMBER,0);
        HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        if (!handle) {
            DWORD error=GetLastError();
            SDL_KillProcess(process,true);
            SDL_DestroyProcess(process);
            SDL_SetError("Cannot track helper process exit status (Windows error %lu)",(unsigned long)error);
            return NULL;
        }
        if (!SDL_SetPointerPropertyWithCleanup(state,PORT_PROCESS_HANDLE,handle,Port_CloseProcessHandle,NULL)) {
            SDL_KillProcess(process,true);
            SDL_DestroyProcess(process);
            return NULL;
        }
    }
#endif
    return process;
}
static inline bool Port_WaitProcess(SDL_Process *process,bool block,int *code) {
    if (!SDL_WaitProcess(process,block,code)) return false;
#ifdef _WIN32
    if (code) {
        HANDLE handle=(HANDLE)SDL_GetPointerProperty(SDL_GetProcessProperties(process),PORT_PROCESS_HANDLE,NULL);
        DWORD status;
        if (!handle || !GetExitCodeProcess(handle,&status)) {
            *code=-1;
            SDL_SetError("Cannot read helper process exit status");
        } else *code=(int)status;
    }
#endif
    return true;
}
static inline void *Port_ReadProcess(SDL_Process *process,size_t *size,int *code) {
    void *data=SDL_ReadProcess(process,size,code);
    if (code) Port_WaitProcess(process,true,code);
    return data;
}
#endif
