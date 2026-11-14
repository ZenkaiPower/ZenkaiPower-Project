/* Run in an empty directory. */
#include "../../src/port_save.h"
#include <assert.h>
static void put(const char *path, const char *text) { FILE *f=fopen(path,"wb"); assert(f); fputs(text,f); fclose(f); }
static void check(const char *path,const char *text) { char b[64]={0}; FILE *f=fopen(path,"rb"); assert(f); fread(b,1,63,f); fclose(f); assert(!strcmp(b,text)); }
int main(void) {
 SAVE_MKDIR("save_game"); put("save_game/settings.dat","settings"); put("save_game/custom","custom");
 Port_SavePrepare(); check("save_game/settings.dat","settings"); check("save_game/custom","custom");
 struct stat st; assert(stat("savegame", &st)==0); assert(stat("savegame/settings.dat", &st)!=0);
 put("savegame/settings.dat","new"); Port_SavePrepare(); check("savegame/settings.dat","new");
 puts("PASS: savegame is created without migrating save_game files"); return 0;
}
