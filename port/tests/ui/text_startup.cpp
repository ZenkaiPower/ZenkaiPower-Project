/* c++ -std=c++17 port/tests/ui/text_startup.cpp -lSDL3 -o /tmp/zenkaipower-text-startup && /tmp/zenkaipower-text-startup */
#include "../../src/gs/text_prepare.cpp"
#include <cassert>
#include <filesystem>

extern "C" void UiText_ExtractOnStartup(const char *dir) {
    const std::string ui=std::string(dir)+"/translations/ui";
    assert(SDL_CreateDirectory(ui.c_str()));
    FILE *f=fopen((ui+"/en.json").c_str(),"wb"); assert(f);
        fputs(R"({"schema_version":1,"locale":"en","source":"extracted USA game text, version 2","labels":{
            "pzs3us1/00448/1/25/6/13":{"original":"Save game data?","text":"Save game data?"},
            "pzs3us1/00448/1/25/6/17":{"original":"Save successful.","text":"Save successful."},
            "pzs3us1/00449/1/12/6/13":{"original":"Save game data?","text":"Save game data?"},
            "pzs3us1/00449/1/12/6/17":{"original":"Save successful.","text":"Save successful."},
            "pzs3us1/00451/6/5/6/13":{"original":"Save game data?","text":"Save game data?"},
            "pzs3us1/00451/6/5/6/17":{"original":"Different result","text":"Different result"},
            "fixture/untouched":{"original":"Leave this unchanged","text":"Leave this unchanged"}
    }})",f); fclose(f);
}
extern "C" int MenuTranslation_BundledUiCatalogPath(const char *locale,char *path,size_t size) {
    const char *source=SDL_getenv(!strcmp(locale,"en") ? "ZENKAIPOWER_TEST_UI_CATALOG" :
                                  !strcmp(locale,"es") ? "ZENKAIPOWER_TEST_ES_CATALOG" : "ZENKAIPOWER_TEST_NO_CATALOG");
    if (!source || strlen(source)>=size) return 0;
    strcpy(path,source);
    return 1;
}
extern "C" int MenuLabels_Make(const char *,const char *dir,int,char *,size_t) {
    FILE *f=fopen((std::string(dir)+"/labels.tsv").c_str(),"wb"); assert(f);
    fputs("fixture\n",f); fclose(f);
    return MENU_LABELS_MADE;
}
extern "C" unsigned char *MenuLabels_TitleLogo(const char *,int *,int *) { return nullptr; }
extern "C" unsigned char *MenuLabels_MeteorLogo(const char *,int *,int *) { return nullptr; }
extern "C" int UiText_EuropeanOnStartup(const char *,char *,size_t) { return 0; }

int main(int argc,char **argv) {
    if (SDL_getenv("ZENKAIPOWER_PREPARE_TEXT")) return Port_PrepareText();
    (void)argc;
    const auto data=std::filesystem::temp_directory_path()/
        ("zenkaipower-text-startup-"+std::to_string(SDL_GetPerformanceCounter()));
    assert(SDL_CreateDirectory(data.string().c_str()));
    const auto source=data/"source-ui.json";
    const auto spanish=data/"source-ui-es.json";
    {
        FILE *f=fopen(source.string().c_str(),"wb"); assert(f);
        fputs(R"({"schema_version":1,"locale":"en","name":"English","labels":{
          "pzs3us1/00448/1/25/6/13":{"original":"Save game data?","text":"Save progress now?"},
                    "pzs3us1/00448/1/25/6/17":{"original":"Save successful.","text":"Save complete."},
          "fixture/untouched":{"original":"This does not match","text":"Must not be applied"}
        }})",f); fclose(f);
    }
    {
        FILE *f=fopen(spanish.string().c_str(),"wb"); assert(f);
        fputs(R"({"schema_version":1,"locale":"es","name":"Español","labels":{
          "pzs3us1/00448/1/25/6/13":{"original":"Save game data?","text":"¿Guardar el progreso?"}
        }})",f); fclose(f);
    }
    SDL_setenv_unsafe("ZENKAIPOWER_DATA",data.string().c_str(),1);
    SDL_setenv_unsafe("ZENKAIPOWER_TEST_UI_CATALOG",source.string().c_str(),1);
    SDL_setenv_unsafe("ZENKAIPOWER_TEST_ES_CATALOG",spanish.string().c_str(),1);
    SDL_setenv_unsafe("ZENKAIPOWER_MENU_LANG","es",1);
    SDL_setenv_unsafe("ZENKAIPOWER_SETTINGS","preserve-settings",1);
    assert(Port_EnsureTextPackage(argv[0])==0);
    assert(std::filesystem::is_regular_file(data/"game-text.pak"));
    assert(!std::filesystem::exists(data/"game-text"));
    assert(Port_PackExists((data/"game-text/.prepared").string().c_str()));
    assert(Port_PackExists((data/"game-text/translations/ui/en.json").string().c_str()));
    {
        FILE *f=Port_PackFopen((data/"game-text/translations/ui/en.json").string().c_str(),"rb"); assert(f);
        nlohmann::json catalog=nlohmann::json::parse(f); fclose(f);
        assert(catalog["labels"]["pzs3us1/00448/1/25/6/13"]["original"]=="Save game data?");
        assert(catalog["labels"]["pzs3us1/00448/1/25/6/13"]["text"]=="Save progress now?");
        assert(catalog["labels"]["pzs3us1/00448/1/25/6/17"]["text"]=="Save complete.");
        assert(catalog["labels"]["pzs3us1/00449/1/12/6/13"]["text"]=="Save progress now?");
        assert(catalog["labels"]["pzs3us1/00449/1/12/6/17"]["text"]=="Save complete.");
        assert(catalog["labels"]["pzs3us1/00451/6/5/6/13"]["text"]=="Save game data?");
        assert(catalog["labels"]["fixture/untouched"]["text"]=="Leave this unchanged");
    }
    {
        FILE *f=Port_PackFopen((data/"game-text/translations/ui/es.json").string().c_str(),"rb"); assert(f);
        nlohmann::json catalog=nlohmann::json::parse(f); fclose(f);
        assert(catalog["locale"]=="es" && catalog["name"]=="Español");
        assert(catalog["labels"]["pzs3us1/00448/1/25/6/13"]["text"]=="¿Guardar el progreso?");
        assert(catalog["labels"]["pzs3us1/00449/1/12/6/13"]["text"]=="¿Guardar el progreso?");
        assert(catalog["labels"]["fixture/untouched"]["text"]=="Leave this unchanged");
    }
    assert(!strcmp(SDL_getenv("ZENKAIPOWER_MENU_LANG"),"es"));
    assert(!strcmp(SDL_getenv("ZENKAIPOWER_SETTINGS"),"preserve-settings"));
    assert(!SDL_getenv("ZENKAIPOWER_PREPARE_TEXT"));
    auto stamp=std::filesystem::last_write_time(data/"game-text.pak");
    assert(Port_EnsureTextPackage(argv[0])==0);
    assert(std::filesystem::last_write_time(data/"game-text.pak")==stamp);
    std::filesystem::remove_all(data);
    puts("Startup text packaging checks passed");
}
