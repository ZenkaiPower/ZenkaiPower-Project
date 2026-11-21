#include "locations.h"
// The setup window of the PC port: pick your disc image, watch the steps, press Play.
//
// A native SDL3 + Dear ImGui program. Source checkouts run install.py to build the port;
// release folders use native.cpp to extract all four supported discs without Python.
// Both report event lines (@step, @note, @ok, @skip, @fail, @missing, @stopped, @done).
//
//   build:  port/setup/build.sh   ->  ./ZenkaiPower-setup
//   test:   ZENKAIPOWER_SETUP_ISO=<file> starts the installation at once; ZENKAIPOWER_SETUP_SHOT=<prefix> writes the window's
//           picture to <prefix>_<n>.ppm whenever the page or the step changes (no person needed to check it)
#include "text.h"
#include "launch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <mutex>
#include <future>
#include <chrono>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "../src/port_process.h"
#include "imgui.h"
#include "artwork.h"


template<class... Args>
static std::string setup_format(const char *key, Args... args) {
    const char *format=SetupText(key);
    int size=snprintf(nullptr,0,format,args...);
    if (size<0) return format;
    std::vector<char> buffer((size_t)size+1);
    snprintf(buffer.data(),buffer.size(),format,args...);
    return std::string(buffer.data(),(size_t)size);
}

static ImFontAtlasRectId sCoverRect=ImFontAtlasRectId_Invalid;
static ImFontAtlasRectId sDiscIconRect[4]={ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,
    ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid};
