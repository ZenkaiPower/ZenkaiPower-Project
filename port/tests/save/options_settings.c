/* Isolated settings cache test; does not touch player files. */
#include "../../src/plat_settings.c"
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
static int active;
int Port_ZenkaiEnabled(void) { return active; }
int Port_ZenkaiSelected(void) { return active; }
int main(void) {
    char dir[]="/tmp/zenkai-options-settings-XXXXXX",file[256],cwd[PATH_MAX];
    assert(mkdtemp(dir));assert(getcwd(cwd,sizeof(cwd)));assert(chdir(dir)==0);
    /* Normal play never creates either historical default settings location. */
    assert(mkdir("savegame",0700)==0);unsetenv("ZENKAIPOWER_SETTINGS");
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
    remove(file);rmdir("savegame");assert(chdir(cwd)==0);rmdir(dir);
    puts("ZENKAI settings cache commit/isolation checks passed");return 0;
}
