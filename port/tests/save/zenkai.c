/* gcc -std=gnu11 -Iinclude -fsanitize=address,undefined port/tests/save/zenkai.c port/src/plat_zenkai.c port/src/zenkai_crypto.c -o /tmp/zenkai-save && /tmp/zenkai-save */
#include "../../src/plat_mc.c"
#include <assert.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
#define PORT
#include "../../../include/menu/port_story_badge.h"
#include "../../../include/menu/port_story_endings.h"
/* Build a v1 envelope to verify old save formats are rejected. */
extern void crypto_aead_lock(uint8_t *ciphertext, uint8_t tag[16], const uint8_t key[32],
                             const uint8_t nonce[24], const uint8_t *ad, size_t ad_size,
                             const uint8_t *plaintext, size_t plaintext_size);
static const u8 legacyKey[32] = {
 0xa1,0xe4,0x49,0x78,0xa6,0x8f,0x23,0x59,0xf1,0x32,0xb7,0x04,0x4d,0x9e,0x57,0xc2,
 0xf0,0x36,0xb5,0x8e,0x75,0x1d,0xc7,0xa9,0x0a,0xd6,0x63,0x20,0x51,0xef,0x84,0x3b
};
static int setting;
int Port_Setting(const char *name, int fallback) { return !strcmp(name,"zenkai_power") && setting>=0 ? setting : fallback; }
void Port_StateTouch(void *p, unsigned long n) { (void)p;(void)n; }
static int result(void) { int r; assert(func_002A2498(0,NULL,&r)==1); return r; }
static int open_card(int mode) {
 func_002A1E58(0,0,"/BASLUS-21678DBZT3/BASLUS-21678DBZT3",mode);return result();
}
static void close_card(int fd) { func_002A1FB8(fd);assert(result()==0); }
int main(void) {
 char dir[]="/tmp/zenkai-save-XXXXXX",path[600];
 u8 data[0x4000]={0}, readback[0x4000], raw[0x4000+88], first[0x4000+88];
 char save_id[33], save_hash[65], old_id[33], old_hash[65];
 int fd; FILE *f;
 assert(mkdtemp(dir) && !chdir(dir));unsetenv("ZENKAIPOWER_SAVES");
 assert(!Port_ZenkaiEnabled());
 setting=-1;Port_ZenkaiSelect();assert(Port_ZenkaiSelected());
 Port_ZenkaiFinish();assert(!Port_ZenkaiEnabled());
 setting=0;Port_ZenkaiSelect();assert(!Port_ZenkaiSelected());Port_ZenkaiFinish();
 setting=1;Port_ZenkaiSelect();assert(Port_ZenkaiSelected() && !Port_ZenkaiEnabled());
 Port_ZenkaiFinish();assert(!Port_ZenkaiSelected()); /* cancellation */
 Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();assert(Port_ZenkaiEnabled());
 Port_StoryRecordClear(data,0,0,1);assert(Port_StoryClearLevel(data,0,0)==2);
 Port_StoryRecordClear(data,0,0,0);assert(Port_StoryClearLevel(data,0,0)==2);
 /* Legacy clears do not invent endings; collect exact IDs without overlap. */
 assert(Port_StoryEndingCount(data,0,0)==0);
 data[0x37ff]=0xa5;data[0x3bf8]=0x5a;data[0x3f00]=0x77;
 assert(Port_StoryRecordEnding(data,0,0,10030));
 assert(Port_StoryRecordEnding(data,0,0,10032));
 assert(Port_StoryRecordEnding(data,0,0,10031));
 assert(!Port_StoryRecordEnding(data,0,0,10030));
 assert(Port_StoryRecordEnding(data,8,15,10050));
 assert(data[0x37ff]==0xa5 && data[0x3bf8]==0x5a && data[0x3f00]==0x77);
 assert(Port_StoryClearLevel(data,0,0)==2);
 data[30]=42;
 fd=open_card(0x202);assert(fd>=0);
 func_002A2320(fd,data,100);assert(result()==100);
 func_002A2320(fd,data+100,sizeof(data)-100);assert(result()==sizeof(data)-100);close_card(fd);
 assert(access("savegame/game.sav",F_OK)!=0);
 f=fopen("savegame/zenkaipower.sav","rb");assert(f);
 assert(fread(raw,1,sizeof raw,f)==sizeof raw && fgetc(f)==EOF);fclose(f);
 assert(!memcmp(raw,"ZENKAI\0\3",8) && !memcmp(raw+88,data,sizeof data));memcpy(first,raw,sizeof raw);
 assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 assert(strlen(save_id)==32 && strlen(save_hash)==64);
 strcpy(old_id,save_id);strcpy(old_hash,save_hash);
 fd=open_card(1);assert(fd>=0);func_002A2208(fd,readback,sizeof readback);assert(result()==sizeof readback);
 assert(!memcmp(data,readback,sizeof data));close_card(fd);
 assert(Port_StoryEndingCount(readback,0,0)==3);
 assert(Port_StoryHasEnding(readback,0,0,10030) && Port_StoryHasEnding(readback,0,0,10032));
 assert(Port_StoryHasEnding(readback,0,0,10031) && Port_StoryHasEnding(readback,8,15,10050));
 /* Seek/update is transparent; identical data keeps the same binary file. */
 fd=open_card(2);assert(fd>=0);func_002A2078(fd,30,0);assert(result()==30);
 func_002A2320(fd,data+30,1);assert(result()==1);close_card(fd);
 f=fopen("savegame/zenkaipower.sav","rb");assert(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 assert(!memcmp(first,raw,sizeof raw));
 assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 assert(!strcmp(save_id,old_id) && !strcmp(save_hash,old_hash));
 /* A modified plaintext payload changes only its content hash. */
 fd=open_card(2);assert(fd>=0);func_002A2078(fd,30,0);assert(result()==30);
 data[30]++;func_002A2320(fd,data+30,1);assert(result()==1);close_card(fd);
 assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 assert(!strcmp(save_id,old_id) && strcmp(save_hash,old_hash));
 strcpy(old_hash,save_hash);
 /* A new game at the SAME PATH gets a new ID, even with identical content. */
 Port_ZenkaiNewGame();
 fd=open_card(0x202);assert(fd>=0);func_002A2320(fd,data,sizeof data);assert(result()==sizeof data);close_card(fd);
 assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 assert(strcmp(save_id,old_id) && !strcmp(save_hash,old_hash));
 strcpy(old_id,save_id);
 /* Loading an existing save cancels the provisional Save_Init reset marker. */
 Port_ZenkaiNewGame();fd=open_card(1);assert(fd>=0);close_card(fd);
 fd=open_card(2);assert(fd>=0);func_002A2078(fd,30,0);assert(result()==30);
 func_002A2320(fd,data+30,1);assert(result()==1);close_card(fd);
 assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 assert(!strcmp(save_id,old_id) && !strcmp(save_hash,old_hash));
 /* Switching the preference alone never reroutes the active game's saves. */
 setting=0;Port_SaveCardPath(path,sizeof path,"BASLUS-21678DBZT3/BASLUS-21678DBZT3");assert(strstr(path,"zenkaipower.sav"));
 Port_ZenkaiSelect();Port_ZenkaiFinish();assert(Port_ZenkaiEnabled());
 Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();assert(!Port_ZenkaiEnabled());
 Port_StoryRecordClear(data,0,1,2);assert(data[0x3e00+5]==0 && Port_StoryClearLevel(data,0,0)==0);
 memcpy(readback,data,sizeof data);
 assert(!Port_StoryRecordEnding(data,0,0,10033) && !memcmp(readback,data,sizeof data));
 fd=open_card(0x202);assert(fd>=0);func_002A2320(fd,"vanilla",7);assert(result()==7);close_card(fd);
 f=fopen("savegame/game.sav","rb");assert(fread(readback,1,7,f)==7 && !memcmp(readback,"vanilla",7));fclose(f);
 setting=1;Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
 fd=open_card(1);assert(fd>=0);close_card(fd);
 /* v1 envelopes are rejected and are not migrated. */
 {
  u8 legacy[48]={'Z','E','N','K','A','I',0,1};
  memset(legacy+8,0x5a,24);
  f=fopen("savegame/zenkaipower.sav","rb");assert(f);
  assert(fread(first,1,sizeof first,f)==sizeof first && fgetc(f)==EOF);fclose(f);
  memcpy(raw+48,data,sizeof data);
  crypto_aead_lock(raw+48,legacy+32,legacyKey,legacy+8,legacy,8,raw+48,sizeof data);
  memcpy(raw,legacy,sizeof legacy);
  f=fopen("savegame/zenkaipower.sav","wb");assert(f);
  assert(fwrite(raw,1,sizeof data+48,f)==sizeof data+48);fclose(f);
  assert(!Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
  assert(open_card(1)<0);
  f=fopen("savegame/zenkaipower.sav","wb");assert(f);
  assert(fwrite(first,1,sizeof first,f)==sizeof first);fclose(f);
 }
 /* Legacy v2 loads without modifying disk, then migrates on a successful write. */
 {
  u8 legacy[0x4000+96];
  f=fopen("savegame/zenkaipower.sav","rb");assert(fread(first,1,sizeof first,f)==sizeof first);fclose(f);
  memcpy(legacy,first,56);legacy[7]=2;memset(legacy+56,0x5a,24);
  crypto_aead_lock(legacy+96,legacy+80,legacyKey,legacy+56,legacy,56,data,sizeof data);
  f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(legacy,1,sizeof legacy,f)==sizeof legacy);fclose(f);
  assert(Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
  assert(!strcmp(save_id,old_id) && !strcmp(save_hash,old_hash));
  fd=open_card(1);assert(fd>=0);func_002A2208(fd,readback,sizeof readback);assert(result()==sizeof readback);close_card(fd);
  assert(!memcmp(data,readback,sizeof data));
  f=fopen("savegame/zenkaipower.sav","rb");assert(fread(legacy,1,8,f)==8 && legacy[7]==2);fclose(f);
  fd=open_card(2);assert(fd>=0);func_002A2320(fd,data,sizeof data);assert(result()==sizeof data);close_card(fd);
  f=fopen("savegame/zenkaipower.sav","rb");assert(fread(raw,1,sizeof raw,f)==sizeof raw && raw[7]==3);fclose(f);
  assert(!memcmp(first,raw,sizeof raw));
 }
 /* Corruption of binary data OR either checksummed metadata field is rejected. */
 f=fopen("savegame/zenkaipower.sav","rb");assert(fread(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 memcpy(first,raw,sizeof raw);
 /* A stale content digest is rejected. */
 raw[24]^=1;
 f=fopen("savegame/zenkaipower.sav","wb");assert(f);assert(fwrite(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 assert(open_card(1)<0 && !Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 memcpy(raw,first,sizeof raw);
 raw[10]^=1;f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 assert(open_card(1)<0 && !Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 raw[10]^=1;raw[26]^=1;f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 assert(open_card(1)<0 && !Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 raw[26]^=1;raw[100]^=1;f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(raw,1,sizeof raw,f)==sizeof raw);fclose(f);
 assert(open_card(1)<0 && !Port_ZenkaiMetadata("savegame/zenkaipower.sav",save_id,save_hash));
 /* Truncation and raw data without the versioned envelope are rejected. */
 f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(first,1,80,f)==80);fclose(f);assert(open_card(1)<0);
 f=fopen("savegame/zenkaipower.sav","wb");assert(fwrite(data,1,sizeof data,f)==sizeof data);fclose(f);assert(open_card(1)<0);
 setenv("ZENKAIPOWER_SAVES","session",1);assert(!Port_ZenkaiEnabled() && !Port_ZenkaiSelected());
 Port_SaveCardPath(path,sizeof path,"BASLUS-21678DBZT3/BASLUS-21678DBZT3");assert(strstr(path,"/game.sav"));
 puts("ZENKAI save checks passed: unencrypted binary v3, legacy migration, stable/new IDs, checksums, mode selection and isolation.");
 return 0;
}
