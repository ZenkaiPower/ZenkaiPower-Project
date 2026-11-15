#ifndef ZENKAI_BOT_CREDENTIAL_H
#define ZENKAI_BOT_CREDENTIAL_H
#include <filesystem>
#include <fstream>
#include <string>
#include "../third_party/nlohmann/json.hpp"

// Explicit environment value wins; the file is operator-owned and is never rewritten.
static inline bool BotCredential(const char *environment, const std::filesystem::path &directory,
                                 std::string &credential, std::string &error) {
    credential.clear(); error.clear();
    if (environment && environment[0]) credential = environment;
    else {
        std::error_code ec;
        const auto path = directory / "bot.local.json";
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) { error = "Cannot inspect bot.local.json"; return false; }
        if (!exists) return true; // LAN does not require an online credential.
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in || in.tellg() < 0 || in.tellg() > 4096) {
            error = "Cannot read bot.local.json (maximum 4096 bytes)"; return false;
        }
        std::string bytes(static_cast<size_t>(in.tellg()), '\0');
        in.seekg(0); in.read(bytes.data(), bytes.size());
        auto json = nlohmann::json::parse(bytes, nullptr, false);
        if (!in || !json.is_object() || !json.contains("ZENKAIPOWER_BOT_CREDENTIAL") ||
            !json["ZENKAIPOWER_BOT_CREDENTIAL"].is_string()) {
            error = "bot.local.json must contain a ZENKAIPOWER_BOT_CREDENTIAL string"; return false;
        }
        credential = json["ZENKAIPOWER_BOT_CREDENTIAL"].get<std::string>();
    }
    if (credential.size() != 64 || credential.find_first_not_of("0123456789abcdef") != std::string::npos) {
        credential.clear();
        error = "Bot credential must contain 64 lowercase hex characters (use the raw credential, not its hash)";
        return false;
    }
    return true;
}
#endif
