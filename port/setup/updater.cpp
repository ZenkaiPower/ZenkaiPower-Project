#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#endif
#include "text.h"
#include "updater.h"
#include "../third_party/nlohmann/json.hpp"
#include <SDL3/SDL.h>
#include "../src/port_process.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#include <signal.h>
#endif
namespace fs = std::filesystem;
namespace {
#ifdef _WIN32
const char *platform = "windows", *archiveFolder = "ZenkaiPower-Windows", *setup = "ZenkaiPower-setup.exe", *game = "ZenkaiPower.exe";
#else
const char *platform = "linux", *archiveFolder = "ZenkaiPower-Linux", *setup = "ZenkaiPower-setup", *game = "ZenkaiPower";
#endif
bool gameRunning(const fs::path &root) {
    std::error_code ec;
#ifdef _WIN32
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if (snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error(SetupText("error.cannot_check_whether_the_game_is_running"));
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry);
    bool found=false;
    for (BOOL more=Process32FirstW(snapshot,&entry);more;more=Process32NextW(snapshot,&entry)) {
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
        if (!process) continue;
        wchar_t path[32768]; DWORD size=32768;
        if (QueryFullProcessImageNameW(process,0,path,&size)) {
            if (fs::equivalent(fs::path(path),root/game,ec)) found=true;
        }
        CloseHandle(process);
        if (found) break;
    }
    CloseHandle(snapshot); return found;
#else
    for (const auto &entry : fs::directory_iterator("/proc")) {
        fs::path executable=entry.path()/"exe";
        if (fs::equivalent(executable,root/game,ec)) return true;
    }
    return false;
#endif
}
std::string run(const std::vector<std::string> &args, bool background = false, int *exitCode = nullptr) {
    std::vector<const char *> argv;
    for (const auto &arg : args) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, background ? SDL_PROCESS_STDIO_NULL : SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, background);
    SDL_Process *process = Port_CreateHiddenProcess(props);
    SDL_DestroyProperties(props);
    if (!process) throw std::runtime_error(SetupText("error.could_not_start") + args[0] + SetupText("error.install_it_and_try_again"));
    if (background) { SDL_DestroyProcess(process); return {}; }
    size_t size = 0;
    int code = -1;
    void *data = Port_ReadProcess(process, &size, &code);
    std::string output = data ? std::string((char *)data, size) : "";
    SDL_free(data);
    SDL_DestroyProcess(process);
    if (exitCode) *exitCode=code;
    else if (code) throw std::runtime_error(args[0] + SetupText("error.failed_check_your_connection_available_disk_space_and_file_permissions"));
    return output;
}
void download(const std::string &url, const fs::path &file, int timeout, const char *limit = "2147483648") {
    int code=0;
    std::string status=run({"curl", "--disable", "--fail", "--silent", "--show-error", "--location", "--proto", "=https", "--proto-redir", "=https",
         "--connect-timeout", "10", "--max-time", std::to_string(timeout), "--max-filesize", limit,
         "--write-out", "%{http_code}", "--user-agent", "ZenkaiPower-setup", "--output", file.string(), url},false,&code);
    if (!code) return;
    if (status=="404") throw std::runtime_error(SetupText("error.no_published_github_release_or_matching_archive_was_found"));
    if (status=="403" || status=="429") throw std::runtime_error(SetupText("error.github_refused_the_request_or_its_rate_limit_was_reached_try_again_later"));
    throw std::runtime_error(SetupText("error.download_failed_check_your_connection_available_disk_space_and_file_permissi"));
}
std::string read(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error(SetupText("error.cannot_read") + p.string());
    return std::string(std::istreambuf_iterator<char>(f), {});
}
#ifdef _WIN32
std::string psquote(const std::string &s) {
    std::string q = "'";
    for (char c : s) q += c == '\'' ? "''" : std::string(1, c);
    return q + "'";
}
#endif
// Only release-owned paths may enter the install. Saves, game data and mods are excluded.
bool owned(const fs::path &p) {
    const std::string first = p.begin()->string();
    if (first == "lib" || first == "licenses" || first == "tools") return true;
    if (first == "bot") return p == "bot" || p.filename() == "ZenkaiPower-bot" ||
        p.filename() == "ZenkaiPower-bot.exe" || p.extension() == ".dll";
    if (first == "setup") return p == "setup" || p == "setup/en.json";
    if (first == "textures") return p == "textures" || p == "textures/README.txt";
    if (std::distance(p.begin(), p.end()) != 1) return false;
    return first == game || first == setup || first == "ZenkaiPower.dat" || first == std::string(game) + ".mem" ||
        p.extension() == ".dll" || first == "fonts.pak" || first == "translations.pak" || first == "game-info.pak" ||
        first == "dragon-net-battle.pak" || first == "setup.pak" || first == "README.txt";
}
// Validate ZIP central and local names BEFORE invoking the platform extractor. No symlinks, traversal or ZIP64.
void validateZip(const fs::path &file) {
    std::ifstream f(file, std::ios::binary);
    auto bytes = [&](uint64_t at, size_t n) {
        std::string b(n, '\0'); f.clear(); f.seekg(at); f.read(b.data(), n);
        if ((size_t)f.gcount() != n) throw std::runtime_error(SetupText("error.truncated_update_archive"));
        return b;
    };
    auto u16 = [](const std::string &b, size_t o) { return (unsigned char)b[o] | ((unsigned char)b[o+1] << 8); };
    auto u32 = [&](const std::string &b, size_t o) { return (uint32_t)u16(b,o) | ((uint32_t)u16(b,o+2) << 16); };
    uint64_t size = fs::file_size(file);
    size_t tailSize = (size_t)std::min<uint64_t>(size, 65557);
    std::string tail = bytes(size-tailSize, tailSize);
    size_t e = tail.rfind(std::string("PK\5\6", 4));
    if (e == std::string::npos || e+22 > tail.size() || e+22+u16(tail,e+20) != tail.size() ||
        u16(tail,e+4) || u16(tail,e+6) || u16(tail,e+8) != u16(tail,e+10))
        throw std::runtime_error(SetupText("error.unsupported_update_archive"));
    unsigned count = u16(tail,e+10);
    if (!count || count == 65535 || u32(tail,e+16) == UINT32_MAX) throw std::runtime_error(SetupText("error.unsupported_zip64_archive"));
    uint64_t at = u32(tail,e+16), total = 0;
    std::set<std::string> names;
    auto extra = [&](const std::string &b) {
        for (size_t k=0; k<b.size();) {
            if (k+4>b.size() || k+4+u16(b,k+2)>b.size() || u16(b,k)==0x7075 || u16(b,k)==1)
                throw std::runtime_error(SetupText("error.unsupported_zip_path_or_zip64_extension"));
            k+=4+u16(b,k+2);
        }
    };
    for (unsigned i=0; i<count; ++i) {
        std::string h = bytes(at,46);
        if (h.substr(0,4) != std::string("PK\1\2",4)) throw std::runtime_error(SetupText("error.invalid_zip_directory"));
        std::string name = bytes(at+46,u16(h,28));
        std::string normalized=name;
        std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c) { return (char)std::tolower(c); });
        if (!names.insert(normalized).second || (u16(h,8)&1) || (u16(h,10)!=0 && u16(h,10)!=8))
            throw std::runtime_error(SetupText("error.duplicate_or_unsupported_zip_entry"));
        std::string archiveRoot = std::string(archiveFolder) + "/";
        if (name.rfind(archiveRoot,0) != 0 || name.find('\\') != std::string::npos || name.find(':') != std::string::npos ||
            name.find('\0') != std::string::npos || ((u32(h,38)>>16)&0170000) == 0120000)
            throw std::runtime_error(SetupText("error.unsafe_update_archive_path"));
        fs::path rel = fs::u8path(name.substr(archiveRoot.size()));
        for (const auto &part : rel) if (part == ".." || part == ".") throw std::runtime_error(SetupText("error.unsafe_update_archive_path"));
        if (!rel.empty() && !owned(rel)) throw std::runtime_error(SetupText("error.unexpected_file_in_update") + name);
        total += u32(h,24);
        if (total > UINT64_C(4294967296)) throw std::runtime_error(SetupText("error.update_archive_is_too_large"));
        std::string local = bytes(u32(h,42),30);
        if (local.substr(0,4) != std::string("PK\3\4",4) || bytes((uint64_t)u32(h,42)+30,u16(local,26)) != name)
            throw std::runtime_error(SetupText("error.inconsistent_zip_entry"));
        extra(bytes(at+46+u16(h,28),u16(h,30)));
        extra(bytes((uint64_t)u32(h,42)+30+u16(local,26),u16(local,28)));
        at += 46 + u16(h,28) + u16(h,30) + u16(h,32);
    }
}
std::vector<unsigned long> version(std::string s) {
    if (!s.empty() && (s[0]=='v' || s[0]=='V')) s.erase(0,1);
    std::vector<unsigned long> result;
    std::istringstream stream(s); std::string part;
    while (std::getline(stream,part,'.')) {
        if (part.empty() || part.size()>9 || part.find_first_not_of("0123456789") != std::string::npos) return {};
        result.push_back(std::stoul(part));
    }
    return result;
}
}
bool SetupUpdate_Newer(const std::string &candidate, const std::string &installed) {
    auto a=version(candidate), b=version(installed);
    if (a.empty() || b.empty()) return false;
    a.resize(std::max(a.size(),b.size())); b.resize(a.size());
    return a>b;
}
std::string SetupUpdate_Repository(const std::string &source) {
    const std::string prefix="https://github.com/";
    if (source.rfind(prefix,0)!=0) return {};
    std::string path=source.substr(prefix.size());
    while (!path.empty() && path.back()=='/') path.pop_back();
    if (path.size()>4 && path.substr(path.size()-4)==".git") path.resize(path.size()-4);
    size_t slash=path.find('/');
    if (slash==std::string::npos || slash==0 || slash+1==path.size() || path.find('/',slash+1)!=std::string::npos ||
        path.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._/")!=std::string::npos)
        return {};
    for (const auto &part : {path.substr(0,slash),path.substr(slash+1)})
        if (part=="." || part=="..") return {};
    return prefix+path;
}
SetupUpdate SetupUpdate_Check(const std::string &installed, const std::string &source) {
    SetupUpdate result;
    fs::path temp;
    try {
        if (version(installed).empty()) throw std::runtime_error(SetupText("error.installed_version_is_missing_or_invalid"));
        const std::string repository=SetupUpdate_Repository(source);
        if (repository.empty()) throw std::runtime_error(SetupText("error.the_source_in_game_info_must_be_a_github_repository_url"));
        temp=fs::temp_directory_path()/fs::path("zenkai-release-"+std::to_string(SDL_GetTicksNS())+".json");
        download("https://api.github.com/repos/"+repository.substr(std::string("https://github.com/").size())+"/releases/latest",temp,30,"4194304");
        if (fs::file_size(temp)>4*1024*1024) throw std::runtime_error(SetupText("error.release_response_is_too_large"));
        auto j=nlohmann::json::parse(read(temp));
        if (j.value("draft",true) || j.value("prerelease",true)) throw std::runtime_error(SetupText("error.no_stable_release_available"));
        result.version=j.at("tag_name").get<std::string>();
        if (version(result.version).empty()) throw std::runtime_error(SetupText("error.release_tag_must_be_a_numeric_version_such_as_v1_0_1"));
        result.available=SetupUpdate_Newer(result.version,installed);
        std::string number=result.version;
        if (number[0]=='v' || number[0]=='V') number.erase(0,1);
        const std::string asset="ZenkaiPower-v"+number+"-"+platform+"-x64.zip";
        const std::string legacy=std::string("ZenkaiPower-")+platform+"-x64.zip";
        // Prefer the versioned archive, while still accepting older releases.
        for (const auto &name : {asset,legacy}) {
            for (const auto &a : j.at("assets")) if (a.value("name","")==name) {
                result.url=a.at("browser_download_url").get<std::string>();
                result.digest=a.value("digest","");
                break;
            }
            if (!result.url.empty()) break;
        }
        if (result.available && (result.url.rfind(repository+"/releases/download/",0)!=0 ||
            result.digest.rfind("sha256:",0)!=0 || result.digest.size()!=71 ||
            result.digest.substr(7).find_first_not_of("0123456789abcdef")!=std::string::npos))
            throw std::runtime_error(SetupText("error.release_needs_the_platform_zip_asset_and_its_github_sha_256_digest"));
    } catch (const std::exception &e) { result.error=e.what(); result.available=false; }
    if (!temp.empty()) { std::error_code ec; fs::remove(temp,ec); }
    return result;
}
std::string SetupUpdate_Prepare(const SetupUpdate &update, const std::string &root) {
    fs::path stage;
    try {
        fs::path destination=fs::absolute(root);
        if (gameRunning(destination)) throw std::runtime_error(SetupText("error.close_the_game_before_updating"));
        stage=destination/(".zenkai-update-"+std::to_string(SDL_GetTicksNS()));
        if (!fs::create_directory(stage)) throw std::runtime_error(SetupText("error.cannot_create_update_staging_directory"));
        fs::path zip=stage/"release.zip";
        download(update.url,zip,600);
#ifdef _WIN32
        std::string hash=run({"powershell.exe","-NoProfile","-NonInteractive","-Command",
            "(Get-FileHash -Algorithm SHA256 -LiteralPath "+psquote(zip.string())+").Hash.ToLower()"});
#else
        std::string hash=run({"sha256sum",zip.string()});
#endif
        if (hash.substr(0,64)!=update.digest.substr(7)) throw std::runtime_error(SetupText("error.downloaded_update_checksum_does_not_match"));
        validateZip(zip);
        fs::path extracted=stage/"extracted";
#ifdef _WIN32
        run({"powershell.exe","-NoProfile","-NonInteractive","-Command",
            "$ErrorActionPreference='Stop'; Expand-Archive -LiteralPath "+psquote(zip.string())+" -DestinationPath "+psquote(extracted.string())});
#else
        run({"unzip","-q",zip.string(),"-d",extracted.string()});
#endif
        fs::path payload=extracted/archiveFolder;
        for (const auto &p : {game,setup,"ZenkaiPower.dat","game-info.pak"})
            if (!fs::is_regular_file(payload/p)) throw std::runtime_error(SetupText("error.incomplete_update_archive"));
        // Run the CURRENT helper code, using copies of its current runtime libraries.
        fs::path helper=stage/setup;
        fs::copy_file(destination/setup,helper);
#ifdef _WIN32
        for (const auto &entry : fs::directory_iterator(destination))
            if (entry.path().extension()==".dll") fs::copy_file(entry.path(),stage/entry.path().filename());
        unsigned long pid=GetCurrentProcessId();
#else
        fs::copy(destination/"lib",stage/"lib",fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        unsigned long pid=getpid();
#endif
        run({helper.string(),"--apply-update",std::to_string(pid),destination.string(),stage.string()},true);
        return {};
    } catch (const std::exception &e) {
        if (!stage.empty()) { std::error_code ec; fs::remove_all(stage,ec); }
        return e.what();
    }
}
int SetupUpdate_Helper(int argc, char **argv) {
    if (argc<2 || std::string(argv[1])!="--apply-update") return -1;
    if (argc!=5) return 1;
    fs::path root=argv[3], stage=argv[4], payload=stage/"extracted"/archiveFolder, backup=stage/"backup";
    std::vector<fs::path> installed, saved;
    try {
        unsigned long pid=std::stoul(argv[2]);
#ifdef _WIN32
        HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
        if (parent) { DWORD wait=WaitForSingleObject(parent,60000); CloseHandle(parent); if (wait!=WAIT_OBJECT_0) return 1; }
#else
        for (int i=0; i<600 && kill((pid_t)pid,0)==0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (kill((pid_t)pid,0)==0) return 1;
#endif
        if (gameRunning(root)) throw std::runtime_error(SetupText("error.the_game_is_running_close_it_and_try_the_update_again"));
        // Refuse symlink destinations, including parent directories, before changing any files.
        std::vector<fs::path> files;
        for (const auto &entry : fs::recursive_directory_iterator(payload)) {
            if (entry.is_symlink()) throw std::runtime_error(SetupText("error.symlink_in_update"));
            if (!entry.is_regular_file()) continue;
            fs::path rel=fs::relative(entry.path(),payload);
            if (!owned(rel)) throw std::runtime_error(SetupText("error.unexpected_update_file"));
            fs::path check=root;
            for (const auto &part : rel) {
                check/=part;
                if (fs::is_symlink(fs::symlink_status(check))) throw std::runtime_error(SetupText("error.symlink_in_installation_path"));
            }
            if (fs::exists(root/rel) && !fs::is_regular_file(root/rel)) throw std::runtime_error(SetupText("error.a_release_file_conflicts_with_a_directory"));
            files.push_back(rel);
        }
        for (const auto &rel : files) {
            // Keep the user's wording; missing new keys use embedded defaults.
            if (rel==fs::path("setup/en.json") && fs::is_regular_file(root/rel)) continue;
            fs::create_directories((root/rel).parent_path());
            if (fs::exists(root/rel)) {
                fs::create_directories((backup/rel).parent_path());
                fs::rename(root/rel,backup/rel); saved.push_back(rel);
            }
            fs::rename(payload/rel,root/rel); installed.push_back(rel);
        }
        run({(root/setup).string()},true);
        // Cleanup must never trigger rollback after the updated setup has started.
#ifdef _WIN32
        // The helper executable and its DLLs are still loaded from stage. Let PowerShell
        // wait for this process to exit before removing the directory that contains them.
        try {
            run({"powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                "Wait-Process -Id " + std::to_string(GetCurrentProcessId()) +
                " -ErrorAction SilentlyContinue; Remove-Item -LiteralPath " +
                psquote(stage.string()) + " -Recurse -Force -ErrorAction SilentlyContinue"}, true);
        } catch (...) { /* Retain staging if the cleanup process cannot start. */ }
#else
        std::error_code cleanupError;
        fs::remove_all(stage, cleanupError);
#endif
        return 0;
    } catch (const std::exception &e) {
        std::error_code ec;
        for (auto it=installed.rbegin();it!=installed.rend();++it) fs::remove(root/ *it,ec);
        for (auto it=saved.rbegin();it!=saved.rend();++it) fs::rename(backup/ *it,root/ *it,ec);
        std::ofstream(stage/"update-error.txt") << e.what();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,SetupText("error.update_failed"),e.what(),nullptr);
        return 1;
    }
}