static ImFontAtlasRectId sEditionImageRect[4][3]={
    {ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid},
    {ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid},
    {ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid},
    {ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid}
};
static int sDetailDisc=-1;
static int sDetailReturnPage;
static ImFontAtlasRectId sZoomImageRect=ImFontAtlasRectId_Invalid;
static std::string sRoot;
static ImFontAtlasRectId add_atlas_image(SDL_Surface *decoded) {
    if(!decoded) return ImFontAtlasRectId_Invalid;
    SDL_Surface *surface=SDL_ConvertSurface(decoded,SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(decoded);
    if(!surface) return ImFontAtlasRectId_Invalid;
    ImFontAtlas *atlas=ImGui::GetIO().Fonts;
    ImFontAtlasRect rect;
    ImFontAtlasRectId id=atlas->AddCustomRect(surface->w,surface->h,&rect);
    if(id!=ImFontAtlasRectId_Invalid) {
        atlas->TexPixelsUseColors=true;
        atlas->TexData->UseColors=true;
        for(int y=0;y<surface->h;y++)
            memcpy(atlas->TexData->GetPixelsAt(rect.x,rect.y+y),(unsigned char *)surface->pixels+y*surface->pitch,surface->w*4);
    }
    SDL_DestroySurface(surface);
    return id;
}
static void load_cover() {
    sCoverRect=add_atlas_image(Setup_LoadArtwork(sRoot,"cover.jpg",512,600));
}
static void load_disc_icons() {
    static const char *const names[]={"SLUS-21678-disc.png","SLES-54945-disc.png","RDSPAF-disc.png","SLPS-25815-disc.png"};
    for(int i=0;i<4;i++) sDiscIconRect[i]=add_atlas_image(Setup_LoadArtwork(sRoot,names[i],128,128));
}
static void load_edition_images() {
    static const char *const ids[]={"SLUS-21678","SLES-54945","RDSPAF","SLPS-25815"};
    static const char *const kinds[]={"front","back","disc"};
    for(int edition=0;edition<4;edition++) for(int image=0;image<3;image++) {
        std::string name=std::string(ids[edition])+"-"+kinds[image]+".png";
        sEditionImageRect[edition][image]=add_atlas_image(Setup_LoadArtwork(sRoot,name.c_str()));
    }
}
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h" // glViewport, glClear for the OpenGL window (set up by ImGui_ImplOpenGL3_Init)
#include "native.h"
#include "updater.h"
#include "../src/port_pack.h"

#define GAME "ZenkaiPower" /* the program this sets up: GAME, GAME.dat; this program is GAME-setup */

enum Page { PAGE_WELCOME, PAGE_PICK, PAGE_TASKS, PAGE_RUN, PAGE_DONE, PAGE_FAILED, PAGE_EDITION };
enum State { PENDING, RUNNING, OK, SKIPPED, FAILED };
struct StepRow { std::string title, detail; State state = PENDING; Uint64 start = 0, end = 0; };
struct Missing { std::string name, why, pkg; };

// The steps of the two ways to install: from a source checkout (install.py does the work and needs build tools) and
// in a release folder (ZenkaiPower and ZenkaiPower.dat next to this program: native.cpp unpacks the disc itself).
static const char *kSourceSteps[] = {"Requirements", "Disc image", "Game data", "Executable data", "Build", "Text resources"};
static const char *kReleaseSteps[] = {"Disc image", "Game data", "Text resources"};
static const struct { const char *title, *operation, *help; } kHelp[] = {
    {"Requirements", "steps.requirements.title", "steps.requirements.description"},
    {"Disc image", "steps.disc.title", "steps.disc.description"},
    {"Game data", "steps.game_data.title", "steps.game_data.description"},
    {"Executable data", "steps.executable_data.title", "steps.executable_data.description"},
    {"Build", "steps.build.title", "steps.build.description"},
    {"European languages", "steps.languages.title", "steps.languages.description"},
    {"Sparking! METEOR logo and soundtrack", "steps.japanese.title", "steps.japanese.description"},
    {"Wii Dragon Net Battle", "steps.wii.title", "steps.wii.description"},
    {"Text resources", "steps.text.title", "steps.text.description"},
};
static bool sRelease;
static bool sDesktopShortcut=true;
static bool sCustomData=false, sCustomSaves=false, sLocationsDirty=false;
static char sDataFolder[1024], sSaveFolder[1024];
static std::string sLocationError;
static bool commit_locations();
static Page sTasksBack=PAGE_PICK;

static const char *step_help(const std::string &title) {
    for (auto &h : kHelp) {
        if (title == h.title) {
            return SetupText(h.help);
        }
    }
    return "";
}

static SDL_Window *sWindow;
static SDL_GPUDevice *sDevice;
// The window is drawn through SDL's GPU layer (Vulkan on Linux, Direct3D 12 on Windows) or, where that cannot
// start (no Vulkan driver, a graphics chip too old for it: reported for Intel Ivy Bridge), through OpenGL.
// ZENKAIPOWER_GPU_API=gl asks for OpenGL at once, as it does for the game.
static SDL_GLContext sGl;
static bool sUseGl;
static Page sPage = PAGE_WELCOME;
static char sIso[1024];
// Optional: a disc image of the European release (SLES-54945), for Spanish, German, French and Italian. The game is
// always set up from the USA one; from this one only the languages' archive entries are taken (native.cpp step_eu,
// install.py --eu-iso).
static char sIsoEu[1024], sIsoWii[1024], sIsoJp[1024];
static bool sDiscChecked[4], sDiscInstalled[4];

static ImFontAtlasRectId sTitleLogo[2]={ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid};
static Uint64 sTitleLogoCheck;
static int sSelectedTitleLogo=-1;
static std::future<NativeTitleLogo> sTitlePreview[2];
static std::string sTitlePreviewPath[2];

static void load_title_logos() {
    if (sTitleLogoCheck && SDL_GetTicks()-sTitleLogoCheck<1000) return;
    sTitleLogoCheck=SDL_GetTicks();
    for(int i=0;i<2;i++) {
        int disc=i ? 3 : 0;
        const char *selected=i ? sIsoJp : sIso;
        if(sDiscChecked[disc] && selected[0] && sTitlePreviewPath[i]!=selected && !sTitlePreview[i].valid()) {
            sTitlePreviewPath[i]=selected;
            sTitlePreview[i]=std::async(std::launch::async,Native_ReadTitleLogo,sTitlePreviewPath[i],i!=0);
        }
        NativeTitleLogo preview;
        if(sTitlePreview[i].valid() && sTitlePreview[i].wait_for(std::chrono::seconds(0))==std::future_status::ready)
            preview=sTitlePreview[i].get();
        if(sTitleLogo[i]!=ImFontAtlasRectId_Invalid && preview.pixels.empty()) continue;
        SDL_Surface *logo=nullptr;
        if(!preview.pixels.empty()) {
            logo=SDL_CreateSurfaceFrom(preview.width,preview.height,SDL_PIXELFORMAT_RGBA32,preview.pixels.data(),preview.width*4);
        } else {
        std::string path=Setup_DataRoot(sRoot)+"game-text/"+(i ? "title-sm.png" : "title-zenkaipower.png");
        FILE *file=Port_PackFopen(path.c_str(),"rb");
        if(!file) continue;
        std::vector<unsigned char> bytes;
        unsigned char buffer[4096]; size_t n;
        while((n=fread(buffer,1,sizeof(buffer),file))!=0) bytes.insert(bytes.end(),buffer,buffer+n);
        fclose(file);
        SDL_Surface *decoded=bytes.empty() ? nullptr : SDL_LoadPNG_IO(SDL_IOFromConstMem(bytes.data(),bytes.size()),true);
        logo=decoded ? SDL_ConvertSurface(decoded,SDL_PIXELFORMAT_RGBA32) : nullptr;
        SDL_DestroySurface(decoded);
        }
        if(!logo) continue;
        // Trim transparent padding so the full logo fills the panel naturally.
        int x0=logo->w,y0=logo->h,x1=0,y1=0;
        for(int y=0;y<logo->h;y++) for(int x=0;x<logo->w;x++)
            if(((unsigned char *)logo->pixels)[y*logo->pitch+x*4+3]) {
                x0=std::min(x0,x); y0=std::min(y0,y); x1=std::max(x1,x+1); y1=std::max(y1,y+1);
            }
        if(x1>x0 && y1>y0) {
            ImFontAtlas *atlas=ImGui::GetIO().Fonts; ImFontAtlasRect rect;
            if(sTitleLogo[i]!=ImFontAtlasRectId_Invalid) atlas->RemoveCustomRect(sTitleLogo[i]);
            sTitleLogo[i]=atlas->AddCustomRect(x1-x0,y1-y0,&rect);
            if(sTitleLogo[i]!=ImFontAtlasRectId_Invalid) {
                atlas->TexPixelsUseColors=true; atlas->TexData->UseColors=true;
                for(int y=y0;y<y1;y++) memcpy(atlas->TexData->GetPixelsAt(rect.x,rect.y+y-y0),
                    (unsigned char *)logo->pixels+y*logo->pitch+x0*4,(x1-x0)*4);
            }
        }
        SDL_DestroySurface(logo);
    }
}
static void refresh_installed_discs();
static std::future<std::string> sVerification[4];
static std::string sVerifyPath[4], sVerifiedPath[4], sVerifyError[4];
static Uint64 sVerifyChanged[4];
static char *disc_path(int index);
static void refresh_installed_discs() {
    for (int i = 0; i < 4; i++) sDiscInstalled[i] = Native_DiscInstalled(sRoot, i);
}


static void pump_verification() {
    if (sPage != PAGE_WELCOME && sPage != PAGE_PICK && sPage != PAGE_DONE) return;
    for (int i = 0; i < 4; i++) {
        if (sVerifyPath[i] != disc_path(i)) {
            sVerifyPath[i] = disc_path(i);
            sVerifiedPath[i].clear();
            sVerifyError[i].clear();
            sDiscChecked[i] = false;
            sVerifyChanged[i] = SDL_GetTicks();
        }
        if (sVerification[i].valid()) {
            if (sVerification[i].wait_for(std::chrono::seconds(0)) != std::future_status::ready) continue;
            std::string result = sVerification[i].get();
            if (!sVerifiedPath[i].empty() && sVerifiedPath[i] == sVerifyPath[i]) {
                sVerifyError[i] = result;
                sDiscChecked[i] = result.empty();
            } else sVerifiedPath[i].clear();
        }
        if (!sVerifyPath[i].empty() && sVerifiedPath[i] == sVerifyPath[i] && sVerifyError[i].empty())
            sDiscChecked[i] = true;
        if (!sVerifyPath[i].empty() && sVerifiedPath[i].empty() && SDL_GetTicks() - sVerifyChanged[i] >= 400) {
            sVerifiedPath[i] = sVerifyPath[i];
            sVerification[i] = std::async(std::launch::async, Native_VerifyDisc, sRoot, sVerifyPath[i], i);
        }
    }
}

static void finish_disc_selection() {
    refresh_installed_discs();
    // Completed selections now represent installed data, not pending reinstalls.
    for (int i = 0; i < 4; i++) {
        disc_path(i)[0] = '\0';
        sDiscChecked[i] = false;
        sVerifyPath[i].clear();
        sVerifiedPath[i].clear();
        sVerifyError[i].clear();
    }
}

static char *disc_path(int index) {
    return index == 0 ? sIso : index == 1 ? sIsoEu : index == 2 ? sIsoWii : sIsoJp;
}
static bool sEuOnly;    // only the languages are added (the game was set up before: the "Ready to play" page)
static int sDialogFor;  // which field the file dialog fills: 0 the USA image, 1 the European one
static std::string sError, sLauncher, sLine;
static std::vector<StepRow> sRows;
static std::vector<Missing> sMissing;
static SDL_Process *sProc;
static bool sNative; // the native installer is at work (release folder)
static std::mutex sDialogLock;
static std::string sDialogResult;
static bool sDialogReady, sQuit;
static Uint64 sRunStart, sRunEnd;
static const ImVec4 kAccent(0.08f, 0.24f, 0.50f, 1.0f), kGreen(0.10f, 0.48f, 0.16f, 1.0f), kRed(0.70f, 0.12f, 0.10f, 1.0f),
    kDim(0.38f, 0.38f, 0.36f, 1.0f);
static float sStepFraction;

static bool file_exists(const char *path) {
    SDL_PathInfo info;
    return path[0] != '\0' && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

// The folder to install into: a release folder (this program's own), or the source checkout that holds install.py.
static std::string find_root() {
    std::string dir = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    if ((file_exists((dir + GAME).c_str()) || file_exists((dir + GAME ".exe").c_str())) && file_exists((dir + GAME ".dat").c_str())) {
        sRelease = true; // a release folder: the finished program is here, only the game data is missing
        return dir;
    }
    for (int up = 0; up < 4; up++) {
        if (file_exists((dir + "install.py").c_str())) {
            return dir;
        }
        dir += "../";
    }
    return "";
}

static void style() {
    ImGui::StyleColorsLight();
    ImGuiStyle &st=ImGui::GetStyle();
    st.WindowRounding=st.ChildRounding=st.FrameRounding=st.GrabRounding=0;
    st.WindowPadding=ImVec2(16,14);
    st.FramePadding=ImVec2(8,5);
    st.ItemSpacing=ImVec2(8,8);
    st.FrameBorderSize=1;
    st.WindowBorderSize=0;
    auto &c=st.Colors;
    c[ImGuiCol_WindowBg]=ImVec4(0.93f,0.92f,0.88f,1);
    c[ImGuiCol_PopupBg]=ImVec4(0.93f,0.92f,0.88f,1);
    c[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);
    c[ImGuiCol_Text]=ImVec4(0.08f,0.08f,0.08f,1);
    c[ImGuiCol_TextDisabled]=kDim;
    c[ImGuiCol_Border]=ImVec4(0.62f,0.62f,0.58f,1);
    c[ImGuiCol_FrameBg]=ImVec4(1,1,1,1);
    c[ImGuiCol_Button]=ImVec4(0.94f,0.94f,0.91f,1);
    c[ImGuiCol_ButtonHovered]=ImVec4(0.86f,0.91f,0.97f,1);
    c[ImGuiCol_ButtonActive]=ImVec4(0.74f,0.83f,0.94f,1);
    c[ImGuiCol_CheckMark]=kAccent;
    c[ImGuiCol_PlotHistogram]=kGreen;
    c[ImGuiCol_Header]=ImVec4(0.80f,0.86f,0.93f,1);
}

// ---------------------------------------------------------------------------------------------- the engine script

// Keep progress rows in the same order as native.cpp and install.py.
static void reset_rows() {
    std::vector<std::string> titles;
    if (sRelease) {
        if (!sEuOnly && sIso[0]) {
            titles.push_back("Disc image");
            titles.push_back("Game data");
        }
        if (!sEuOnly && sIsoJp[0]) titles.push_back("Sparking! METEOR logo and soundtrack");
        if (sIsoEu[0]) titles.push_back("European languages");
        if (!sEuOnly && sIsoWii[0]) titles.push_back("Wii Dragon Net Battle");
        titles.push_back("Text resources");
    } else {
        titles.assign(kSourceSteps, kSourceSteps + sizeof(kSourceSteps) / sizeof(kSourceSteps[0]));
        if (sEuOnly) titles = {"European languages", "Text resources"};
        else {
            if (sIsoEu[0]) titles.insert(titles.end() - 1, "European languages");
            if (sIsoJp[0]) titles.insert(titles.end() - 1 - (sIsoEu[0] ? 1 : 0), "Sparking! METEOR logo and soundtrack");
            if (sIsoWii[0]) titles.insert(titles.end() - 1, "Wii Dragon Net Battle");
        }
    }
    sRows.assign(titles.size(), StepRow());
    for (size_t i = 0; i < titles.size(); i++) {
        sRows[i].title = titles[i];
    }
}

static void start_install() {
    if (!commit_locations()) { sPage=PAGE_PICK; return; }
    static std::string iso, euIso, wiiIso, jpIso;
    const char *args[16];
    SDL_PropertiesID props = SDL_CreateProperties();
    std::string script = sRoot + "install.py";
    int n = 0;

    if (sRelease) {
        SDL_DestroyProperties(props);
        reset_rows();
        sMissing.clear();
        sError.clear();
        sLauncher.clear();
        sRunStart = SDL_GetTicks();
        sNative = true;
        Native_Start(sRoot, sEuOnly ? "" : sIso, sIsoEu, sEuOnly ? "" : sIsoWii, sEuOnly ? "" : sIsoJp, sDesktopShortcut);
        sPage = PAGE_RUN;
        return;
    }
    iso = sIso;
    args[n++] = "python3";
    args[n++] = script.c_str();
    args[n++] = "--machine";
    args[n++] = "--iso";
    args[n++] = iso.c_str();
    if (sIsoEu[0] != '\0') {
        euIso = sIsoEu;
        args[n++] = "--eu-iso";
        args[n++] = euIso.c_str();
    }
    if (sIsoWii[0]) {
        wiiIso = sIsoWii;
        args[n++] = "--eu-wii-iso";
        args[n++] = wiiIso.c_str();
    }
    if (sIsoJp[0]) {
        jpIso = sIsoJp;
        args[n++] = "--jp-iso";
        args[n++] = jpIso.c_str();
    }
    if (getenv("ZENKAIPOWER_SETUP_ARGS") != NULL) { // one extra argument for testing, such as --skip-test
        args[n++] = getenv("ZENKAIPOWER_SETUP_ARGS");
    }
    args[n] = NULL;
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, sRoot.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    reset_rows();
    sMissing.clear();
    sError.clear();
    sLine.clear();
    sLauncher.clear();
    sProc = Port_CreateHiddenProcess(props);
    SDL_DestroyProperties(props);
    sRunStart = SDL_GetTicks();
    if (sProc == NULL) {
        sError = std::string(SetupText("failure.python_start")) + SDL_GetError() + SetupText("failure.python_required");
        sPage = PAGE_FAILED;
        return;
    }
    sPage = PAGE_RUN;
}

static StepRow *current() {
    for (auto &r : sRows) {
        if (r.state == RUNNING) {
            return &r;
        }
    }
    return NULL;
}

static std::string unescape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out += '\n';
            i++;
            while (i + 1 < s.size() && s[i + 1] == ' ') { // the terminal version indents continuation lines
                i++;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static void event(const std::string &line) {
    if (line.empty() || line[0] != '@') {
        return;
    }
    size_t sp = line.find(' ');
    std::string kind = line.substr(1, sp == std::string::npos ? std::string::npos : sp - 1);
    std::string text = sp == std::string::npos ? "" : unescape(line.substr(sp + 1));
    StepRow *cur = current();

    if (kind == "disc-verified") {
        for (int i = 0; i < 4; i++) {
            const char *ids[] = {"SLUS-21678", "SLES-54945", "RDSPAF", "SLPS-25815"};
            if (text == ids[i]) sDiscChecked[i] = true;
        }
    } else if (kind == "step") {
        int i = atoi(text.c_str());
        if (i >= 1 && i <= (int)sRows.size()) {
            sRows[i - 1].state = RUNNING;
            sStepFraction=0;
            sRows[i - 1].start = SDL_GetTicks();
            sRows[i - 1].detail.clear();
        }
    } else if (cur != NULL && kind == "note") {
        cur->detail = text;
        int percent=0;
        if (sscanf(text.c_str(),"%d%%",&percent)==1 && text.find('%')!=std::string::npos)
            sStepFraction=std::max(0.0f,std::min(1.0f,percent/100.0f));
    } else if (cur != NULL && (kind == "ok" || kind == "skip" || kind == "fail")) {
        sStepFraction=0;
        cur->state = kind == "ok" ? OK : kind == "skip" ? SKIPPED : FAILED;
        cur->end = SDL_GetTicks();
        cur->detail = kind == "fail" ? "" : text;
    } else if (kind == "missing") {
        Missing m;
        size_t a = text.find('|'), b = text.find('|', a == std::string::npos ? 0 : a + 1);
        if (a != std::string::npos && b != std::string::npos) {
            m.name = text.substr(0, a);
            m.why = text.substr(a + 1, b - a - 1);
            m.pkg = text.substr(b + 1);
            sMissing.push_back(m);
        }
    } else if (kind == "stopped") {
        sError = text;
    } else if (kind == "done") {
        sLauncher = text;
    }
}

static void pump_install() {
    char buf[4096];
    SDL_IOStream *out;
    int code = 0;

    if (sNative) {
        std::string line;
        bool ended = !Native_Running(); // asked before the queue is emptied: nothing is left behind
        while (Native_Poll(line)) {
            event(line);
        }
        if (ended) {
            sNative = false;
            sRunEnd = SDL_GetTicks();
            for (auto &r : sRows) {
                if (r.state == RUNNING) {
                    r.state = FAILED;
                    r.end = sRunEnd;
                }
            }
            if (!sLauncher.empty()) {
                finish_disc_selection();
                sPage = PAGE_DONE;
            } else {
                if (sError.empty()) {
                    sError = SetupText("failure.unexpected");
                }
                sPage = PAGE_FAILED;
            }
        }
        return;
    }
    if (sProc == NULL) {
        return;
    }
    out = SDL_GetProcessOutput(sProc);
    for (;;) {
        size_t n = out != NULL ? SDL_ReadIO(out, buf, sizeof(buf)) : 0;
        if (n == 0) {
            break;
        }
        for (size_t i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                event(sLine);
                sLine.clear();
            } else if (buf[i] != '\r') {
                sLine += buf[i];
            }
        }
    }
    if (Port_WaitProcess(sProc, false, &code)) {
        event(sLine);
        SDL_DestroyProcess(sProc);
        sProc = NULL;
        sRunEnd = SDL_GetTicks();
        for (auto &r : sRows) {
            if (r.state == RUNNING) {
                r.state = FAILED;
                r.end = sRunEnd;
            }
        }
        if (code == 0 && !sLauncher.empty()) {
            finish_disc_selection();
            sPage = PAGE_DONE;
        } else {
            if (sError.empty()) {
                sError = SetupText("failure.script_unexpected");
            }
            sPage = PAGE_FAILED;
        }
    }
}

// --------------------------------------------------------------------------------------------------------- pages

static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter) {
    (void)filter;
    std::lock_guard<std::mutex> lock(sDialogLock);
    if (files != NULL && files[0] != NULL) {
        sDialogResult = files[0];
        sDialogFor = (int)(intptr_t)userdata; // (NULL: the USA image's field)
        sDialogReady = true;
    }
}

static void load_locations() {
    if (sRoot.empty()) return;
    Port_LocationRead(sRoot.c_str(),"data",sDataFolder,sizeof(sDataFolder));
    Port_LocationRead(sRoot.c_str(),"saves",sSaveFolder,sizeof(sSaveFolder));
    sCustomData=sDataFolder[0]!=0; sCustomSaves=sSaveFolder[0]!=0;
}

static bool commit_locations() {
    if (!sRelease || !sLocationsDirty) return true;
    if ((sCustomData && !sDataFolder[0]) || (sCustomSaves && !sSaveFolder[0])) {
        sLocationError=SetupText("locations.choose_folder"); return false;
    }
    if (!Setup_SaveLocations(sRoot,sCustomData ? sDataFolder : "",sCustomSaves ? sSaveFolder : "",sLocationError)) return false;
    sLocationsDirty=false;
    refresh_installed_discs();
    return true;
}

static void storage_locations() {
#ifndef ZENKAIPOWER_ANDROID_INSTALLER
    if (!sRelease) return;
    ImGui::SeparatorText(SetupText("locations.title"));
    for (int row=0;row<2;row++) {
        ImGui::PushID(row+100);
        bool &custom=row==0 ? sCustomData : sCustomSaves;
        char *folder=row==0 ? sDataFolder : sSaveFolder;
        ImGui::TextUnformatted(SetupText(row==0 ? "locations.data" : "locations.saves"));
        ImGui::SetNextItemWidth(140);
        const char *label=SetupText(custom ? "locations.custom" : "locations.local");
        if (ImGui::BeginCombo("##location-mode",label)) {
            if (ImGui::Selectable(SetupText("locations.local"),!custom)) { custom=false;sLocationsDirty=true; }
            if (ImGui::Selectable(SetupText("locations.custom"),custom)) { custom=true;sLocationsDirty=true; }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        std::string shown=custom ? folder : sRoot+(row==0 ? "gamedata" : "savegame");
        ImGui::SetNextItemWidth(std::max(80.0f,ImGui::GetContentRegionAvail().x-102.0f));
        ImGui::BeginDisabled(!custom);
        ImGui::InputText("##location-path",const_cast<char *>(shown.c_str()),shown.size()+1,ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button(SetupText("buttons.browse"),ImVec2(88,0)))
            SDL_ShowOpenFolderDialog(dialog_done,(void *)(intptr_t)(row+4),sWindow,folder[0] ? folder : sRoot.c_str(),false);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    ImGui::TextWrapped("%s",SetupText("locations.no_move"));
    if (!sLocationError.empty()) ImGui::TextColored(kRed,"%s",sLocationError.c_str());
    ImGui::BeginDisabled(!sLocationsDirty);
    if (ImGui::Button(SetupText("locations.apply")) && commit_locations()) {
        sPage=Native_Installed(sRoot) ? PAGE_DONE : PAGE_PICK;
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
#endif
}

// Use the same packaged metadata as the game's settings footer.
static std::string sGameTitle = GAME, sGameEdition, sSourceUrl, sOriginalSourceUrl, sPageUrl;
static std::string sInstalledVersion, sUpdateMessage;
static std::future<SetupUpdate> sUpdateCheck;
static std::future<std::string> sUpdatePrepare;
static SetupUpdate sUpdate;
static bool sShowUpdate;
static void load_game_info() {
    const char *keys[] = {"title.txt", "subtitle.txt", "version.txt", "source.txt", "original_source.txt", "page.txt"};
    std::string values[6];
    const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    for (int i = 0; i < 6; i++) {
        for (const auto &dir : {base, sRoot}) {
            if (dir.empty()) continue;
            int64_t offset = 0, length = 0;
            FILE *file = Port_NamedPackage((dir + "game-info.pak").c_str(), keys[i], &offset, &length, -1, NULL, 0);
            if (!file) continue;
            char text[1024];
            bool valid = length >= 0 && length < (long)sizeof(text);
            if (valid) valid = fread(text, 1, (size_t)length, file) == (size_t)length;
            fclose(file);
            if (!valid) continue;
            for (long n = 0; n < length; n++) if ((unsigned char)text[n] < 32) valid = false;
            if (!valid) continue;
            values[i].assign(text, (size_t)length);
            break;
        }
    }
    if (!values[0].empty()) sGameTitle = values[0];
    sGameEdition = values[1];
    sInstalledVersion = values[2];
    if (!values[2].empty()) {
        if (!sGameEdition.empty()) sGameEdition += "  ";
        sGameEdition += values[2];
    }
    auto web_url = [](const std::string &url) {
        return url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
    };
    if (web_url(values[3])) sSourceUrl = values[3];
    sPageUrl = web_url(values[5]) ? values[5] : sSourceUrl;
    if (web_url(values[4])) sOriginalSourceUrl = values[4];
    SDL_SetWindowTitle(sWindow, setup_format("window.title",sGameTitle.c_str()).c_str());
}

static ImFont *sHeaderFont;
static ImFont *load_dejavu(const char *name) {
    const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "./";
    for (const auto &dir : {base, sRoot}) {
        if (dir.empty()) continue;
        int64_t offset = 0, length = 0;
        std::string relative = std::string("dejavu/") + name;
        FILE *file = Port_NamedPackage((dir + "fonts.pak").c_str(), relative.c_str(), &offset, &length, -1, NULL, 0);
        if (!file) {
            file = fopen((dir + "port/third_party/fonts/" + relative).c_str(), "rb");
            if (file) {
                if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || fseek(file, 0, SEEK_SET)) {
                    fclose(file); continue;
                }
            }
        }
        if (!file) continue;
        if (length <= 0 || length > (8 << 20)) { fclose(file); continue; }
        void *data = ImGui::MemAlloc((size_t)length);
        bool read = fread(data, 1, (size_t)length, file) == (size_t)length;
        fclose(file);
        if (!read) { ImGui::MemFree(data); continue; }
        return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, (int)length, 18.0f);
    }
    return NULL;
}

static void heading(const char *title, const char *sub) {
    ImVec2 start=ImGui::GetCursorScreenPos();
    float width=ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(start.x-8,start.y-8),ImVec2(start.x+width+8,start.y+90),IM_COL32(255,255,255,255));
    ImGui::PushFont(sHeaderFont,22);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(start.x,start.y+40));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
    ImGui::TextUnformatted(sub);
    ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(start.x,start.y+104));
    // The failure page opens a modal next, so commit this spacing in the parent.
    ImGui::Dummy(ImVec2(0,0));
}

