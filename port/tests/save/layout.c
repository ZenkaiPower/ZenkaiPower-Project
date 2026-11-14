/* gcc -std=gnu11 port/tests/save/layout.c port/src/plat_zenkai.c port/src/zenkai_crypto.c -o /tmp/save-layout && /tmp/save-layout */
#include "../../src/plat_mc.c"
#include <assert.h>
int Port_Setting(const char *name, int fallback) {(void)name; return fallback;}
void Port_StateTouch(void *p, unsigned long n) {(void)p; (void)n;}
static int result(void) { int r; assert(func_002A2498(0,NULL,&r)==1); return r; }
static int open_card(const char *name,int mode) {func_002A1E58(0,0,(char *)name,mode);return result();}
static void put(const char *path,const char *data) {FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,strlen(data),f)==strlen(data));assert(fclose(f)==0);}
static void check(const char *path,const char *expected) {char b[64]={0};FILE *f=fopen(path,"rb");assert(f);fread(b,1,63,f);fclose(f);assert(!strcmp(b,expected));}
int main(void) {
 char dir[]="/tmp/zenkai-save-XXXXXX", path[600]; assert(mkdtemp(dir));assert(chdir(dir)==0);unsetenv("ZENKAIPOWER_SAVES");
 const char *data="/BASLUS-21678DBZT3/BASLUS-21678DBZT3";
 int fd=open_card(data,0x202);assert(fd>=0);
 func_002A2320(fd,"progress",8);assert(result()==8);func_002A1FB8(fd);assert(result()==0);
 check("savegame/game.sav","progress");
 const char *icons[]={"icon.sys","dbzsm.ico","dbzsmr.ico"};
 for(int i=0;i<3;i++) {
  snprintf(path,sizeof path,"/BASLUS-21678DBZT3/%s",icons[i]);
  fd=open_card(path,0x202);assert(fd>=0);func_002A2320(fd,"icon",4);assert(result()==4);func_002A1FB8(fd);result();
  fd=open_card(path,1);assert(fd>=0);func_002A1FB8(fd);result();
  snprintf(path,sizeof path,"savegame/%s",icons[i]);assert(access(path,F_OK)!=0);
 }
 func_002A27A8(0,0,"/BASLUS-21678DBZT3",0,1,NULL);assert(result()==1);
 setenv("ZENKAIPOWER_SAVES","session",1);func_002A1F80(0,0,"/BASLUS-21678DBZT3");assert(result()==0);
 fd=open_card(data,0x202);assert(fd>=0);func_002A2320(fd,"online",6);assert(result()==6);func_002A1FB8(fd);result();
 fd=open_card(data,1);assert(fd>=0);func_002A1FB8(fd);result();check("session/card1/BASLUS-21678DBZT3/game.sav","online");check("savegame/game.sav","progress");
 puts("Save layout checks passed (fresh save, virtual icons, online isolation).");
 return 0;
}
