#!/usr/bin/env python3
"""Native Video navigation and its real host apply/save bridge, without a game build."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
header=(root/'include/menu/port_options_page.h').read_text()
a=header.index('static s32 Option_PortInput(')
b=header.index('/* Wrap by measured glyph width',a)
top=header[header.index('static s32 Option_PortSettingCount('):header.index('extern void Port_FlashOptionsPlate(')]
ui=(root/'port/src/gs/ui.cpp').read_text()
c=ui.index('static SDL_SpinLock sNativeVideoLock;')
d=ui.index('static void video_tab(',c)
source=r'''
#include <assert.h>
#include <string.h>
#include <map>
#include <string>
#include "menu/port_video_options.h"
#include "menu/port_options_save.h"
extern "C" int Port_ZenkaiEnabled(void);
typedef int s32;typedef float f32;
struct PortMenuMouse { float x,y;int moved,confirm,cancel,wheel; };
int Port_PageAspectMilli(void) { return 1778; }
int Port_TitleMainChars43(void) { return 1; }
int Port_Menus43(void) { return 0; }
static int hover;
void PortMenuMouse_SetHover(int value) { hover=value; }
static int footerHit;
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) { return footerHit; }
#define PADG_UP 8
#define PADG_DOWN 4
#define PADG_LEFT 1
#define PADG_RIGHT 2
#define PADG_CROSS 0x200
#define PADG_TRIANGLE 0x400
#define OPT_ST_TOP 0
#define OPT_ITEM_CTRL 4
static int sOptionPcGamepad,sOptionPcPlayer,sOptionPcPlayerMenu;
static int Option_PcInput(void) { return 0; }
static int Option_PcFooter(void) { return 0; }
static int pcBindings[PORT_PC_BINDINGS];
static int player2Pc[PORT_PC_BINDINGS],player2Pad[PORT_GAMEPAD_BINDINGS],physicalSlots=0x21;
extern "C" int PortInput_GamepadSlotsGet(void) {return physicalSlots;}
extern "C" void PortInput_GamepadSlotsSet(int value) {physicalSlots=value ? value:0x21;}
extern "C" void PortInput_PlayerDefaults(int player,int gamepad) { if(gamepad) memset(player2Pad,0,sizeof(player2Pad));else memset(player2Pc,0,sizeof(player2Pc)); }
extern "C" void PortInput_PlayerGet(int player,int *pc,int *pad) { memcpy(pc,player2Pc,sizeof(player2Pc));memcpy(pad,player2Pad,sizeof(player2Pad)); }
extern "C" void PortInput_PlayerSet(int player,const int *pc,const int *pad) { memcpy(player2Pc,pc,sizeof(player2Pc));memcpy(player2Pad,pad,sizeof(player2Pad)); }
extern "C" void PortInput_PcGet(int *out) { memcpy(out,pcBindings,sizeof(pcBindings)); }
static int gamepadBindings[PORT_GAMEPAD_BINDINGS];
extern "C" void PortInput_GamepadGet(int *out) { memcpy(out,gamepadBindings,sizeof(gamepadBindings)); }
extern "C" void PortInput_GamepadSet(const int *in) { memcpy(gamepadBindings,in,sizeof(gamepadBindings)); }
extern "C" void PortInput_PcSet(const int *in) { memcpy(pcBindings,in,sizeof(pcBindings)); }
#define OPT_CLIP_BOTTOM 3
#define OPT_CLIP_ROW 0
#define OPT_CLIP_PICK 4
typedef int SDL_SpinLock;
void SDL_LockSpinlock(int *) {}
void SDL_UnlockSpinlock(int *) {}
static int sMeter=1,gsInterp,settingsWrites,apiSetting;
extern "C" int Port_ClashSquare(void) { return 0; }
extern "C" void Port_ClashSquareApply(int) {}
extern "C" int Port_VirtualGamepad(void) { return 0; }
extern "C" void Port_VirtualGamepadApply(int) {}

bool meter_on(void) { return sMeter!=0; }
static std::map<std::string,int> prefs;
static int japaneseAvailable=1,wiiAvailable=1;
int MenuTranslation_EuropeanAvailable(void) { return 1; }
void Port_SettingText(const char *,const char *fallback,char *out,int size) { if(size>0) { strncpy(out,fallback,size-1);out[size-1]=0; } }

int PortSongs_SoundtrackAvailable(void) { return japaneseAvailable; }
extern "C" int Port_WiiArtAvailable(void) { return wiiAvailable; }
int Port_Setting(const char *key,int fallback) { return !strcmp(key,"gpu_api") ? apiSetting:prefs.count(key) ? prefs[key]:fallback; }
void Port_SettingSave(const char *key,int value) { if(!strcmp(key,"gpu_api")) apiSetting=value;else prefs[key]=value; }
void Port_SettingsWrite(void) { settingsWrites++; }
struct PortVideo { int scale,aspectMilli,other,smooth2d,fullscreen,vsync,frameSkip,fxOff,sharpBlur; } video;
static int applied;
void GsGpu_GetSettings(PortVideo *v) { *v=video; }
void GsGpu_SetSettings(const PortVideo *v) { video=*v;applied++; }
static unsigned char optionsDisk[PORT_OPTIONS_FILE_BYTES];
static int optionsFileState,optionsWriteOk=1,optionsWrites;
extern "C" int Port_OptionsFileRead(void *payload) {
    if(optionsFileState==1) memcpy(payload,optionsDisk,sizeof(optionsDisk));
    return optionsFileState;
}
extern "C" int Port_OptionsFileWrite(const void *payload) {
    if(!optionsWriteOk) return 0;
    memcpy(optionsDisk,payload,sizeof(optionsDisk));optionsFileState=1;optionsWrites++;return 1;
}
'''+ui[c:d]+r'''
struct { int gamePressed,gameRepeat; } gPad[1];
struct { int state,cursor,voiceLine,greeted,resetStep,value,picker,dirty,mcBusy,page,fromPage;void *flash; } option,*gOption=&option;
static int sOptionVideoOriginal,sOptionVideoEditing,sOptionVideoRow,sOptionVideoFirst,sOptionVideoPhase,sOptionVideoClosing,sOptionFooterSelected,sOptionPickerClosing,plates=1,msg=1,zenkai=1;
static unsigned char gameSave[0x4000];
static void *gSaveData=gameSave;
static int saveFlow=-1;
void McFlow_Start(int mode) { saveFlow=mode; }
static int pageChanges;
void Flash_GotoLabel(void *,const char *label,int restart) { pageChanges++; }
extern "C" int Port_ZenkaiEnabled(void) { return zenkai; }
int MenuTranslation_PortTextCopy(const char *id,const char *fallback,char *out,int capacity) {
  (void)id;if(capacity<1)return 0;strncpy(out,fallback,capacity-1);out[capacity-1]=0;return 1;
}
static int sOptionFromPage,sOptionTopFirst,sOptionSettingsCategory,sOptionReturningTop;
static float sOptionTopScroll;
static const int sOptionTopOrder[]={1,2,3,4,PORT_OPTION_MORE,5};
void Option_PortPlates(int visible) { plates=visible; }
static int msgOpens;
void MsgWin_Open(void) { msg=1;msgOpens++; }
void MsgWin_Close(void) { msg=0; }
static int lastSound=-1,soundCount;
void Snd_PlaySe(int a,int b) { lastSound=b;soundCount++; }
static const char *footerLabel="";
static int nativeFooter;
struct MFlashRef {};
struct MFlashUv { int x0,y0,x1,y1; };
static int exitCaptionUpdates;
void Option_SetBottomText(void *,MFlashRef *,const char *name,MFlashUv uv) {
    assert(!strcmp(name,"mc_bottom_plate_1") || !strcmp(name,"mc_bottom_plate_2"));
    assert(uv.x0==0 && uv.y0==0 && uv.x1==256 && uv.y1==32);
    exitCaptionUpdates++;
}
void Port_FlashOptionsVideoPicker(int row) {}
static int pickerHit;
int Port_SelectionMouseHit(PortMenuMouse *,int,const char *,int,int,int,int,int) { return pickerHit; }
void Port_FlashOptionsVideoFooter(int enabled) { nativeFooter=enabled; }
void Option_PlateGoto(int a,int b,const char *label) { if(b==OPT_CLIP_BOTTOM) footerLabel=label; }
static void Option_PortPrepareFooter(void);
static void Option_PortDefaults(void);
static int sOptionPackConfirm,packOn=1,packCanDisable=1;
int Port_ZenkaiPackEnabled(void) { return packOn; }
int Port_ZenkaiPackCanDisable(void) { return packCanDisable; }
int Port_ZenkaiPackDisable(void) { if(!packCanDisable) return 0;packCanDisable=0;packOn=0;return 1; }
static int Option_PortAvailable(int row) {
 return row==PORT_CLASSIC_OPTION_ROW ? packCanDisable:Port_OptionsAvailable(row);
}

static int resetAnswer;
void Option_UpdateReset(void) {
    if(resetAnswer>0) Option_PortDefaults();
    sOptionPackConfirm=0;
    option.state=PORT_OPTION_VIDEO_PAGE;
}
#define OPT_VOICE(n) ((void)0)
'''+top+r'''
void Ps_PageSet(void) {}
'''+header[header.index('static s32 Option_PortHit('):header.index('static void Option_PortPlates(')]+header[header.index('static void Option_PortVideoEnsure('):header.index('/* Return 1 when')]+header[a:b]+header[header.index('static const char *Option_PortHeading('):header.index('/* Use the original footer,')]+header[header.index('static void Option_PortPrepareFooter(void) {'):header.index('/* Same native blue/gold decoration')]+r'''
static void key(int repeat,int pressed) {
    gPad[0].gameRepeat=repeat;gPad[0].gamePressed=pressed;Option_PortInput();native_video_apply();
}
int main(void) {
    option.state=0;option.cursor=4;video.other=42;
    key(PADG_DOWN,0);assert(option.cursor==6);
    key(PADG_DOWN,0);assert(option.cursor==5);
    key(PADG_DOWN,0);assert(option.cursor==1);
    key(PADG_UP,0);assert(option.cursor==5);
    key(PADG_UP,0);assert(option.cursor==6);
    key(0,PADG_CROSS);assert(option.state==PORT_OPTION_MORE_PAGE && plates && msg && sOptionVideoPhase==-16 && pageChanges==1 && msgOpens==0);
    key(0,PADG_CROSS);assert(video.scale==0); /* opening animation blocks input */
    for(int i=0;i<15;i++) {
        Option_PortAdvance();assert(sOptionVideoPhase<0 && plates);
        key(PADG_RIGHT,PADG_CROSS);assert(video.scale==0);
    }
    Option_PortAdvance();assert(sOptionVideoPhase==0 && !plates);
    key(0,PADG_CROSS);assert(video.scale==0); /* submenu entry has not completed */
    for(int i=0;i<16;i++) Option_PortAdvance();
    Option_PortPrepareFooter();assert(nativeFooter==2 && !strcmp(Option_PortHeading(),"More Options"));
    { PortMenuMouse mouse={500,88,1,1,0,0};
      gPad[0].gamePressed=gPad[0].gameRepeat=0;
      Option_PortVideoMouse(&mouse);Option_PortInput(); }
    assert(option.state==PORT_OPTION_VIDEO_PAGE && sOptionVideoPhase==-16 && pageChanges==2);
    key(PADG_RIGHT,PADG_CROSS);assert(video.scale==0); /* transition blocks input */
    for(int i=0;i<32;i++) Option_PortAdvance();
    Option_PortPrepareFooter();assert(nativeFooter==1 && !strcmp(Option_PortHeading(),"Video Settings"));
    key(PADG_RIGHT,0);assert(video.scale==0 && !sOptionVideoEditing);
    key(0,PADG_CROSS);assert(video.scale==0 && video.other==42 && sOptionVideoEditing && Option_PortVideoFocus(0)==2);
    key(PADG_RIGHT,0);assert(video.scale==1);
    for(int i=0;i<7;i++) key(PADG_RIGHT,0);
    assert(video.scale==8);
    key(PADG_RIGHT,0);assert(video.scale==0);
    key(PADG_LEFT,0);assert(video.scale==8);
    key(0,PADG_TRIANGLE);assert(!sOptionVideoEditing && !sOptionVideoClosing && video.scale==0);
    key(PADG_LEFT,0);assert(video.scale==0);
    key(PADG_DOWN,0);assert(sOptionVideoRow==1);
    key(PADG_RIGHT,0);assert(video.aspectMilli==0);
    key(0,PADG_CROSS);assert(sOptionVideoEditing && video.aspectMilli==0);
    for(int i=1;i<sPortVideoOptions[1].count;i++) {
        key(PADG_RIGHT,0);assert(video.aspectMilli==sPortVideoRatios[i]);
    }
    key(PADG_RIGHT,0);assert(video.aspectMilli==0);
    key(0,PADG_CROSS);assert(!sOptionVideoEditing && Option_PortVideoFocus(1)==1);
    assert(applied>0 && video.other==42);
    for(int row=2;row<PORT_VIDEO_OPTION_COUNT;row++) {
        int old=Port_OptionsVideoGet(row);
        Port_OptionsVideoChange(row,1);native_video_apply();
        assert(Port_OptionsVideoGet(row)==(old+1)%sPortVideoOptions[row].count);
        assert(video.other==42);
    }
    assert(video.smooth2d==1 && video.fullscreen==1 && video.vsync==1 && sMeter==1 &&
           video.frameSkip==1 && apiSetting==1 && gsInterp==1);
    assert(settingsWrites>0);
    /* Every binary setting opens the native selector; selection is provisional. */
    for(int row=0;row<PORT_VIDEO_OPTION_COUNT;row++) if(sPortVideoOptions[row].count==2) {
        option.state=PORT_OPTION_VIDEO_PAGE;sOptionVideoRow=row;
        int old=Port_OptionsVideoGet(row);
        key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PICKER && option.value==old && option.picker==3 && Option_PortVideoFocus(row)==2);
        key(PADG_LEFT,0);assert(option.value==1-old && Port_OptionsVideoGet(row)==old);
        key(0,PADG_TRIANGLE);assert(sOptionPickerClosing && option.state==PORT_OPTION_VIDEO_PAGE && Port_OptionsVideoGet(row)==old && Option_PortVideoFocus(row)==1);
        key(0,0);assert(option.state==PORT_OPTION_VIDEO_PAGE);
        key(0,PADG_CROSS);
        { PortMenuMouse mouse={};mouse.moved=1;pickerHit=old ? 1:2;
          gPad[0].gamePressed=gPad[0].gameRepeat=0;
          Option_PortPickerMouse(&mouse);assert(option.value==1-old && Port_OptionsVideoGet(row)==old);
          mouse.confirm=1;Option_PortPickerMouse(&mouse);Option_PortInput();native_video_apply(); }
        assert(Port_OptionsVideoGet(row)==1-old && sOptionPickerClosing && option.state==PORT_OPTION_VIDEO_PAGE && Option_PortVideoFocus(row)==1);
        key(0,0);assert(option.state==PORT_OPTION_VIDEO_PAGE);
    }
    pickerHit=0;sOptionVideoFirst=0;sOptionVideoRow=0;
    /* Wheel scroll and native chevrons use the same four-row viewport. */
    { PortMenuMouse mouse={500,88,1,0,0,0};gPad[0].gamePressed=gPad[0].gameRepeat=0;
      Option_PortVideoMouse(&mouse);assert(sOptionVideoRow==0);
      mouse.wheel=1;Option_PortVideoMouse(&mouse);Option_PortInput();assert(sOptionVideoRow==1);
      mouse.wheel=0;mouse.confirm=1;mouse.x=512+(272-512)*(1333.333f/1778);mouse.y=252;
      gPad[0].gamePressed=gPad[0].gameRepeat=0;Option_PortVideoMouse(&mouse);Option_PortInput();
      assert(sOptionVideoRow==4 && sOptionVideoFirst==1);
      mouse.y=64;gPad[0].gamePressed=gPad[0].gameRepeat=0;
      Option_PortVideoMouse(&mouse);Option_PortInput();assert(sOptionVideoRow==0 && sOptionVideoFirst==0);
    }
    /* A first click anywhere on an inline row opens editing without changing
       it. Hidden arrows cannot adjust values; hover cannot steal active focus. */
    for(int row : {0,1,7}) {
        sOptionVideoRow=row;sOptionVideoFirst=row<4 ? 0:4;
        int old=Port_OptionsVideoGet(row),slot=row-sOptionVideoFirst;
        float narrow=1333.333f/1778;
        PortMenuMouse mouse={512+(360-512)*narrow,88+44.f*slot,1,1,0,0};
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();native_video_apply();
        assert(sOptionVideoEditing && Option_PortVideoEditing(row) && !Option_PortVideoEditing((row+1)%8));
        assert(Port_OptionsVideoGet(row)==old);
        int first=sOptionVideoFirst;
        key(PADG_UP,0);key(PADG_DOWN,0);
        assert(sOptionVideoEditing && sOptionVideoRow==row && sOptionVideoFirst==first);
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();native_video_apply();
        assert(Port_OptionsVideoGet(row)==(old+sPortVideoOptions[row].count-1)%sPortVideoOptions[row].count);
        mouse.x=512+(480-512)*narrow;
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();native_video_apply();
        assert(sOptionVideoEditing && Port_OptionsVideoGet(row)==old);
        mouse.confirm=0;mouse.y=88+44.f*((slot+1)%4);
        gPad[0].gamePressed=gPad[0].gameRepeat=0;Option_PortVideoMouse(&mouse);
        assert(sOptionVideoRow==row && sOptionVideoEditing);
        mouse.confirm=1;gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();
        assert(sOptionVideoRow==row && sOptionVideoEditing && sOptionVideoFirst==first);
        mouse.x=512+(272-512)*narrow;mouse.y=252;
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();
        assert(sOptionVideoRow==row && sOptionVideoEditing && sOptionVideoFirst==first);
        footerHit=1;mouse.x=500;mouse.y=300;
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        Option_PortVideoMouse(&mouse);Option_PortInput();footerHit=0;
        assert(sOptionVideoRow==row && sOptionVideoEditing && option.state==PORT_OPTION_VIDEO_PAGE);
        key(PADG_RIGHT,0);assert(Port_OptionsVideoGet(row)==(old+1)%sPortVideoOptions[row].count);
        mouse.cancel=1;Option_PortVideoMouse(&mouse);Option_PortInput();native_video_apply();
        assert(Port_OptionsVideoGet(row)==old);
        assert(!sOptionVideoEditing && !sOptionVideoClosing && !Option_PortVideoEditing(row));
        key(PADG_RIGHT,0);assert(Port_OptionsVideoGet(row)==old);
        key(0,PADG_CROSS);key(PADG_LEFT,0);
        int confirmed=(old+sPortVideoOptions[row].count-1)%sPortVideoOptions[row].count;
        key(0,PADG_CROSS);assert(!sOptionVideoEditing && Port_OptionsVideoGet(row)==confirmed && lastSound==1);
        key(0,PADG_CROSS);key(PADG_RIGHT,0);key(0,PADG_TRIANGLE);
        assert(!sOptionVideoEditing && Port_OptionsVideoGet(row)==confirmed && lastSound==2);
    }
    sOptionVideoRow=1;sOptionVideoFirst=0;

    for(int i=1;i<PORT_VIDEO_OPTION_COUNT;i++) key(PADG_DOWN,0);
    assert(sOptionVideoRow==PORT_VIDEO_OPTION_COUNT);Option_PortPrepareFooter();
    assert(nativeFooter && !strcmp(footerLabel,"fl_on_start"));
    key(0,PADG_CROSS);Option_PortPrepareFooter();
    assert(!strcmp(footerLabel,"fl_ok"));
    assert(option.state==PORT_OPTION_VIDEO_RESET && !sOptionVideoClosing);
    int unchanged=video.scale;resetAnswer=0;key(0,0);assert(video.scale==unchanged && option.state==23);
    Option_PortPrepareFooter();assert(!strcmp(footerLabel,"fl_on_start"));
    key(0,PADG_CROSS);resetAnswer=1;key(0,0);
    assert(video.scale==0 && video.aspectMilli==0 && video.other==42 && option.state==23);
    assert(video.smooth2d==1 && video.fullscreen==1 && video.vsync==0 && sMeter==1 &&
           video.frameSkip==0 && apiSetting==0 && gsInterp==0);
    int changesBeforeClose=pageChanges;key(0,PADG_TRIANGLE);
    assert(sOptionVideoClosing && pageChanges==changesBeforeClose+1);
    for(int i=0;i<16;i++) Option_PortAdvance();
    assert(option.state==PORT_OPTION_MORE_PAGE && sOptionVideoRow==0 && msg && msgOpens==0);
    assert(nativeFooter==2); /* Updated during Advance, before another draw setup. */
    assert(sOptionVideoPhase==0 && !sOptionVideoClosing);
    for(int i=0;i<16;i++) Option_PortAdvance();
    assert(sOptionVideoPhase==16);
    Option_PortPrepareFooter();assert(nativeFooter==2 && exitCaptionUpdates>0);
    { PortMenuMouse mouse={500,300,1,1,0,0};footerHit=1;
      gPad[0].gamePressed=gPad[0].gameRepeat=0;
      Option_PortVideoMouse(&mouse);Option_PortInput();footerHit=0; }
    assert(sOptionVideoRow==4 && sOptionVideoClosing && option.state==PORT_OPTION_MORE_PAGE && lastSound==1);
    assert(option.page==0 && option.fromPage==1);
    int returnTimelineStarts=pageChanges;
    for(int i=0;i<16;i++) Option_PortAdvance();
    assert(option.state==0 && option.cursor==6 && plates && msg && msgOpens==0);
    assert(sOptionReturningTop && pageChanges==returnTimelineStarts);
    { int before=exitCaptionUpdates;Option_PortPrepareFooter();
      assert(nativeFooter==2 && exitCaptionUpdates==before+2); }
    sOptionReturningTop=0;
    /* More Options -> Battle Effects uses the same native page and picker lifecycle. */
    option.state=PORT_OPTION_MORE_PAGE;sOptionVideoPhase=16;sOptionVideoRow=0;
    key(PADG_DOWN,0);assert(sOptionVideoRow==1);
    { PortMenuMouse mouse={512+(72-512)*(1333.333f/1778)+1,132,1,1,0,0};gPad[0].gamePressed=gPad[0].gameRepeat=0;
      Option_PortVideoMouse(&mouse);Option_PortInput(); }
    assert(option.state==PORT_OPTION_VIDEO_PAGE && sOptionSettingsCategory==1);
    for(int i=0;i<32;i++) Option_PortAdvance();
    assert(!strcmp(Option_PortHeading(),"Battle Effects"));
    video.scale=3;video.aspectMilli=1778;video.fxOff=128|2|16;video.sharpBlur=1;native_video_apply();
    for(int row=0;row<PORT_BATTLE_EFFECT_COUNT;row++) {
        sOptionVideoRow=row;Option_PortVideoEnsure();
        int old=Option_PortSettingGet(row),mask=video.fxOff,sharp=video.sharpBlur;
        key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PICKER && option.picker==3);
        key(PADG_RIGHT,0);key(0,PADG_TRIANGLE);key(0,0);
        assert(video.fxOff==mask && video.sharpBlur==sharp);
        key(0,PADG_CROSS);key(PADG_RIGHT,0);key(0,PADG_CROSS);
        assert(Option_PortSettingGet(row)==1-old);
        int bit=sPortBattleEffectBits[row];
        if(bit) assert(video.fxOff==(mask^bit) && video.sharpBlur==sharp);
        else assert(video.fxOff==mask && video.sharpBlur==1-sharp);
        assert((video.fxOff&128) && video.scale==3 && video.aspectMilli==1778 && video.other==42 && sMeter==1);
        key(0,0);
    }
    int effectMask=video.fxOff,effectSharp=video.sharpBlur;
    sOptionVideoRow=PORT_BATTLE_EFFECT_COUNT;key(0,PADG_CROSS);resetAnswer=0;key(0,0);
    assert(video.fxOff==effectMask && video.sharpBlur==effectSharp);
    key(0,PADG_CROSS);resetAnswer=1;key(0,0);
    assert(video.fxOff==128 && video.sharpBlur==1 && video.scale==3 && video.aspectMilli==1778 && sMeter==1);
    /* Video defaults must also leave all effect fields untouched. */
    video.fxOff=128|1|64;video.sharpBlur=0;native_video_apply();
    Port_OptionsVideoDefaults();native_video_apply();
    assert(video.fxOff==(128|1|64) && video.sharpBlur==0 && sMeter==1);
    key(0,PADG_TRIANGLE);for(int i=0;i<16;i++) Option_PortAdvance();
    assert(option.state==PORT_OPTION_MORE_PAGE && sOptionVideoRow==1);
    for(int i=0;i<16;i++) Option_PortAdvance();
    key(0,PADG_TRIANGLE);
    for(int i=0;i<16;i++) Option_PortAdvance();
    assert(option.state==OPT_ST_TOP && option.cursor==PORT_OPTION_MORE);
    sOptionSettingsCategory=0;
    video.aspectMilli=1777;native_video_apply();assert(Port_OptionsVideoGet(1)==2);
    /* Four visible rows; chevrons reveal More Options without moving EXIT. */
    {
        PortMenuMouse mouse={};
        option.state=0;option.cursor=1;sOptionTopFirst=0;sOptionTopScroll=0;
        mouse.moved=1;mouse.x=500;
        for(int i=0;i<4;i++) {
            mouse.y=72+i*44+16;assert(Option_PortTopMouse(&mouse)==i+1);
        }
        mouse.x=512+(272-512)*(1333.333f/1778);mouse.y=252;mouse.confirm=1;
        gPad[0].gamePressed=gPad[0].gameRepeat=0;
        assert(Option_PortTopMouse(&mouse)==-1 && (gPad[0].gameRepeat&PADG_DOWN));
        Option_PortInput();assert(sOptionTopFirst==1 && option.cursor==PORT_OPTION_MORE);
        sOptionTopScroll=0;gPad[0].gameRepeat=0;
        Option_PortTopMouse(&mouse);assert(!gPad[0].gameRepeat); /* no more entries below */
        mouse.confirm=0;mouse.x=500;mouse.y=220;assert(Option_PortTopMouse(&mouse)==PORT_OPTION_MORE);
        footerHit=1;mouse.y=300;assert(Option_PortTopMouse(&mouse)==5);footerHit=0;
        mouse.x=512+(272-512)*(1333.333f/1778);mouse.y=64;mouse.confirm=1;
        Option_PortTopMouse(&mouse);Option_PortInput();assert(sOptionTopFirst==0 && option.cursor==1);
    }
    /* EXIT and every category's Default play selection only on focus change. */
    { int state=option.state,row=sOptionVideoRow,category=sOptionSettingsCategory,phase=sOptionVideoPhase;
      PortMenuMouse mouse={500,300,1,0,0,0};footerHit=1;sOptionVideoPhase=16;sOptionVideoClosing=0;
      for(int page=-1;page<PORT_OPTION_CATEGORY_COUNT;page++) {
        option.state=page<0 ? PORT_OPTION_MORE_PAGE:PORT_OPTION_VIDEO_PAGE;
        sOptionSettingsCategory=page<0 ? 0:page;sOptionVideoRow=0;
        int sounds=soundCount;Option_PortVideoMouse(&mouse);
        assert(lastSound==0 && soundCount==sounds+1);
        Option_PortVideoMouse(&mouse);assert(soundCount==sounds+1);
      }
      footerHit=0;option.state=state;sOptionVideoRow=row;sOptionSettingsCategory=category;sOptionVideoPhase=phase;
    }
    /* Current category order, resource gates, and save-owned Classic Mode. */
    option.state=PORT_OPTION_MORE_PAGE;sOptionVideoPhase=16;sOptionVideoClosing=0;sOptionVideoRow=2;
    key(0,PADG_CROSS);assert(sOptionSettingsCategory==2 && Option_PortSettingCount()==3);
    for(int i=0;i<32;i++) Option_PortAdvance();
    assert(!strcmp(Option_PortHeading(),"Visual Settings"));
    Port_OptionsVideoChange(17,1);native_video_apply();assert(prefs["skip_intro"]==1);
    Port_OptionsCategoryDefaults(2);native_video_apply();assert(prefs["skip_intro"]==0);
    option.state=PORT_OPTION_MORE_PAGE;sOptionVideoPhase=16;sOptionVideoRow=3;
    wiiAvailable=0;native_video_apply();key(0,PADG_CROSS);
    for(int i=0;i<32;i++) Option_PortAdvance();
    assert(sOptionSettingsCategory==3 && Option_PortSettingCount()==4);
    assert(Option_PortSettingRow(0)==21 && Option_PortSettingRow(1)==19 &&
           Option_PortSettingRow(2)==20 && Option_PortSettingRow(3)==PORT_CLASSIC_OPTION_ROW);
    japaneseAvailable=0;native_video_apply();sOptionVideoRow=1;
    key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PAGE);
    Port_OptionsVideoChange(19,1);native_video_apply();assert(!prefs.count("game_version"));
    japaneseAvailable=1;native_video_apply();Port_OptionsVideoChange(19,1);native_video_apply();assert(prefs["game_version"]==1);
    sOptionVideoRow=2;key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PAGE);
    wiiAvailable=1;native_video_apply();prefs["dragon_net_mode"]=0;native_video_apply();assert(prefs["dragon_net_mode"]==1);
    sOptionVideoRow=3;assert(!strcmp(Option_PortDescriptor(3)->label,"Classic Mode"));
    assert(Option_PortSettingGet(3)==0 && Option_PortAvailable(PORT_CLASSIC_OPTION_ROW));
    key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PICKER);
    key(PADG_LEFT,0);assert(option.value==1 && packOn);
    key(0,PADG_TRIANGLE);key(0,0);assert(packOn && !sOptionPackConfirm);
    key(0,PADG_CROSS);key(PADG_LEFT,0);key(0,PADG_CROSS);
    assert(sOptionPackConfirm && packOn && option.state==PORT_OPTION_VIDEO_PAGE);
    key(0,0);assert(option.state==PORT_OPTION_VIDEO_RESET && packOn);
    resetAnswer=-1;key(0,0);assert(packOn && !sOptionPackConfirm);
    key(0,PADG_CROSS);key(PADG_LEFT,0);key(0,PADG_CROSS);key(0,0);
    resetAnswer=1;key(0,0);assert(!packOn && !packCanDisable && option.dirty);
    assert(!Option_PortAvailable(PORT_CLASSIC_OPTION_ROW) && Option_PortSettingGet(3)==1);
    assert(Option_PortSettingCount()==4 && Option_PortSettingRow(3)==PORT_CLASSIC_OPTION_ROW);
    key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PAGE && !sOptionPackConfirm);
    Port_OptionsCategoryDefaults(3);native_video_apply();assert(!packOn);
    packOn=1;packCanDisable=0;
    assert(Option_PortSettingCount()==4 && Option_PortSettingGet(3)==0);
    sOptionVideoRow=2;key(PADG_DOWN,0);assert(sOptionVideoRow==3);
    key(0,PADG_CROSS);assert(option.state==PORT_OPTION_VIDEO_PAGE && packOn);
    key(PADG_DOWN,0);assert(sOptionVideoRow==4);
    key(PADG_DOWN,0);assert(sOptionVideoRow==0);
    packCanDisable=1;option.dirty=0;
    /* All Options live in the companion; Sync leaves the main save intact. */
    unsigned char save[0x4000]={0},original[0x4000];
    memset(save,0xA5,sizeof(save));memcpy(original,save,sizeof(save));
    memset(sOptionsSnapshot,0,sizeof(sOptionsSnapshot));
    assert(!Port_OptionsSaveSync(save) && !memcmp(save,original,sizeof(save)));
    assert(optionsWrites==0 && Port_OptionsSaveCommit() && optionsWrites==1);
    int savedValues[PORT_OPTION_SETTING_COUNT];assert(Port_OptionsFileDecode(optionsDisk,savedValues));
    Port_OptionsVideoChange(0,1);assert(!Port_OptionsSaveSync(save));native_video_apply();assert(Port_OptionsSaveSync(save));
    assert(!Port_OptionsSaveSync(save) && optionsWrites==1); /* staging is not a disk write */
    optionsWriteOk=0;assert(!Port_OptionsSaveCommit() && optionsWrites==1);
    optionsWriteOk=1;assert(Port_OptionsSaveCommit());
    assert(Port_OptionsFileDecode(optionsDisk,savedValues));
    { /* Legacy embedded choices migrate only if no companion exists. */
      unsigned char legacySave[0x4000];int legacyValues[PORT_OPTION_SETTING_COUNT];
      memcpy(legacySave,save,sizeof(save));Port_OptionsSaveEncode(legacySave,savedValues);
      legacySave[PORT_OPTIONS_SAVE_OFFSET+3]='1';
      for(int row=16;row<=18;row++) legacySave[PORT_OPTIONS_SAVE_OFFSET+8+row*2]^=1;
      assert(Port_OptionsSaveDecode(legacySave,legacyValues));
      assert(!memcmp(savedValues,legacyValues,sizeof(savedValues)));
      optionsFileState=0;Port_OptionsSaveLoad(legacySave);native_video_apply();
      Port_OptionsSaveSync(legacySave);assert(!Port_OptionsSaveDecode(legacySave,legacyValues));
      assert(Port_OptionsSaveCommit());
    }
    Port_OptionsVideoChange(0,1);native_video_apply();
    Port_OptionsSaveLoad(save);native_video_apply();assert(video.scale==savedValues[0]);
    optionsFileState=-1; /* corrupt companion is ignored, with no legacy fallback */
    int scaleBefore=video.scale;Port_OptionsSaveLoad(save);native_video_apply();assert(video.scale==scaleBefore);
    optionsFileState=1;Port_OptionsSaveLoad(save);native_video_apply();
    pcBindings[3]=0;pcBindings[PORT_PC_STORAGE_ACTIONS+3]=1;
    assert(Port_OptionsSaveSync(save) && Port_OptionsSaveCommit());
    memset(pcBindings,0,sizeof(pcBindings));Port_OptionsSaveLoad(save);native_video_apply();
    assert(pcBindings[3]==0 && pcBindings[PORT_PC_STORAGE_ACTIONS+3]==1);
    /* Native settings restore without replacing progress or unknown flag bits. */
    save[0x16A8]=7;assert(Port_OptionsSaveSync(save));assert(Port_OptionsSaveCommit());
    save[0x16A8]=2;save[0x1608]^=0x80;save[0x168C]=42;save[123]=99;
    Port_OptionsSaveLoad(save);native_video_apply();
    assert(save[0x16A8]==7 && save[0x1608]==(original[0x1608]^0x80) && save[0x168C]==42 && save[123]==99);
    /* Both Autosave policies keep changes dirty until the native Options exit. */
    assert(!Option_PortSaveChanges());
    int pageBefore=option.state;Port_OptionsVideoChange(0,1);
    assert(!Option_PortSaveChanges());native_video_apply();
    assert(!Option_PortSaveChanges() && saveFlow==-1 && option.dirty && !option.mcBusy);
    assert(option.state==pageBefore && !Option_PortSaveChanges());native_video_apply();
    option.mcBusy=0;option.dirty=0;saveFlow=-1;
    Port_OptionsVideoChange(21,1);native_video_apply();assert(prefs["save_quick"]==1);
    assert(!Option_PortSaveChanges() && option.dirty && !option.mcBusy && saveFlow==-1);
    Port_OptionsVideoChange(0,1);native_video_apply();
    assert(!Option_PortSaveChanges() && option.dirty && !option.mcBusy && saveFlow==-1);
    assert(option.state==pageBefore); /* returning through submenus does not prompt */
    Port_OptionsVideoChange(21,1);native_video_apply();assert(prefs["save_quick"]==0);
    assert(!Option_PortSaveChanges() && option.dirty && !option.mcBusy && saveFlow==-1);
    option.mcBusy=0;
    Port_OptionsSaveSync(save);int retained[PORT_OPTION_SETTING_COUNT];
    assert(Port_OptionsSaveCommit() && Port_OptionsFileDecode(optionsDisk,retained));
    wiiAvailable=0;native_video_apply();Port_OptionsSaveLoad(save);native_video_apply();
    assert(!Port_OptionsSaveSync(save)); /* missing Wii assets do not erase saved mode */
    zenkai=0;memcpy(save,original,sizeof(save));
    assert(!Port_OptionsSaveSync(save) && !memcmp(save,original,sizeof(save)));
    zenkai=0;option.state=0;option.cursor=4;
    gPad[0].gameRepeat=PADG_DOWN;gPad[0].gamePressed=0;
    assert(!Option_PortInput() && option.cursor==4 && Option_PortHeading()==NULL); /* Original heading. */
    option.cursor=6;assert(!Option_PortInput() && option.cursor==4);
    option.state=PORT_OPTION_MORE_PAGE;option.cursor=6;plates=msg=0;
    int savedScale=video.scale;
    key(PADG_RIGHT,0);
    assert(option.state==0 && option.cursor==4 && plates && msg && video.scale==savedScale);
    zenkai=1;
    int before=video.scale;
    Port_OptionsVideoChange(0,1);assert(video.scale==before); /* queued, not applied on game thread */
    int queued=Port_OptionsVideoGet(0);native_video_apply();assert(video.scale==sPortVideoScales[queued]);
    int count=applied;Port_OptionsVideoChange(PORT_OPTION_SETTING_COUNT,1);assert(applied==count);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'video.cpp';src.write_text(source);exe=Path(tmp)/'video'
    subprocess.run(['c++','-std=c++17','-fsanitize=undefined','-I'+str(root/'include'),str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Native Video navigation and settings bridge checks passed')