static void update_dialog() {
    if (sUpdateCheck.valid() && sUpdateCheck.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        sUpdate = sUpdateCheck.get();
        sUpdateMessage = !sUpdate.error.empty() ? sUpdate.error : sUpdate.available ?
            setup_format("updates.available",sUpdate.version.c_str(),sInstalledVersion.c_str()) :
            setup_format("updates.current",sInstalledVersion.c_str());
        sShowUpdate = true;
    }
    if (sUpdatePrepare.valid() && sUpdatePrepare.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::string error = sUpdatePrepare.get();
        if (error.empty()) sQuit = true;
        else sUpdateMessage = error;
    }
    if (sShowUpdate) { ImGui::OpenPopup("Setup updates"); sShowUpdate = false; }
    bool available=sUpdate.available && sUpdate.error.empty();
    ImGui::SetNextWindowSize(ImVec2(580,available ? 350.0f : 260.0f),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,ImVec2(0.5f,0.5f));
    bool open=ImGui::BeginPopupModal("Setup updates",NULL,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
    if(open) {
        ImVec2 origin=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
        ImDrawList *draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin,ImVec2(origin.x+size.x,origin.y+74),IM_COL32(255,255,255,255));
        draw->AddLine(ImVec2(origin.x,origin.y+74),ImVec2(origin.x+size.x,origin.y+74),IM_COL32(190,190,180,255));
        ImGui::PushFont(sHeaderFont,22);
        ImGui::TextUnformatted(SetupText("updates.title"));
        ImGui::PopFont();
        ImGui::TextColored(kDim,"%s",SetupText("updates.subtitle"));
        ImGui::SetCursorPos(ImVec2(20,94));
        ImGui::BeginChild("update-content",ImVec2(0,-64));
        ImGui::TextWrapped("%s",sUpdateMessage.c_str());
        bool busy=sUpdatePrepare.valid();
        if(available) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s",SetupText("updates.description"));
            ImGui::Spacing();
            ImGui::TextWrapped("%s",SetupText("updates.close_game"));
            if(!sRelease) ImGui::TextWrapped("%s",SetupText("updates.release_only"));
        }
        ImGui::EndChild();
        float footer=size.y-58;
        draw->AddLine(ImVec2(origin.x,origin.y+footer),ImVec2(origin.x+size.x,origin.y+footer),IM_COL32(160,160,150,255));
        draw->AddLine(ImVec2(origin.x,origin.y+footer+1),ImVec2(origin.x+size.x,origin.y+footer+1),IM_COL32(255,255,255,255));
        std::string repository=SetupUpdate_Repository(sSourceUrl);
        if(!repository.empty()) {
            ImGui::SetCursorPos(ImVec2(18,footer+20));
            ImGui::PushFont(NULL,13);
            ImGui::TextLinkOpenURL(SetupText("updates.releases_link"),(repository+"/releases/latest").c_str());
            ImGui::PopFont();
        }
        if(available) {
            ImGui::SetCursorPos(ImVec2(size.x-320,footer+14));
            ImGui::BeginDisabled(busy || !sRelease || sPage==PAGE_RUN);
            ImGui::PushStyleColor(ImGuiCol_Border,kAccent);
            if(ImGui::Button(busy ? SetupText("updates.updating") : SetupText("updates.install"),ImVec2(184,30))) {
                sUpdateMessage=SetupText("updates.downloading");
                sUpdatePrepare=std::async(std::launch::async,SetupUpdate_Prepare,sUpdate,sRoot);
            }
            ImGui::PopStyleColor();
            ImGui::EndDisabled();
        }
        ImGui::SetCursorPos(ImVec2(size.x-128,footer+14));
        ImGui::BeginDisabled(busy);
        if(ImGui::Button(SetupText("buttons.close"),ImVec2(112,30))) ImGui::CloseCurrentPopup();
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}

