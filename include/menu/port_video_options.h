#ifndef MENU_PORT_VIDEO_OPTIONS_H
#define MENU_PORT_VIDEO_OPTIONS_H
/* Shared by the native Options page and the host settings bridge. */
#define PORT_OPTION_MORE 6
#define PORT_OPTION_MORE_PAGE 25
#define PORT_OPTION_VIDEO_PAGE 23
#define PORT_OPTION_VIDEO_RESET 24
#define PORT_OPTION_VIDEO_PICKER 26
#include "port_render_scale.h"
#ifdef __ANDROID__
static const int sPortVideoScales[] = {50,1,150,2,250,3};
static const char *const sPortVideoScaleNames[] = {"0.5x","1x","1.5x","2x","2.5x","3x"};
static const char *const sPortVideoScaleNameIds[] = {"options/value/0-5x","options/value/1x","options/value/1-5x","options/value/2x","options/value/2-5x","options/value/3x"};
#define PORT_RENDER_SCALE_DEFAULT 150
#else
static const int sPortVideoScales[] = {0,1,2,3,4,5,6,7,8};
static const char *const sPortVideoScaleNames[] = {"Auto","1x","2x","3x","4x","5x","6x","7x","8x"};
static const char *const sPortVideoScaleNameIds[] = {"options/value/auto","options/value/1x","options/value/2x","options/value/3x","options/value/4x","options/value/5x","options/value/6x","options/value/7x","options/value/8x"};
#define PORT_RENDER_SCALE_DEFAULT 0
#endif
static const int sPortVideoRatios[] = {0,1333,1778,1600,2389,3556};
static const char *const sPortVideoRatioNames[] = {"Auto","4:3","16:9","16:10","21:9","32:9"};
static const char *const sPortVideoRatioNameIds[] = {"options/value/auto","options/value/4-3","options/value/16-9","options/value/16-10","options/value/21-9","options/value/32-9"};
static const int sPortVideoToggle[]={0,1};
static const char *const sPortVideoToggleNames[]={"Off","On"};
static const char *const sPortVideoToggleNameIds[]={"options/value/off","options/value/on"};
static const char *const sPortVideoFilterNames[]={"Nearest","Bilinear"};
static const char *const sPortVideoFilterNameIds[]={"options/value/nearest","options/value/bilinear"};
static const char *const sPortVideoApiNames[]={"Vulkan","OpenGL"};
static const char *const sPortVideoApiNameIds[]={"options/value/vulkan","options/value/opengl"};
static const int sPortVideoFrames[]={0,1,2,3};
static const char *const sPortVideoFrameNames[]={"30","60","120","240"};
static const char *const sPortVideoFrameNameIds[]={"options/value/30","options/value/60","options/value/120","options/value/240"};
#define PORT_VIDEO_OPTION_COUNT 8
#define PORT_VIDEO_VISIBLE_ROWS 4
#define PORT_BATTLE_EFFECT_COUNT 8
#define PORT_OPTION_CATEGORY_COUNT 4
#define PORT_VISUAL_OPTION_OFFSET 16
#define PORT_NETWORK_OPTION_OFFSET 20
/* Persistent ZOP row, independent of General Settings display order. */
#define PORT_AUTOSAVE_OPTION_OFFSET 21
#define PORT_OPTION_SETTING_COUNT 22
static const char *const sPortOptionCategories[]={"Video Settings","Battle Effects","Visual Settings","General Settings"};
static const char *const sPortOptionCategoryIds[]={"options/category/video","options/category/battle-effects","options/category/visual","options/category/general"};
/* These offsets are also the persisted ZOP* row indices. Keep row 19
   (Game Edition) unchanged when moving it to General Settings. */
