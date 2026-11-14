/*
 * Memory card library (Sony libmc) on PC: slot 1 is a formatted card whose contents are ordinary files in a folder;
 * slot 2 is empty.
 *
 *     savegame/...     (explicit $ZENKAIPOWER_SAVES retains its legacy card1 layout)
 *
 * The data file is named game.sav. Console icon files are virtual: their open/write/close
 * requests succeed for the original save state machine, without producing host files.
 *
 * The library is asynchronous: a call only starts a request and returns 0; sceMcSync reports the request's number
 * and its result. Here every request is carried out at once and sceMcSync hands the stored result over.
 * Results follow libmc (what src/battle/stgm_a_b.c and src/sys/mcflow_a.c test):
 *     sceMcGetInfo   0 = the card seen last time, -1 = a formatted card seen for the first time, -10 = no card;
 *                    type 2 = a PS2 card, free = free clusters of 1 KB, format 1 = formatted
 *     sceMcOpen      a file handle >= 0, or -4 = no such file      (mode: 1 read, 2 write, 0x200 create)
 *     sceMcMkdir     0, or -4 = it exists already
 *     sceMcGetDir    the number of entries that match (the game only asks "is this directory there": 1 or 0)
 *     sceMcRead / sceMcWrite   bytes transferred;   sceMcSeek   the new position;   sceMcClose / sceMcFormat   0
 * The names are the library's addresses in the PS2 executable; the game sources #define the sce names onto them.
 */
#include "port_save.h"
#include "zenkai_crypto.h"
#include "port_pack.h"
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path) /* Windows: no permission bits */
#endif

#define MC_FILES 8
#define MC_FREE_CLUSTERS 8000 /* an empty 8 MB card */

static int sLastCmd, sLastResult;
static int sSeen;             /* slot 1 was reported once: later sceMcGetInfo calls answer "same card" */
/* An open file as the game knows it (part of the game's state): open or not, how it was opened, which host file,
   the position. The host's own file object is beside it (not state) and is brought in line when it is used, so a
   frame that is run again after the state was restored finds the files as that state says. */
#include "port_host.h"
extern void Port_StateTouch(void *p, unsigned long n); /* gs/state.c */
static struct { int open, mode; long pos; char path[600]; } sMc[MC_FILES];
PORT_HOST static FILE *sHost[MC_FILES] = {NULL};
PORT_HOST static char sHostPath[MC_FILES][600] = {{0}};

/* Only presentation metadata is virtual; gameplay/replay data still uses real files. */
static int icon_path(const char *path) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    return !strcmp(name, "icon.sys") || !strcmp(name, "dbzsm.ico") || !strcmp(name, "dbzsmr.ico");
}

static int zenkai_path(const char *path) {
    const char *name = strrchr(path, '/');
    return !strcmp(name ? name + 1 : path, "zenkaipower.sav");
}

static FILE *host_file(int fd) {
    if (sHost[fd] != NULL && strcmp(sHostPath[fd], sMc[fd].path) != 0) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    if (sHost[fd] == NULL) {
        sHost[fd] = zenkai_path(sMc[fd].path) ? Port_ZenkaiOpen(sMc[fd].path, sMc[fd].mode & ~0x200) :
            fopen(sMc[fd].path, (sMc[fd].mode & 2) ? "r+b" : "rb");
        snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", sMc[fd].path);
    }
    return sHost[fd];
}

static int request(int cmd, int result) {
    sLastCmd = cmd;
    sLastResult = result;
    return 0;
}

/* The host path of a card path; creates the card's folder on first use. Returns 0 for slot 2 (no card). */
static int host_path(int port, const char *name, char *out, size_t size) {
    const char *root = getenv("ZENKAIPOWER_SAVES") != NULL ? getenv("ZENKAIPOWER_SAVES") : "savegame";
    char dir[512];

    if (port != 0) {
        return 0;
    }
    if (!getenv("ZENKAIPOWER_SAVES")) Port_SavePrepare();
    mkdir(root, 0755);
    if (getenv("ZENKAIPOWER_SAVES")) {
        snprintf(dir, sizeof(dir), "%s/card1", root);
        mkdir(dir, 0755);
    }
    while (*name == '/') {
        name++;
    }
    if (strstr(name, "..") != NULL) {
        return 0;
    }
    Port_SaveCardPath(out, size, name);
    return 1;
}

