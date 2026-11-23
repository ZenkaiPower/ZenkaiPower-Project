#include "locations.h"
// The setup's own installer, for a release folder (ZenkaiPower, ZenkaiPower.dat and ZenkaiPower-setup together, no build tools):
// reads the user's disc image directly, checks it, unpacks the game data and prepares text resources. No Python, no 7z.
// It reports through the same event lines as install.py (@step, @note, @ok, @skip, @fail, @stopped, @done), so the
// window (setup.cpp) shows both the same way.
//
//   disc image   ISO 9660: the volume descriptor at sector 16, directory records from the root
//   checks       SHA-1 of BIN/DBZP.BIN and of SLUS_216.78 laid out as a flat image from 0x100000 (the values of
//                config/DBZP.yaml and config/SLUS_216.78.yaml)
//   game data    gamedata/disc/<loose files>, and every DATA/*.AFS archive retained as gamedata/<name>.afs
//                (the layout of port/tools/extract_disc.py)
//   European     optional, a second image (SLES-54945): 410 entries of DATA/PZS3EU1.AFS for translated menus/HUD,
//                plus entry 1 of DATA/PZS3EU0.AFS for the native language selector and ZS3EUOP.ADX for the
//                EU opening soundtrack (as port/tools/extract_eu.py).
#include "text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <atomic>
#include <memory>
#include "../src/port_pack.h"
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <SDL3/SDL.h>
#include "../src/port_process.h"
#include "native.h"
#include "shortcut.h"
#include "title_launcher.h"
#include "../src/remove_empty_dirs.h"
#if defined(ZENKAIPOWER_ANDROID_INSTALLER) || defined(ZENKAIPOWER_NOD_INSTALLER)
#include <nod.h>
#endif
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
extern bool Android_PrepareText(const std::string &root);
#endif

namespace {

const char *kRomSha1 = "caee6c2269bba89fc51eea9e7beac3adbec9dc52";
const char *kDbzpSha1 = "4f910969e05d9b25c83af7642b60da9949a4348b";
const Uint32 kRomBase = 0x100000, kRomEnd = 0x2FF180;
#ifdef _WIN32
const char *kGame = "ZenkaiPower.exe";
#else
const char *kGame = "ZenkaiPower";
#endif

std::mutex sLock;
std::vector<std::string> sLines;
std::thread sThread;
std::atomic<bool> sRunning(false), sCancel(false);

void say(const std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    sLines.push_back(line);
}

struct Stop { std::string why; };
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
#include "../../android/app/src/main/cpp/installer_iso.h"
#endif

// ------------------------------------------------------------------------------------------------------- SHA-1
struct Sha1 {
    Uint32 h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    Uint8 block[64];
    Uint64 total = 0;
    size_t fill = 0;

