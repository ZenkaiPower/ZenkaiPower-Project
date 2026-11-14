#include "../../bot/process.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    SDL_Init(0);
    if (getenv("ZENKAIPOWER_NET_BOT")) {
        assert(std::string(getenv("ZENKAIPOWER_GS")) == "none");
        assert(std::string(getenv("ZENKAIPOWER_NOSOUND")) == "1");
        assert(std::string(getenv("ZENKAIPOWER_NET_NAME")) == "TestBot");
        assert(std::string(getenv("ZENKAIPOWER_NET_BOT_LEVEL")) == "3");
        assert(std::string(getenv("ZENKAIPOWER_ACCOUNT_FILE")).find("net_account_bot.dat") != std::string::npos);
        assert(std::string(getenv("ZENKAIPOWER_BOT_ACCESS_CODE")) == "ABC123");
        auto request = std::string(getenv("ZENKAIPOWER_LOBBY"));
        fprintf(stderr, "REQUEST %s\n", request.c_str());
        if (request == "join:OPENNO" || request == "join:VERBAD" || request == "join:NONETW") {
            int state = request == "join:OPENNO" ? -1 : request == "join:VERBAD" ? -2 : -4;
            fprintf(stderr, "zenkaipower: lobby: state %d, code ''\n", state);
            fflush(stderr);
            for (;;) SDL_Delay(20);
        }
        if (request == "join:BADBAD") {
            fprintf(stderr, "zenkaipower: lobby: state -3, code '', Verification failed\n");
            return 0;
        }
        if (request == "join:RESULT") {
            fprintf(stderr, "zenkaipower: battle: WIN | TestBot vs Rival\n");
            fprintf(stderr, "zenkaipower: battle: LOSS | TestBot vs Rival\n");
            fprintf(stderr, "zenkaipower: battle: DRAW | TestBot vs Rival\n");
        }
        fprintf(stderr, "zenkaipower: lobby: state 1, code 'ABC123'\n");
        fflush(stderr);
        if (request == "quick:RS") { fprintf(stderr, "zenkaipower: net: the session has ended\n"); return 0; }
        for (;;) SDL_Delay(20);
    }
    auto temporary = std::filesystem::temp_directory_path() / ("zenkaipower-bot-test-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directories(temporary);
    auto game = temporary / "game";
    std::filesystem::copy_file(std::filesystem::absolute(argv[0]), game);
    std::filesystem::permissions(game, std::filesystem::perms::owner_all);
    BotOptions options; options.game = game.u8string(); options.name = "TestBot";
    options.level = 4; options.access = "ABC123"; options.code = "ABC123";
    auto until = [](auto condition) {
        Uint64 end = SDL_GetTicks() + 15000;
        while (!condition() && SDL_GetTicks() < end) SDL_Delay(10);
        assert(condition());
    };
    BotProcess bot;
    assert(BotCode("ABC123") && !BotCode("ABCDE!") && !BotCode("short"));
    for (int kind = 0; kind < 4; kind++) {
        options.connection = kind;
        assert(bot.start(options));
        until([&] { return bot.log().size() >= 2; });
        auto lines = bot.log();
        std::string expected = kind == 0 ? "join:ABC123" : kind == 1 ? "host:S" : kind == 2 ? "quick:S" : "lan:join:127.0.0.1:S";
        assert(lines[lines.size()-2] == "REQUEST " + expected);
        if (kind == 1) assert(bot.room() == "ABC123");
        else assert(bot.room().empty());
        bot.stop(); assert(!bot.active()); bot.clearLog();
    }
    options.connection = 0; options.code = "RESULT";
    assert(bot.start(options)); until([&] { return bot.results()[2] == 1; });
    assert(bot.results()[0] == 1 && bot.results()[1] == 1); bot.stop();
    options.connection = 0; options.code = "BADBAD";
    assert(bot.start(options));
    until([&] { bot.update(); return !bot.active(); });
    assert(bot.status() == "Verification failed");
    const char *errors[][2] = {{"OPENNO", "Could not open the network lobby"},
                              {"VERBAD", "The two games use different versions"},
                              {"NONETW", "Could not reach the other player"}};
    for (const auto &entry : errors) {
        options.code = entry[0];
        assert(bot.start(options));
        until([&] { bot.update(); return !bot.active(); });
        assert(bot.status() == entry[1]);
    }
    options.connection = 2; options.mode = 5; options.ranked_delay_seconds = 1; bot.clearLog();
    Uint64 retryStarted = SDL_GetTicks();
    assert(bot.start(options));
    until([&] {
        bot.update(); int launches = 0;
        for (auto &line : bot.log()) if (line == "REQUEST quick:RS") launches++;
        return launches >= 2;
    });
    assert(SDL_GetTicks() - retryStarted >= 1000);
    bot.stop(); assert(!bot.active());
    options.ranked_delay = false; bot.clearLog();
    assert(bot.start(options));
    until([&] {
        bot.update(); int launches = 0;
        for (auto &line : bot.log()) if (line == "REQUEST quick:RS") launches++;
        return launches >= 2;
    });
    bot.stop();
    std::filesystem::remove_all(temporary);
    SDL_Quit();
    puts("PASS: all connection modes, environment, room code, error recovery, ranked requeue and disconnect");
}
