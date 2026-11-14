#ifndef SETUP_LAUNCH_H
#define SETUP_LAUNCH_H
#include <SDL3/SDL.h>
#include <string>

// Resolve at click time: title launchers may have been created during this run.
static inline std::string Setup_PlayProgram(const std::string &root,const std::string &fallback) {
#ifdef _WIN32
    for (const char *name : {"sm.exe","bt3.exe"}) {
        std::string path=root+name;
        SDL_PathInfo info;
        if (SDL_GetPathInfo(path.c_str(),&info) && info.type==SDL_PATHTYPE_FILE && info.size>0) return path;
    }
#endif
    return fallback;
}

static inline bool Setup_LaunchGame(const std::string &root,const std::string &fallback) {
    std::string program=Setup_PlayProgram(root,fallback);
    const char *args[]={program.c_str(),nullptr};
    SDL_PropertiesID props=SDL_CreateProperties();
    SDL_Environment *env=SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env,"ZENKAIPOWER_GS","gpu",true);
    for (const char *key : {"ZENKAIPOWER_DEMO","ZENKAIPOWER_REPLAY","ZENKAIPOWER_PREPARE_TEXT","ZENKAIPOWER_TITLE_LAUNCHER"})
        SDL_UnsetEnvironmentVariable(env,key);
#ifdef _WIN32
    if (program==root+"sm.exe") SDL_SetEnvironmentVariable(env,"ZENKAIPOWER_TITLE_LAUNCHER","sm",true);
    else if (program==root+"bt3.exe") SDL_SetEnvironmentVariable(env,"ZENKAIPOWER_TITLE_LAUNCHER","zenkaipower",true);
#endif
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)args);
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER,env);
    SDL_SetStringProperty(props,SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,root.c_str());
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,true);
    SDL_Log("Setup Play: %s",program.c_str());
    // Launchers hand off to the game and exit immediately. They do not need the
    // extra Windows exit-status handle used for installer worker processes.
    SDL_Process *process=SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (!process) return false;
    SDL_DestroyProcess(process);
    return true;
}
#endif
