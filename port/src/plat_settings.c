/*
 * The player's settings, kept between runs in a versioned little-endian binary file:
 *
 *     only at the explicit path in $ZENKAIPOWER_SETTINGS (an unset or empty
 *     $ZENKAIPOWER_SETTINGS: no file)
 *
 * The settings overlay (port/src/gs/gs_gpu.c) writes it whenever a setting changes. At start an environment
 * variable (ZENKAIPOWER_SCALE, ZENKAIPOWER_ASPECT, ...) still wins over the file, so a single run can be started differently.
 * Names: scale, aspect_milli, fullscreen, fx_off, glow, music, effects, voices, stage_sounds, hud_layout, game_speed, music_follows_speed, display; the bindings of port/src/gs/gs_input.c
 * (key_p1_cross = an SDL scancode, pad_p1_cross = a controller button, padslot_p1 = which controller,
 * deadzone_p1_ls / deadzone_p1_rs = a stick's deadzone, vibration_p1 and vibration_p1_strength).
 */
#include "port_save.h"
#include <pthread.h>
#include <stdint.h>
#include <SDL3/SDL_filesystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SETTINGS 200
static struct { char name[24]; int value; } sSet[MAX_SETTINGS];
/* Legacy string settings follow numeric records. ZENKAI stores its menu
   language and online player name only in the encrypted .options companion. */
#define MAX_TEXTS 8
static struct { char name[24]; char value[64]; } sText[MAX_TEXTS];
static int sTextCount;
/* ZENKAI's menu language belongs to the selected .options companion, not
   settings.dat. Cache it so a choice made before saving is also used in-game. */
static char sZenkaiLanguage[16];
static int sZenkaiLanguageLoaded, sZenkaiLanguageValid;
/* Online identity also belongs to the selected encrypted .options companion. */
static char sZenkaiName[20];
static int sZenkaiNameLoaded, sZenkaiNameValid;
/* Cache the startup prompt flag from the same .options companion. */
static int sZenkaiPromptLoaded, sZenkaiPromptValid, sZenkaiPromptAlways;
/* BootCard also runs before the game save: read its Autosave choice early. */
static int sZenkaiAutoSaveLoaded, sZenkaiAutoSaveValid, sZenkaiAutoSaveEnabled;
/* The standalone font/HUD test links plat_settings.c but not plat_mc.c.
   On ELF targets, weak references allow that test to use the ordinary
   settings fallback; the full game resolves these to plat_mc.c. */
#if defined(__GNUC__) || defined(__clang__)
#define PORT_LANGUAGE_OPTIONAL __attribute__((weak))
#else
#define PORT_LANGUAGE_OPTIONAL
#endif
extern int Port_OptionsLanguageRead(char *locale,int size) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsLanguageSave(const char *locale) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsNameRead(char *name,int size) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsNameSave(const char *name) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsPromptRead(int *always) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsPromptSave(int always,const char *locale) PORT_LANGUAGE_OPTIONAL;
extern int Port_OptionsAutoSaveRead(int *enabled) PORT_LANGUAGE_OPTIONAL;
void Port_SettingText(const char *name,const char *def,char *out,int size);
#undef PORT_LANGUAGE_OPTIONAL
static int sDirty;
static int sCount;
static pthread_once_t sOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;

/* More Options edits are live in ZENKAI mode, but become a startup cache
   only after the game save succeeds. Unrelated settings still save normally. */
static struct { const char *name; int value,valid; } sGameSet[]={
    {"scale",0,0},{"aspect_milli",0,0},{"smooth_2d",0,0},{"fullscreen",0,0},
    {"vsync",0,0},{"frame_skip",0,0},{"gpu_api",0,0},{"interp",0,0},
    {"fx_off",0,0},{"sharp_blur",0,0},{"skip_logos",0,0},{"skip_copyright",0,0},
    {"skip_intro",0,0},{"game_version",0,0},
    {"dragon_net_mode",0,0},{"save_quick",0,0},
    {"language_prompt_always",0,0}
};
static int game_key(const char *name) {
    int i;
    for(i=0;i<(int)(sizeof(sGameSet)/sizeof(sGameSet[0]));i++)
        if(!strcmp(name,sGameSet[i].name)) return i;
    return -1;
}

