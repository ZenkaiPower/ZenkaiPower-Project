#ifndef MENU_PORT_OPTIONS_SAVE_H
#define MENU_PORT_OPTIONS_SAVE_H
#include <string.h>
#include "port_video_options.h"
#include "port_pc_bindings.h"
/* ZENKAI Options companion; the old embedded offset is migration-only. */
#define PORT_OPTIONS_SAVE_OFFSET 0x3F00
#define PORT_OPTIONS_SAVE_BYTES (8+2*PORT_OPTION_SETTING_COUNT)
static int Port_OptionsPayloadDecode(const void *data,int *values) {
    const unsigned char *p=(const unsigned char *)data;
    int row,i;
    if(p[0]!='Z' || p[1]!='O' || p[2]!='P' || (p[3]!='1' && p[3]!='2') ||
       p[4]!=PORT_OPTION_SETTING_COUNT || p[5] || p[6] || p[7]) return 0;
    for(row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        const PortVideoOption *option=Port_OptionsDescriptor(row);
        int value=p[8+row*2] | (p[9+row*2]<<8),valid=0;
        for(i=0;i<option->count;i++) if(option->values[i]==value) valid=1;
        if(!valid) return 0;
        /* ZOP1 stored skip toggles; ZOP2 stores positive show/play toggles. */
        if(p[3]=='1' && row>=16 && row<=18) value=!value;
        values[row]=value;
    }
    return 1;
}
static void Port_OptionsPayloadEncode(void *data,const int *values) {
    unsigned char *p=(unsigned char *)data;
    int row;
    p[0]='Z';p[1]='O';p[2]='P';p[3]='2';p[4]=PORT_OPTION_SETTING_COUNT;p[5]=p[6]=p[7]=0;
    for(row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        p[8+row*2]=(unsigned char)values[row];
        p[9+row*2]=(unsigned char)(values[row]>>8);
    }
}
/* Legacy embedded records are read only for migration. */
static int Port_OptionsSaveDecode(const void *save,int *values) {
    return Port_OptionsPayloadDecode((const unsigned char *)save+PORT_OPTIONS_SAVE_OFFSET,values);
}
static void Port_OptionsSaveEncode(void *save,const int *values) {
    Port_OptionsPayloadEncode((unsigned char *)save+PORT_OPTIONS_SAVE_OFFSET,values);
}
/* Native Options: flags/key maps, followed by camera, display and audio.
   Leave the unrelated words at 0x168C and unknown flag bits with gameplay. */
#define PORT_OPTIONS_NATIVE_BYTES 160
#define PORT_OPTIONS_FILE_V1_BYTES (8+PORT_OPTIONS_SAVE_BYTES+PORT_OPTIONS_NATIVE_BYTES)
#define PORT_OPTIONS_FILE_V3_BYTES (PORT_OPTIONS_FILE_V1_BYTES+4*PORT_PC_BINDINGS)
/* v7 appends a language; v8 appends the online player name after it.
   Preserve every pre-existing field offset for old encrypted .options files. */
#define PORT_OPTIONS_FILE_V6_BYTES (PORT_OPTIONS_FILE_V3_BYTES+4*PORT_GAMEPAD_BINDINGS)
#define PORT_OPTIONS_LOCALE_BYTES 16
#define PORT_OPTIONS_FILE_V7_BYTES (PORT_OPTIONS_FILE_V6_BYTES+PORT_OPTIONS_LOCALE_BYTES)
#define PORT_OPTIONS_PLAYER_NAME_BYTES 20
#define PORT_OPTIONS_FILE_BYTES (PORT_OPTIONS_FILE_V7_BYTES+PORT_OPTIONS_PLAYER_NAME_BYTES)

