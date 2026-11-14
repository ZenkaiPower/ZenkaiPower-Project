#include "text.h"
#include "../src/port_pack.h"
#include "setup_text_defaults.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace {
std::map<std::string,std::string> &catalog() {
    static std::map<std::string,std::string> entries(kSetupTextDefaults.begin(),kSetupTextDefaults.end());
    return entries;
}

// Preserve printf argument types and escaped percent signs in editable messages.
bool formats(const std::string &value,std::vector<std::string> &out) {
    for (size_t i=0;i<value.size();++i) {
        if (value[i]!='%') continue;
        size_t begin=i++;
        if (i==value.size()) return false;
        if (value[i]=='%') { out.push_back("%%"); continue; }
        while (i<value.size() && std::string("-+ #0.123456789hljztL").find(value[i])!=std::string::npos) ++i;
        if (i==value.size() || std::string("diuoxXfFeEgGaAcsp").find(value[i])==std::string::npos) return false;
        out.push_back(value.substr(begin,i-begin+1));
    }
    return true;
}
}

const char *SetupText(const char *key) {
    const auto &entries=catalog();
    auto found=entries.find(key);
    return found==entries.end() ? key : found->second.c_str();
}

void SetupText_Load(const std::string &base) {
    namespace fs=std::filesystem;
    const fs::path candidates[]={fs::u8path(base)/"setup/en.json",
        fs::u8path(base)/"port/setup/text/en.json",fs::path("port/setup/text/en.json")};
    for (const auto &path : candidates) {
        std::ifstream input(path,std::ios::binary);
        std::string packed;
        if (!input) {
            if (path != candidates[0]) continue;
            int64_t offset=0,length=0;
            FILE *file=Port_NamedPackage((fs::u8path(base)/"setup.pak").u8string().c_str(),
                "en.json",&offset,&length,-1,nullptr,0);
            if (!file) continue;
            if (length<0 || length>16*1024*1024) { fclose(file); continue; }
            packed.resize(static_cast<size_t>(length));
            bool read=fread(packed.data(),1,packed.size(),file)==packed.size();
            fclose(file);
            if (!read) continue;
        }
        try {
            auto json=input ? nlohmann::json::parse(input) : nlohmann::json::parse(packed);
            if (!json.is_object()) throw std::runtime_error("expected a JSON object");
            for (auto &entry : catalog()) {
                auto found=json.find(entry.first);
                if (found==json.end()) continue;
                if (!found->is_string()) { SDL_Log("Setup text: %s must be a string",entry.first.c_str()); continue; }
                auto value=found->get<std::string>();
                std::vector<std::string> before,after;
                if (value.find('\0')!=std::string::npos || value.size()>16384 ||
                    !formats(entry.second,before) || (!before.empty() && (!formats(value,after) || before!=after))) {
                    SDL_Log("Setup text: ignoring invalid format in %s",entry.first.c_str());
                    continue;
                }
                entry.second=std::move(value);
            }
        } catch (const std::exception &error) {
            SDL_Log("Setup text: could not load %s: %s",path.u8string().c_str(),error.what());
        }
        return;
    }
}
