// Windows regression: freshly created, short-lived title launchers must launch
// successfully without the installer's worker-process exit-status tracking.
#include "../../setup/launch.h"
#include <cassert>
#include <filesystem>
#include <fstream>

int main(int argc,char **argv) {
    namespace fs=std::filesystem;
    SDL_Environment *env=SDL_GetEnvironment();
    if (fs::path(argv[0]).filename()!="play-test.exe" && fs::path(argv[0]).filename()!="play-test") {
        for (const char *key : {"ZENKAIPOWER_PREPARE_TEXT","ZENKAIPOWER_DEMO","ZENKAIPOWER_REPLAY"})
            if (SDL_GetEnvironmentVariable(env,key)) return 2;
        const char *gpu=SDL_GetEnvironmentVariable(env,"ZENKAIPOWER_GS");
        if (!gpu || std::string(gpu)!="gpu") return 3;
        const char *title=SDL_GetEnvironmentVariable(env,"ZENKAIPOWER_TITLE_LAUNCHER");
        std::ofstream("played.txt") << (title ? title : "direct");
        return 0;
    }
    auto dir=fs::temp_directory_path()/("setup-play-"+std::to_string(SDL_GetPerformanceCounter()));
    fs::create_directories(dir);
    const std::string root=dir.u8string()+"/";
    const std::string fallback=root+"ZenkaiPower.exe";
    const fs::path self=fs::absolute(fs::u8path(argv[0]));
    fs::copy_file(self,fs::u8path(fallback));
#ifdef _WIN32
    fs::copy_file(self.parent_path()/"SDL3.dll",dir/"SDL3.dll");
#endif
    for (const char *key : {"ZENKAIPOWER_PREPARE_TEXT","ZENKAIPOWER_DEMO","ZENKAIPOWER_REPLAY"})
        SDL_setenv_unsafe(key,"1",1);
    auto play=[&](const char *expected) {
        fs::remove(dir/"played.txt");
        assert(Setup_LaunchGame(root,fallback));
        std::string actual;
        for (int i=0;i<500 && actual.empty();++i) {
            std::ifstream input(dir/"played.txt"); input >> actual;
            SDL_Delay(10);
        }
        assert(actual==expected);
    };
#ifdef _WIN32
    assert(Setup_PlayProgram(root,fallback)==fallback);
    // The same setup process first sees no launcher, then newly installed ones.
    fs::copy_file(self,dir/"bt3.exe");
    assert(Setup_PlayProgram(root,fallback)==root+"bt3.exe");
    play("zenkaipower");
    fs::copy_file(self,dir/"sm.exe");
    assert(Setup_PlayProgram(root,fallback)==root+"sm.exe");
    play("sm");
    play("sm"); // Same files and selection when reopening the installed game.
    fs::remove(dir/"sm.exe"); fs::remove(dir/"bt3.exe");
#endif
    play("direct");
    assert(std::string(SDL_getenv_unsafe("ZENKAIPOWER_PREPARE_TEXT"))=="1");
    fs::remove_all(dir);
    puts("PASS: fresh and existing launcher selection, fast launcher exit, and clean gameplay environment");
}