/* The same locale syntax used by the translation catalog filenames: ll[-RRR]. */
static int Port_OptionsLocaleValid(const char *locale) {
    int i=0,start;
    if(!locale) return 0;
    while(i<PORT_OPTIONS_LOCALE_BYTES && locale[i]>='a' && locale[i]<='z') i++;
    if(i<2 || i>3) return 0;
    if(!locale[i]) return 1;
    if(locale[i++]!='-') return 0;
    start=i;
    while(i<PORT_OPTIONS_LOCALE_BYTES &&
          ((locale[i]>='A' && locale[i]<='Z') || (locale[i]>='a' && locale[i]<='z'))) i++;
    return i-start>=2 && i-start<=3 && i<PORT_OPTIONS_LOCALE_BYTES && !locale[i];
}
/* Versions 7 and 8 header byte 7: bit 0 = metadata-only (no native
   options), bit 2 = explicit startup prompt preference, bit 3 = its On value.
   Older files use byte 7 = 2 for an appended language, or 3 for an
   appended language and player name, keeping their original bindings. */
#define PORT_OPTIONS_PROMPT_SET 4
#define PORT_OPTIONS_PROMPT_ON 8
static int Port_OptionsFileLanguageOnly(const void *data) {
    const unsigned char *p=(const unsigned char *)data;
    return (p[4]==7 || p[4]==8) && (p[7]&1)!=0;
}
static int Port_OptionsFileGetLanguage(const void *data,char *locale,int size) {
    const unsigned char *p=(const unsigned char *)data;
    const char *saved=(const char *)p+PORT_OPTIONS_FILE_V6_BYTES;
    int i;
    if(!locale || size<=0 || !(p[4]==7 || p[4]==8 || (p[4]>=1 && p[4]<=6 && (p[7]==2 || p[7]==3))) ||
       !Port_OptionsLocaleValid(saved)) return 0;
    for(i=0;i<size-1 && saved[i];i++) locale[i]=saved[i];
    locale[i]=0;
    return 1;
}
static int Port_OptionsFileSetLanguage(void *data,const char *locale) {
    unsigned char *p=(unsigned char *)data+PORT_OPTIONS_FILE_V6_BYTES;
    int i;
    if(!Port_OptionsLocaleValid(locale)) return 0;
    for(i=0;i<PORT_OPTIONS_LOCALE_BYTES;i++) p[i]=0;
    for(i=0;locale[i];i++) p[i]=(unsigned char)locale[i];
    return 1;
}
/* The name lives only inside the encrypted ZENKAI .options companion.
   Missing/empty names mean that the normal "Player" default applies. */