    static Uint32 rol(Uint32 v, int n) { return (v << n) | (v >> (32 - n)); }
    void compress() {
        Uint32 w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 16; i++) {
            w[i] = (Uint32)block[i * 4] << 24 | (Uint32)block[i * 4 + 1] << 16 | (Uint32)block[i * 4 + 2] << 8 | block[i * 4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        for (int i = 0; i < 80; i++) {
            Uint32 f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            Uint32 k = i < 20 ? 0x5A827999 : i < 40 ? 0x6ED9EBA1 : i < 60 ? 0x8F1BBCDC : 0xCA62C1D6;
            Uint32 t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void add(const Uint8 *p, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - fill < n ? 64 - fill : n;
            memcpy(block + fill, p, take);
            fill += take; p += take; n -= take;
            if (fill == 64) {
                compress();
                fill = 0;
            }
        }
    }
    std::string hex() {
        Uint64 bits = total * 8;
        Uint8 pad = 0x80, zero = 0, len[8];
        char out[41];
        add(&pad, 1);
        while (fill != 56) {
            add(&zero, 1);
        }
        for (int i = 0; i < 8; i++) {
            len[i] = (Uint8)(bits >> (56 - i * 8));
        }
        add(len, 8);
        for (int i = 0; i < 5; i++) {
            snprintf(out + i * 8, 9, "%08x", h[i]);
        }
        return out;
    }
};

// ------------------------------------------------------------------------------------------------- the disc image
struct IsoFile { std::string path; Uint64 offset, size; };

struct Iso {
    SDL_IOStream *io = NULL;
    std::vector<IsoFile> files;
    Uint64 imageSize = 0, directoryBytes = 0;

    void read(Uint64 offset, void *buf, size_t n) {
        if (offset > imageSize || n > imageSize - offset || SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, buf, n) != n) {
            throw Stop{SetupText("error.the_disc_image_could_not_be_read_to_its_end_it_may_be_incomplete")};
        }
    }
    void walk(Uint32 lba, Uint32 size, const std::string &dir, int depth) {
        if (size > (16u << 20) || directoryBytes + size > (64u << 20))
            throw Stop{SetupText("error.the_disc_directory_is_too_large_or_recursive")};
        directoryBytes += size;
        std::vector<Uint8> d(size);
        read((Uint64)lba * 2048, d.data(), size);
        for (size_t pos = 0; pos + 34 <= size;) {
            Uint8 len = d[pos];
            if (len == 0) { // records do not cross sectors: the rest of this one is empty
                pos = (pos / 2048 + 1) * 2048;
                continue;
            }
            if (len < 34 || pos + len > size || d[pos + 32] > len - 33)
                throw Stop{SetupText("error.the_disc_directory_contains_a_damaged_record")};
            Uint32 ext, bytes;
            memcpy(&ext, &d[pos + 2], 4);
            memcpy(&bytes, &d[pos + 10], 4);
            Uint8 flags = d[pos + 25], nameLen = d[pos + 32];
            std::string name((const char *)&d[pos + 33], nameLen);
            pos += len;
            if (nameLen == 1 && (name[0] == 0 || name[0] == 1)) {
                continue; // this directory and its parent
            }
            size_t semi = name.find(';');
            if (semi != std::string::npos) {
                name.resize(semi);
            }
            if (!name.empty() && name.back() == '.') {
                name.pop_back();
            }
            if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos || name.find('\0') != std::string::npos)
                throw Stop{SetupText("error.the_disc_directory_contains_an_invalid_filename")};
            if (files.size() >= 100000) throw Stop{SetupText("error.the_disc_directory_contains_too_many_files")};
            if (flags & 2) {
                if (depth < 8) {
                    walk(ext, bytes, dir + name + "/", depth + 1);
                }
            } else if (!files.empty() && files.back().path == dir + name) {
                files.back().size += bytes; // a file in several extents: they follow each other
            } else {
                files.push_back({dir + name, (Uint64)ext * 2048, bytes});
            }
        }
    }
    void open(const std::string &path) {
        Uint8 pvd[2048];
        Uint32 lba, size;
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
        io = android_iso_stream(path);
#else
        io = SDL_IOFromFile(path.c_str(), "rb");
#endif
        if (io == NULL) {
            throw Stop{SetupText("error.the_disc_image_could_not_be_opened") + path};
        }
        Sint64 length = SDL_GetIOSize(io);
        if (length < 0) throw Stop{SetupText("error.cannot_determine_the_disc_image_size")};
        imageSize = (Uint64)length;
        if (SDL_SeekIO(io, 16 * 2048, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, pvd, sizeof(pvd)) != sizeof(pvd) || pvd[0] != 1 ||
            memcmp(pvd + 1, "CD001", 5) != 0) {
            throw Stop{SetupText("error.this_file_is_not_a_disc_image_iso_compressed_images_chd_cso_gz_have_to_be_co")};
        }
        memcpy(&lba, pvd + 156 + 2, 4);
        memcpy(&size, pvd + 156 + 10, 4);
        walk(lba, size, "", 0);
    }
    const IsoFile *find(const char *path) {
        for (auto &f : files) {
            if (SDL_strcasecmp(f.path.c_str(), path) == 0) {
                return &f;
            }
        }
        return NULL;
    }
    ~Iso() {
        if (io != NULL) {
            SDL_CloseIO(io);
        }
    }
};

std::string lower(std::string s) {
    for (auto &c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

std::string upper(std::string s) {
    for (auto &c : s) {
        c = (char)toupper((unsigned char)c);
    }
    return s;
}

void check_cancel() {
    if (sCancel) {
        throw Stop{SetupText("error.cancelled_install_continues_from_where_it_stopped")};
    }
}

// Copies `size` bytes of the image at `offset` into a new file.
void copy_out(Iso &iso, Uint64 offset, Uint64 size, const std::string &path, std::vector<Uint8> &buf) {
    SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
    if (out == NULL) {
        throw Stop{SetupText("error.could_not_write") + path + SetupText("error.is_the_disk_full_or_the_folder_read_only")};
    }
    while (size > 0) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        iso.read(offset, buf.data(), n);
        if (SDL_WriteIO(out, buf.data(), n) != n) {
            SDL_CloseIO(out);
            throw Stop{SetupText("error.could_not_write") + path + SetupText("error.is_the_disk_full")};
        }
        offset += n;
        size -= n;
    }
    if (!SDL_CloseIO(out)) {
        throw Stop{SetupText("error.could_not_write") + path + SetupText("error.is_the_disk_full")};
    }
}

// "@step k n Title": the steps of this run are numbered as the window lists them (setup.cpp, reset_rows).
int sStep, sSteps;
void step(const char *title) {
    char line[96];
    snprintf(line, sizeof(line), "@step %d %d %s", ++sStep, sSteps, title);
    say(line);
}

void verify_us_programs(const std::vector<Uint8> &elf,const std::vector<Uint8> &menu) {
    std::vector<Uint8> rom(kRomEnd-kRomBase,0);
    if (elf.size() > 0x34 && memcmp(elf.data(), "\177ELF", 4) == 0) {
        Uint32 shoff;
        Uint16 shentsize, shnum;
        memcpy(&shoff, &elf[0x20], 4);
        memcpy(&shentsize, &elf[0x2E], 2);
        memcpy(&shnum, &elf[0x30], 2);
        for (Uint32 i = 0; shentsize >= 24 && i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elf.size(); i++) {
            Uint32 sh[6]; // name, type, flags, address, offset, size
            memcpy(sh, &elf[shoff + (size_t)i * shentsize], sizeof(sh));
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= kRomBase && sh[3] <= kRomEnd && sh[5] <= kRomEnd - sh[3] && (size_t)sh[4] + sh[5] <= elf.size()) {
                memcpy(&rom[sh[3] - kRomBase], &elf[sh[4]], sh[5]);
            }
        }
    }
    Sha1 a, b;
    a.add(rom.data(), rom.size());
    b.add(menu.data(), menu.size());
    if (a.hex() != kRomSha1 || b.hex() != kDbzpSha1) {
        throw Stop{SetupText("error.the_game_s_programs_on_this_disc_do_not_have_the_expected_checksums_the_port")};
    }
}

void step_disc(Iso &iso, const std::string &path, bool report = true) {
    if (report) step("Disc image");
    iso.open(path);
    const IsoFile *slus = iso.find("SLUS_216.78"), *dbzp = iso.find("BIN/DBZP.BIN");
    if (slus == NULL || dbzp == NULL) {
        throw Stop{SetupText("error.this_is_not_a_disc_image_of_the_usa_release_of_budokai_tenkaichi_3_no_slus_2_2")};
    }
    if (slus->size > (64u << 20) || dbzp->size > (64u << 20))
        throw Stop{SetupText("error.the_disc_programs_are_larger_than_expected_the_image_may_be_damaged")};
    if (report) say((std::string("@note ") + SetupText("progress.checking")));
    std::vector<Uint8> elf((size_t)slus->size), menu((size_t)dbzp->size);
    iso.read(slus->offset, elf.data(), elf.size());
    iso.read(dbzp->offset, menu.data(), menu.size());
    verify_us_programs(elf, menu);
    if (report) {
        say("@disc-verified SLUS-21678");
        say((std::string("@ok ") + SetupText("progress.the_usa_release_checksums_match")));
    }
}

void step_data(Iso &iso, const std::string &root) {
    std::string data = Setup_DataRoot(root), marker = data + ".installed";
    SDL_PathInfo info;
    step("Game data");
    // Selecting the USA disc explicitly requests fresh extraction, including repairs.
    // An interrupted reinstall must not leave the completion marker behind.
    if (SDL_GetPathInfo(marker.c_str(), &info) && !SDL_RemovePath(marker.c_str()))
        throw Stop{SetupText("error.could_not_reset_the_installed_marker_for_reinstallation")};
    std::vector<const IsoFile *> take;
    Uint64 total = 0, done = 0;
    for (auto &f : iso.files) {
        std::string u = upper(f.path);
        if (u == "SLUS_216.78" || u.rfind("BIN/", 0) == 0 || u.rfind("DATA/", 0) == 0) {
            take.push_back(&f);
            total += f.size;
        }
    }
    std::vector<Uint8> buf(1 << 20);
    int files = 0, lastPercent = -1;
    auto progress = [&](Uint64 n) {
        done += n;
        int percent = total ? (int)(done * 100 / total) : 0;
        if (percent != lastPercent) {
            char line[64];
            snprintf(line, sizeof(line), "@note %d%%, %d files", percent, files);
            say(line);
            lastPercent = percent;
        }
        check_cancel();
    };
    for (const IsoFile *f : take) {
        std::string u = upper(f->path);
        bool afs = u.rfind("DATA/", 0) == 0 && u.size() > 4 && u.compare(u.size() - 4, 4, ".AFS") == 0;
        if (!afs) {
            std::string out = data + "disc/" + u; // upper case, as the game asks for them
            SDL_CreateDirectory(out.substr(0, out.rfind('/')).c_str());
            copy_out(iso, f->offset, f->size, out, buf);
            files++;
            progress(f->size);
            continue;
        }
        // Retain the disc's seekable AFS archive as the runtime package.
        // Loose disc programs and generated/custom resources stay editable.
        std::string stem = lower(u.substr(5, u.size() - 9));
        Uint8 head[8];
        iso.read(f->offset, head, sizeof(head));
        Uint32 count;
        memcpy(&count, head + 4, 4);
        count = SDL_Swap32LE(count);
        if (memcmp(head, "AFS\0", 4) != 0 || (Uint64)count * 8 + 8 > f->size)
            throw Stop{SetupText("error.an_archive_on_the_disc") + f->path + SetupText("error.is_damaged")};
        copy_out(iso, f->offset, f->size, data + stem + ".afs", buf);
        files += (int)count;
        progress(f->size);
    }
    char line[64];
    snprintf(line, sizeof(line), SetupText("progress.game_files_installed"), files);
    say(std::string("@ok ")+line);
}

void step_prepare_text(const std::string &root) {
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
    step("Text resources");
    if (!Android_PrepareText(root)) throw Stop{"Text resource preparation failed. See logs/zenkaipower_log.txt."};
    say("@ok Translations generated and included in game-text.pak");
#else
    std::string exe = root + kGame, saves = Setup_DataRoot(root) + ".selftest";
    const char *args[2] = {exe.c_str(), NULL};
    step("Text resources");
    say((std::string("@note ") + SetupText("progress.generating_and_packaging_translations_from_disc")));
    SDL_Environment *env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env,"ZENKAIPOWER_PREPARE_TEXT","1",true);
    SDL_SetEnvironmentVariable(env, "ZENKAIPOWER_DEMO", "1", true);
    SDL_SetEnvironmentVariable(env, "ZENKAIPOWER_SETTINGS", "", true);
    SDL_SetEnvironmentVariable(env, "ZENKAIPOWER_SAVES", saves.c_str(), true);
    SDL_SetEnvironmentVariable(env, "ZENKAIPOWER_GS", "none", true); // no window: a release program opens one by default
    for (const char *name : {"ZENKAIPOWER_REPLAY", "ZENKAIPOWER_PAD_PLAY", "ZENKAIPOWER_PAD_REC", "ZENKAIPOWER_DATA"}) {
        SDL_UnsetEnvironmentVariable(env, name);
    }
    SDL_SetEnvironmentVariable(env, "ZENKAIPOWER_DATA", Setup_DataRoot(root).c_str(), true);
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, root.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process *proc = Port_CreateHiddenProcess(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (proc == NULL) {
        throw Stop{std::string(SetupText("error.the_game_could_not_be_started")) + SDL_GetError()};
    }
    size_t size = 0;
    int code = -1;
    char *out = (char *)Port_ReadProcess(proc, &size, &code);
    std::string text = out != NULL ? std::string(out, size) : "";
    SDL_free(out);
    SDL_DestroyProcess(proc);
    // How the game ended goes into the log too: when it never got as far as printing anything (the system refused
    // to start it, or something stopped it), the exit code is all there is to go on.
    char ended[160];
    const char *why = "";
    switch ((unsigned)code) {
    case 0xC0000135u: why = SetupText("error.a_dll_the_program_needs_was_not_found"); break;
    case 0xC0000018u: why = SetupText("error.the_program_could_not_be_loaded_at_its_fixed_address"); break;
    case 0xC0000022u: why = SetupText("error.access_denied_often_security_software_blocking_the_program"); break;
    case 0xC000007Bu: why = SetupText("error.a_dll_of_the_wrong_kind_was_loaded"); break;
    case 0xC0000005u: why = SetupText("error.crashed_bad_memory_access"); break;
    case 0xC0000409u: why = SetupText("error.stopped_by_a_security_check"); break;
    default: break;
    }
    snprintf(ended, sizeof(ended), SetupText("error.setup_the_game_exited_with_code_d_0x_08x_s"), code, (unsigned)code, why);
    text += ended;
    if (!text.empty()) SDL_Log("%s", text.c_str());
    if (code!=0 || text.find("text resources prepared and packaged")==std::string::npos)
        throw Stop{SetupText("error.text_resource_generation_failed") + text.substr(text.size()>4096 ? text.size()-4096 : 0)};
    say((std::string("@ok ") + SetupText("progress.translations_generated_and_included_in_game_text_pak")));
#endif
}

// Publish completion only after every requested extraction and resource preparation succeeds.
void mark_installed(const std::string &root) {
    const std::string marker = Setup_DataRoot(root) + ".installed";
    SDL_IOStream *file = SDL_IOFromFile(marker.c_str(), "wb");
    if (!file) throw Stop{SetupText("error.could_not_write_the_installed_marker_check_folder_permissions_and_available_")};
    if (!SDL_CloseIO(file)) {
        SDL_RemovePath(marker.c_str());
        throw Stop{SetupText("error.could_not_finish_writing_the_installed_marker")};
    }
}

// The European release's archive entries: the battle HUD (5 to 9, one per language: English, Spanish, German,
// French, Italian), the menu archives (USA 448 + k is 451 + k + 31 per language), and story dialogue.
std::vector<Uint32> eu_entries() {
    std::vector<Uint32> out;
    for (Uint32 bank = 0; bank < 5; bank++) {
        out.push_back(5 + bank);
        for (Uint32 usa = 448; usa < 479; usa++) {
            out.push_back(451 + (usa - 448) + bank * 31);
        }
    }
    for (Uint32 entry = 656; entry < 906; entry++) out.push_back(entry);
    return out;
}

struct ClosePackageFile { void operator()(FILE *f) const { if (f) fclose(f); } };

// Package the optional language subset without copying the full EU archive.
void pack_subset(const std::string &dir, const std::vector<Uint32> &ids) {
    Uint32 count=0;
    for (Uint32 id: ids) if (id>=count) count=id+1;
    std::vector<Uint32> table((size_t)count*2,0);
    std::string package=dir.substr(0,dir.size()-1)+".afs", temp=package+".part";
    std::unique_ptr<FILE,ClosePackageFile> out(fopen(temp.c_str(),"wb"));
    if (!out) throw Stop{SetupText("error.cannot_create") + temp};
    Uint32 little=SDL_Swap32LE(count);
    if (fwrite("AFS\0",1,4,out.get())!=4 || fwrite(&little,4,1,out.get())!=1 ||
        fwrite(table.data(),4,table.size(),out.get())!=table.size()) throw Stop{SetupText("error.cannot_write") + temp};
    Uint64 offset=((8+(Uint64)count*8+2047)/2048)*2048;
    std::vector<Uint8> buffer(1<<20);
    for (Uint32 id: ids) {
        char name[32]; snprintf(name,sizeof(name),"%05u.bin",id);
        std::unique_ptr<FILE,ClosePackageFile> input(fopen((dir+name).c_str(),"rb"));
        if (!input || fseek(input.get(),0,SEEK_END)) throw Stop{SetupText("error.cannot_read") + dir + name};
        long length=ftell(input.get());
        if (length<0 || offset+(Uint64)length>0x7fffffff) throw Stop{SetupText("error.language_package_is_too_large")};
        rewind(input.get());
        table[id*2]=SDL_Swap32LE((Uint32)offset); table[id*2+1]=SDL_Swap32LE((Uint32)length);
        if (fseek(out.get(),(long)offset,SEEK_SET)) throw Stop{SetupText("error.cannot_seek") + temp};
        long left=length;
        while (left) {
            size_t n=(size_t)left>buffer.size()?buffer.size():(size_t)left;
            if (fread(buffer.data(),1,n,input.get())!=n || fwrite(buffer.data(),1,n,out.get())!=n) throw Stop{SetupText("error.cannot_write") + temp};
            left-=(long)n; check_cancel();
        }
        offset=(offset+length+2047)/2048*2048;
    }
    if (fseek(out.get(),8,SEEK_SET) || fwrite(table.data(),4,table.size(),out.get())!=table.size() || fflush(out.get())) throw Stop{SetupText("error.cannot_finish") + temp};
    if (fclose(out.release())) throw Stop{SetupText("error.cannot_close") + temp};
    if (!SDL_RenamePath(temp.c_str(),package.c_str())) throw Stop{SetupText("error.cannot_publish") + package};
    for (Uint32 id: ids) { char name[32]; snprintf(name,sizeof(name),"%05u.bin",id); SDL_RemovePath((dir+name).c_str()); }
}

void step_eu(const std::string &root, const std::string &path) {
    Iso iso;
    std::string dir = Setup_DataRoot(root) + "pzs3eu1/";
    step("European languages");
    iso.open(path);
    const IsoFile *sles = iso.find("SLES_549.45"), *afs = iso.find("DATA/PZS3EU1.AFS"), *selector = iso.find("DATA/PZS3EU0.AFS");
    const IsoFile *openingAudio = iso.find("DATA/ZS3EUOP.ADX");
    if (sles == NULL || afs == NULL || selector == NULL || openingAudio == NULL) {
        throw Stop{SetupText("error.this_is_not_a_disc_image_of_the_european_release_of_budokai_tenkaichi_3_no_s_2")};
    }
    Uint8 head[8];
    Uint32 count;
    iso.read(afs->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count == 0 || (Uint64)count * 8 + 8 > afs->size) {
        throw Stop{SetupText("error.an_archive_on_the_european_disc_image_data_pzs3eu1_afs_is_damaged")};
    }
    std::vector<Uint32> table((size_t)count * 2);
    iso.read(afs->offset + 8, table.data(), table.size() * 4);
    std::vector<Uint8> buf(1 << 20);
    std::vector<Uint32> want = eu_entries();
    SDL_CreateDirectory(dir.c_str());
    int done = 0;
    for (Uint32 e : want) {
        char name[32], line[64];
        if (e >= count || (Uint64)table[e * 2] + table[e * 2 + 1] > afs->size) {
            throw Stop{SetupText("error.an_archive_on_the_european_disc_image_data_pzs3eu1_afs_is_damaged_or_not_the")};
        }
        snprintf(name, sizeof(name), "%05u", e);
        copy_out(iso, afs->offset + table[e * 2], table[e * 2 + 1], dir + name + ".part", buf);
        if (!SDL_RenamePath((dir + name + ".part").c_str(), (dir + name + ".bin").c_str())) {
            throw Stop{SetupText("error.could_not_write") + dir + name + ".bin."};
        }
        snprintf(line, sizeof(line), "@note %d of %d", ++done, (int)want.size());
        say(line);
        check_cancel();
    }
    iso.read(selector->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count < 2 || selector->size < 24) {
        throw Stop{SetupText("error.the_language_selector_archive_on_the_european_disc_image_data_pzs3eu0_afs_is")};
    }
    Uint32 selectorEntry[2];
    iso.read(selector->offset + 16, selectorEntry, sizeof(selectorEntry));
    if (selectorEntry[1] == 0 || selectorEntry[1] > (1u << 20) ||
        (Uint64)selectorEntry[0] + selectorEntry[1] > selector->size) {
        throw Stop{SetupText("error.the_language_selector_archive_on_the_european_disc_image_data_pzs3eu0_afs_is")};
    }
    std::string selectorDir = Setup_DataRoot(root) + "pzs3eu0/";
    SDL_CreateDirectory(selectorDir.c_str());
    copy_out(iso, selector->offset + selectorEntry[0], selectorEntry[1], selectorDir + "00001.part", buf);
    if (!SDL_RenamePath((selectorDir + "00001.part").c_str(), (selectorDir + "00001.bin").c_str())) {
        throw Stop{SetupText("error.could_not_write") + selectorDir + "00001.bin."};
    }
    std::string movieDir = Setup_DataRoot(root) + "disc/DATA/";
    SDL_CreateDirectory((Setup_DataRoot(root) + "disc/").c_str());
    SDL_CreateDirectory(movieDir.c_str());
    Uint8 magic[2];
    if (openingAudio->size < 2 || openingAudio->size > (16u << 20)) {
        throw Stop{SetupText("error.the_european_opening_soundtrack_has_an_invalid_size")};
    }
    iso.read(openingAudio->offset, magic, sizeof(magic));
    if (magic[0] != 0x80 || magic[1] != 0) {
        throw Stop{SetupText("error.the_european_opening_soundtrack_has_an_invalid_header")};
    }
    std::string dest = movieDir + "ZS3EUOP.ADX";
    copy_out(iso, openingAudio->offset, openingAudio->size, dest + ".part", buf);
    if (!SDL_RenamePath((dest + ".part").c_str(), dest.c_str())) {
        throw Stop{SetupText("error.could_not_write") + dest + "."};
    }
    pack_subset(dir, want);
    pack_subset(Setup_DataRoot(root) + "pzs3eu0/", {1});
    say((std::string("@ok ") + SetupText("progress.european_languages_and_opening_soundtrack_the_game_makes_the_languages_at_it")));
}

#include "optional_extract.inc"
#include "title_texture.h"
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
#include "../../android/app/src/main/cpp/installer_buttons.h"
#endif

void step_optional(const std::string &root, const std::string &path, bool wii) {
    step(wii ? "Wii Dragon Net Battle" : "Sparking! METEOR logo and soundtrack");
    if (wii) extract_wii(root,path);
    else extract_japanese(root,path);
    say((std::string("@ok ") + SetupText("progress.optional_disc_content_read_and_installed_region_and_asset_checks_passed")));
}

void work(std::string root, std::string path, std::string eu, std::string wii, std::string jp, bool desktopShortcut) {
    try {
        sStep = 0;
        sSteps = (!wii.empty() ? 1 : 0) + (!jp.empty() ? 1 : 0) + (path.empty() ? 0 : 3) + (eu.empty() ? 0 : 1) + ((!path.empty() || !eu.empty() || !wii.empty() || !jp.empty()) ? 1 : 0);
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
        if (!path.empty()) ++sSteps;
#endif
        const std::string optional[] = {eu, wii, jp};
        const char *ids[] = {"SLES-54945", "RDSPAF", "SLPS-25815"};
        for (int i = 0; i < 3; i++) if (!optional[i].empty()) {
            say((std::string("@note ") + SetupText("progress.checking_optional_disc_checksum")));
            std::string error = Native_VerifyDisc(root, optional[i], i + 1);
            if (!error.empty()) throw Stop{std::string(ids[i]) + ": " + error};
            say(std::string("@disc-verified ") + ids[i]);
            check_cancel();
        }
// Android can be stopped mid-install: retain the old pak until text preparation
// publishes its replacement atomically. It also preserves edited catalogs.
#ifndef ZENKAIPOWER_ANDROID_INSTALLER
        if (!path.empty() || !eu.empty() || !wii.empty() || !jp.empty()) {
            std::string textPackage = Setup_DataRoot(root) + "game-text.pak";
            SDL_PathInfo textInfo;
            if (SDL_GetPathInfo(textPackage.c_str(), &textInfo) && !SDL_RemovePath(textPackage.c_str()))
                throw Stop{SetupText("error.could_not_write") + textPackage};
        }
#endif
        if (!path.empty()) {
            Iso iso;
            step_disc(iso, path);
            check_cancel();
            step_data(iso, root);
            check_cancel();
        }
        if (!jp.empty()) step_optional(root, jp, false);
        if (!eu.empty()) {
            step_eu(root, eu);
            check_cancel();
        }
        if (!wii.empty()) step_optional(root, wii, true);
#ifdef ZENKAIPOWER_ANDROID_INSTALLER
        if (!path.empty()) android_buttons(root);
#endif
        if (!path.empty() || !eu.empty() || !wii.empty() || !jp.empty() || Native_NeedsShellAssets(root)) step_prepare_text(root);
        check_cancel();
        Port_RemoveEmptyChildren(Setup_DataRoot(root));
#ifdef _WIN32
        {
            auto shortcut=[&](bool japanese) {
                DWORD launcherError=ERROR_SUCCESS;
                if (!Setup_CreateTitleLauncher(std::filesystem::u8path(root),japanese,&launcherError)) {
                    char message[256];
                    snprintf(message,sizeof(message),SetupText("error.could_not_create_s_windows_error_0x_08lx_check_write_permissions_in_the_game"),
                             japanese ? "sm.exe" : "bt3.exe",(unsigned long)launcherError);
                    throw Stop{message};
                }
                HRESULT error=S_OK;
                if (!Setup_CreateShortcut(std::filesystem::u8path(root),japanese,&error)) {
                    char message[256];
                    snprintf(message,sizeof(message),
                        SetupText("error.shortcut_warning"),
                        japanese ? SetupText("titles.meteor") : SetupText("titles.zenkaipower"),(unsigned long)error);
                    say(std::string("@note ")+message);
                } else {
                    say(japanese ? (std::string("@ok ") + SetupText("progress.sparking_meteor_lnk_created")) : (std::string("@ok ") + SetupText("progress.budokai_tenkaichi_3_lnk_created")));
                }
                if (desktopShortcut && !Setup_CreateShortcut(std::filesystem::u8path(root),japanese,&error,true)) {
                    char message[256];
                    snprintf(message,sizeof(message),SetupText("error.could_not_create_the_desktop_shortcut_windows_error_0x_08lx"),(unsigned long)error);
                    throw Stop{message};
                }
            };
            shortcut(false);
            if (Native_DiscInstalled(root,3)) shortcut(true);
        }
#endif
        if (!path.empty()) mark_installed(root);
        say("@done " + root + kGame);
    } catch (const std::exception &error) {
        say("@fail");
        say(std::string((std::string("@stopped ") + SetupText("progress.optional_resource_processing_failed")))+error.what());
    } catch (const Stop &s) {
        std::string why = s.why;
        for (size_t i = 0; (i = why.find('\n', i)) != std::string::npos;) {
            why.replace(i, 1, "\\n");
        }
        say("@fail");
        say("@stopped " + why);
    }
    sRunning = false;
}

} // namespace

