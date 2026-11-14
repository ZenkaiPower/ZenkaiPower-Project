#ifndef ZENKAI_BOT_PROCESS_H
#define ZENKAI_BOT_PROCESS_H
#include <SDL3/SDL.h>
#include <atomic>
#include <array>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct BotOptions {
    std::string game, name = "SparkingBot", access, host = "127.0.0.1", code;
    int connection = 0, mode = 0, level = 5;
    bool ranked_retry = true, ranked_delay = true;
    int ranked_delay_seconds = 5;
};
static const char *const BotModes[] = {"S", "T", "D10", "D15", "D20", "RS"};
static inline bool BotCode(const std::string &code) {
    if (code.size() != 6) return false;
    for (unsigned char c : code) if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
    return true;
}
// SDL resolves the executable directory independently of the working directory.
static inline std::filesystem::path BotDirectory() {
    if (const char *overridePath = SDL_getenv("ZENKAIPOWER_BOT_CONFIG_DIR"))
        return std::filesystem::u8path(overridePath);
    const char *base = SDL_GetBasePath();
    return base ? std::filesystem::u8path(base) : std::filesystem::path();
}
class BotProcess {
    SDL_Process *process = nullptr;
    std::thread reader;
    std::mutex lock, processLock;
    std::vector<std::string> lines;
    std::string partial, roomCode, message = "Disconnected";
    std::atomic<bool> finished{false}, ended{false}, failed{false};
    int exitCode = 0;
    std::array<int,3> totals{};
    Uint64 retryAt = 0;
    BotOptions options;
    void line(const std::string &text) {
        std::lock_guard<std::mutex> guard(lock);
        lines.push_back(text);
        if (lines.size() > 400) lines.erase(lines.begin(), lines.begin() + 100);
        if (text.rfind("zenkaipower: battle: ", 0) == 0) {
            if (text.rfind("zenkaipower: battle: WIN |", 0) == 0) totals[0]++;
            if (text.rfind("zenkaipower: battle: LOSS |", 0) == 0) totals[1]++;
            if (text.rfind("zenkaipower: battle: DRAW |", 0) == 0) totals[2]++;
            message = text.substr(13);
        }
        if (text == "zenkaipower: net: the session has ended") ended = true;
        size_t code = text.find(", code '");
        if (code != std::string::npos && text.size() >= code + 15) {
            std::string value = text.substr(code + 8, 6);
            if (options.connection == 1 && BotCode(value)) { roomCode = value; message = "Room ready"; }
        }
        if (text.find("zenkaipower: lobby: state -") != std::string::npos ||
            text.find("zenkaipower: bot login failed:") != std::string::npos) {
            size_t detail = text.find("', ");
            message = detail == std::string::npos ? text : text.substr(detail + 3);
            if (text.find("zenkaipower: lobby: state -1,") != std::string::npos)
                message = "Could not open the network lobby";
            else if (text.find("zenkaipower: lobby: state -2,") != std::string::npos)
                message = "The two games use different versions";
            else if (text.find("zenkaipower: lobby: state -4,") != std::string::npos)
                message = "Could not reach the other player";
            failed = true;
        } else if (text.find("zenkaipower: lobby: state 2") != std::string::npos ||
                   text.find("zenkaipower: lobby: the other player is there") != std::string::npos) message = "Connected";
    }
public:
    ~BotProcess() { stop(); }
    bool active() const { return process != nullptr; }
    std::array<int,3> results() { std::lock_guard<std::mutex> guard(lock); return totals; }
    std::string status() { std::lock_guard<std::mutex> guard(lock); return message; }
    std::string room() { std::lock_guard<std::mutex> guard(lock); return roomCode; }
    std::vector<std::string> log() { std::lock_guard<std::mutex> guard(lock); return lines; }
    void clearLog() { std::lock_guard<std::mutex> guard(lock); lines.clear(); }
    void stop(bool disconnected = true) {
        retryAt = 0;
        if (process) {
            if (!finished) { std::lock_guard<std::mutex> guard(processLock); SDL_KillProcess(process, true); }
            if (reader.joinable()) reader.join();
            SDL_DestroyProcess(process); process = nullptr;
        }
        if (disconnected) { std::lock_guard<std::mutex> guard(lock); message = "Disconnected"; roomCode.clear(); }
    }
    bool start(const BotOptions &next) {
        stop(); options = next;
        auto error = [&](const std::string &text) {
            std::lock_guard<std::mutex> guard(lock); message = text; return false;
        };
        if (options.mode < 0 || options.mode > 5 || options.level < 1 || options.level > 5 ||
            options.connection < 0 || options.connection > 3 || options.ranked_delay_seconds < 0 || options.ranked_delay_seconds > 3600) return error("Invalid bot options");
        if (options.name.empty()) return error("Enter the bot account name");
        if (options.connection == 0 && !BotCode(options.code)) return error("Enter a six-character room code");
        if (!options.access.empty() && !BotCode(options.access)) return error("Access codes have six letters or numbers");
        if (options.connection != 2 && options.mode == 5) return error("Ranked is available in online matchmaking");
        if (options.connection == 3 && (options.host.empty() || options.host.find(':') != std::string::npos))
            return error("Enter a LAN host address without a port");
        std::error_code ec;
        auto game = std::filesystem::absolute(std::filesystem::u8path(options.game), ec);
        if (ec || !std::filesystem::is_regular_file(game, ec)) return error("Choose the game executable");
        std::string executable = game.u8string(), directory = game.parent_path().u8string();
        auto account = BotDirectory();
        if (account.empty()) return error("Cannot locate the bot directory");
        std::filesystem::create_directories(account, ec);
        if (ec) return error("Cannot create the bot account directory");
        SDL_Environment *env = SDL_CreateEnvironment(true);
        if (!env) return error(SDL_GetError());
        for (const char *key : {"ZENKAIPOWER_REPLAY", "ZENKAIPOWER_DEMO", "ZENKAIPOWER_NET_HOST", "ZENKAIPOWER_NET_JOIN", "ZENKAIPOWER_LOBBY", "ZENKAIPOWER_BOT_ACCESS_CODE"})
            SDL_UnsetEnvironmentVariable(env, key);
        auto set = [&](const char *key, const std::string &value) { SDL_SetEnvironmentVariable(env, key, value.c_str(), true); };
        set("ZENKAIPOWER_ACCOUNT_FILE", (account / "net_account_bot.dat").u8string());
        if (!options.access.empty()) set("ZENKAIPOWER_BOT_ACCESS_CODE", options.access);
        set("ZENKAIPOWER_NET_BOT", "1"); set("ZENKAIPOWER_NET_BOT_LEVEL", std::to_string(options.level - 1));
        set("ZENKAIPOWER_NET_NAME", options.name); set("ZENKAIPOWER_GS", "none"); set("ZENKAIPOWER_NOSOUND", "1");
        std::string request;
        if (options.connection == 0) request = "join:" + options.code;
        if (options.connection == 1) request = std::string("host:") + BotModes[options.mode];
        if (options.connection == 2) request = std::string("quick:") + BotModes[options.mode];
        if (options.connection == 3) request = "lan:join:" + options.host + ":" + BotModes[options.mode];
        set("ZENKAIPOWER_LOBBY", request);
        const char *args[] = {executable.c_str(), nullptr};
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, directory.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        process = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props); SDL_DestroyEnvironment(env);
        if (!process) return error(std::string("Cannot start game: ") + SDL_GetError());
        finished = false; ended = false; failed = false; partial.clear();
        { std::lock_guard<std::mutex> guard(lock); message = "Connecting..."; roomCode.clear(); }
        reader = std::thread([this] {
            SDL_IOStream *out = SDL_GetProcessOutput(process);
            char bytes[4096];
            auto consume = [&](size_t count) {
                for (size_t i = 0; i < count; i++) {
                    if (bytes[i] == '\n') { line(partial); partial.clear(); }
                    else if (bytes[i] != '\r' && partial.size() < 8192) partial += bytes[i];
                }
            };
            for (;;) {
                if (out) consume(SDL_ReadIO(out, bytes, sizeof(bytes)));
                bool done;
                { std::lock_guard<std::mutex> guard(processLock); done = SDL_WaitProcess(process, false, &exitCode); }
                if (done) {
                    size_t count;
                    while (out && (count = SDL_ReadIO(out, bytes, sizeof(bytes))) != 0) consume(count);
                    break;
                }
                SDL_Delay(5); // Process pipes are nonblocking; no data is not EOF.
            }
            if (!partial.empty()) line(partial);
            finished = true;
        });
        return true;
    }
    void update() {
        if (!process) return;
        if (failed) { stop(false); return; }
        if (finished || ended) {
            if (options.connection == 2 && options.mode == 5 && options.ranked_retry) {
                Uint64 now = SDL_GetTicks();
                if (!retryAt) retryAt = now + (options.ranked_delay ? options.ranked_delay_seconds * 1000u : 0);
                if (now >= retryAt) { BotOptions copy = options; start(copy); }
                else {
                    std::lock_guard<std::mutex> guard(lock);
                    message = "Rejoining ranked in " + std::to_string((retryAt-now+999)/1000) + " seconds...";
                }
            } else {
                stop(false);
                std::lock_guard<std::mutex> guard(lock);
                message = ended ? "Session ended" : "Game exited (" + std::to_string(exitCode) + ")";
            }
        }
    }
};
#endif