int func_002A1A48(void) { return 0; } /* sceMcInit */

/* sceMcGetInfo: the outputs are filled when the request completes. */
int func_002A25B8(int port, int slot, int *type, int *freeClusters, int *format) {
    char path[600];
    int card = slot == 0 && host_path(port, "", path, sizeof(path));

    if (type != NULL) { *type = card ? 2 : 0; }
    if (freeClusters != NULL) { *freeClusters = card ? MC_FREE_CLUSTERS : 0; }
    if (format != NULL) { *format = card ? 1 : 0; }
    if (!card) {
        return request(1, -10);
    }
    if (!sSeen) {
        sSeen = 1;
        return request(1, -1);
    }
    return request(1, 0);
}

/* sceMcOpen */
int func_002A1E58(int port, int slot, char *name, int mode) {
    char path[600];
    FILE *f;
    int fd;

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(2, -10);
    }
    for (fd = 0; fd < MC_FILES && sMc[fd].open; fd++) {
    }
    if (fd == MC_FILES) {
        return request(2, -7); /* too many open files */
    }
    f = icon_path(path) ? NULL : zenkai_path(path) ? Port_ZenkaiOpen(path, mode) :
        fopen(path, (mode & 2) ? "r+b" : "rb");
    if (!icon_path(path) && !zenkai_path(path) && f == NULL && (mode & 0x200)) {
        f = fopen(path, "w+b");
    }
    if (f == NULL && !icon_path(path)) {
        return request(2, -4);
    }
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
    }
    sHost[fd] = f;
    /* Loading an existing game after Save_Init() cancels its provisional
       new-game marker. A write/create open does not: it may be a new game
       deliberately replacing a save at the same path. */
    if (f && zenkai_path(path) && (mode & 1) && !(mode & (2 | 0x200)))
        Port_ZenkaiExistingSaveLoaded();
    snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", path);
    sMc[fd].open = 1;
    sMc[fd].mode = mode;
    sMc[fd].pos = 0;
    snprintf(sMc[fd].path, sizeof(sMc[fd].path), "%s", path);
    return request(2, fd);
}

/* sceMcMkdir */
int func_002A1F80(int port, int slot, char *name) {
    char path[600];

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(11, -10);
    }
    if (!getenv("ZENKAIPOWER_SAVES") && !strcmp(path, "savegame")) return request(11, 0);
    return request(11, mkdir(path, 0755) == 0 ? 0 : -4);
}

/* sceMcClose */
int func_002A1FB8(int fd) {
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open) {
        return request(3, -4);
    }
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    sMc[fd].open = 0;
    return request(3, 0);
}

