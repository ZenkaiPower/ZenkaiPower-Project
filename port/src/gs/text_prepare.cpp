/* Setup-only resource generation. Runs before game memory or graphics startup. */
#include <SDL3/SDL.h>
#include "../port_process.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "../../third_party/nlohmann/json.hpp"
#include "../port_pack.h"
#include "../remove_empty_dirs.h"
#include "menu_labels.h"
#ifdef _WIN32
#include "title_icon.h"
#endif
#include "menu_translation.h"
extern "C" {
#include "ui_text_extract.h"
}
namespace {
struct CloseFile { void operator()(FILE *f) const { if (f) fclose(f); } };
using File=std::unique_ptr<FILE,CloseFile>;
struct Entry { std::string name,path; Uint64 size,offset; };
struct Scan { std::string root; std::vector<Entry> files; bool ok=true; };
struct UiOverride { std::string line,original,text; };
SDL_EnumerationResult collect(void *user,const char *dir,const char *name) {
    Scan *scan=(Scan *)user;
    std::string path=std::string(dir)+name;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path.c_str(),&info)) { scan->ok=false; return SDL_ENUM_FAILURE; }
    if (info.type==SDL_PATHTYPE_DIRECTORY) {
        if (!SDL_EnumerateDirectory(path.c_str(),collect,user)) { scan->ok=false; return SDL_ENUM_FAILURE; }
    } else if (info.type==SDL_PATHTYPE_FILE) {
        std::string rel=path.substr(scan->root.size()+1);
        std::replace(rel.begin(),rel.end(),'\\','/');
        if (rel.size()>=1200 || rel.find(".part")!=std::string::npos || rel.find(".tmp")!=std::string::npos) return SDL_ENUM_CONTINUE;
        scan->files.push_back({rel,path,info.size,0});
    }
    return SDL_ENUM_CONTINUE;
}
void word(FILE *f,Uint64 value,int bytes) { for (int i=0;i<bytes;i++) fputc((int)(value>>(8*i))&255,f); }
bool restore(const std::string &dir) {
    File source(fopen((dir+".pak").c_str(),"rb"));
    if (!source) return true;
    unsigned char h[12],record[20];
    if (fread(h,1,12,source.get())!=12 || memcmp(h,"BT3TEXT1",8)) return false;
    Uint32 count=Port_PackU32(h+8);
    if (count>100000) return false;
    std::vector<std::string> names;
    for (Uint32 i=0;i<count;i++) {
        if (fread(record,1,20,source.get())!=20) return false;
        Uint32 length=Port_PackU32(record);
        if (!length || length>=1200) return false;
        std::string name(length,'\0');
        if (fread(&name[0],1,length,source.get())!=length) return false;
        std::replace(name.begin(),name.end(),'\\','/');
        if (name[0]=='/' || name.find("..")!=std::string::npos || name.find(':')!=std::string::npos || name.find('\0')!=std::string::npos) return false;
        names.push_back(name);
    }
    for (const auto &name:names) {
        std::string path=dir+"/"+name;
        SDL_PathInfo info;
        if (SDL_GetPathInfo(path.c_str(),&info)) continue;
        if (!SDL_CreateDirectory(path.substr(0,path.rfind('/')).c_str())) return false;
        File input(Port_PackFopen(path.c_str(),"rb"));
        File output(fopen(path.c_str(),"wb"));
        if (!input || !output || fseek(input.get(),0,SEEK_END)) return false;
        long size=ftell(input.get());
        if (size<0 || fseek(input.get(),0,SEEK_SET) || !Port_PackCopy(input.get(),output.get(),size)) return false;
        if (fclose(output.release())) return false;
    }
    return true;
}
bool package(const std::string &dir) {
    Scan scan; scan.root=dir;
    if (!SDL_EnumerateDirectory(dir.c_str(),collect,&scan) || !scan.ok || scan.files.empty() || scan.files.size()>100000) return false;
    std::sort(scan.files.begin(),scan.files.end(),[](const Entry&a,const Entry&b){return a.name<b.name;});
    Uint64 offset=12;
    for (auto &e:scan.files) offset+=20+e.name.size();
    for (auto &e:scan.files) { e.offset=offset; offset+=e.size; }
    if (offset>0x7fffffff) return false;
    const std::string temp=dir+".pak.part";
    File output(fopen(temp.c_str(),"wb")); if (!output) return false;
    fwrite("BT3TEXT1",1,8,output.get()); word(output.get(),scan.files.size(),4);
    for (const auto&e:scan.files) { word(output.get(),e.name.size(),4); word(output.get(),e.offset,8); word(output.get(),e.size,8); fwrite(e.name.data(),1,e.name.size(),output.get()); }
    for (const auto&e:scan.files) {
        File input(fopen(e.path.c_str(),"rb"));
        if (!input || !Port_PackCopy(input.get(),output.get(),(long)e.size)) return false;
    }
    if (ferror(output.get()) || fflush(output.get()) || fclose(output.release())) return false;
    if (!SDL_RenamePath(temp.c_str(),(dir+".pak").c_str())) return false;
    for (const auto&e:scan.files) SDL_RemovePath(e.path.c_str());
    Port_RemoveEmptyChildren(dir);
    SDL_RemovePath(dir.c_str()); // only succeeds after all packaged files were removed
    return true;
}
bool readJson(const std::string &path,nlohmann::json &value) {
    File input(Port_PackFopen(path.c_str(),"rb"));
    long size;
    std::string data;
    if (!input || fseek(input.get(),0,SEEK_END) || (size=ftell(input.get()))<1 || size>64*1024*1024 ||
        fseek(input.get(),0,SEEK_SET)) return false;
    data.resize((size_t)size);
    if (fread(&data[0],1,data.size(),input.get())!=data.size()) return false;
    value=nlohmann::json::parse(data,nullptr,false);
    return value.is_object();
}
bool applyBundledUiOverrides(const std::string &dataDir,const char *locale) {
    char sourcePath[1200];
    const std::string targetPath=dataDir+"/game-text/translations/ui/"+locale+".json";
    const std::string englishPath=dataDir+"/game-text/translations/ui/en.json";
    nlohmann::json extracted,source;
    if (!MenuTranslation_BundledUiCatalogPath(locale,sourcePath,sizeof(sourcePath))) return true;
    if (!readJson(sourcePath,source) || !source.contains("locale") || source["locale"]!=locale ||
        !source.contains("name") || !source["name"].is_string() ||
        !source.contains("labels") || !source["labels"].is_object() ||
        !source["labels"].is_object()) return false;

    SDL_PathInfo targetInfo;
    if (SDL_GetPathInfo(targetPath.c_str(),&targetInfo)) {
        if (!readJson(targetPath,extracted)) return strcmp(locale,"en") != 0;
        if (!extracted.contains("source") || !extracted["source"].is_string()) return true;
        const std::string targetSource=extracted["source"].get<std::string>();
        const bool generated=(targetSource=="extracted USA game text, version 2" || targetSource=="extracted USA game text, version 3") ||
            targetSource.rfind("European disc, version ",0)==0 ||
            targetSource=="bundled UI overrides, based on extracted USA game text, version 2";
        if (!generated) return true;
    } else {
        if (!strcmp(locale,"en") || !readJson(englishPath,extracted) ||
            !extracted.contains("source") || (extracted["source"]!="extracted USA game text, version 2" && extracted["source"]!="extracted USA game text, version 3") ||
            !extracted.contains("labels") || !extracted["labels"].is_object()) return !strcmp(locale,"en");
        extracted["locale"]=locale;
        extracted["name"]=source["name"];
        extracted["source"]="bundled UI overrides, based on extracted USA game text, version 2";
    }
    if (!extracted.contains("labels") || !extracted["labels"].is_object()) return false;

    std::map<std::string,std::vector<UiOverride>> overrides;
    std::set<std::string> targetGroups;
    for (auto item=source["labels"].begin();item!=source["labels"].end();++item) {
        const size_t slash=item.key().rfind('/');
        if (slash==std::string::npos || !item.value().is_object() ||
            !item.value().contains("original") || !item.value()["original"].is_string() ||
            !item.value().contains("text") || !item.value()["text"].is_string()) continue;
        overrides[item.key().substr(0,slash)].push_back({item.key().substr(slash+1),
            item.value()["original"].get<std::string>(),item.value()["text"].get<std::string>()});
    }
    for (auto item=extracted["labels"].begin();item!=extracted["labels"].end();++item) {
        const size_t slash=item.key().rfind('/');
        if (slash!=std::string::npos) targetGroups.insert(item.key().substr(0,slash));
    }
    size_t applied=0;
    for (const auto &group:overrides) for (const auto &targetGroup:targetGroups) {
        bool matches=true;
        for (const auto &override:group.second) {
            auto target=extracted["labels"].find(targetGroup+"/"+override.line);
            if (target==extracted["labels"].end() || !target.value().is_object() ||
                !target.value().contains("original") || !target.value()["original"].is_string() ||
                target.value()["original"]!=override.original) { matches=false;break; }
        }
        if (!matches) continue;
        for (const auto &override:group.second) {
            extracted["labels"][targetGroup+"/"+override.line]["text"]=override.text;
            applied++;
        }
    }
    if (applied==0) return true;

    const std::string temporary=targetPath+".part";
    const std::string serialized=extracted.dump(2)+"\n";
    FILE *output=fopen(temporary.c_str(),"wb");
    if (!output) return false;
    bool ok=fwrite(serialized.data(),1,serialized.size(),output)==serialized.size() && fflush(output)==0;
    if (fclose(output)!=0) ok=false;
    if (ok) ok=SDL_RenamePath(temporary.c_str(),targetPath.c_str());
    if (!ok) remove(temporary.c_str());
    if (ok) fprintf(stderr,"zenkaipower: applied %zu bundled %s UI overrides\n",applied,locale);
    return ok;
}
}
extern "C" int Port_PrepareText(void) {
    const char *root=SDL_getenv("ZENKAIPOWER_DATA");
    std::string data=root && *root?root:"gamedata", dir=data+"/game-text";
    char why[512]={0};
    SDL_setenv_unsafe("ZENKAIPOWER_SETTINGS","",1);
    SDL_setenv_unsafe("ZENKAIPOWER_MENU_LANG","en",1);
    if (!SDL_CreateDirectory(dir.c_str()) || !restore(dir)) {
        fprintf(stderr,"zenkaipower: cannot restore existing text resources for setup\n"); return 1;
    }
    UiText_ExtractOnStartup(dir.c_str()); // synchronous in setup mode
    UiText_ExtractSkills(dir.c_str()); // generated catalogs publish as game-skills.pak
    if (MenuLabels_Make(data.c_str(),dir.c_str(),0,why,sizeof(why))!=MENU_LABELS_MADE) {
        fprintf(stderr,"zenkaipower: text generation failed: %s\n",why); return 1;
    }
    if (UiText_EuropeanOnStartup(dir.c_str(),why,sizeof(why))<0) {
        fprintf(stderr,"zenkaipower: European text generation failed: %s\n",why); return 1;
    }
    for (const char *locale : {"en","de","es","fr","it"}) {
        if (!applyBundledUiOverrides(data,locale)) {
            fprintf(stderr,"zenkaipower: failed to apply bundled %s UI overrides\n",locale); return 1;
        }
    }
    if (!Port_PackExists((dir+"/translations/ui/en.json").c_str())) return 1;
    // Reuse the icon's exact disc artwork in setup's cover panel on both OSes.
    for (bool japanese : {false,true}) {
        int width=0,height=0;
        unsigned char *pixels=japanese ? MenuLabels_MeteorLogo(data.c_str(),&width,&height) :
                                        MenuLabels_TitleLogo(data.c_str(),&width,&height);
        if (!pixels) continue;
        SDL_Surface *logo=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,pixels,width*4);
        std::string path=dir+(japanese ? "/title-sm.png" : "/title-zenkaipower.png");
        if (logo) SDL_SavePNG(logo,path.c_str());
        SDL_DestroySurface(logo); free(pixels);
    }
    File marker(fopen((dir+"/.prepared").c_str(),"wb"));
    if (!marker) return 1;
    fputs("1\n",marker.get()); marker.reset();
    if (!package(dir)) { fprintf(stderr,"zenkaipower: text packaging failed; working files retained\n"); return 1; }
    const std::string disc=data+"/disc";
    SDL_PathInfo discInfo;
    if (SDL_GetPathInfo(disc.c_str(),&discInfo) && discInfo.type==SDL_PATHTYPE_DIRECTORY) {
        if (!restore(disc) || !package(disc)) {
            fprintf(stderr,"zenkaipower: disc packaging failed; source files retained\n"); return 1;
        }
    }
    const std::string dragon=data+"/dragon-net-battle";
    SDL_PathInfo dragonInfo;
    if (SDL_GetPathInfo(dragon.c_str(),&dragonInfo) && dragonInfo.type==SDL_PATHTYPE_DIRECTORY) {
        if (!restore(dragon) || !package(dragon)) {
            fprintf(stderr,"zenkaipower: Dragon Net Battle packaging failed; source files retained\n"); return 1;
        }
    }