/* Before the boot menu, Port_ZenkaiEnabled() may still be false because the
   game's memory card has not committed its mode yet.  Only the two startup
   language preferences may consult the configured (default-on) ZENKAI mode.
   Do this BEFORE acquiring sLock: Port_Setting("zenkai_power") is recursive. */
int Port_Setting(const char *name, int def);
static int language_zenkai_mode(const char *name) {
    if(Port_ZenkaiEnabled()) return 1;
    if(strcmp(name,"menu_lang") && strcmp(name,"language_prompt_always") &&
       strcmp(name,"save_quick") && strcmp(name,"net_name")) return 0;
    if(getenv("ZENKAIPOWER_SAVES")) return 0;
    if(Port_ZenkaiSelected()) return 1;
    return Port_Setting("zenkai_power",1)!=0;
}

/* Keep settings-file persistence opt-in: F1 changes use the active runtime
 * owners and ZENKAI save companion, and normal play does not create
 * savegame/settings.dat. */
static const char *path(void) {
    const char *p = getenv("ZENKAIPOWER_SETTINGS");
    return p != NULL && p[0] != '\0' ? p : NULL;
}

static int write_u32(FILE *fp, uint32_t v) {
    unsigned char b[4] = {v, v >> 8, v >> 16, v >> 24};
    return fwrite(b, 1, 4, fp) == 4;
}
static int read_u32(FILE *fp, uint32_t *v) {
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    *v = (uint32_t)b[0] | (uint32_t)b[1]<<8 | (uint32_t)b[2]<<16 | (uint32_t)b[3]<<24;
    return 1;
}
/* Fixed-size records avoid compiler padding and host byte-order dependencies. */
static int write_file(void) {
    const char *target = path();
    char temporary[4096];
    FILE *fp;
    int i, ok;
    if (!target || snprintf(temporary,sizeof(temporary),"%s.part",target) >= (int)sizeof(temporary)) return 0;
    fp = fopen(temporary,"wb");
    if (!fp) return 0;
    ok = fwrite("BT3SET01",1,8,fp)==8 && write_u32(fp,sCount) && write_u32(fp,sTextCount);
    for (i=0; ok && i<sCount; i++)
        ok = fwrite(sSet[i].name,1,24,fp)==24 && write_u32(fp,(uint32_t)sSet[i].value);
    for (i=0; ok && i<sTextCount; i++)
        ok = fwrite(sText[i].name,1,24,fp)==24 && fwrite(sText[i].value,1,64,fp)==64;
    if (fclose(fp)) ok=0;
    if (ok) ok=SDL_RenamePath(temporary,target);
    if (!ok) remove(temporary);
    return ok;
}
static void load(void) {
    const char *target = path();
    FILE *fp = target ? fopen(target,"rb") : NULL;
    char magic[8];
    uint32_t count,texts,v,i;
    if (!target) return;
    if (!fp) return;
    if (fread(magic,1,8,fp)==8 && !memcmp(magic,"BT3SET01",8)) {
        int ok=read_u32(fp,&count) && read_u32(fp,&texts) && count<=MAX_SETTINGS && texts<=MAX_TEXTS;
        for (i=0; ok && i<count; i++) {
            ok=fread(sSet[i].name,1,24,fp)==24 && memchr(sSet[i].name,0,24) && read_u32(fp,&v);
            if (ok) sSet[i].value=(int32_t)v;
        }
        for (i=0; ok && i<texts; i++)
            ok=fread(sText[i].name,1,24,fp)==24 && memchr(sText[i].name,0,24) &&
               fread(sText[i].value,1,64,fp)==64 && memchr(sText[i].value,0,64);
        if (ok && fgetc(fp)==EOF && !ferror(fp)) { sCount=count; sTextCount=texts; }
    } else {
        char line[128],name[24],text[64];
        int value;
        rewind(fp);
        while (fgets(line,sizeof(line),fp)) {
            if (sscanf(line," %23[a-z0-9_]=\"%63[^\"\n]",name,text)==2 && sTextCount<MAX_TEXTS) {
                strcpy(sText[sTextCount].name,name); strcpy(sText[sTextCount++].value,text);
            } else if (sscanf(line," %23[a-z0-9_]=%d",name,&value)==2 && sCount<MAX_SETTINGS) {
                strcpy(sSet[sCount].name,name); sSet[sCount++].value=value;
            }
        }
        sDirty=1;
    }
    fclose(fp);
    if (sDirty && write_file()) sDirty=0;
}