/* sceMcSeek: origin 0 = start, 1 = current, 2 = end */
int func_002A2078(int fd, int offset, int origin) {
    FILE *f;
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && icon_path(sMc[fd].path)) {
        sMc[fd].pos = origin == 0 ? offset : sMc[fd].pos + offset;
        return request(4, (int)sMc[fd].pos);
    }
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(4, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    fseek(f, offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
    sMc[fd].pos = ftell(f);
    return request(4, (int)sMc[fd].pos);
}

/* sceMcRead */
int func_002A2208(int fd, void *buf, int size) {
    FILE *f;
    int n;
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && icon_path(sMc[fd].path)) {
        return request(5, 0);
    }
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(5, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    Port_StateTouch(buf, (size_t)size); /* (gs/state.c: file data straight into watched memory) */
    n = (int)fread(buf, 1, (size_t)size, f);
    sMc[fd].pos += n;
    return request(5, n);
}

/* sceMcWrite */
int func_002A2320(int fd, void *buf, int size) {
    int n;

    FILE *f;
    if (fd >= 0 && fd < MC_FILES && sMc[fd].open && icon_path(sMc[fd].path)) {
        sMc[fd].pos += size;
        return request(6, size);
    }
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(6, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    n = (int)fwrite(buf, 1, (size_t)size, f);
    if (fflush(f) || (zenkai_path(sMc[fd].path) && !Port_ZenkaiFlush(sMc[fd].path, f))) return request(6, -4);
    sMc[fd].pos += n;
    return request(6, n);
}

/* sceMcGetDir: the number of entries matching `name` ("*" = everything in the root, otherwise one name). The
   table is not filled: nothing in the game reads it. */
int func_002A27A8(int port, int slot, char *name, int mode, int maxent, void *table) {
    char path[600];
    struct stat st;
    int n = 0;

    (void)mode; (void)maxent; (void)table;
    if (slot != 0 || !host_path(port, strcmp(name, "*") == 0 ? "" : name, path, sizeof(path))) {
        return request(13, -10);
    }
    if (!getenv("ZENKAIPOWER_SAVES") && !strcmp(path, "savegame")) {
        n = stat(Port_ZenkaiSelected() ? "savegame/zenkaipower.sav" : "savegame/game.sav", &st) == 0 ? 1 : 0;
    } else if (strcmp(name, "*") == 0) {
        DIR *d = opendir(path);
        struct dirent *e;
        while (d != NULL && (e = readdir(d)) != NULL) {
            if (e->d_name[0] != '.') {
                n++;
            }
        }
        if (d != NULL) {
            closedir(d);
        }
    } else {
        n = stat(path, &st) == 0 ? 1 : 0;
    }
    return request(13, n);
}

/* sceMcFormat: the folder is always "formatted"; nothing is erased. */
int func_002A2AC8(int port, int slot) {
    (void)slot;
    return request(16, port == 0 ? 0 : -10);
}

/* sceMcSync: 1 = the request has finished (its number and result are returned), -1 = nothing was pending. */
int func_002A2498(int mode, int *cmd, int *result) {
    (void)mode;
    if (sLastCmd == 0) {
        return -1;
    }
    if (cmd != NULL) { *cmd = sLastCmd; }
    if (result != NULL) { *result = sLastResult; }
    sLastCmd = 0;
    return 1;
}

/* The complete Options companion follows the selected save, with the same atomic
   checksummed atomic writer as the main ZENKAI file. Session cards remain isolated. */
#include "port_save.h"
#include "zenkai_crypto.h"
#include "../../include/menu/port_options_save.h"
#include <errno.h>
#include <pthread.h>
static pthread_mutex_t sOptionsFileLock = PTHREAD_MUTEX_INITIALIZER;
/* The boot language chooser can write before libmc has made the save folder. */
static int options_parent_prepare(const char *path) {
    char parent[640];
    size_t i,n=strlen(path);
    if(n>=sizeof(parent)) return 0;
    memcpy(parent,path,n+1);
    for(i=0;i<n;i++) if(parent[i]=='/' || parent[i]=='\\') {
        char slash=parent[i];
        if(i==0 || (i==2 && parent[1]==':')) continue;
        parent[i]=0;
        if(mkdir(parent,0755)!=0 && errno!=EEXIST) return 0;
        parent[i]=slash;
    }
    return 1;
}
static void options_path(char *path,size_t size) {
    /* Port_SaveCardPath uses Port_ZenkaiSelected(), which can still be false
       when the boot language chooser runs.  The ZENKAI companion has one
       stable location, independent of the memory-card initialization order. */
    snprintf(path,size,"savegame/zenkaipower.sav.options");
}
/* 0 means missing (legacy migration allowed); -1 means unreadable/corrupt. */
static int options_file_read_unlocked(void *payload) {
    char path[640];FILE *file;int values[PORT_OPTION_SETTING_COUNT],ok;
    const unsigned char *p=(const unsigned char *)payload;
    size_t bytes,expected;
    /* Startup language and prompt reads happen before Port_ZenkaiCommit().
       Callers only request these early reads for the configured ZENKAI mode. */
    if(getenv("ZENKAIPOWER_SAVES")) return 0;
    options_path(path,sizeof(path));
    errno=0;
    file=fopen(path,"rb");
    if(!file) {
        if(getenv("ZENKAIPOWER_DEBUG_LANGUAGE"))
            fprintf(stderr,"ZENKAI options: %s: %s\n",path,strerror(errno));
        return errno==ENOENT ? 0:-1;
    }
    fclose(file);
    file=Port_ZenkaiOpen(path,1);
    if(!file) {
        if(getenv("ZENKAIPOWER_DEBUG_LANGUAGE"))
            fprintf(stderr,"ZENKAI options: cannot read/validate %s\n",path);
        return -1;
    }
    memset(payload,0,PORT_OPTIONS_FILE_BYTES);
    bytes=fread(payload,1,PORT_OPTIONS_FILE_BYTES,file);
    switch(p[4]) {
    case 1: expected=PORT_OPTIONS_FILE_V1_BYTES;break;
    case 2:case 3: expected=PORT_OPTIONS_FILE_V3_BYTES;break;
    case 4:case 5:case 6: expected=PORT_OPTIONS_FILE_V6_BYTES;break;
    case 7: expected=PORT_OPTIONS_FILE_V7_BYTES;break;
    case 8: expected=PORT_OPTIONS_FILE_BYTES;break;
    default: expected=0;break;
    }
    /* Transitional legacy files keep their binding layout: byte 7 = 2
       appends a language, and byte 7 = 3 also appends a player name. */
    if(p[4]>=1 && p[4]<=6) {
        if(p[7]==2) expected=PORT_OPTIONS_FILE_V7_BYTES;
        if(p[7]==3) expected=PORT_OPTIONS_FILE_BYTES;
    }
    ok=expected && bytes==expected && fgetc(file)==EOF && !ferror(file) &&
       Port_OptionsFileDecode(payload,values);
    fclose(file);
    if(!ok && getenv("ZENKAIPOWER_DEBUG_LANGUAGE"))
        fprintf(stderr,"ZENKAI options: invalid version/size or payload in %s\n",path);
    return ok ? 1:-1;
}
int Port_OptionsFileRead(void *payload) {
    int result;
    if(!Port_ZenkaiEnabled()) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    result=options_file_read_unlocked(payload);
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
static int options_file_write_unlocked(const void *payload) {
    char path[640];FILE *file;int values[PORT_OPTION_SETTING_COUNT],ok;
    const unsigned char *p=(const unsigned char *)payload;
    size_t bytes;
    if(getenv("ZENKAIPOWER_SAVES") || !Port_OptionsFileDecode(payload,values)) return 0;
    if(p[4]==8 || (p[4]<=6 && p[7]==3)) bytes=PORT_OPTIONS_FILE_BYTES;
    else if(p[4]==7 || (p[4]<=6 && p[7]==2)) bytes=PORT_OPTIONS_FILE_V7_BYTES;
    else return 0;
    options_path(path,sizeof(path));
    if(!options_parent_prepare(path)) return 0;
    file=Port_PackTempFile();
    if(!file) return 0;
    ok=fwrite(payload,1,bytes,file)==bytes && Port_ZenkaiFlush(path,file);
    fclose(file);
    return ok;
}
int Port_OptionsFileWrite(const void *payload) {
    unsigned char merged[PORT_OPTIONS_FILE_BYTES],previous[PORT_OPTIONS_FILE_BYTES];
    char name[PORT_OPTIONS_PLAYER_NAME_BYTES];
    int result;
    if(!Port_ZenkaiEnabled() || !payload) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    memcpy(merged,payload,sizeof(merged));
    /* A game save reconstructs native Options from memory. Keep any name
       entered on the render thread between that snapshot and this write. */
    if(merged[4]==8 && options_file_read_unlocked(previous)==1) {
        /* Also honor a name cleared by the user, rather than restoring a
           stale name from an earlier game-save snapshot. */
        memset(merged+PORT_OPTIONS_FILE_V7_BYTES,0,PORT_OPTIONS_PLAYER_NAME_BYTES);
        if(Port_OptionsFileGetName(previous,name,sizeof(name)))
            Port_OptionsFileSetName(merged,name);
    }
    result=options_file_write_unlocked(merged);
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
/* The online name is read and written only from the checksummed .options file.
   A name-only companion is allowed before the first gameplay save. */
int Port_OptionsNameRead(char *name,int size) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int result=0;
    if(!name || size<=0 || getenv("ZENKAIPOWER_SAVES")) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    if(options_file_read_unlocked(payload)==1)
        result=Port_OptionsFileGetName(payload,name,size);
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
int Port_OptionsNameSave(const char *name) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int found,ok=0;
    if(!name || getenv("ZENKAIPOWER_SAVES")) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    found=options_file_read_unlocked(payload);
    if(found==0) {
        memset(payload,0,sizeof(payload));
        payload[0]='Z';payload[1]='O';payload[2]='P';payload[3]='T';
        payload[4]=8;payload[5]=PORT_OPTIONS_SAVE_BYTES;
        payload[6]=PORT_OPTIONS_NATIVE_BYTES;payload[7]=1;
    } else if(found==1) {
        if(payload[4]>=1 && payload[4]<=5) {
            /* Extend, but do not remap any legacy bindings before a game save. */
            payload[7]=3;
        } else if(payload[4]==6) {
            /* v6 and newer use the same native and binding layouts as v8. */
            payload[4]=8;payload[7]=0;
        } else if(payload[4]==7) payload[4]=8;
    } else goto done; /* Don't replace a corrupt options file. */
    if(!Port_OptionsFileSetName(payload,name)) goto done;
    ok=options_file_write_unlocked(payload);
done:
    pthread_mutex_unlock(&sOptionsFileLock);
    return ok;
}
/* Called by Port_SettingText at startup, before the game save is loaded. */
int Port_OptionsLanguageRead(char *locale,int size) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int result=0;
    if(!locale || size<=0) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    if(options_file_read_unlocked(payload)==1)
        result=Port_OptionsFileGetLanguage(payload,locale,size);
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
/* The boot selector runs before the normal game-save load.  Read its
   visual option from the checksummed .options companion, never from the
   gameplay memory card state.  A language-only companion can also carry
   a prompt override without pretending to contain native Options data. */
int Port_OptionsPromptRead(int *always) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int values[PORT_OPTION_SETTING_COUNT],result=0;
    if(!always || getenv("ZENKAIPOWER_SAVES")) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    if(options_file_read_unlocked(payload)==1) {
        if(payload[4]>=7 && (payload[7]&PORT_OPTIONS_PROMPT_SET)) {
            *always=(payload[7]&PORT_OPTIONS_PROMPT_ON)!=0;
            result=1;
        } else if(!Port_OptionsFileLanguageOnly(payload) &&
                  Port_OptionsFileDecode(payload,values)) {
            *always=values[PORT_VISUAL_OPTION_OFFSET+2] != 0;
            result=1;
        }
    }
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
/* Read Autosave before BootCard_Run and before Port_ZenkaiCommit().
   A language-only companion is not a complete Options record; it cannot
   supply the Autosave preference. Saved row 21 is stable regardless of the
   order of the entries in the in-game General Settings menu. */
int Port_OptionsAutoSaveRead(int *enabled) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int values[PORT_OPTION_SETTING_COUNT],result=0;
    if(!enabled || getenv("ZENKAIPOWER_SAVES")) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    if(options_file_read_unlocked(payload)==1 &&
       !Port_OptionsFileLanguageOnly(payload) &&
       Port_OptionsFileDecode(payload,values)) {
        *enabled=values[PORT_AUTOSAVE_OPTION_OFFSET]!=0;
        result=1;
    }
    pthread_mutex_unlock(&sOptionsFileLock);
    return result;
}
/* Persist a Visual Settings toggle immediately, including before the first
   game save.  Preserve all native options, key bindings and the locale.
   This does not modify the game's save until its normal save/commit path. */
int Port_OptionsPromptSave(int always,const char *locale) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int values[PORT_OPTION_SETTING_COUNT],found,ok=0;
    unsigned char *p=payload;
    int row=PORT_VISUAL_OPTION_OFFSET+2;
    pthread_mutex_lock(&sOptionsFileLock);
    found=options_file_read_unlocked(payload);
    if(found==0) {
        /* No main save yet: a language-only record is sufficient. */
        if(!Port_OptionsLocaleValid(locale)) goto done;
        memset(payload,0,sizeof(payload));
        p[0]='Z';p[1]='O';p[2]='P';p[3]='T';
        p[4]=8;p[5]=PORT_OPTIONS_SAVE_BYTES;p[6]=PORT_OPTIONS_NATIVE_BYTES;p[7]=1;
        Port_OptionsFileSetLanguage(payload,locale);
    } else if(found!=1) {
        goto done; /* Do not overwrite an unreadable options companion. */
    }
    if(Port_OptionsFileLanguageOnly(payload)) {
        p[7]=(p[7]&1)|PORT_OPTIONS_PROMPT_SET|
             (always ? PORT_OPTIONS_PROMPT_ON:0);
    } else {
        if(!Port_OptionsFileDecode(payload,values)) goto done;
        if((p[4]>=6 && p[4]<=8) && Port_OptionsLocaleValid(locale)) {
            char existing[PORT_OPTIONS_LOCALE_BYTES];
            if(!Port_OptionsFileGetLanguage(payload,existing,sizeof(existing)))
                Port_OptionsFileSetLanguage(payload,locale);
        }
        /* Old ZOP1 payloads store negative skip/show values. */
        p[8+8+2*row]=(unsigned char)(p[8+3]=='1' ? !always:!!always);
        p[8+9+2*row]=0;
        if(p[4]==6) p[4]=7; /* full v6 may migrate without remapping binds */
        if(p[4]>=7) {
            p[7]=PORT_OPTIONS_PROMPT_SET|(always ? PORT_OPTIONS_PROMPT_ON:0);
        } else if(p[7]==0) {
            /* Retain pre-v6 bindings layout in transitional records. */
            if(!Port_OptionsLocaleValid(locale)) goto done;
            if(!Port_OptionsFileSetLanguage(payload,locale)) goto done;
            p[7]=2;
        }
    }
    ok=options_file_write_unlocked(payload);
done:
    pthread_mutex_unlock(&sOptionsFileLock);
    return ok;
}
/* Write the language immediately, retaining any existing options and bindings.
   Before the first game save, create a language-only companion; the main save
   path will replace it with a full companion after saving succeeds. */
int Port_OptionsLanguageSave(const char *locale) {
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int found,ok=0;
    if(getenv("ZENKAIPOWER_SAVES") || !Port_OptionsLocaleValid(locale)) return 0;
    pthread_mutex_lock(&sOptionsFileLock);
    found=options_file_read_unlocked(payload);
    if(found==0) {
        memset(payload,0,sizeof(payload));
        payload[0]='Z';payload[1]='O';payload[2]='P';payload[3]='T';
        payload[4]=8;payload[5]=PORT_OPTIONS_SAVE_BYTES;
        payload[6]=PORT_OPTIONS_NATIVE_BYTES;payload[7]=1;
    } else if(found==1 && payload[4]>=1 && payload[4]<=8) {
        /* Keep v1-v5's exact binding encoding until the next regular game
           save migrates it. v6 can be promoted directly to v7. */
        if(payload[4]==6) payload[4]=7;
        else if(payload[4]<6 && payload[7]!=3) payload[7]=2;
    } else {
        goto done;
    }
    if(Port_OptionsFileSetLanguage(payload,locale))
        ok=options_file_write_unlocked(payload);
done:
    pthread_mutex_unlock(&sOptionsFileLock);
    return ok;
}