static bool big_button(const char *label, bool primary, bool enabled = true) {
    if (primary) ImGui::PushStyleColor(ImGuiCol_Border,kAccent);
    ImGui::BeginDisabled(!enabled);
    bool pressed=ImGui::Button(label,ImVec2(112,30));
    ImGui::EndDisabled();
    if (primary) ImGui::PopStyleColor();
    return pressed;
}

// Buttons of a page sit on one line at the bottom right.
static void bottom_row(int buttons) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float w = buttons * 112.0f + (buttons - 1) * ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + avail.x - w, ImGui::GetCursorPosY() + avail.y - 30.0f));
}

// Center wrapped text within a card without changing the surrounding layout.
static void card_text(const char *text, ImVec2 origin, float width, float y, float size, ImVec4 color) {
    ImGui::PushFont(NULL, size);
    ImVec2 extent = ImGui::CalcTextSize(text, NULL, false, width);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + (width - extent.x) * 0.5f, origin.y + y));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// Milestones reflect installed data or successful verification of a selected image.
static void disc_checklist(bool fields) {
    static const SDL_DialogFileFilter filters[]={{SetupText("file_dialog.disc_image"),"iso;bin;img;rvz"},{SetupText("file_dialog.all_files"),"*"}};
    const char *titles[]={SetupText("discs.us.title"), SetupText("discs.eu_ps2.title"), SetupText("discs.eu_wii.title"), SetupText("discs.jp.title")};
    const char *ids[]={"SLUS-21678","SLES-54945","RDSPAF","SLPS-25815"};
    const char *uses[]={
        SetupText("discs.us.description"),
        SetupText("discs.eu_ps2.description"),
        SetupText("discs.eu_wii.description"),
        SetupText("discs.jp.description")
    };
    ImGui::PushFont(NULL,15);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(6,3));
    for(int i=0;i<4;i++) {
        ImGui::PushID(i);
        ImVec2 row=ImGui::GetCursorScreenPos();
        float width=ImGui::GetContentRegionAvail().x;
        bool selected=disc_path(i)[0]!=0;
        bool installed=sDiscInstalled[i];
        bool complete=installed || sDiscChecked[i];
        bool failed=selected && !sVerifyError[i].empty();
        ImDrawList *draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(row,ImVec2(row.x+width,row.y+119),IM_COL32(248,247,243,255));
        draw->AddRectFilled(row,ImVec2(row.x+3,row.y+119),ImGui::GetColorU32(complete ? kGreen : failed ? kRed : kDim));
        // Draw at the current UI scale so the disc stays crisp on high-DPI displays.
        ImVec2 disc(row.x+27,row.y+24);
        ImU32 rim=ImGui::GetColorU32(complete ? kGreen : failed ? kRed : kDim);
        ImFontAtlasRect discIcon;
        if(complete && sDiscIconRect[i]!=ImFontAtlasRectId_Invalid &&
           ImGui::GetIO().Fonts->GetCustomRect(sDiscIconRect[i],&discIcon)) {
            draw->AddImage(ImGui::GetIO().Fonts->TexRef,ImVec2(row.x+11,row.y+8),ImVec2(row.x+43,row.y+40),discIcon.uv0,discIcon.uv1);
        } else {
            draw->AddCircleFilled(disc,15.5f,IM_COL32(221,225,230,255),40);
            draw->AddCircle(disc,15.5f,rim,40,1.2f);
            draw->PathArcTo(disc,11.5f,3.65f,5.15f,16);
            draw->PathStroke(IM_COL32(255,255,255,255),0,3.0f);
            draw->PathArcTo(disc,11.5f,0.50f,1.90f,16);
            draw->PathStroke(IM_COL32(183,193,207,255),0,2.0f);
            draw->AddCircleFilled(disc,5,rim,24);
            draw->AddCircleFilled(disc,2.5f,IM_COL32(248,247,243,255),20);
        }
        ImGui::SetCursorScreenPos(ImVec2(row.x+11,row.y+8));
        bool openDetails=ImGui::InvisibleButton("edition-details",ImVec2(32,32));
        if(complete && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s",SetupText("discs.details_tooltip"));
            if(openDetails) {
                sDetailDisc=i;
                sDetailReturnPage=sPage;
                sPage=PAGE_EDITION;
            }
        }
        ImGui::SetCursorScreenPos(ImVec2(row.x+52,row.y+5));
        ImGui::TextUnformatted(titles[i]);
        ImGui::SameLine();
        ImGui::TextColored(kDim,"(%s)",i==0 ? SetupText("discs.required") : SetupText("discs.optional"));
        ImGui::SetCursorScreenPos(ImVec2(row.x+52,row.y+26));
        const char *status=installed ? SetupText("discs.installed") : sDiscChecked[i] ? SetupText("discs.verified") :
            !selected ? SetupText("discs.empty") : failed ? SetupText("discs.verification_failed") : SetupText("discs.verifying");
        ImGui::TextColored(complete ? kGreen : failed ? kRed : kDim,"%s",status);
        ImGui::SameLine();
        ImGui::TextColored(kDim,"  |  %s",ids[i]);
        if(fields) {
            ImGui::SetCursorScreenPos(ImVec2(row.x+11,row.y+48));
            ImGui::SetNextItemWidth(width-175);
            ImGui::InputTextWithHint("##path",SetupText("discs.path_placeholder"),disc_path(i),sizeof(sIso),ImGuiInputTextFlags_ReadOnly);
            if(selected && ImGui::IsItemHovered()) ImGui::SetTooltip("%s",disc_path(i));
            ImGui::SameLine(0,6);
            if(ImGui::Button(SetupText("buttons.browse"),ImVec2(88,0)))
                SDL_ShowOpenFileDialog(dialog_done,(void *)(intptr_t)i,sWindow,filters,2,NULL,false);
            ImGui::SameLine(0,6);
            ImGui::BeginDisabled(!selected);
            if(ImGui::Button(SetupText("buttons.clear"),ImVec2(52,0))) {
                disc_path(i)[0]=0; sDiscChecked[i]=false; sVerifyError[i].clear();
            }
            ImGui::EndDisabled();
            if(failed && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s",sVerifyError[i].c_str());
        }
        ImGui::SetCursorScreenPos(ImVec2(row.x+11,row.y+(fields ? 79 : 48)));
        ImGui::PushStyleColor(ImGuiCol_Text,kDim);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width-22);
        ImGui::TextUnformatted(uses[i]);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2(row.x,row.y+(fields ? 127 : 96)));
        // Register the row spacing before EndChild checks the content bounds.
        ImGui::Dummy(ImVec2(0,0));
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
}

