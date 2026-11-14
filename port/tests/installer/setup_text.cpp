// Run with text.cpp and a generated setup_text_defaults.h; no game data is needed.
#include "../../setup/text.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <SDL3/SDL.h>

int main() {
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/std::string("setup-text-test-"+std::to_string(SDL_GetPerformanceCounter()));
    fs::create_directories(root/"setup");
    std::string original=SetupText("welcome.title");
    std::string progress=SetupText("progress.percent");
    std::ofstream(root/"setup/en.json") << R"({
      "welcome.title": "Custom setup\n100% ready — Olá",
      "progress.percent": "Progress: %s",
      "buttons.install": false,
      "unknown.key": "Ignored"
    })";
    SetupText_Load(root.u8string());
    assert(std::string(SetupText("welcome.title"))=="Custom setup\n100% ready — Olá");
    assert(std::string(SetupText("progress.percent"))==progress);
    assert(std::string(SetupText("buttons.install"))=="Install");
    assert(std::string(SetupText("unknown.key"))=="unknown.key");
    std::ofstream(root/"setup/en.json") << R"({"progress.percent":"Completed: %d%%"})";
    SetupText_Load(root.u8string());
    assert(std::string(SetupText("progress.percent"))=="Completed: %d%%");
    std::ofstream(root/"setup/en.json") << "{invalid";
    SetupText_Load(root.u8string());
    assert(std::string(SetupText("progress.percent"))=="Completed: %d%%");
    fs::remove_all(root);
    return 0;
}