#ifdef _WIN32
    auto writeIcon=[&](bool japanese,const char *iconPath) {
        int width=0,height=0;
        unsigned char *logo=japanese ? MenuLabels_MeteorLogo(data.c_str(),&width,&height) :
                                      MenuLabels_TitleLogo(data.c_str(),&width,&height);
        if (!logo) {
            fprintf(stderr,"zenkaipower: warning: cannot decode title artwork for %s\n",iconPath);
            return false;
        }
        auto icon=TitleIcon_Encode(logo,width,height);
        free(logo);
        if (icon.empty()) {
            fprintf(stderr,"zenkaipower: warning: cannot encode %s (%dx%d): %s\n",iconPath,width,height,SDL_GetError());
            return false;
        }
        const std::string temp=std::string(iconPath)+".part";
        SDL_IOStream *file=SDL_IOFromFile(temp.c_str(),"wb");
        if (!file) {
            fprintf(stderr,"zenkaipower: warning: cannot create %s: %s\n",temp.c_str(),SDL_GetError());
            return false;
        }
        bool written=SDL_WriteIO(file,icon.data(),icon.size())==icon.size();
        bool closed=SDL_CloseIO(file);
        if (!written || !closed || !SDL_RenamePath(temp.c_str(),iconPath)) {
            fprintf(stderr,"zenkaipower: warning: cannot publish %s: %s\n",iconPath,SDL_GetError());
            SDL_RemovePath(temp.c_str()); return false;
        }
        return true;
    };
    // Shortcut artwork is optional; generated game text must remain usable.
    writeIcon(false,"zenkaipower.ico");
    if (Port_PackExists((data+"/title/sparking-meteor.bin").c_str())) writeIcon(true,"sm.ico");