static void page_edition() {
    static const char *const titles[]={
        "Dragon Ball Z: Budokai Tenkaichi 3",
        "Dragon Ball Z: Budokai Tenkaichi 3",
        "Dragon Ball Z: Budokai Tenkaichi 3",
        "Dragon Ball Z: Sparking! METEOR"
    };
    static const char *const dates[]={"November 13, 2007","November 9, 2007","February 15, 2008","October 4, 2007"};
    static const char *const ids[]={"SLUS-21678","SLES-54945","RDSPAF","SLPS-25815"};
    static const char *const discTitleKeys[]={"discs.us.title","discs.eu_ps2.title","discs.eu_wii.title","discs.jp.title"};
    static const char *const labels[]={"edition.front","edition.back","edition.disc"};
    int edition=sDetailDisc;
    if(edition<0 || edition>=4) edition=0;

    // Only the details scroll. The shared Back button stays in the fixed bottom row.
    // The heading and field labels follow the same localized text and style as the other pages.
    bool openViewer=false;
    ImGui::BeginChild("edition-details-content",ImVec2(0,-64.0f));
    heading(SetupText(discTitleKeys[edition]),SetupText("edition.details"));
    if(ImGui::BeginTable("edition-information",2,
            ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("##edition-field",ImGuiTableColumnFlags_WidthFixed,140);
        ImGui::TableSetupColumn("##edition-value",ImGuiTableColumnFlags_WidthStretch);
        const char *fields[]={SetupText("edition.title"),SetupText("edition.release_date"),SetupText("edition.game_id")};
        const char *values[]={titles[edition],dates[edition],ids[edition]};
        for(int row=0;row<3;row++) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s",fields[row]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s",values[row]);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if(ImGui::BeginTable("edition-artwork",3,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchSame)) {
        for(int column=0;column<3;column++) ImGui::TableSetupColumn(SetupText(labels[column]));
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        for(int column=0;column<3;column++) {
            ImGui::TableSetColumnIndex(column);
            ImFontAtlasRect image;
            if(sEditionImageRect[edition][column]!=ImFontAtlasRectId_Invalid &&
               ImGui::GetIO().Fonts->GetCustomRect(sEditionImageRect[edition][column],&image)) {
                ImVec2 available=ImGui::GetContentRegionAvail();
                // ImageButton adds frame padding: account for it so images stay inside their columns.
                float padding=ImGui::GetStyle().FramePadding.x;
                float maxWidth=std::max(1.0f,available.x-2.0f*padding-2.0f);
                float scale=std::min(maxWidth/(float)image.w,420.0f/(float)image.h);
                float width=image.w*scale,height=image.h*scale;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,(available.x-width-2.0f*padding)*0.5f));
                ImGui::PushID(column);
                if(ImGui::ImageButton("edition-image",ImGui::GetIO().Fonts->TexRef,
                        ImVec2(width,height),image.uv0,image.uv1)) {
                    sZoomImageRect=sEditionImageRect[edition][column];
                    openViewer=true;
                }
                ImGui::PopID();
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",SetupText(labels[column]));
            } else {
                ImGui::TextWrapped("%s",SetupText("edition.back_art_unavailable"));
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    bottom_row(1);
    if(big_button(SetupText("buttons.back"),false)) {
        sZoomImageRect=ImFontAtlasRectId_Invalid;
        sPage=(Page)sDetailReturnPage;
    }

    // Open and render the modal at the same ImGui ID scope, outside the table and
    // scrolling child. Otherwise a click creates a popup ID that cannot be found.
    if(openViewer) ImGui::OpenPopup("edition-image-viewer");
    ImGuiViewport *viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size,ImGuiCond_Always);
    if(ImGui::BeginPopupModal("edition-image-viewer",NULL,
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-112.0f-ImGui::GetStyle().WindowPadding.x);
        if(ImGui::Button(SetupText("buttons.close"),ImVec2(112,30)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImFontAtlasRect image;
        if(sZoomImageRect!=ImFontAtlasRectId_Invalid &&
           ImGui::GetIO().Fonts->GetCustomRect(sZoomImageRect,&image)) {
            ImVec2 available=ImGui::GetContentRegionAvail();
            float scale=std::min(available.x/(float)image.w,available.y/(float)image.h);
            float width=image.w*scale,height=image.h*scale;
            ImVec2 cursor=ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(cursor.x+std::max(0.0f,(available.x-width)*0.5f),
                                       cursor.y+std::max(0.0f,(available.y-height)*0.5f)));
            ImGui::Image(ImGui::GetIO().Fonts->TexRef,ImVec2(width,height),image.uv0,image.uv1);
        }
        ImGui::EndPopup();
    }
}

static bool selection_reinstalls_disc() {
    for (int i = 0; i < 4; i++)
        if (disc_path(i)[0] && sDiscInstalled[i]) return true;
    return false;
}

static void confirm_install() {
#ifdef _WIN32
    if (sRelease) {
        sTasksBack=sPage;
        sPage=PAGE_TASKS;
        return;
    }
#endif
    start_install();
}

static void page_tasks() {
    heading(SetupText("tasks.title"), SetupText("tasks.subtitle"));
    ImGui::TextWrapped("%s",SetupText("tasks.description"));
    ImGui::Dummy(ImVec2(0,20));
    ImGui::TextUnformatted(SetupText("tasks.shortcuts_label"));
    ImGui::Spacing();
    ImGui::Indent(16);
    ImGui::Checkbox(SetupText("tasks.desktop_shortcut"),&sDesktopShortcut);
    ImGui::Unindent(16);
    bottom_row(3);
    if (big_button(SetupText("buttons.back"),false)) sPage=sTasksBack;
    ImGui::SameLine();
    if (big_button(SetupText("buttons.install"),true)) start_install();
    ImGui::SameLine();
    if (big_button(SetupText("buttons.cancel"),false)) sPage=sTasksBack;
}

static void page_pick() {
    heading(SetupText("discs.title"), SetupText("discs.subtitle"));
    if (!sRelease && !sRoot.empty()) {
        ImGui::TextColored(kAccent,"%s",SetupText("source.title"));
        ImGui::TextWrapped("%s",SetupText("source.description"));
    }
    ImGui::BeginChild("discs", ImVec2(0, -60));
    disc_checklist(true);
    storage_locations();
    if (sRoot.empty()) ImGui::TextColored(kRed,"%s",SetupText("failure.game_files_missing"));
    ImGui::EndChild();
    bool ok = sDiscChecked[0] && sVerifyPath[0] == sIso && !sRoot.empty();
    for (int i = 1; i < 4; i++) ok = ok && (!disc_path(i)[0] || (sDiscChecked[i] && sVerifyPath[i] == disc_path(i)));
    bottom_row(2);
    if (big_button(SetupText("buttons.back"), false)) sPage = PAGE_WELCOME;
    ImGui::SameLine();
    if (big_button(selection_reinstalls_disc() ? SetupText("buttons.reinstall") : SetupText("buttons.install"), true, ok)) {
        sEuOnly = false;
        confirm_install();
    }
}

static void page_run() {
    int done = 0;
    heading(SetupText("progress.title"), SetupText("progress.subtitle"));
    ImVec2 origin=ImGui::GetCursorScreenPos();
    origin.y+=16;
    ImGui::SetCursorScreenPos(origin);
    const StepRow *current=nullptr;
    for (const auto &r : sRows) {
        if (r.state==RUNNING) { current=&r; break; }
        if (!current && r.state==PENDING) current=&r;
    }
    const char *operation=SetupText("progress.finishing_title");
    if (current) {
        operation=current->title.c_str();
        for (const auto &h : kHelp) if (current->title==h.title) { operation=SetupText(h.operation); break; }
    }
    ImGui::PushFont(sHeaderFont,22);
    ImGui::TextWrapped("%s",operation);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+34));
    ImGui::BeginChild("operation-description",ImVec2(0,54));
    ImGui::TextWrapped("%s",current ? step_help(current->title) : SetupText("progress.finishing_description"));
    ImGui::EndChild();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+92));
    ImGui::BeginChild("operation-status",ImVec2(0,54));
    if (current && !current->detail.empty()) {
        int percent=0,files=0,total=0;
        if (sscanf(current->detail.c_str(),"%d%%, %d files",&percent,&files)==2)
            ImGui::TextColored(kDim,SetupText("progress.files_extracted"),files,percent);
        else if (sscanf(current->detail.c_str(),"%d of %d",&files,&total)==2)
            ImGui::TextColored(kDim,SetupText("progress.files_processed"),files,total);
        else
            ImGui::TextWrapped("%s",current->detail.c_str());
    }
    ImGui::EndChild();
    for (auto &r : sRows) {
        done += r.state == OK || r.state == SKIPPED;
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+154));
    float fraction=(done+sStepFraction)/(float)(sRows.empty() ? 1 : sRows.size());
    fraction=std::min(1.0f,fraction);
    ImGui::Text(SetupText("progress.percent"),(int)(fraction*100));
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+180));
    ImVec2 bar=ImGui::GetCursorScreenPos();
    float width=ImGui::GetContentRegionAvail().x;
    ImDrawList *draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(bar,ImVec2(bar.x+width,bar.y+24),IM_COL32(255,255,255,255));
    draw->AddRect(bar,ImVec2(bar.x+width,bar.y+24),IM_COL32(120,120,115,255));
    for(float x=3;x+8<=fraction*(width-3);x+=11)
        draw->AddRectFilled(ImVec2(bar.x+x,bar.y+3),ImVec2(bar.x+x+8,bar.y+21),IM_COL32(44,158,58,255));
    ImGui::Dummy(ImVec2(width,28));
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+218));
    ImGui::TextColored(kDim,SetupText("progress.completed_operations"),done,(int)sRows.size());
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+244));
    ImGui::TextColored(kDim,SetupText("progress.elapsed"),
        current && current->state==RUNNING ? (unsigned long long)((SDL_GetTicks()-current->start)/1000) : 0ULL);
    ImGui::Spacing();
    bottom_row(1);
    if (big_button(SetupText("buttons.cancel"), false)) {
        if (sProc != NULL) {
            SDL_KillProcess(sProc, false);
        }
        Native_Cancel();
        sError = SetupText("progress.cancelled");
    }
}

