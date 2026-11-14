#ifndef ZENKAI_BOT_PREFERENCES_H
#define ZENKAI_BOT_PREFERENCES_H
#include "process.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

class BotPreferences {
    std::filesystem::path directory;
    std::string previous, previousAccess;
    bool write(const char *name, const std::string &bytes) {
        auto target = directory / name, temporary = directory / (std::string(name) + ".tmp");
#ifdef _WIN32
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), bytes.size()); out.close();
        if (!out) return false;
#else
        int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) return false;
        if (fchmod(fd, 0600)) { close(fd); return false; }
        size_t at = 0;
        while (at < bytes.size()) {
            ssize_t n = ::write(fd, bytes.data() + at, bytes.size() - at);
            if (n <= 0) { close(fd); return false; }
            at += (size_t)n;
        }
        bool ok = fsync(fd) == 0;
        if (close(fd)) ok = false;
        if (!ok) return false;
#endif
        return SDL_RenamePath(temporary.u8string().c_str(), target.u8string().c_str());
    }
    std::string read(const char *name) {
        auto path = directory / name;
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in || in.tellg() < 0 || in.tellg() > 16384) return "";
        size_t size = (size_t)in.tellg(); in.seekg(0);
        std::string bytes(size, '\0'); in.read(bytes.data(), size);
        return in ? bytes : "";
    }
    static std::string encode(const BotOptions &o, bool scroll) {
        std::ostringstream out;
        out << "ZENKAIBOT1\n" << std::quoted(o.game) << '\n' << std::quoted(o.name) << '\n'
            << std::quoted(o.host) << '\n' << std::quoted(o.code) << '\n'
            << o.connection << ' ' << o.mode << ' ' << o.level << ' ' << o.ranked_retry << ' ' << scroll << '\n' << o.ranked_delay << ' ' << o.ranked_delay_seconds << '\n';
        return out.str();
    }
public:
    std::string error;
    explicit BotPreferences(const std::filesystem::path &path): directory(path) {}
    static std::filesystem::path location() {
        return BotDirectory();
    }
    void load(BotOptions &options, bool &scroll) {
        std::string text = read("settings.cfg"), magic;
        std::istringstream in(text);
        BotOptions next; bool nextScroll; bool validSettings = false;
        if (std::getline(in, magic) && magic == "ZENKAIBOT1" &&
            (in >> std::quoted(next.game) >> std::quoted(next.name) >> std::quoted(next.host) >> std::quoted(next.code)
                >> next.connection >> next.mode >> next.level >> next.ranked_retry >> nextScroll) &&
            next.game.size() < 2048 && next.name.size() < 64 && next.host.size() < 256 && next.code.size() < 16 &&
            next.connection >= 0 && next.connection <= 3 && next.mode >= 0 && next.mode <= 5 && next.level >= 1 && next.level <= 5) {
            bool delay; int seconds;
            if ((in >> delay >> seconds) && seconds >= 0 && seconds <= 3600) {
                next.ranked_delay = delay; next.ranked_delay_seconds = seconds;
            }
            options = next; scroll = nextScroll; validSettings = true;
        }
        std::string secret = validSettings ? read("access.dat") : "";
        options.access = secret;
        if (!options.access.empty() && !BotCode(options.access)) options.access.clear();
        previous = encode(options, scroll); previousAccess = options.access;
    }
    bool save(const BotOptions &options, bool scroll) {
        auto text = encode(options, scroll);
        if (text == previous && options.access == previousAccess) return true;
        if (directory.empty()) { error = "Cannot locate bot preferences folder"; return false; }
        std::error_code ec; std::filesystem::create_directories(directory, ec);
        if (ec) { error = "Cannot create bot preferences folder"; return false; }
#ifndef _WIN32
        // Do not change permissions on the directory containing the executable.
#endif
        if (text != previous && !write("settings.cfg", text)) { error = "Cannot save bot options"; return false; }
        if (options.access != previousAccess) {
            if (options.access.empty()) {
                std::filesystem::remove(directory / "access.dat", ec);
                if (ec) { error = "Cannot clear saved access code"; return false; }
            } else {
                std::string secret = options.access;
                if (!write("access.dat", secret)) { error = "Cannot save access code"; return false; }
            }
        }
        previous = text; previousAccess = options.access; error.clear(); return true;
    }
};
#endif