static int Port_OptionsFileGetName(const void *data,char *name,int size) {
    const unsigned char *p=(const unsigned char *)data;
    const unsigned char *saved=p+PORT_OPTIONS_FILE_V7_BYTES;
    int i;
    if(!name || size<=0 || !(p[4]==8 || (p[4]>=1 && p[4]<=6 && p[7]==3))) return 0;
    for(i=0;i<PORT_OPTIONS_PLAYER_NAME_BYTES;i++) {
        if(!saved[i]) break;
        if(saved[i]<32 || saved[i]==127) return 0;
    }
    if(i==0 || i==PORT_OPTIONS_PLAYER_NAME_BYTES) return 0;
    for(;i<PORT_OPTIONS_PLAYER_NAME_BYTES;i++) if(saved[i]) return 0;
    i=0;
    while(i<size-1 && saved[i]) { name[i]=(char)saved[i]; i++; }
    name[i]=0;
    return 1;
}
static int Port_OptionsFileSetName(void *data,const char *name) {
    unsigned char *saved=(unsigned char *)data+PORT_OPTIONS_FILE_V7_BYTES;
    size_t i,n;
    if(!name) return 0;
    n=strlen(name);
    if(n>=PORT_OPTIONS_PLAYER_NAME_BYTES) return 0;
    for(i=0;i<n;i++) if((unsigned char)name[i]<32 || (unsigned char)name[i]==127) return 0;
    memset(saved,0,PORT_OPTIONS_PLAYER_NAME_BYTES);
    memcpy(saved,name,n);
    return 1;
}
static int Port_OptionsFileDecode(const void *data,int *values) {
    const unsigned char *p=(const unsigned char *)data;
    if(p[0]!='Z' || p[1]!='O' || p[2]!='P' || p[3]!='T' ||
       (p[4]<1 || p[4]>8) || p[5]!=PORT_OPTIONS_SAVE_BYTES || p[6]!=PORT_OPTIONS_NATIVE_BYTES ||
       (p[4]>=7 ? ((p[7]&~13)!=0 || ((p[7]&8)!=0 && (p[7]&4)==0)) :
                    (p[7]!=0 && p[7]!=2 && p[7]!=3))) return 0;
    /* A metadata-only companion may be written before the first game save.
       Do not interpret it as native Options, video settings or bindings. */
    if(Port_OptionsFileLanguageOnly(data)) {
        if(p[PORT_OPTIONS_FILE_V6_BYTES] &&
           !Port_OptionsLocaleValid((const char *)p+PORT_OPTIONS_FILE_V6_BYTES)) return 0;
        if(p[4]==7) return Port_OptionsLocaleValid((const char *)p+PORT_OPTIONS_FILE_V6_BYTES);
        /* A v8 metadata-only file may contain only a name, no language yet. */
        if(p[PORT_OPTIONS_FILE_V7_BYTES]) {
            char name[PORT_OPTIONS_PLAYER_NAME_BYTES];
            if(!Port_OptionsFileGetName(data,name,sizeof(name))) return 0;
        }
        return 1; /* An empty metadata-only record is harmless. */
    }
    if((p[4]>=7 || p[7]==2 || p[7]==3) &&
       (p[7]==2 || p[PORT_OPTIONS_FILE_V6_BYTES]) &&
       !Port_OptionsLocaleValid((const char *)p+PORT_OPTIONS_FILE_V6_BYTES)) return 0;
    if(p[4]==8 || p[7]==3) {
        if(p[PORT_OPTIONS_FILE_V7_BYTES]) {
            char name[PORT_OPTIONS_PLAYER_NAME_BYTES];
            if(!Port_OptionsFileGetName(data,name,sizeof(name))) return 0;
        }
    }
    if(p[4]>=2) {
        int i;
        for(i=0;i<PORT_PC_BINDINGS;i++) {
            const unsigned char *b=p+PORT_OPTIONS_FILE_V1_BYTES+4*i;
            unsigned value=b[0]|(b[1]<<8)|(b[2]<<16)|((unsigned)b[3]<<24);
            if(value>(i<PORT_PC_STORAGE_ACTIONS ? 511u:(p[4]>=6 ? 9u:5u))) return 0;
        }
    }
    if(p[4]>=4) {
        int i;for(i=0;i<PORT_GAMEPAD_BINDINGS;i++) {
            const unsigned char *b=p+PORT_OPTIONS_FILE_V3_BYTES+4*i;
            unsigned v=b[0]|(b[1]<<8)|(b[2]<<16)|((unsigned)b[3]<<24);
            if(p[4]>=5 ? !Port_GamepadBindingValid(v):(v>26 && (v<100 || v>111))) return 0;
        }
    }
    return Port_OptionsPayloadDecode(p+8,values);
}
static void Port_OptionsFileEncode(void *data,const void *save,const int *values) {
    unsigned char *p=(unsigned char *)data;
    const unsigned char *s=(const unsigned char *)save;
    int i;
    for(i=PORT_OPTIONS_FILE_V1_BYTES;i<PORT_OPTIONS_FILE_BYTES;i++) p[i]=0;
    p[0]='Z';p[1]='O';p[2]='P';p[3]='T';p[4]=8;
    p[5]=PORT_OPTIONS_SAVE_BYTES;p[6]=PORT_OPTIONS_NATIVE_BYTES;p[7]=0;
    Port_OptionsPayloadEncode(p+8,values);
    p+=8+PORT_OPTIONS_SAVE_BYTES;
    for(i=0;i<132;i++) p[i]=s[0x1608+i];
    p[0]&=31;p[1]=p[2]=p[3]=0;
    for(i=0;i<28;i++) p[132+i]=s[0x1694+i];
}
/* Byte 1 of the native mirror is reserved by the original game. Use it for
   the Clash Input choice without moving any existing companion fields. */