static void page_done() {
    heading(sGameTitle.c_str(), SetupText("installed.subtitle"));
    ImGui::BeginChild("installed-details", ImVec2(0, -90));
    disc_checklist(true);
    storage_locations();
    ImGui::EndChild();
    bool selected = false, verified = true;
    for (int i = 0; i < 4; i++) {
        if (disc_path(i)[0]) {
            selected = true;
            verified = verified && sDiscChecked[i] && sVerifyPath[i] == disc_path(i);
        }
    }
    bottom_row(2);
    if (big_button(SetupText("buttons.close"), false)) {
        sQuit = true;
    }
    ImGui::SameLine();
    if (big_button(selected ? (selection_reinstalls_disc() ? SetupText("buttons.reinstall") : SetupText("buttons.install")) : SetupText("buttons.play"), true, !selected || verified)) {
        if (selected) {
            sEuOnly = false;
            confirm_install();
            return;
        }
        if (!commit_locations()) return;
        if (!Native_Installed(sRoot)) { sPage=PAGE_PICK; return; }
        if (Setup_LaunchGame(sRoot,sLauncher)) {
            sQuit=true;
        } else {
            sError=setup_format("failure.launch_game",SDL_GetError());
            sPage=PAGE_FAILED;
        }
    }
}

static void return_to_disc_selection() {
    refresh_installed_discs();
    sEuOnly = false;
    for (int i = 0; i < 4; i++) {
        sDiscChecked[i] = false;
        sVerifyPath[i].clear();
        sVerifiedPath[i].clear();
        sVerifyError[i].clear();
        sVerifyChanged[i] = SDL_GetTicks();
    }
    if (sRelease && sDiscInstalled[0]) {
        sLauncher = sRoot + (file_exists((sRoot + GAME ".exe").c_str()) ? GAME ".exe" : GAME);
        sPage = PAGE_DONE;
    } else {
        sPage = PAGE_PICK;
    }
}