std::string Native_VerifyUs(const std::string &path) {
    try {
        Iso iso;
        step_disc(iso, path, false);
        return "";
    } catch (const Stop &error) {
        return error.why;
    } catch (const std::exception &error) {
        return error.what();
    }
}

std::string Native_VerifyDisc(const std::string &root, const std::string &path, int disc) {
    if (disc == 0) return Native_VerifyUs(path);
    // Owner-provided reference images; provenance and hash scope: checksums/README.md.
    const char *expected[] = {"", "e8c4c81294ee86723da74b3db59d63930ae64817",
                             "a6eb9199a5d19eef10af7cfa30699fb032fb3f4b", "fe56a52156ef3d6eb0ad14001736203a77ca95b7"};
    if (disc < 1 || disc > 3) return SetupText("error.unknown_disc_type");
    try {
        Sha1 hash;
        std::vector<Uint8> buffer(1 << 20);
        auto hash_range = [&](auto read, Uint64 offset, Uint64 size) {
            require(size > 0 && size <= (64u << 20), SetupText("error.invalid_verification_file_size"));
            while (size) {
                size_t count = (size_t)std::min<Uint64>(buffer.size(), size);
                read(offset, buffer.data(), count);
                hash.add(buffer.data(), count);
                offset += count;
                size -= count;
            }
        };
        if (disc == 2) {
            // Read the small EU menu archive, not the entire reconstructed Wii disc.
            TemporaryDirectory temporary(std::filesystem::temp_directory_path().u8string());
            disc_run(root, {"extract", "-i", path, "-o", temporary.path, "-g", "-s", "wzs3eu0.afs", "-q"}, false);
            DiskReader menu(find_extracted(temporary.path, "wzs3eu0.afs"));
            hash_range([&](Uint64 at, void *out, size_t n) { menu.read(at, out, n); }, 0, menu.size);
        } else {
            Iso iso;
            iso.open(path);
            const char *serial = disc == 1 ? "SLES_549.45" : "SLPS_258.15";
            if (!iso.find(serial)) return SetupText("error.the_selected_image_has_the_wrong_region_or_disc_id");
            for (const char *name : {serial, "BIN/DBZP.BIN"}) {
                const IsoFile *file = iso.find(name);
                require(file != nullptr, std::string(SetupText("error.missing_verification_file")) + name);
                hash_range([&](Uint64 at, void *out, size_t n) { iso.read(at, out, n); }, file->offset, file->size);
            }
        }
        std::string digest = hash.hex();
        if (digest != expected[disc]) return SetupText("error.the_disc_checksum_does_not_match_the_reference_dump_check_the_region_revisio");
        return "";
    } catch (const Stop &error) { return error.why; }
      catch (const std::exception &error) { return error.what(); }
}

