// Run with SDL_VIDEODRIVER=dummy and a temporary XDG_DATA_HOME.
#include "../../src/gs/title_taskbar.cpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iterator>

static int japanese,usCalls,jpCalls;
extern "C" const char *Port_FileRoot(void) { return "unused"; }
extern "C" int Port_TitleLogoEnabled(void) { return japanese; }
extern "C" unsigned char *MenuLabels_MeteorLogo(const char *,int *,int *) { jpCalls++; return nullptr; }
extern "C" unsigned char *MenuLabels_TitleLogo(const char *,int *w,int *h) {
    usCalls++; *w=8; *h=4;
    unsigned char *data=(unsigned char *)malloc(8*4*4);
    SDL_memset(data,255,8*4*4);
    return data;
}
int main() {
    Port_TitleTaskbarPrepare();
    assert(SDL_strcmp(SDL_GetHint(SDL_HINT_APP_ID),"org.zenkaipower.Game")==0);
    assert(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window=SDL_CreateWindow("Test",32,32,SDL_WINDOW_HIDDEN);
    assert(window);
    for (japanese=0;japanese<2;japanese++) Port_TitleTaskbar(window);
    assert(usCalls==2 && jpCalls==1);
    std::string data=SDL_getenv("XDG_DATA_HOME");
    SDL_Surface *icon=SDL_LoadPNG((data+"/icons/hicolor/256x256/apps/org.zenkaipower.Game.png").c_str());
    assert(icon && icon->w==256 && icon->h==256); SDL_DestroySurface(icon);
    std::ifstream file(data+"/applications/org.zenkaipower.Game.desktop");
    std::string entry((std::istreambuf_iterator<char>(file)),{});
    assert(entry.find("StartupWMClass=org.zenkaipower.Game\n")!=std::string::npos);
    assert(entry.find("Exec=\"")!=std::string::npos);
    assert(entry.find("Icon="+data)!=std::string::npos);
    assert(desktop_value("a%b",true)=="a%%b");
    assert(desktop_value("a\nb")=="a\\nb");
    SDL_DestroyWindow(window); SDL_Quit();
    SDL_Log("PASS: Linux icon, desktop entry, app ID, and missing-Japanese fallback");
}
