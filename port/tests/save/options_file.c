/* cc -std=gnu11 -fsanitize=address,undefined port/tests/save/options_file.c
 * port/src/plat_zenkai.c port/src/zenkai_crypto.c -o /tmp/options-file && /tmp/options-file */
#include "../../src/plat_mc.c"
#include "../../../include/menu/port_options_save.h"
#include <assert.h>

void Port_StateTouch(void *p,unsigned long n) { (void)p;(void)n; }
int Port_Setting(const char *name,int fallback) { (void)name;return fallback; }
int main(void) {
    char dir[]="/tmp/zenkai-options-file-XXXXXX";
    unsigned char save[0x4000]={0},payload[PORT_OPTIONS_FILE_BYTES],readback[PORT_OPTIONS_FILE_BYTES];
    int values[PORT_OPTION_SETTING_COUNT],decoded[PORT_OPTION_SETTING_COUNT];
    FILE *file;int bindings[PORT_PC_BINDINGS]={0},restored[PORT_PC_BINDINGS];
    int gamepad[PORT_GAMEPAD_BINDINGS]={109,10,4,3,111,11,2,1,102,103,100,101,104,105,5,PORT_GAMEPAD_CHORD|8|(9<<8),9},padRestored[PORT_GAMEPAD_BINDINGS];
    bindings[3]=13;bindings[PORT_PC_STORAGE_ACTIONS+1]=1;
    assert(mkdtemp(dir) && !chdir(dir));unsetenv("ZENKAIPOWER_SAVES");
    for(int i=0;i<PORT_OPTION_SETTING_COUNT;i++) values[i]=Port_OptionsDescriptor(i)->defaultValue;
    values[0]=3;values[1]=3556;save[0x16A8]=7;save[0x1608]=0x86;
    Port_OptionsFileEncode(payload,save,values);Port_OptionsFileEncodeBindings(payload,bindings);
    Port_OptionsFileEncodeGamepad(payload,gamepad);
    assert(!Port_OptionsFileWrite(payload)); /* Original mode never writes the companion. */
    Port_ZenkaiSelect();Port_ZenkaiCommit();Port_ZenkaiFinish();
    assert(Port_ZenkaiEnabled() && Port_OptionsFileRead(readback)==0);
    assert(Port_OptionsFileWrite(payload));
    assert(access("savegame/zenkaipower.sav",F_OK)!=0); /* no main-save IO */
    assert(Port_OptionsFileRead(readback)==1 && !memcmp(payload,readback,sizeof(payload)));
    assert(Port_OptionsFileDecode(readback,decoded) && !memcmp(values,decoded,sizeof(values)));
    assert(Port_OptionsFileDecodeBindings(readback,restored) && !memcmp(bindings,restored,sizeof(bindings)));
    assert(Port_OptionsFileDecodeGamepad(readback,padRestored) && !memcmp(gamepad,padRestored,sizeof(gamepad)));
    bindings[PORT_PC_STORAGE_ACTIONS+1]=6;Port_OptionsFileEncodeBindings(payload,bindings);
    assert(Port_OptionsFileWrite(payload) && Port_OptionsFileRead(readback)==1);
    assert(Port_OptionsFileDecodeBindings(readback,restored) && restored[PORT_PC_STORAGE_ACTIONS+1]==6);
    bindings[PORT_PC_STORAGE_ACTIONS+1]=1;Port_OptionsFileEncodeBindings(payload,bindings);
    /* The previous companion format loads without inventing bindings. */
    file=tmpfile();assert(file);payload[4]=1;
    assert(fwrite(payload,1,PORT_OPTIONS_FILE_V1_BYTES,file)==PORT_OPTIONS_FILE_V1_BYTES &&
        Port_ZenkaiFlush("savegame/zenkaipower.sav.options",file));fclose(file);
    assert(Port_OptionsFileRead(readback)==1 && !Port_OptionsFileDecodeBindings(readback,restored));
    /* Version 2: drop vertical camera/Pause and migrate dual bindings. */
    { int old[PORT_PC_BINDINGS]={0};
      old[12]=22;old[13]=23;old[14]=24;old[15]=25;old[16]=26;old[17]=27;old[18]=28;old[19]=29;
      old[3]=13;old[PORT_PC_STORAGE_ACTIONS+3]=1;
      payload[4]=2;Port_OptionsFileEncodeBindings(payload,old);
      assert(Port_OptionsFileDecodeBindings(payload,restored));
      assert(restored[12]==24 && restored[13]==25 && restored[14]==26 && restored[15]==27 && restored[16]==28);
      assert(!restored[3] && restored[PORT_PC_STORAGE_ACTIONS+3]==1 && !restored[17] && !restored[18] && !restored[19]);
      Port_OptionsFileEncodeBindings(payload,bindings);
    }
    payload[4]=3;
    file=tmpfile();assert(file);
    assert(fwrite(payload,1,PORT_OPTIONS_FILE_V3_BYTES,file)==PORT_OPTIONS_FILE_V3_BYTES &&
        Port_ZenkaiFlush("savegame/zenkaipower.sav.options",file));fclose(file);
    assert(Port_OptionsFileRead(readback)==1 && !Port_OptionsFileDecodeGamepad(readback,padRestored));
    /* Reading the historical payload above is supported; new publications use
       the current envelope rather than an obsolete writer version. */
    Port_OptionsFileEncode(payload,save,values);
    Port_OptionsFileEncodeBindings(payload,bindings);
    Port_OptionsFileEncodeGamepad(payload,gamepad);
    assert(Port_OptionsFileWrite(payload));
    save[0x16A8]=1;save[0x1608]=0x40;save[0x168C]=99;save[100]=42;
    Port_OptionsFileApplyNative(readback,save);
    assert(save[0x16A8]==7 && save[0x1608]==0x46 && save[0x168C]==99 && save[100]==42);
    /* Failed publication leaves the last complete companion intact. */
    assert(!mkdir("savegame/zenkaipower.sav.options.part",0700));
    assert(!Port_OptionsFileWrite(payload));
    assert(Port_OptionsFileRead(readback)==1 && !memcmp(payload,readback,sizeof(payload)));
    assert(!rmdir("savegame/zenkaipower.sav.options.part"));
    file=fopen("savegame/zenkaipower.sav.options","r+b");assert(file);
    assert(!fseek(file,50,SEEK_SET));int byte=fgetc(file);assert(byte!=EOF);
    assert(!fseek(file,50,SEEK_SET) && fputc(byte^1,file)!=EOF && !fclose(file));
    assert(Port_OptionsFileRead(readback)==-1); /* checksum fails */
    assert(Port_OptionsFileWrite(payload));
    payload[4]=255;assert(!Port_OptionsFileWrite(payload));
    setenv("ZENKAIPOWER_SAVES","session",1);
    assert(!Port_OptionsFileWrite(readback) && !Port_OptionsFileRead(readback));
    puts("ZENKAI Options companion checks passed");
    return 0;
}