void Native_Start(const std::string &root, const std::string &iso, const std::string &eu, const std::string &wii, const std::string &jp, bool desktopShortcut) {
    if (sThread.joinable()) {
        sThread.join();
    }
    sCancel = false;
    sRunning = true;
    sThread = std::thread(work, root, iso, eu, wii, jp, desktopShortcut);
}

NativeTitleLogo Native_ReadTitleLogo(const std::string &path,bool japanese) {
    try {
        Iso iso; iso.open(path);
        const IsoFile *file=iso.find(japanese ? "DATA/PZS3JP1.AFS" : "DATA/PZS3US1.AFS");
        require(file!=nullptr,SetupText("error.missing_title_archive"));
        Uint64 base=file->offset;
        AfsReader archive([&](Uint64 at,void *p,size_t n){iso.read(base+at,p,n);},file->size);
        Bytes menu=archive.entry(japanese ? 448 : 449,4<<20);
        auto members=ps2_pack(menu);
        require(!members.empty(),SetupText("error.missing_title_screen"));
        Bytes screen=bpe_expand(members[0],false);
        auto parts=ps2_pack(screen);
        require(parts.size()>4,SetupText("error.missing_title_texture"));
        return title_pixels(parts[4]);
    } catch (...) { return {}; }
}

bool Native_NeedsShellAssets(const std::string &root) {
    if (!Port_PackExists((Setup_DataRoot(root) + "game-text/title-zenkaipower.png").c_str()) ||
        (Native_DiscInstalled(root,3) && !Port_PackExists((Setup_DataRoot(root) + "game-text/title-sm.png").c_str()))) return true;
#ifdef _WIN32
    SDL_PathInfo info;
    auto missing=[&](const char *name) {
        return !SDL_GetPathInfo((root+name).c_str(),&info) || info.type!=SDL_PATHTYPE_FILE || info.size==0;
    };
    return missing("zenkaipower.ico") || missing("bt3.exe") || missing("Budokai Tenkaichi 3.lnk") ||
        (Native_DiscInstalled(root,3) && (missing("sm.ico") || missing("sm.exe") || missing("Sparking! METEOR.lnk")));
#else
    (void)root;
    return false;
#endif
}

