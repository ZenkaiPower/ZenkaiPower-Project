// UI recovery regression. Link with the installer's native and ImGui objects.
#define main installer_application_main
#include "../../setup/setup.cpp"
#undef main
#include <filesystem>
#include <cassert>
int main() {
    auto dir = std::filesystem::temp_directory_path() /
        ("zenkaipower-retry-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directories(dir / "gamedata");
    sRoot = dir.u8string() + "/";
    sRelease = true;
    sPage = PAGE_FAILED;
    SDL_strlcpy(sIsoEu, "optional.iso", sizeof(sIsoEu));
    sDiscChecked[1] = true;
    sVerifyError[1] = "previous failure";
    return_to_disc_selection();
    assert(sPage == PAGE_PICK); // First installation still needs the USA disc.
    FILE *marker = fopen((sRoot + "gamedata/.installed").c_str(), "wb");
    assert(marker); fclose(marker);
    sPage = PAGE_FAILED;
    sEuOnly = true;
    return_to_disc_selection();
    assert(sPage == PAGE_DONE); // Existing installs can retry an optional disc alone.
    assert(!sIso[0] && std::string(sIsoEu) == "optional.iso");
    assert(!sEuOnly && !sDiscChecked[1] && sVerifyError[1].empty());
    assert(sVerifyPath[1].empty() && sVerifiedPath[1].empty());
    assert(!sLauncher.empty());
    std::filesystem::remove_all(dir);
    puts("PASS: first-install and optional-only retry routes, selection preservation and fresh verification");
}