static int Port_OptionsFileClashMode(const void *data) {
    const unsigned char *p=(const unsigned char *)data+8+PORT_OPTIONS_SAVE_BYTES;
    return p[1]==1;
}
static void Port_OptionsFileSetClashMode(void *data,int strike) {
    unsigned char *p=(unsigned char *)data+8+PORT_OPTIONS_SAVE_BYTES;
    p[1]=strike ? 1:0;
}
/* Reserved byte 2: zero preserves On for existing companions. */
static int Port_OptionsFileVirtualGamepad(const void *data) {
    const unsigned char *p=(const unsigned char *)data+8+PORT_OPTIONS_SAVE_BYTES;
    return p[2]!=1;
}
static void Port_OptionsFileSetVirtualGamepad(void *data,int enabled) {
    unsigned char *p=(unsigned char *)data+8+PORT_OPTIONS_SAVE_BYTES;
    p[2]=enabled ? 0:1;
}
static void Port_OptionsFileApplyNative(const void *data,void *save) {
    const unsigned char *p=(const unsigned char *)data+8+PORT_OPTIONS_SAVE_BYTES;
    unsigned char *s=(unsigned char *)save;
    int i;
    s[0x1608]=(s[0x1608]&~31)|(p[0]&31);
    for(i=4;i<132;i++) s[0x1608+i]=p[i];
    for(i=0;i<28;i++) s[0x1694+i]=p[132+i];
}
static void Port_OptionsFileEncodeBindings(void *data,const int *bindings) {
    unsigned char *p=(unsigned char *)data+PORT_OPTIONS_FILE_V1_BYTES;
    int i,j;
    for(i=0;i<PORT_PC_BINDINGS;i++) for(j=0;j<4;j++) p[4*i+j]=(unsigned)bindings[i]>>(8*j);
}
static int Port_OptionsFileDecodeBindings(const void *data,int *bindings) {
    const unsigned char *header=(const unsigned char *)data;
    const unsigned char *p=header+PORT_OPTIONS_FILE_V1_BYTES;
    int old[PORT_PC_BINDINGS],i;
    if(header[4]<2) return 0;
    for(i=0;i<PORT_PC_BINDINGS;i++) old[i]=p[4*i]|(p[4*i+1]<<8)|(p[4*i+2]<<16)|((unsigned)p[4*i+3]<<24);
    for(i=0;i<PORT_PC_BINDINGS;i++) bindings[i]=0;
    for(i=0;i<PORT_PC_ACTIONS;i++) {
        int source=header[4]==2 && i>=12 ? i+2:i;
        bindings[i]=old[source];
        bindings[PORT_PC_STORAGE_ACTIONS+i]=old[PORT_PC_STORAGE_ACTIONS+source];
        if(bindings[PORT_PC_STORAGE_ACTIONS+i]) bindings[i]=0;
    }
    return 1;
}
static void Port_OptionsFileEncodeGamepad(void *data,const int *bindings) {
    unsigned char *p=(unsigned char *)data+PORT_OPTIONS_FILE_V3_BYTES;int i,j;
    for(i=0;i<PORT_GAMEPAD_BINDINGS;i++) for(j=0;j<4;j++) p[4*i+j]=(unsigned)bindings[i]>>(8*j);
}
static int Port_OptionsFileDecodeGamepad(const void *data,int *bindings) {
    const unsigned char *p=(const unsigned char *)data;int i;
    if(p[4]<4) return 0;
    p+=PORT_OPTIONS_FILE_V3_BYTES;
    for(i=0;i<PORT_GAMEPAD_BINDINGS;i++) bindings[i]=p[4*i]|(p[4*i+1]<<8)|(p[4*i+2]<<16)|((unsigned)p[4*i+3]<<24);
    return 1;
}
#endif