bool Native_Poll(std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    if (sLines.empty()) {
        return false;
    }
    line = sLines.front();
    sLines.erase(sLines.begin());
    return true;
}

bool Native_Running() { return sRunning; }

void Native_Cancel() { sCancel = true; }

void Native_Join() {
    sCancel = true;
    if (sThread.joinable()) {
        sThread.join();
    }
}

bool Native_Installed(const std::string &root) {
    SDL_PathInfo info;
    return SDL_GetPathInfo((Setup_DataRoot(root) + ".installed").c_str(), &info);
}

// Read an optional subset archive table once, instead of reopening it for every
// entry through Android's document provider. Loose and mixed installs still work.
static bool installed_subset(const std::string &directory, const std::vector<Uint32> &ids) {
    std::unique_ptr<FILE,decltype(&fclose)> archive(fopen((directory+".afs").c_str(),"rb"),fclose);
    std::vector<Uint8> table;
    Uint32 count=0;
    int64_t total=0;
    if (archive) {
        Uint8 header[8];
        if (!Port_PackSeek(archive.get(),0,SEEK_END)) total=Port_PackTell(archive.get());
        if (total>=8 && !Port_PackSeek(archive.get(),0,SEEK_SET) && fread(header,1,8,archive.get())==8 && !memcmp(header,"AFS\0",4)) {
            count=Port_PackU32(header+4);
            if (count<=1000000 && 8+(Uint64)count*8<=(Uint64)total) {
                table.resize((size_t)count*8);
                if (fread(table.data(),1,table.size(),archive.get())!=table.size()) table.clear();
            }
        }
    }
    for (Uint32 id:ids) {
        if (!table.empty() && id<count) {
            Uint32 offset=Port_PackU32(table.data()+id*8), bytes=Port_PackU32(table.data()+id*8+4);
            if (offset>=8+(Uint64)count*8 && (Uint64)offset<=(Uint64)total && bytes<=(Uint64)total-offset) continue;
        }
        char name[32]; snprintf(name,sizeof(name),"/%05u.bin",id);
        struct stat info;
        if (stat((directory+name).c_str(),&info)!=0 || !S_ISREG(info.st_mode)) return false;
    }
    return true;
}
bool Native_EuInstalled(const std::string &root) {
    if (!installed_subset(Setup_DataRoot(root) + "pzs3eu0",{1}) ||
        !installed_subset(Setup_DataRoot(root) + "pzs3eu1",eu_entries())) return false;
    struct stat info;
    return Port_PackStat((Setup_DataRoot(root) + "disc/DATA/ZS3EUOP.ADX").c_str(),&info)==0;
}