static void page_failed() {
    heading(SetupText("failure.title"), SetupText("failure.description"));
    ImGui::OpenPopup("Setup - Error");
    ImGui::SetNextWindowSize(ImVec2(580,0),ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x*0.5f,ImGui::GetIO().DisplaySize.y*0.5f),ImGuiCond_Always,ImVec2(0.5f,0.5f));
    if (ImGui::BeginPopupModal("Setup - Error",NULL,
            ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar)) {
        ImVec2 origin=ImGui::GetWindowPos();
        float popupWidth=ImGui::GetWindowSize().x;
        ImDrawList *header=ImGui::GetWindowDrawList();
        header->AddRectFilled(origin,ImVec2(origin.x+popupWidth,origin.y+74),IM_COL32(255,255,255,255));
        header->AddLine(ImVec2(origin.x,origin.y+74),ImVec2(origin.x+popupWidth,origin.y+74),IM_COL32(190,190,180,255));
        ImGui::PushFont(sHeaderFont,22);
        ImGui::TextUnformatted(SetupText("failure.title"));
        ImGui::PopFont();
        ImGui::TextColored(kDim,"%s",SetupText("failure.description"));
        ImGui::SetCursorPos(ImVec2(16,94));
        ImVec2 icon=ImGui::GetCursorScreenPos();
        ImVec2 center(icon.x+26,icon.y+26);
        ImDrawList *draw=ImGui::GetWindowDrawList();
        draw->AddCircleFilled(center,19,IM_COL32(188,38,32,255),40);
        draw->AddCircle(center,19,IM_COL32(125,24,20,255),40,1.0f);
        draw->AddLine(ImVec2(center.x-7,center.y-7),ImVec2(center.x+7,center.y+7),IM_COL32(255,255,255,255),3.0f);
        draw->AddLine(ImVec2(center.x-7,center.y+7),ImVec2(center.x+7,center.y-7),IM_COL32(255,255,255,255),3.0f);
        ImGui::Dummy(ImVec2(60,52));
        ImGui::SameLine();
        const char *message=sError.empty() ? SetupText("failure.description") : sError.c_str();
        float messageHeight=ImGui::CalcTextSize(message,NULL,false,480).y+16;
        if (!sMissing.empty()) messageHeight+=60+sMissing.size()*24;
        messageHeight=std::max(64.0f,std::min(240.0f,messageHeight));
        ImGui::BeginChild("error-message",ImVec2(0,messageHeight));
        ImGui::TextWrapped("%s",sError.empty() ? SetupText("failure.description") : sError.c_str());
        if (!sMissing.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s",SetupText("failure.missing_packages"));
            for (const auto &m : sMissing) ImGui::BulletText("%s (%s)",m.name.c_str(),m.pkg.c_str());
        }
        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        float buttons=224+ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-buttons);
        if (big_button(SetupText("buttons.back"),true)) {
            ImGui::CloseCurrentPopup();
            return_to_disc_selection();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (big_button(SetupText("buttons.cancel"),false)) {
            ImGui::CloseCurrentPopup();
            sQuit=true;
        }
        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

static void page_welcome() {
    ImGui::Dummy(ImVec2(0,22));
    ImGui::PushFont(sHeaderFont,28);
    ImGui::TextWrapped("%s",SetupText("welcome.title"));
    ImGui::PopFont();
    ImGui::Spacing(); ImGui::Spacing();
    ImGui::TextWrapped(SetupText("welcome.description"),sGameTitle.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("%s",SetupText("welcome.discs"));
    ImGui::Spacing();
    ImGui::TextWrapped("%s",SetupText("welcome.close_game"));
    ImGui::Spacing(); ImGui::Spacing();
    ImGui::TextWrapped("%s",SetupText("welcome.continue"));
    bottom_row(2);
    if(big_button(SetupText("buttons.next"),true)) sPage=PAGE_PICK;
    ImGui::SameLine();
    if(big_button(SetupText("buttons.cancel"),false)) sQuit=true;
}

static void build_ui() {
    ImGuiIO &io=ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("setup",NULL,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    ImDrawList *draw=ImGui::GetWindowDrawList();
    float footer=io.DisplaySize.y-62;
    const float panel=sPage==PAGE_EDITION ? 0.0f : 224.0f;
    bool welcome=sPage==PAGE_WELCOME;
    bool usDiscReady=sDiscChecked[0] || sDiscInstalled[0];
    bool showCover=welcome || sPage==PAGE_PICK || sPage==PAGE_TASKS ||
                   sPage==PAGE_RUN || sPage==PAGE_DONE;
    draw->AddRectFilled(ImVec2(0,0),ImVec2(io.DisplaySize.x,footer),welcome ? IM_COL32(255,255,255,255) : IM_COL32(238,237,230,255));
    draw->AddRectFilled(ImVec2(0,footer),io.DisplaySize,IM_COL32(238,237,230,255));
    draw->AddLine(ImVec2(0,footer),ImVec2(io.DisplaySize.x,footer),IM_COL32(160,160,150,255));
    draw->AddLine(ImVec2(0,footer+1),ImVec2(io.DisplaySize.x,footer+1),IM_COL32(255,255,255,255));
    if(showCover) {
        ImFontAtlasRect cover;
        bool hasCover=sCoverRect!=ImFontAtlasRectId_Invalid && ImGui::GetIO().Fonts->GetCustomRect(sCoverRect,&cover);
        if(hasCover) {
            float fraction=(panel/footer)/((float)cover.w/cover.h);
            float left=(1.0f-fraction)*0.5f;
            ImVec2 uv0(cover.uv0.x+(cover.uv1.x-cover.uv0.x)*left,cover.uv0.y);
            ImVec2 uv1(cover.uv1.x-(cover.uv1.x-cover.uv0.x)*left,cover.uv1.y);
            draw->AddImage(ImGui::GetIO().Fonts->TexRef,ImVec2(0,0),ImVec2(panel,footer),uv0,uv1);
            draw->AddRectFilledMultiColor(ImVec2(0,footer-130),ImVec2(panel,footer),IM_COL32(0,0,0,0),IM_COL32(0,0,0,0),IM_COL32(0,0,0,220),IM_COL32(0,0,0,220));
        }
        if(usDiscReady) load_title_logos();
        ImFontAtlasRect logo;
        int region=sSelectedTitleLogo>=0 ? sSelectedTitleLogo : (sDiscChecked[3] || sDiscInstalled[3]) ? 1 : 0;
        if(sTitleLogo[region]==ImFontAtlasRectId_Invalid) region=1-region;
        if(sTitleLogo[region]!=ImFontAtlasRectId_Invalid && ImGui::GetIO().Fonts->GetCustomRect(sTitleLogo[region],&logo)) {
            float w=panel-4,h=w*logo.h/logo.w;
            if(h>170) { w*=170/h; h=170; }
            float top=hasCover ? footer-h-10 : (footer-h)*0.5f;
            draw->AddImage(ImGui::GetIO().Fonts->TexRef,ImVec2((panel-w)*0.5f,top),
                ImVec2((panel+w)*0.5f,top+h),logo.uv0,logo.uv1);
            if(sTitleLogo[0]!=ImFontAtlasRectId_Invalid && sTitleLogo[1]!=ImFontAtlasRectId_Invalid) {
                ImGui::SetCursorScreenPos(ImVec2((panel-w)*0.5f,top));
                if(ImGui::InvisibleButton("switch-title-logo",ImVec2(w,h))) sSelectedTitleLogo=1-region;
                if(ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        }
    }
    /* Keep the cover column reserved on other pages even when its artwork is hidden. */
    ImGui::SetCursorPos(ImVec2(panel+28,36));
    ImGui::BeginChild("page-content",ImVec2(io.DisplaySize.x-ImGui::GetCursorPosX()-18,0));
    switch(sPage) {
    case PAGE_WELCOME: page_welcome(); break;
    case PAGE_PICK: page_pick(); break;
    case PAGE_TASKS: page_tasks(); break;
    case PAGE_RUN: page_run(); break;
    case PAGE_DONE: page_done(); break;
    case PAGE_FAILED: page_failed(); break;
    case PAGE_EDITION: page_edition(); break;
    }
    ImGui::EndChild();
    ImGui::PushFont(NULL,13);
    bool checking=sUpdateCheck.valid();
    const char *updateLabel=checking ? SetupText("updates.checking") : SetupText("updates.check");
    std::string versionLabel=sInstalledVersion.empty() ? SetupText("version.unavailable") : setup_format("version.number",sInstalledVersion.c_str());
    // Reserve the full link width even while checking, so the version never shifts.
    float updateWidth=ImGui::CalcTextSize(SetupText("updates.check")).x;
    float versionWidth=ImGui::CalcTextSize(versionLabel.c_str()).x;
    float updateX=io.DisplaySize.x-18-updateWidth;
    ImGui::SetCursorPos(ImVec2(updateX-versionWidth-16,12));
    ImGui::TextColored(kDim,"%s",versionLabel.c_str());
    ImGui::SetCursorPos(ImVec2(updateX,12));
    ImGui::BeginDisabled(checking || sUpdatePrepare.valid() || sPage==PAGE_RUN);
    if (ImGui::TextLink(updateLabel))
        sUpdateCheck=std::async(std::launch::async,SetupUpdate_Check,sInstalledVersion,sSourceUrl);
    ImGui::EndDisabled();
    ImGui::PopFont();
    if(!sPageUrl.empty() || !sOriginalSourceUrl.empty()) {
        ImGui::SetCursorPos(ImVec2(18,footer+22));
        ImGui::PushFont(NULL,13);
        if(!sPageUrl.empty()) ImGui::TextLinkOpenURL(SetupText("links.project"),sPageUrl.c_str());
        if(!sOriginalSourceUrl.empty()) {
            if(!sPageUrl.empty()) ImGui::SameLine();
            ImGui::TextLinkOpenURL(SetupText("links.original_project"),sOriginalSourceUrl.c_str());
        }
        ImGui::PopFont();
    }
    update_dialog();
    ImGui::End();
}

// ---------------------------------------------------------------------------------------------------- the window

// ZENKAIPOWER_SETUP_SHOT: the picture drawn a second time into a texture and written as a PPM file.
static void shot(ImDrawData *dd, Uint32 w, Uint32 h, int number) {
    SDL_GPUTextureFormat fmt = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
    bool bgr = fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM || fmt == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUColorTargetInfo target;
    SDL_GPUTextureRegion src;
    SDL_GPUTextureTransferInfo dst;
    char name[512];

    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = fmt;
    ci.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width = w;
    ci.height = h;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    SDL_GPUTexture *tex = SDL_CreateGPUTexture(sDevice, &ci);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(target);
    target.texture = tex;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
    SDL_zero(ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ti.size = w * h * 4;
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(sDevice, &ti);
    SDL_zero(src);
    src.texture = tex;
    src.w = w;
    src.h = h;
    src.d = 1;
    SDL_zero(dst);
    dst.transfer_buffer = tb;
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(cp, &src, &dst);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(sDevice, true, &fence, 1);
    SDL_ReleaseGPUFence(sDevice, fence);
    const Uint8 *px = (const Uint8 *)SDL_MapGPUTransferBuffer(sDevice, tb, false);
    snprintf(name, sizeof(name), "%s_%d.ppm", getenv("ZENKAIPOWER_SETUP_SHOT"), number);
    FILE *fp = fopen(name, "wb");
    if (fp != NULL && px != NULL) {
        fprintf(fp, "P6\n%u %u\n255\n", w, h);
        for (Uint32 i = 0; i < w * h; i++) {
            Uint8 rgb[3] = {px[i * 4 + (bgr ? 2 : 0)], px[i * 4 + 1], px[i * 4 + (bgr ? 0 : 2)]};
            fwrite(rgb, 1, 3, fp);
        }
        fclose(fp);
    }
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    SDL_ReleaseGPUTexture(sDevice, tex);
}

#ifdef _WIN32
// The few functions of Windows used below, declared here: its headers define names this file has (FAILED, ...).
extern "C" {
__declspec(dllimport) wchar_t *__stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t **__stdcall CommandLineToArgvW(const wchar_t *line, int *count);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned page, unsigned long flags, const wchar_t *wide, int wideLen, char *out, int outLen,
                                                        const char *def, int *usedDef);
__declspec(dllimport) void *__stdcall LocalFree(void *mem);
}

// The program's arguments in UTF-8. Those main() gets on Windows are in the system's code page: a disc image whose
// path has letters outside it arrives with '?' in their place, and one with letters inside it as bytes that are not
// UTF-8, which is what SDL opens files by. Either way the image was "not found". NULL when Windows gives none.
static char **utf8_args(int *count) {
    int n = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &n);
    char **out = wide != NULL ? (char **)SDL_calloc((size_t)n + 1, sizeof(char *)) : NULL;

    for (int i = 0; out != NULL && i < n; i++) {
        int len = WideCharToMultiByte(65001 /* CP_UTF8 */, 0, wide[i], -1, NULL, 0, NULL, NULL);
        out[i] = (char *)SDL_calloc((size_t)(len > 0 ? len : 1), 1);
        if (out[i] == NULL) {
            out = NULL; // (out of memory at start-up: main's own arguments are used)
            break;
        }
        if (len > 0) {
            WideCharToMultiByte(65001, 0, wide[i], -1, out[i], len, NULL, NULL);
        }
    }
    if (wide != NULL) {
        LocalFree(wide);
    }
    *count = n;
    return out;
}
#endif

int main(int argc, char **argv) {
    ImGui_ImplSDLGPU3_InitInfo info;
    int shots = 0, shotKey = -1;

#ifdef _WIN32
    {
        int count = 0;
        char **args = utf8_args(&count); // (kept for the whole run)
        if (args != NULL) {
            argc = count;
            argv = args;
        }
    }
#endif
    SetupText_Load(SDL_GetBasePath() ? SDL_GetBasePath() : "./");
    int updateResult = SetupUpdate_Helper(argc, argv);
    if (updateResult >= 0) return updateResult;
    if (argc > 2 && strcmp(argv[1], "--install") == 0) {
        // Without a window: --install <USA.iso> [<PS2 EU.iso>] [<Wii EU.iso/rvz>] [<PS2 JP.iso>].
        // An empty argument omits a disc when adding content to an existing installation. Release folders only.
        std::string line;
        bool ok = false;
        SDL_Init(0);
        sRoot = find_root();
        if (!sRelease) {
            fprintf(stderr, SetupText("cli.release_required"),GAME,GAME,GAME);
            return 2;
        }
        Native_Start(sRoot, argv[2], argc > 3 ? argv[3] : "", argc > 4 ? argv[4] : "", argc > 5 ? argv[5] : "");
        for (bool last = false; !last;) {
            last = !Native_Running(); // one more round after the worker has ended: nothing is left behind
            while (Native_Poll(line)) {
                printf("%s\n", line.c_str());
                fflush(stdout);
                ok = ok || line.rfind("@done", 0) == 0;
            }
            SDL_Delay(50);
        }
        Native_Join();
        return ok ? 0 : 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, SetupText("cli.initialization_error"),GAME,SDL_GetError());
        return 1;
    }
    const char *glsl = "#version 330";
    {
        const char *api = getenv("ZENKAIPOWER_GPU_API");
        bool wantGl = api != NULL && (strcmp(api, "gl") == 0 || strcmp(api, "opengl") == 0);
        std::string why;
        if (!wantGl) {
            sWindow = SDL_CreateWindow(setup_format("window.title",GAME).c_str(), 820, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY);
            sDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, false, NULL);
            if (sWindow == NULL || sDevice == NULL || !SDL_ClaimWindowForGPUDevice(sDevice, sWindow)) {
                why = SDL_GetError();
                if (sDevice != NULL) {
                    SDL_DestroyGPUDevice(sDevice);
                    sDevice = NULL;
                }
                if (sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
        }
        if (sDevice == NULL) {
            // OpenGL 3.3 core, and failing that whatever 3.0 context the driver gives
            for (int attempt = 0; attempt < 2 && sGl == NULL; attempt++) {
                SDL_GL_ResetAttributes();
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, attempt == 0 ? 3 : 0);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, attempt == 0 ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                glsl = attempt == 0 ? "#version 330" : "#version 130";
                sWindow = SDL_CreateWindow(setup_format("window.title",GAME).c_str(), 820, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL);
                sGl = sWindow != NULL ? SDL_GL_CreateContext(sWindow) : NULL;
                if (sGl == NULL && sWindow != NULL) {
                    SDL_DestroyWindow(sWindow);
                    sWindow = NULL;
                }
            }
            if (sGl == NULL) {
                fprintf(stderr, SetupText("cli.window_error"),GAME,
                        why.c_str(), why.empty() ? "" : SetupText("cli.opengl_separator"), SDL_GetError(),GAME);
                return 1;
            }
            if (!wantGl) {
                fprintf(stderr, SetupText("cli.opengl_fallback"),GAME,why.c_str());
            }
            SDL_GL_MakeCurrent(sWindow, sGl);
            SDL_GL_SetSwapInterval(1);
            sUseGl = true;
        } else {
            SDL_SetGPUSwapchainParameters(sDevice, sWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
        }
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    // The keyboard reaches every control: Tab and the arrows move, Space or Enter presses. Without this, Tab stops at
    // the text field only and no button can be pressed without a mouse.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    style();
    sRoot = find_root();
    load_locations();
    io.FontDefault = load_dejavu("DejaVuSansCondensed.ttf");
    sHeaderFont = load_dejavu("DejaVuSansCondensed-Bold.ttf");
    load_cover();
    load_disc_icons();
    load_edition_images();
    if (sUseGl) {
        ImGui_ImplSDL3_InitForOpenGL(sWindow, sGl);
        ImGui_ImplOpenGL3_Init(glsl);
    } else {
        ImGui_ImplSDL3_InitForSDLGPU(sWindow);
        info.Device = sDevice;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(sDevice, sWindow);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        ImGui_ImplSDLGPU3_Init(&info);
    }
    sRoot = find_root();
    load_game_info();
    refresh_installed_discs();
    if (sRelease && Native_Installed(sRoot) && getenv("ZENKAIPOWER_SETUP_ISO") == NULL) { // nothing left to do: offer Play
        reset_rows();
        for (auto &r : sRows) {
            r.state = SKIPPED;
            r.detail = "already done";
        }
        sLauncher = sRoot + (file_exists((sRoot + GAME ".exe").c_str()) ? GAME ".exe" : GAME);
        sPage = PAGE_DONE;
        if (Native_NeedsShellAssets(sRoot)) start_install();
    }
    if (argc > 1) {
        SDL_strlcpy(sIso, argv[1], sizeof(sIso));
    }
    if (getenv("ZENKAIPOWER_SETUP_WII_ISO")) SDL_strlcpy(sIsoWii, getenv("ZENKAIPOWER_SETUP_WII_ISO"), sizeof(sIsoWii));
    if (getenv("ZENKAIPOWER_SETUP_JP_ISO")) SDL_strlcpy(sIsoJp, getenv("ZENKAIPOWER_SETUP_JP_ISO"), sizeof(sIsoJp));
    if (getenv("ZENKAIPOWER_SETUP_EU_ISO") != NULL) { // (testing, with ZENKAIPOWER_SETUP_ISO: the European image too)
        SDL_strlcpy(sIsoEu, getenv("ZENKAIPOWER_SETUP_EU_ISO"), sizeof(sIsoEu));
    }
    if (getenv("ZENKAIPOWER_SETUP_ISO") != NULL) {
        SDL_strlcpy(sIso, getenv("ZENKAIPOWER_SETUP_ISO"), sizeof(sIso));
        if (!sRoot.empty() && file_exists(sIso)) {
            start_install();
        }
    }
    if (getenv("ZENKAIPOWER_SETUP_WELCOME")) sPage=PAGE_WELCOME;
    if (getenv("ZENKAIPOWER_SETUP_ERROR_PREVIEW")) {
        sError=SetupText("preview.launcher_error");
        sPage=PAGE_FAILED;
    }
    if (getenv("ZENKAIPOWER_SETUP_UPDATE_PREVIEW")) {
        sUpdateMessage=setup_format("updates.current",sInstalledVersion.c_str());
        sShowUpdate=true;
    }
    while (!sQuit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT || ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                sQuit = true;
            } else if (ev.type == SDL_EVENT_DROP_FILE && (sPage == PAGE_PICK || sPage == PAGE_DONE) && ev.drop.data != NULL) {
                int target = 0;
                while (target < 3 && disc_path(target)[0]) target++;
                SDL_strlcpy(disc_path(target), ev.drop.data, 1024);
                sDiscChecked[target] = false;
            }
        }
        {
            std::lock_guard<std::mutex> lock(sDialogLock);
            if (sDialogReady) {
                if (sDialogFor==4 || sDialogFor==5) {
                    SDL_strlcpy(sDialogFor==4 ? sDataFolder : sSaveFolder,sDialogResult.c_str(),1024);
                    sLocationsDirty=true;
                } else {
                    SDL_strlcpy(disc_path(sDialogFor), sDialogResult.c_str(), 1024);
                    sDiscChecked[sDialogFor] = false;
                }
                sDialogReady = false;
            }
        }
        pump_verification();
        pump_install();
        if (sUseGl) {
            ImGui_ImplOpenGL3_NewFrame();
        } else {
            ImGui_ImplSDLGPU3_NewFrame();
        }
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        build_ui();
        ImGui::Render();
        ImDrawData *dd = ImGui::GetDrawData();
        if (sUseGl) {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(sWindow, &pw, &ph);
            glViewport(0, 0, pw, ph);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(dd);
            if (getenv("ZENKAIPOWER_SETUP_SHOT") != NULL && ImGui::GetFrameCount() == 20 && pw > 0 && ph > 0) {
                // testing: the picture of the OpenGL window, <ZENKAIPOWER_SETUP_SHOT>_gl.ppm (rows come bottom first)
                std::vector<unsigned char> px((size_t)pw * ph * 4);
                char name[512];
                glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                snprintf(name, sizeof(name), "%s_gl.ppm", getenv("ZENKAIPOWER_SETUP_SHOT"));
                FILE *fp = fopen(name, "wb");
                if (fp != NULL) {
                    fprintf(fp, "P6\n%d %d\n255\n", pw, ph);
                    for (int y = ph - 1; y >= 0; y--) {
                        for (int x = 0; x < pw; x++) {
                            fwrite(&px[((size_t)y * pw + x) * 4], 1, 3, fp);
                        }
                    }
                    fclose(fp);
                }
            }
            SDL_GL_SwapWindow(sWindow);
            if (getenv("ZENKAIPOWER_SETUP_QUIT") != NULL && ImGui::GetFrameCount() > 30) {
                sQuit = true; // testing: a few frames of the OpenGL window, then leave
            }
            continue;
        }
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sDevice);
        SDL_GPUTexture *swap = NULL;
        Uint32 w = 0, h = 0;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, sWindow, &swap, &w, &h) && swap != NULL) {
            SDL_GPUColorTargetInfo target;
            ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
            SDL_zero(target);
            target.texture = swap;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, NULL);
            ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
            SDL_EndGPURenderPass(pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
        if (getenv("ZENKAIPOWER_SETUP_SHOT") != NULL && w != 0 && ImGui::GetFrameCount() > 3) {
            int running = 0;
            for (size_t i = 0; i < sRows.size(); i++) {
                running += sRows[i].state != PENDING;
            }
            int key = (int)sPage * 100 + running;
            if (key != shotKey) {
                shotKey = key;
                shot(dd, w, h, shots++);
            }
            if (getenv("ZENKAIPOWER_SETUP_QUIT") != NULL && (sPage == PAGE_DONE || sPage == PAGE_FAILED || ((sPage == PAGE_PICK || sPage == PAGE_WELCOME) && sProc == NULL && shots > 0 && getenv("ZENKAIPOWER_SETUP_ISO") == NULL))) {
                shot(dd, w, h, shots++);
                sQuit = true; // testing: leave once the result is on screen
            }
        }
    }
    if (sProc != NULL) {
        SDL_KillProcess(sProc, false);
        SDL_DestroyProcess(sProc);
    }
    for (auto &verification : sVerification) if (verification.valid()) verification.wait();
    Native_Join();
    if (sUseGl) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(sGl);
        SDL_DestroyWindow(sWindow);
        SDL_Quit();
        return 0;
    }
    SDL_WaitForGPUIdle(sDevice);
    ImGui_ImplSDL3_Shutdown();
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    SDL_ReleaseWindowFromGPUDevice(sDevice, sWindow);
    SDL_DestroyGPUDevice(sDevice);
    SDL_DestroyWindow(sWindow);
    SDL_Quit();
    return 0;
}
