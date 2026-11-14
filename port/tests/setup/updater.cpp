// Include implementation to exercise archive validation without a public test-only API.
#include "../../setup/updater.cpp"
#include <cassert>
#include <iostream>
int main(int argc, char **argv) {
    if (argc>1 && std::string(argv[1])=="zip-windows") {
        archiveFolder="ZenkaiPower-Windows"; game="ZenkaiPower.exe"; setup="ZenkaiPower-setup.exe";
    }
    if (argc>1 && (std::string(argv[1])=="zip" || std::string(argv[1])=="zip-windows")) {
        try { validateZip(argv[2]); return 0; } catch (const std::exception &e) { std::cerr << e.what(); return 1; }
    }
    if (argc>1 && std::string(argv[1])=="check") {
        SetupUpdate r=SetupUpdate_Check("1.0.0", "https://github.com/example/game");
        std::cout << r.available << "|" << r.version << "|" << r.error;
        return 0;
    }
    if (argc>1 && std::string(argv[1])=="--apply-update") return SetupUpdate_Helper(argc,argv);
    if (argc>1 && std::string(argv[1])=="prepare") {
        SetupUpdate u; u.available=true; u.url="https://github.com/example/game/releases/download/v1.1.0/ZenkaiPower-linux-x64.zip";
        u.digest=argv[3];
        std::string error=SetupUpdate_Prepare(u,argv[2]);
        std::cerr << error;
        return error.empty() ? 0 : 1;
    }
    assert(SetupUpdate_Newer("v1.0.10","1.0.9"));
    assert(SetupUpdate_Repository("https://github.com/example/game.git/")=="https://github.com/example/game");
    assert(SetupUpdate_Repository("https://github.com/another/project")=="https://github.com/another/project");
    assert(SetupUpdate_Repository("https://github.com/example/game/releases").empty());
    assert(SetupUpdate_Repository("https://github.com.evil.com/example/game").empty());
    assert(SetupUpdate_Repository("https://github.com/example/..").empty());
    assert(!SetupUpdate_Check("1.0.0", "").error.empty());
    assert(!SetupUpdate_Newer("v1.0.0","1.0"));
    assert(!SetupUpdate_Newer("1.0.0","1.1.0"));
    assert(!SetupUpdate_Newer("latest","1.0.0"));
    assert(!SetupUpdate_Newer("v1.1.0-beta","1.0.0"));
    assert(!SetupUpdate_Newer("1.1.0",""));
    assert(!owned("savegame/settings.ini"));
    assert(!owned("gamedata/data.afs"));
    assert(!owned("textures/user.dds"));
    assert(!owned("bot/preferences.json"));
    assert(owned("lib/libSDL3.so.0"));
    assert(owned(std::string(game) + ".mem"));
    assert(!owned("other.exe.mem"));
    assert(!owned("nested/" + std::string(game) + ".mem"));
    assert(owned("setup/en.json"));
    assert(owned("setup.pak"));
    assert(!owned("setup/other.json"));
    std::cout << "PASS: numeric versions and preserved paths\n";
}
