#ifndef SETUP_LOCATIONS_H
#define SETUP_LOCATIONS_H
#include "../src/port_locations.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

static inline std::string Setup_DataRoot(const std::string &root) {
#ifndef ZENKAIPOWER_ANDROID_INSTALLER
    char custom[2048];
    if (Port_LocationRead(root.c_str(), "data", custom, sizeof(custom)) && custom[0])
        return std::string(custom) + "/";
#endif
    return root + "gamedata/";
}

static inline bool Setup_SaveLocations(const std::string &root, const std::string &data,
                                       const std::string &saves, std::string &error) {
    namespace fs = std::filesystem;
    try {
        for (const auto &path : {data, saves}) {
            if (path.empty()) continue;
            if (path.size() >= 400 || path.find_first_of("\r\n") != std::string::npos ||
                !fs::u8path(path).is_absolute()) throw std::runtime_error("Choose an absolute folder path.");
            fs::create_directories(fs::u8path(path));
            if (!fs::is_directory(fs::u8path(path))) throw std::runtime_error("The selected path is not a folder.");
            auto probe = fs::u8path(path) / (".zenkaipower-write-test-" + std::to_string(SDL_GetTicksNS()));
            std::ofstream test(probe, std::ios::binary);
            if (!test) throw std::runtime_error("The selected folder is not writable.");
            test.close(); fs::remove(probe);
        }
        auto target = fs::u8path(root) / "locations.ini";
        auto temporary = target; temporary += ".part";
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << "data=" << data << "\nsaves=" << saves << "\n";
        file.close();
        if (!file || !SDL_RenamePath(temporary.u8string().c_str(), target.u8string().c_str()))
            throw std::runtime_error("Could not save folder locations beside the game executable.");
        error.clear(); return true;
    } catch (const std::exception &e) { error = e.what(); return false; }
}
#endif