/* The saved value of a setting, or `def` when the file does not have it. */
int Port_Setting(const char *name, int def) {
    int i, v = def;
    int zenkai=language_zenkai_mode(name);

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sCount; i++) {
        if (strcmp(sSet[i].name, name) == 0) {
            v = sSet[i].value;
        }
    }
    if(zenkai) {
        if(!strcmp(name,"language_prompt_always")) {
            /* The main game save is read later. Honor the persisted Options
               row before deciding whether to show the language screen. */
            if(!sZenkaiPromptLoaded) {
                sZenkaiPromptValid=Port_OptionsPromptRead ?
                    Port_OptionsPromptRead(&sZenkaiPromptAlways):0;
                sZenkaiPromptLoaded=1;
            }
            v=sZenkaiPromptValid ? sZenkaiPromptAlways : def;
        }
        if(!strcmp(name,"save_quick")) {
            /* Reading the memory-card save here is too late: the startup
               storage chooser has already made its decision. */
            if(!sZenkaiAutoSaveLoaded) {
                sZenkaiAutoSaveValid=Port_OptionsAutoSaveRead ?
                    Port_OptionsAutoSaveRead(&sZenkaiAutoSaveEnabled):0;
                sZenkaiAutoSaveLoaded=1;
            }
            v=sZenkaiAutoSaveValid ? sZenkaiAutoSaveEnabled : def;
        }
        i=game_key(name);
        if(i>=0 && sGameSet[i].valid) v=sGameSet[i].value;
    }
    pthread_mutex_unlock(&sLock);
    return v;
}

/* Safe to call from the startup storage scene, before the game save load. */
int Port_BootAutoSaveEnabled(void) {
    if(getenv("ZENKAIPOWER_SAVES")) return 0;
    if(!Port_ZenkaiEnabled() && !Port_ZenkaiSelected() &&
       Port_Setting("zenkai_power",1)==0) return 0;
    return Port_Setting("save_quick",0)!=0;
}

/* Stores a setting; Port_SettingsWrite puts the changes into the file. */
void Port_SettingSave(const char *name, int value) {
    int i;
    int zenkai=language_zenkai_mode(name);

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    i=zenkai ? game_key(name):-1;
    if(i>=0) {
        sGameSet[i].value=value;sGameSet[i].valid=1;
        pthread_mutex_unlock(&sLock);
        if(!strcmp(name,"language_prompt_always") && Port_OptionsPromptSave) {
            /* The boot check needs this preference before the game's next
               save/load.  The companion writer keeps other settings intact. */
            char locale[16];
            Port_SettingText("menu_lang","",locale,sizeof(locale));
            if(!Port_OptionsPromptSave(value!=0,locale))
                fprintf(stderr,"ZENKAI: could not save language prompt preference to .options\n");
        }
        return;
    }
    for (i = 0; i < sCount && strcmp(sSet[i].name, name) != 0; i++) {
    }
    if (i < MAX_SETTINGS) {
        if (i == sCount) {
            snprintf(sSet[sCount++].name, sizeof(sSet[0].name), "%s", name);
            sDirty = 1;
        }
        if (sSet[i].value != value) {
            sSet[i].value = value;
            sDirty = 1;
        }
    }
    pthread_mutex_unlock(&sLock);
}

/* A saved text setting copied into out (size bytes), or `def` when the file does not have it. */
void Port_SettingText(const char *name, const char *def, char *out, int size) {
    int i;
    int zenkai=language_zenkai_mode(name);

    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    snprintf(out, (size_t)size, "%s", def);
    if (zenkai && strcmp(name,"menu_lang")==0) {
        /* In ZENKAI mode, only the selected .options file counts as a
           previously chosen language. Ignore a stale settings.dat value. */
        if (!sZenkaiLanguageLoaded) {
            sZenkaiLanguageValid=Port_OptionsLanguageRead ?
                Port_OptionsLanguageRead(sZenkaiLanguage,sizeof(sZenkaiLanguage)):0;
            sZenkaiLanguageLoaded=1;
        }
        if (sZenkaiLanguageValid) snprintf(out,(size_t)size,"%s",sZenkaiLanguage);
    } else if (zenkai && strcmp(name,"net_name")==0) {
        /* Do not fall back to the deprecated general settings file. */
        if(!sZenkaiNameLoaded) {
            sZenkaiNameValid=Port_OptionsNameRead ?
                Port_OptionsNameRead(sZenkaiName,sizeof(sZenkaiName)):0;
            sZenkaiNameLoaded=1;
        }
        if(sZenkaiNameValid) snprintf(out,(size_t)size,"%s",sZenkaiName);
    } else {
        for (i = 0; i < sTextCount; i++) {
            if (strcmp(sText[i].name, name) == 0) {
                snprintf(out, (size_t)size, "%s", sText[i].value);
            }
        }
    }
    pthread_mutex_unlock(&sLock);
}