#endif
    fprintf(stderr,"zenkaipower: text resources prepared and packaged\n");
    return 0;
}

/* Generate in a separate process: setup temporarily selects English and disables
   settings, and its extraction caches must not leak into the player's game. */
extern "C" int Port_EnsureTextPackage(const char *program) {
    const char *root=SDL_getenv("ZENKAIPOWER_DATA");
    const std::string data=root && *root?root:"gamedata";
    SDL_PathInfo info;
    if (SDL_GetPathInfo((data+"/game-text.pak").c_str(),&info) && info.type==SDL_PATHTYPE_FILE)
        return 0;
#ifdef __ANDROID__
    (void)program;
    fprintf(stderr,"zenkaipower: missing game-text.pak; import data prepared by the desktop setup\n");
    return 1;
#else
    fprintf(stderr,"zenkaipower: preparing missing game-text.pak before startup\n");
    const char *args[]={program,NULL};
    SDL_Environment *env=SDL_CreateEnvironment(true);
    if (!env) return 1;
    SDL_SetEnvironmentVariable(env,"ZENKAIPOWER_PREPARE_TEXT","1",true);
    SDL_PropertiesID props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void *)args);
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER,env);
    SDL_Process *child=Port_CreateHiddenProcess(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    int code=1;
    bool ok=child && Port_WaitProcess(child,true,&code);
    if (child) SDL_DestroyProcess(child);
    if (!ok || code!=0) {
        fprintf(stderr,"zenkaipower: startup text packaging failed; loose resources retained\n");
        return 1;
    }
    return 0;
#endif
}
