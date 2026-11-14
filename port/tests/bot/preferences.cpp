#include "../../bot/preferences.h"
#include <cassert>
int main() {
    SDL_Init(0);
    assert(BotPreferences::location() == std::filesystem::u8path(SDL_GetBasePath()));
    auto dir = std::filesystem::temp_directory_path() / ("zenkaipower-bot-prefs-" + std::to_string(SDL_GetPerformanceCounter()));
    BotOptions options; options.game = "/a path/game"; options.name = "RememberMe";
    options.code = "XYZ789"; options.access = "ABC123"; options.host = "192.0.2.1";
    options.connection = 2; options.mode = 5; options.level = 3; options.ranked_retry = false; options.ranked_delay = false; options.ranked_delay_seconds = 17;
    BotPreferences store(dir); assert(store.save(options, false));
    BotPreferences restored(dir); BotOptions value; bool scroll = true;
    restored.load(value, scroll);
    assert(value.game == options.game && value.name == options.name && value.access == options.access);
    assert(value.host == options.host && value.code == options.code);
    assert(value.connection == 2 && value.mode == 5 && value.level == 3 && !value.ranked_retry && !scroll);
    assert(!value.ranked_delay && value.ranked_delay_seconds == 17);
    std::ifstream in(dir / "settings.cfg"); std::string contents((std::istreambuf_iterator<char>(in)), {});
    assert(contents.find(options.access) == std::string::npos);
#ifndef _WIN32
    assert((std::filesystem::status(dir / "access.dat").permissions() & std::filesystem::perms::group_all) == std::filesystem::perms::none);
    assert((std::filesystem::status(dir / "access.dat").permissions() & std::filesystem::perms::others_all) == std::filesystem::perms::none);
#endif
    value.access.clear(); assert(restored.save(value, true)); assert(!std::filesystem::exists(dir / "access.dat"));
    { std::ofstream broken(dir / "settings.cfg"); broken << "ZENKAIBOT1\ninvalid data"; }
    BotOptions defaults; scroll = true; BotPreferences corrupt(dir); corrupt.load(defaults, scroll);
    assert(defaults.name == "SparkingBot" && defaults.level == 5 && defaults.access.empty());
    std::filesystem::remove_all(dir); SDL_Quit();
    puts("PASS: bot options and access code round trip, separate secret, permissions, clearing and corrupt settings");
}