#ifdef __ANDROID__
static const int sPortOptionCounts[]={7,8,3,3};
#else
static const int sPortOptionCounts[]={8,8,3,3};
#endif
static const int sPortOptionOffsets[]={0,8,16,19};
/* Display rows can omit settings without changing their backing indices. */
static int Port_OptionsCategoryRow(int category,int row) {
#ifdef __ANDROID__
    if(category==0 && row>=6) return row+1; /* Graphics API is desktop-only. */
#endif
    if(category==3 && row>=0 && row<sPortOptionCounts[3]) {
        static const int generalRows[]={2,0,1};
        return sPortOptionOffsets[3]+generalRows[row];
    }
    return sPortOptionOffsets[category]+row;
}
typedef struct PortVideoOption {
    const char *label;
    const int *values;
    const char *const *names;
    int count;
    int defaultValue;
    const char *help;
    const char *labelId;
    const char *helpId;
    const char *const *nameIds;
} PortVideoOption;
static const PortVideoOption sPortVideoOptions[] = {
    {"Resolution",sPortVideoScales,sPortVideoScaleNames,sizeof(sPortVideoScales)/sizeof(sPortVideoScales[0]),PORT_RENDER_SCALE_DEFAULT,"Set the internal rendering resolution.","options/setting/resolution","options/help/resolution",sPortVideoScaleNameIds},
    {"Aspect Ratio",sPortVideoRatios,sPortVideoRatioNames,sizeof(sPortVideoRatios)/sizeof(sPortVideoRatios[0]),0,"Change the screen aspect ratio.","options/setting/aspect-ratio","options/help/aspect-ratio",sPortVideoRatioNameIds},
    {"2D Filtering",sPortVideoToggle,sPortVideoFilterNames,2,1,"Choose sharp pixels or smooth filtering for 2D artwork.","options/setting/2d-filtering","options/help/2d-filtering",sPortVideoFilterNameIds},
    {"Fullscreen",sPortVideoToggle,sPortVideoToggleNames,2,1,"Switch between windowed and fullscreen display.","options/setting/fullscreen","options/help/fullscreen",sPortVideoToggleNameIds},
    {"VSync",sPortVideoToggle,sPortVideoToggleNames,2,0,"Synchronize frames with the display to reduce tearing.","options/setting/vsync","options/help/vsync",sPortVideoToggleNameIds},
    {"Frame Skipping",sPortVideoToggle,sPortVideoToggleNames,2,0,"Skip rendering frames when needed to maintain game speed.","options/setting/frame-skipping","options/help/frame-skipping",sPortVideoToggleNameIds},
    {"Graphics API",sPortVideoToggle,sPortVideoApiNames,2,0,"Choose Vulkan or OpenGL. Takes effect after restarting the game.","options/setting/graphics-api","options/help/graphics-api",sPortVideoApiNameIds},
    {"Frame Rate (FPS)",sPortVideoFrames,sPortVideoFrameNames,4,0,"Use frame interpolation for smoother motion.","options/setting/frame-rate","options/help/frame-rate",sPortVideoFrameNameIds}
};
static const int sPortBattleEffectBits[]={1,2,4,8,16,32,0,64};
static const PortVideoOption sPortBattleEffects[] = {
    {"Character Outlines",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show the dark outlines around fighters.","options/setting/character-outlines","options/help/character-outlines",sPortVideoToggleNameIds},
    {"Occlusion Tint",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show the tint that keeps fighters visible behind scenery.","options/setting/occlusion-tint","options/help/occlusion-tint",sPortVideoToggleNameIds},
    {"Depth Haze",sPortVideoToggle,sPortVideoToggleNames,2,1,"Apply distance-based color tinting to scenery.","options/setting/depth-haze","options/help/depth-haze",sPortVideoToggleNameIds},
    {"Bloom",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show glare and glow around bright objects and the sky.","options/setting/bloom","options/help/bloom",sPortVideoToggleNameIds},
    {"Depth of Field",sPortVideoToggle,sPortVideoToggleNames,2,1,"Soften distant scenery with the game's depth blur.","options/setting/depth-of-field","options/help/depth-of-field",sPortVideoToggleNameIds},
    {"Motion Blur",sPortVideoToggle,sPortVideoToggleNames,2,1,"Blur scenery during camera movement.","options/setting/motion-blur","options/help/motion-blur",sPortVideoToggleNameIds},
    {"Sharp Stage Blur",sPortVideoToggle,sPortVideoToggleNames,2,1,"Keep stage blur crisp at higher internal resolutions.","options/setting/sharp-stage-blur","options/help/sharp-stage-blur",sPortVideoToggleNameIds},
    {"Weather",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show weather haze, dust and mist on supported stages.","options/setting/weather","options/help/weather",sPortVideoToggleNameIds}
};
static const char *const sPortEditionNames[]={"Sparking! METEOR","Budokai Tenkaichi 3"};
static const int sPortNetworkValues[]={1,2};
static const char *const sPortNetworkNames[]={"Online","LAN"};
static const char *const sPortNetworkNameIds[]={"options/value/online","options/value/lan"};
static const char *const sPortEditionNameIds[]={"options/value/sparking-meteor","options/value/budokai-tenkaichi-3"};
static const PortVideoOption sPortVisualOptions[]={
    {"Startup Logos",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show startup logos on game launch.","options/setting/startup-logos","options/help/startup-logos",sPortVideoToggleNameIds},
    {"Intro Movie",sPortVideoToggle,sPortVideoToggleNames,2,1,"Play the opening movie on game launch.","options/setting/intro-movie","options/help/intro-movie",sPortVideoToggleNameIds},
    {"Language Selection",sPortVideoToggle,sPortVideoToggleNames,2,1,"Show the language selector at startup.","options/setting/language-selection","options/help/language-selection",sPortVideoToggleNameIds}
};
static const PortVideoOption sPortGeneralOptions[]={
    {"Game Edition",sPortVideoToggle,sPortEditionNames,2,0,"Choose Sparking! METEOR or Budokai Tenkaichi 3.","options/setting/game-edition","options/help/game-edition",sPortEditionNameIds},
    {"Network Mode",sPortNetworkValues,sPortNetworkNames,2,1,"Choose online or LAN play.","options/setting/network-mode","options/help/network-mode",sPortNetworkNameIds},
    {"Autosave",sPortVideoToggle,sPortVideoToggleNames,2,0,"Automatically save game progress.","options/setting/autosave","options/help/autosave",sPortVideoToggleNameIds}
};
static const PortVideoOption *Port_OptionsDescriptor(int row) {
    if(row<8) return &sPortVideoOptions[row];
    if(row<16) return &sPortBattleEffects[row-8];
    if(row<19) return &sPortVisualOptions[row-16];
    return &sPortGeneralOptions[row-19];
}
static int Port_VideoOptionIndex(int row,int value) {
    const PortVideoOption *option=Port_OptionsDescriptor(row);
    int i,best=0,distance=0x7fffffff;
    for(i=0;i<option->count;i++) {
        int delta=option->values[i]-value;
        if(delta<0) delta=-delta;
        if(delta<distance) { distance=delta;best=i; }
    }
    return best;
}
#endif
