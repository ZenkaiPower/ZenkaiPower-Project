/* Isolated settings cache test; does not touch player files. */
#include "../../src/plat_settings.c"
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
static pthread_mutex_t storageGate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t storageCondition=PTHREAD_COND_INITIALIZER;
static int slowStorage,storageStarted,storageReleased;
/* Delay the real publication point without touching a player's files. */
bool SDL_RenamePath(const char *oldPath,const char *newPath) {
    pthread_mutex_lock(&storageGate);
    if(slowStorage) {
        storageStarted=1;pthread_cond_broadcast(&storageCondition);
        while(!storageReleased) pthread_cond_wait(&storageCondition,&storageGate);
    }
    pthread_mutex_unlock(&storageGate);
    return rename(oldPath,newPath)==0;
}
static void *commit_settings(void *unused) {(void)unused;Port_SettingsGameCommit();return NULL;}
static int active;
int Port_ZenkaiEnabled(void) { return active; }
int Port_ZenkaiSelected(void) { return active; }
int main(void) {
    char dir[]="/tmp/zenkai-options-settings-XXXXXX",file[256],cwd[PATH_MAX];
    assert(mkdtemp(dir));assert(getcwd(cwd,sizeof(cwd)));assert(chdir(dir)==0);
    /* Normal play never creates either historical default settings location. */
    assert(mkdir("savegame",0700)==0);unsetenv("ZENKAIPOWER_SETTINGS");
    Port_SettingSave("zenkai_power",0); /* make the inactive phase explicitly Original mode */
    Port_SettingSave("music",50);Port_SettingsWrite();
    assert(access("settings.dat",F_OK)!=0 && access("savegame/settings.dat",F_OK)!=0);
    snprintf(file,sizeof(file),"%s/settings.dat",dir);
    setenv("ZENKAIPOWER_SETTINGS",file,1);
    Port_SettingSave("save_quick",1);Port_SettingSave("skip_intro",0);Port_SettingsWrite();
    active=1;
    Port_SettingSave("save_quick",0);Port_SettingSave("skip_intro",1);Port_SettingsWrite();
    assert(Port_Setting("save_quick",1)==0 && Port_Setting("skip_intro",0)==1);
    /* An unrelated settings write must not persist an unconfirmed game edit. */
    Port_SettingSave("music",75);Port_SettingsWrite();
    active=0;assert(Port_Setting("save_quick",0)==1 && Port_Setting("skip_intro",1)==0);
    active=1;Port_SettingsGameCommit();
    active=0;assert(Port_Setting("save_quick",1)==0 && Port_Setting("skip_intro",0)==1);
    assert(Port_Setting("music",0)==75);
    active=1;Port_SettingSave("skip_intro",0);slowStorage=1;
    pthread_t writer;assert(!pthread_create(&writer,NULL,commit_settings,NULL));
    pthread_mutex_lock(&storageGate);
    while(!storageStarted) pthread_cond_wait(&storageCondition,&storageGate);
    pthread_mutex_unlock(&storageGate);
    for(int frame=0;frame<120;frame++) assert(Port_Setting("music",0)==75);
    Port_SettingSave("music",76); /* edits during an old write must stay dirty */
    pthread_mutex_lock(&storageGate);storageReleased=1;pthread_cond_broadcast(&storageCondition);pthread_mutex_unlock(&storageGate);
    pthread_join(writer,NULL);assert(sDirty && Port_Setting("music",0)==76);
    Port_SettingsWrite();assert(!sDirty);

    remove(file);rmdir("savegame");assert(chdir(cwd)==0);rmdir(dir);
    puts("ZENKAI settings cache commit/isolation checks passed");return 0;
}
