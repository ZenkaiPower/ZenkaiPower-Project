#include "../../setup/native.cpp"
#include <cassert>
#include <fstream>
const char *SetupText(const char *key) { return key; }
int main() {
    TemporaryDirectory temporary(std::filesystem::temp_directory_path().u8string());
    auto directory=temporary.path+"/subset";
    SDL_CreateDirectory(directory.c_str());
    { std::ofstream f(directory+"/00001.bin"); f << "one"; }
    assert(installed_subset(directory,{1}));
    assert(!installed_subset(directory,{1,2}));
    { std::ofstream f(directory+"/00002.bin"); f << "two"; }
    pack_subset(directory+"/",{1,2});
    assert(installed_subset(directory,{1,2}));
    assert(!installed_subset(directory,{0,1,2}));
    // Truncated data must not pass simply because the table entry exists.
    std::filesystem::resize_file(directory+".afs",32);
    assert(!installed_subset(directory,{1,2}));
    { std::ofstream f(directory+"/00001.bin"); f << "one"; }
    { std::ofstream f(directory+"/00002.bin"); f << "two"; }
    assert(installed_subset(directory,{1,2})); // loose fallback
    puts("PASS: packed and loose disc scans, sparse omissions, truncation and fallback");
}
