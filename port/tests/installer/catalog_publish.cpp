// Regression: Android document storage only replaces files within one parent.
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <cassert>
static bool failRename;
static bool catalogRename(const char *from, const char *to) {
    assert(std::filesystem::path(from).parent_path() == std::filesystem::path(to).parent_path());
    return !failRename && SDL_RenamePath(from, to);
}
#define SDL_RenamePath catalogRename
#include "../../setup/native.cpp"
#undef SDL_RenamePath
const char *SetupText(const char *key) { return key; }
int main() {
    TemporaryDirectory temporary(std::filesystem::temp_directory_path().u8string());
    auto target = temporary.path + "/ui/en.json";
    CatalogUpdates updates; updates[target] = "first";
    publish_catalogs(updates);
    updates[target] = "replacement"; publish_catalogs(updates);
    auto read = [&] { std::ifstream file(target); std::string value; file >> value; return value; };
    assert(read() == "replacement");
    failRename = true; updates[target] = "failed";
    try { publish_catalogs(updates); assert(false); } catch (const Stop &) {}
    assert(read() == "replacement");
    assert(!std::filesystem::exists(target + ".zenkai-catalog.tmp"));
    puts("PASS: sibling catalog publication, replacement, and failure preserves previous catalog");
}
