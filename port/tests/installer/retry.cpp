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
    SDL_strlcpy(sDataFolder,(dir/"custom data").u8string().c_str(),sizeof(sDataFolder));
    SDL_strlcpy(sSaveFolder,(dir/"custom saves").u8string().c_str(),sizeof(sSaveFolder));
    sCustomData=sCustomSaves=sLocationsDirty=true;
    assert(commit_locations());
    assert(!sDiscInstalled[0]); // Existing local data must not count in the new location.
    assert(!sLocationsDirty);
    marker=fopen((Setup_DataRoot(sRoot)+".installed").c_str(),"wb");
    assert(marker);fclose(marker);
    return_to_disc_selection();
    assert(sPage==PAGE_DONE && sDiscInstalled[0]);
    sCustomData=sCustomSaves=false;
    load_locations();
    assert(sCustomData && sCustomSaves);
    assert(std::string(sDataFolder)==(dir/"custom data").u8string());
    sCustomData=sCustomSaves=false;sLocationsDirty=true;
    assert(commit_locations());
    assert(Setup_DataRoot(sRoot)==sRoot+"gamedata/");
    std::filesystem::remove_all(dir);
    puts("PASS: first-install and optional-only retry routes, selection preservation and fresh verification");
}