/* Stores a text setting (a double quote or a line break in it becomes an apostrophe or a space: the file's form). */
void Port_SettingTextSave(const char *name, const char *value) {
    char clean[64];
    int i;
    int zenkai=language_zenkai_mode(name);

    snprintf(clean, sizeof(clean), "%s", value);
    for (i = 0; clean[i] != '\0'; i++) {
        clean[i] = clean[i] == '"' ? '\'' : clean[i] == '\n' || clean[i] == '\r' ? ' ' : clean[i];
    }
    pthread_once(&sOnce, load);
    if (zenkai && strcmp(name,"menu_lang")==0) {
        /* Selection is durable immediately if its companion is writable.
           The next successful game save also records this cached choice. */
        size_t len=strlen(clean);
        if (len>=sizeof(sZenkaiLanguage)) return;
        pthread_mutex_lock(&sLock);
        memcpy(sZenkaiLanguage,clean,len+1);
        sZenkaiLanguageLoaded=sZenkaiLanguageValid=1;
        pthread_mutex_unlock(&sLock);
        if (Port_OptionsLanguageSave && !Port_OptionsLanguageSave(clean))
            fprintf(stderr,"ZENKAI: could not save selected language to .options\n");
        return;
    }
    if (zenkai && strcmp(name,"net_name")==0) {
        for(i=0;clean[i];i++)
            if((unsigned char)clean[i]<32 || (unsigned char)clean[i]==127) clean[i]=' ';
        if(strlen(clean)>=sizeof(sZenkaiName)) return;
        pthread_mutex_lock(&sLock);
        strcpy(sZenkaiName,clean);
        sZenkaiNameLoaded=1;
        sZenkaiNameValid=(clean[0]!='\0');
        pthread_mutex_unlock(&sLock);
        if(Port_OptionsNameSave && !Port_OptionsNameSave(clean))
            fprintf(stderr,"ZENKAI: could not save online player name to .options\n");
        return;
    }
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sTextCount && strcmp(sText[i].name, name) != 0; i++) {
    }
    if (i < MAX_TEXTS) {
        if (i == sTextCount) {
            snprintf(sText[i].name, sizeof(sText[i].name), "%s", name);
            sText[i].value[0] = '\0';
            sTextCount++;
            sDirty = 1;
        }
        if (strcmp(sText[i].value, clean) != 0) {
            strcpy(sText[i].value, clean);
            sDirty = 1;
        }
    }
    pthread_mutex_unlock(&sLock);
}

void Port_SettingsWrite(void) {
    pthread_once(&sOnce, load);
    pthread_mutex_lock(&sLock);
    if (sDirty && write_file()) sDirty=0;
    pthread_mutex_unlock(&sLock);
}

/* Called only after a successful ZENKAI game-save operation, never on cancel. */
void Port_SettingsGameCommit(void) {
    int i,j;
    if(!Port_ZenkaiEnabled()) return;
    pthread_once(&sOnce,load);
    pthread_mutex_lock(&sLock);
    for(i=0;i<(int)(sizeof(sGameSet)/sizeof(sGameSet[0]));i++) if(sGameSet[i].valid) {
        for(j=0;j<sCount && strcmp(sSet[j].name,sGameSet[i].name);j++) {}
        if(j>=MAX_SETTINGS) continue;
        if(j==sCount) snprintf(sSet[sCount++].name,sizeof(sSet[0].name),"%s",sGameSet[i].name);
        sSet[j].value=sGameSet[i].value;sDirty=1;
    }
    if(sDirty && write_file()) sDirty=0;
    pthread_mutex_unlock(&sLock);
}