// Installed state comes from extracted data, independently of a newly selected image.
bool Native_DiscInstalled(const std::string &root, int disc) {
    if (root.empty()) return false;
    if (disc == 0) return Native_Installed(root);
    if (disc == 1) return Native_EuInstalled(root);
    if (disc != 2 && disc != 3) return false;
    const char *archive = disc == 2 ? "dragon-net-battle.pak" : "sparking-meteor.pak";
    const char *entry = disc == 2 ? "home-dialog.bin" : "title/sparking-meteor.bin";
    int64_t offset, length;
    FILE *file = Port_NamedPackage((Setup_DataRoot(root) + archive).c_str(), entry, &offset, &length, -1, NULL, 0);
    if (!file) return false;
    fclose(file);
    return length > 0;
}

#ifdef ZENKAIPOWER_ANDROID_INSTALLER
bool Native_PrepareTouchArtwork(const std::string &root) {
    if (sRunning) return false;
    Native_Join();
    sCancel = false;
    try {
        android_buttons(root);
        return true;
    } catch (...) { return false; }
}
#endif

// Validate packaged programs too: a completion marker alone must not unlock artwork.
bool Native_VerifyInstalledUs(const std::string &root) {
    try {
        auto read=[&](const char *name) {
            std::string path=Setup_DataRoot(root) + "disc/"+name;
            int64_t base=0,size=0;
            FILE *raw=fopen(path.c_str(),"rb");
            if (raw) {
                if (Port_PackSeek(raw,0,SEEK_END)==0) size=Port_PackTell(raw);
                Port_PackSeek(raw,0,SEEK_SET);
            } else raw=Port_PackOpen(path.c_str(),&base,&size);
            std::unique_ptr<FILE,decltype(&fclose)> file(raw,fclose);
            if (!file || size<=0 || size>(64<<20)) throw Stop{"Invalid installed program"};
            std::vector<Uint8> data((size_t)size);
            if (fread(data.data(),1,data.size(),file.get())!=data.size()) throw Stop{"Truncated installed program"};
            return data;
        };
        verify_us_programs(read("SLUS_216.78"),read("BIN/DBZP.BIN"));
        return true;
    } catch (...) { return false; }
}
