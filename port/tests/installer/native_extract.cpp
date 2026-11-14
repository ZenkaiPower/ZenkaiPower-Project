// Test driver for the same readers used by the release installer. No game binary.
#include "../../setup/native.cpp"
int main(int argc,char **argv) {
    if (argc == 3 && std::string(argv[1]) == "list-wii") {
        try {
            const auto listing=disc_run("", {"extract", "-i", argv[2], "-g", "-l"});
            printf("%s",listing.c_str());
            return 0;
        } catch (const Stop &error) { fprintf(stderr,"%s\n",error.why.c_str()); return 1; }
    }
    if (argc == 3 && std::string(argv[1]) == "publish-catalog") {
        try {
            CatalogUpdates catalogs;
            catalogs[std::string(argv[2])+"/ui/en.json"]="replacement catalog";
            publish_catalogs(catalogs);
            return 0;
        } catch (const Stop &error) { fprintf(stderr,"%s\n",error.why.c_str()); return 1; }
    }
    if (argc == 4 && std::string(argv[1]) == "reinstall-data") {
        try {
            Iso iso; iso.open(argv[3]);
            step_data(iso, std::string(argv[2]) + "/");
            return 0;
        } catch (const Stop &error) { fprintf(stderr, "%s\n", error.why.c_str()); return 1; }
    }
    if (argc == 3 && std::string(argv[1]) == "finish-install") {
        try {
            const std::string root = std::string(argv[2]) + "/";
            step_prepare_text(root);
            mark_installed(root);
            return 0;
        } catch (const Stop &error) { fprintf(stderr, "%s\n", error.why.c_str()); return 1; }
    }
    if (argc == 5 && std::string(argv[1]) == "verify-disc") {
        std::string error = Native_VerifyDisc(argv[2], argv[4], atoi(argv[3]));
        if (!error.empty()) fprintf(stderr, "%s\n", error.c_str());
        return error.empty() ? 0 : 1;
    }
    if (argc == 3 && (std::string(argv[1]) == "verify-us" || std::string(argv[1]) == "verify-fixture")) {
        std::string romHash, menuHash;
        if (std::string(argv[1]) == "verify-fixture") {
            // Test-only expected hashes for a synthetic zero-filled program image.
            std::vector<Uint8> rom(kRomEnd - kRomBase, 0);
            Sha1 a, b;
            a.add(rom.data(), rom.size());
            b.add((const Uint8 *)"fixture menu", 12);
            romHash = a.hex(); menuHash = b.hex();
            kRomSha1 = romHash.c_str(); kDbzpSha1 = menuHash.c_str();
        }
        std::string error = Native_VerifyUs(argv[2]);
        if (!error.empty()) fprintf(stderr, "%s\n", error.c_str());
        std::string event;
        if (Native_Poll(event)) return 3; // Preflight must not emit installation events.
        return error.empty() ? 0 : 1;
    }
    if (argc!=4) return 2;
    SDL_Init(0);
    struct QuitSDL { ~QuitSDL() { SDL_Quit(); } } quit;
    try {
        std::string root=std::filesystem::absolute(std::filesystem::u8path(argv[2])).u8string()+"/";
        step_optional(root,argv[3],std::string(argv[1])=="wii");
        if (!Native_DiscInstalled(root, std::string(argv[1])=="wii" ? 2 : 3)) {
            fprintf(stderr, "Extracted optional disc was not detected as installed\n");
            return 4;
        }
    } catch (const Stop &error) { fprintf(stderr,"%s\n",error.why.c_str()); return 1; }
      catch (const std::exception &error) { fprintf(stderr,"%s\n",error.what()); return 1; }
    return 0;
}
