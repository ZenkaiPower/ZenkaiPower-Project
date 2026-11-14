// Console child must have no console, while pipes and nonzero status survive.
#include "../../src/port_process.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(int argc,char **argv) {
    if (argc>1) {
#ifdef _WIN32
        if (GetConsoleWindow()!=NULL) return 98;
#endif
        for(int i=0;i<16384;i++) fputs("helper output\n",stdout);
        fputs("helper error\n",stderr);
        return 7;
    }
    const char *args[]={argv[0],"child",NULL};
    SDL_PropertiesID props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)args);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN,true);
    SDL_Process *child=Port_CreateHiddenProcess(props); assert(child);
    size_t size=0; int code=-1;
    char *out=(char *)Port_ReadProcess(child,&size,&code);
    assert(out && size>200000 && strstr(out,"helper error") && code==7);
    SDL_free(out);
    assert(Port_WaitProcess(child,false,&code) && code==7);
    SDL_DestroyProcess(child);
    // Polling path used by Wii extraction also keeps the real exit status.
    child=Port_CreateHiddenProcess(props); assert(child);
    SDL_IOStream *stream=SDL_GetProcessOutput(child);
    for (;;) {
        char bytes[4096];
        SDL_ReadIO(stream,bytes,sizeof(bytes));
        if (Port_WaitProcess(child,false,&code)) break;
        SDL_Delay(1);
    }
    assert(code==7); SDL_DestroyProcess(child);
    const char *bad[]={"nonexistent-zenkai-test-program",NULL};
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)bad);
    assert(!Port_CreateHiddenProcess(props));
    SDL_DestroyProperties(props);
    puts("PASS: helpers have no console; captured output, errors, polling and exit codes preserved");
}
