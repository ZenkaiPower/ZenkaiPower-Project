#include "../port_pack.h"
// The settings window of the PC build: Dear ImGui over the finished picture. F1 opens and closes it, or START + SELECT
// of a controller, or the key and button of its Controls list (gs_input.c: PortInput_Hotkey).
// Everything it shows lives elsewhere (ui.h): the renderer's settings in gs_gpu.c, the bindings in gs_input.c.
// It runs on the render thread, which owns the window and its events.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_opengl3.h"
#include "ui.h"
#include "../../../include/menu/port_menu_mouse.h"
#include "ui_transition.h"
#include "namefont.h"
#include "../port_release.h"
#include "menu_text.h"
#include "menu_labels.h"
#include "menu_translation.h"
#include "../quit_dialog.h"
#include "net_chat.h"

static const char *option_text(const char *id, const char *fallback) {
    /* F1 settings can be the first UI that needs translations; do not rely on
       the game's menu-font path having loaded the catalog first. */
    MenuTranslation_Ensure();
    const char *translated = MenuTranslation_PortText(id);
    return translated != NULL && translated[0] != '\0' ? translated : fallback;
}

static int sMeter = -1; // the meter (frame rate, connection): -1 not read yet
static bool meter_on(void);
static void menu_mouse_apply(void);
static int sGL; // 1: the OpenGL back end draws the window (ImGui's OpenGL3 backend, no SDL GPU device)
static char sGpuName[256];
/* Settings use a second SDL/ImGui context so the desktop compositor can place them outside the game window. */
static SDL_Window *sSettingsWindow;
static SDL_GLContext sSettingsGL;
static SDL_GLContext sGameGL;
static ImGuiContext *sGameContext, *sSettingsContext;
static ImFont *sSettingsWindowFont;
static bool sSettingsDrawn;
static bool sBuildingSettingsWindow;

extern "C" {
extern volatile int gPortNetMenuRequest;
// Native BattleWork fields, shared with the battle loop (include/battle/battle.h).
extern struct { char prefix[0x19F0]; uint64_t flags; int running; } gBattleWork;
int BtlPause_GetMenuPad(void);
int Port_NetMe(void);
void Port_NetOptions(int rollback, int delay); // gs/net.c
void Port_NetRulesSet(int type, int dp, int time); // gs/net.c: the host's battle type, DP limit row, time limit
void Port_NetSpeedSet(int percent);                // ... and the match's speed, 100..200 (set before the rules)
int Port_NetSpeedStandard(void);                   // what a quick match runs at
void Port_NetNameSet(const char *name);         // gs/net.c: this player's name for the match
int Port_IsWide(void);                         // plat_stub.c: the picture is wider than 4:3
int Port_AspectMilli(void);
void PortSe_UiSound(int id);
void Match_UpdateAsk(void);                    // net_match.c: asks GitHub for the newest release, once
int Match_UpdateLatest(void);                  // ... its number (0.1.33 is 133), 0 while unknown
int Port_Menus43Configured(void);          // saved manual preference
int Port_Menus43(void);                       // plat_stub.c: the menus are shown at 4:3 in a wider picture
int Port_TitleMainChars43(void);             // plat_stub.c: selected title/main-menu foreground art stays in proportion
int Port_LateInput(void);                     // plat_stub.c: "Low input lag", the blanks are waited for before the pad read
void Port_LateInputSet(int on);
void Port_Menus43Set(int on);
void Port_TitleMainChars43Set(int on);
int Port_HudLayout(void);                      // gs_draw.c: 2 = the HUD is about the middle of the screen
int GsDraw_HudShown(void);                     // ... and 1 while the game is drawing the HUD's side panels
const char *Port_NetName(int player);          // the name of the player of side 0 or 1 ("" outside a match)
int Port_HudSwap(void);                        // the two sides of the HUD are in each other's places on this copy
int Port_NetActive(void);
int Port_NetMe(void);                          // gs/net.c: this copy's side, 0 or 1
void *BtlChar_FindByObjId(int objId);          // the game: the fighter of a side, NULL outside a fight
int Port_NetStats(int *out);                   // gs/net.c: the meter's numbers of an online match
int Port_LobbyOther(void);
extern unsigned gPortLiveBlanks;               // plat_stub.c: vertical blanks the game has gone through
int Port_RollCan(void);                        // gs/state.c
const char *Port_FileRoot(void);               // plat_file.c: the game's data folder
unsigned GsGl_StripTexture(const void *rgba, int w, int h); // gs_gl.c
void GsGl_StripTextureFree(unsigned id);
const char *PortStages_Dir(void);                          // plat_stages.c
const char *PortStages_Name(int index);
int PortStages_Count(void);
const char *PortSongs_Dir(void);                           // plat_songs.c
const char *PortSongs_Name(int index);
int PortSongs_Count(void);
int PortSongs_SoundtrackAvailable(void);
int Port_Setting(const char *name, int def);   // plat_settings.c
void Port_SettingSave(const char *name, int value);
void Port_SettingText(const char *name, const char *def, char *out, int size);
void Port_SettingTextSave(const char *name, const char *value);
void Port_SettingsWrite(void);
int Port_ClashSquare(void);                    // gs_input.c: Square counts as a direction in clashes
void Port_ClashSquareSet(int on);
extern int gsInterp;                           // gs_draw.c: in-between pictures on (1) / off (0); -1 before the first frame
void Port_SettingsWrite(void);
int Port_LobbyStart(int host, const char *address, int port); // gs/net.c
int Port_LobbyStartCode(int host, const char *code);         // the same with a room code (the matchmaking service)
int Port_LobbyStartQuick(int queue);                         // a quick match: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20
void Port_LobbyQueuesAsk(void);                              // asks the service how many wait in each queue
void Port_LobbyHelloAsk(void);                               // asks it whether this release is new enough
int Port_LobbyRelease(int *min, int *latest);                // this release's number; the oldest let in and the newest (0: not known)
int Port_LobbyQueueWaiting(int queue);                       // ... as last heard, -1 if not known
const char *Port_LobbyCode(void);                            // the host's room code, "" until it is known
const char *Port_LobbyError(void);                           // why the matchmaking failed (state -3)
int Port_LobbyRelayed(void);                                 // the other player was reached through the relay
int Port_LobbyPoll(void);
void Port_LobbyCancel(void);
void Port_LobbyLaunch(void);
}
static bool sReady, sOpen;


static UiDialogTransition sSettingsTransition;
static bool sSettingsFocus;
static ImFont *sSettingsFont;
static MenuTranslationOption sLanguages[256];
static int sLanguageCount, sLanguageChoice;
static bool sLanguageSelectionAvailable;
static bool sLanguageRestartPending;
static Uint64 sMouseLastMoved;
static bool sNet;                              // the open window is the online screen (Dragon Net Battle), not the settings
static int sForceTab = -1;                     // ZENKAIPOWER_UI_OPEN=<tab>: open at start on that tab (testing)
static int sCapKind, sCapPlayer, sCapAction;   // waiting for a key (1) or a controller button (2) to bind
static Uint64 sCapUntil;                       // ... until then: a controller has no button that is not a binding, so the wait ends by itself
static bool sQuit;                             // the question before the game closes is up
static bool sQuitNo;                           // ... and Esc answered it: no
static bool sPadHold;                          // the controller does not work the window: a binding is waited for, or its button is still down
static int sTab;                               // the settings tab that is open (the shoulder buttons go to its neighbours)
static bool sPadTab;                           // the shoulder buttons changed the tab this frame
static SDL_GPUDevice *sDevice;
// The online chat (net_chat.c): its key opens a line to type, Enter sends it, Esc puts it away.
static bool sChat;                             // a line is being typed
static bool sChatFocus;                        // ... and its field takes the keyboard next frame
static bool sChatKeyHeld;                      // ... and the key that opened it is still down (its letter is not typed)
static bool sChatSoon;                         // the last Enter came within a second of the line before: not sent
static char sChatBuf[CHAT_TEXT + 1];
static void chat_close(void);

/* Name strips (gamedata/stages/names.rgba and gamedata/songs/names.rgba): the names of the stages and tracks added
   from outside the disc, drawn by the port over the menu in the game's own lettering. Raw RGBA, a 12-byte header
   (w, h, count) and then `count` images of w x h stacked. */
static SDL_GPUTexture *sNameTex, *sSongTex;
static uint32_t sNameW, sNameH, sNameCount, sSongW, sSongH, sSongCount;

static uint8_t *read_strip(const char *path, size_t *bytes, uint32_t *w, uint32_t *h, uint32_t *count) {
    FILE *fp = Port_PackFopen(path, "rb");
    uint32_t hdr[3];
    uint8_t *pix;

    if (fp == NULL) {
        return NULL;
    }
    if (fread(hdr, sizeof(hdr), 1, fp) != 1 || hdr[0] == 0 || hdr[1] == 0 || hdr[2] == 0) {
        fclose(fp);
        return NULL;
    }
    *w = hdr[0];
    *h = hdr[1];
    *count = hdr[2];
    *bytes = (size_t)hdr[0] * hdr[1] * hdr[2] * 4;
    pix = (uint8_t *)malloc(*bytes);
    if (pix == NULL || fread(pix, 1, *bytes, fp) != *bytes) {
        free(pix);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return pix;
}

static SDL_GPUTexture *make_strip(uint8_t *pix, size_t bytes, uint32_t w, uint32_t h, uint32_t count) {
    SDL_GPUTextureCreateInfo ci;
    SDL_GPUTransferBufferCreateInfo tbi;
    SDL_GPUTransferBuffer *tb;
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUCopyPass *cp;
    SDL_GPUTextureTransferInfo src;
    SDL_GPUTextureRegion dst;
    SDL_GPUTexture *tex;
    void *map;

    if (sGL) { // the OpenGL back end: a GL texture, its name carried in the pointer (ImGui's OpenGL3 backend takes it so)
        unsigned id = GsGl_StripTexture(pix, (int)w, (int)(h * count));
        free(pix);
        return (SDL_GPUTexture *)(intptr_t)id;
    }
    SDL_zero(ci);
    ci.type = SDL_GPU_TEXTURETYPE_2D;
    ci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ci.width = w;
    ci.height = h * count;
    ci.layer_count_or_depth = 1;
    ci.num_levels = 1;
    ci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex = SDL_CreateGPUTexture(sDevice, &ci);
    if (tex == NULL) {
        free(pix);
        return NULL;
    }
    SDL_zero(tbi);
    tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbi.size = (Uint32)bytes;
    tb = SDL_CreateGPUTransferBuffer(sDevice, &tbi);
    map = SDL_MapGPUTransferBuffer(sDevice, tb, false);
    memcpy(map, pix, bytes);
    SDL_UnmapGPUTransferBuffer(sDevice, tb);
    free(pix);
    cmd = SDL_AcquireGPUCommandBuffer(sDevice);
    cp = SDL_BeginGPUCopyPass(cmd);
    SDL_zero(src);
    src.transfer_buffer = tb;
    src.pixels_per_row = w;
    src.rows_per_layer = h * count;
    SDL_zero(dst);
    dst.texture = tex;
    dst.w = w;
    dst.h = h * count;
    dst.d = 1;
    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(sDevice, tb);
    return tex;
}

static SDL_GPUTexture *load_strip(const char *env, const char *rel, uint32_t *w, uint32_t *h, uint32_t *count,
                                  const char *what) {
    const char *paths[3];
    uint8_t *pix = NULL;
    size_t bytes = 0;
    SDL_GPUTexture *tex;
    uint32_t i;

    char inData[600];
    // in the stages / songs folder (plat_extras.c); the environment variable names another file
    snprintf(inData, sizeof(inData), "%s", rel);
    paths[0] = getenv(env);
    paths[1] = inData;
    paths[2] = NULL;
    for (i = 0; i < 3 && pix == NULL; i++) {
        if (paths[i] != NULL) {
            pix = read_strip(paths[i], &bytes, w, h, count);
        }
    }
    if (pix == NULL) {
        return NULL;
    }
    tex = make_strip(pix, bytes, *w, *h, *count);
    if (tex != NULL) {
        fprintf(stderr, "zenkaipower: %s overlay: %u names, %ux%u each\n", what, *count, *w, *h);
    }
    return tex;
}

// ---- the side bars ----
// Where the picture is narrower than the window (a 4:3 picture, or "Menus in 4:3", in a wide window) the sides are
// black, or with "Side bars: Border" a panel with a border along the picture. Two pictures: the border (trim.png),
// kept in proportion at the picture's edge and as high as the picture, and the plain panel (fill.png), stretched
// over the rest, so a bar of any width looks right. Both are drawn as they are on the left and mirrored on the right.
// Optional trim.png and fill.png files in "bars" next to the game provide artwork.
// Without them the sides stay black; fighter panels can still be drawn.
static SDL_GPUTexture *sBarTrim, *sBarFill;
static int sBars = -1; // 0 black, 1 the border, 2 the border with the fight panels; -1 not read yet
static int sBarsTried;
static int sChatSize = -1; // 0 = compact, 1 = normal, 2 = large; -1 = not read yet

static float sBarTrimShape = 34.0f / 720.0f; // the border's width over its height

static SDL_GPUTexture *bar_texture(const char *file, float *shape) {
    SDL_Surface *s = SDL_LoadPNG(file), *rgba; // the player's own picture, if there is one
    if (s != NULL) {
        fprintf(stderr, "zenkaipower: side bars: %s, %d x %d\n", file, s->w, s->h);
    }
    uint8_t *pix;
    size_t bytes;
    if (s == NULL) {
        return NULL;
    }
    rgba = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (rgba == NULL) {
        return NULL;
    }
    if (shape != NULL && rgba->h > 0) {
        *shape = (float)rgba->w / (float)rgba->h;
    }
    bytes = (size_t)rgba->w * rgba->h * 4;
    pix = (uint8_t *)malloc(bytes);
    for (int y = 0; pix != NULL && y < rgba->h; y++) {
        memcpy(pix + (size_t)y * rgba->w * 4, (const uint8_t *)rgba->pixels + (size_t)y * rgba->pitch, (size_t)rgba->w * 4);
    }
    SDL_GPUTexture *tex = pix != NULL ? make_strip(pix, bytes, (uint32_t)rgba->w, (uint32_t)rgba->h, 1) : NULL;
    SDL_DestroySurface(rgba);
    return tex;
}

static bool bars_on(void) {
    if (sBars < 0) {
        sBars = Port_Setting("side_bars", 0);
        sBars = sBars < 0 ? 0 : sBars > 2 ? 2 : sBars;
    }
    return sBars != 0 && gUiPresentX > 0 && gUiPresentW > 0 && gUiPresentH > 0;
}

// For the fight panels (fight_panels_ui.cpp): textures of their own and the display faces.
static ImFont *sFontBold;
SDL_GPUTexture *Ui_Texture(uint8_t *pix, uint32_t w, uint32_t h) {
    return make_strip(pix, (size_t)w * h * 4, w, h, 1);
}
void Ui_TextureFree(SDL_GPUTexture *tex) {
    if (tex == NULL) {
        return;
    }
    if (sGL) {
        GsGl_StripTextureFree((unsigned)(intptr_t)tex);
    } else {
        SDL_ReleaseGPUTexture(sDevice, tex);
    }
}
ImFont *Ui_FontBold(void) { return sFontBold; }
bool FightPanels_Draw(ImDrawList *dl, float lx, float rx, float y0, float y1, float bar, float trim);

// The real dialog atlas has one corner, a horizontal edge and the body color. Mirror the
// corner for the other three, and stretch only the edge/body texels (never the corner artwork).
static float U(float v);
static SDL_GPUTexture *sSettingsFrame;
static bool sSettingsFrameTried;
static SDL_GPUTexture *sSettingsPlate[2];
static bool settings_frame_load(void) {
    if (!sSettingsFrameTried) {
        sSettingsFrameTried = true;
        const char *data = getenv("ZENKAIPOWER_DATA");
        int width, height;
        unsigned char *pixels = MenuLabels_DialogFrame(data && data[0] ? data : "gamedata", &width, &height);
        if (pixels) sSettingsFrame = Ui_Texture(pixels, (uint32_t)width, (uint32_t)height);
        for (int i = 0; i < 2; i++) {
            pixels = MenuLabels_DialogButton(data && data[0] ? data : "gamedata", i, &width, &height);
            if (pixels) sSettingsPlate[i] = Ui_Texture(pixels, (uint32_t)width, (uint32_t)height);
        }
    }
    return sSettingsFrame != NULL;
}
static void settings_frame(ImDrawList *dl, ImVec2 pos, ImVec2 size, float opacity) {
    ImTextureRef texture((ImTextureID)(intptr_t)sSettingsFrame);
    ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, opacity));
    float cx = U(24.0f), cy = U(32.0f);
    if (cx > size.x * 0.5f) cx = size.x * 0.5f;
    if (cy > size.y * 0.5f) cy = size.y * 0.5f;
    float xs[] = {pos.x, pos.x + cx, pos.x + size.x - cx, pos.x + size.x};
    float ys[] = {pos.y, pos.y + cy, pos.y + size.y - cy, pos.y + size.y};
    for (int row = 0; row < 3; row++) for (int col = 0; col < 3; col++) {
        float u0 = col == 0 ? 20.0f : col == 2 ? 44.0f : 254.5f;
        float u1 = col == 0 ? 44.0f : col == 2 ? 20.0f : 254.5f;
        float v0 = row == 0 ? 0.0f : row == 2 ? 32.0f : 31.5f;
        float v1 = row == 0 ? 32.0f : row == 2 ? 0.0f : 31.5f;
        // The vertical sides use the bottom row of the corner; horizontal edges use the far right of the atlas.
        dl->AddImage(texture, ImVec2(xs[col], ys[row]), ImVec2(xs[col + 1], ys[row + 1]),
                     ImVec2(u0 / 256.0f, v0 / 32.0f), ImVec2(u1 / 256.0f, v1 / 32.0f), tint);
    }
}

static bool sSettingsSkin;

static void settings_plate(ImDrawList *dl, ImVec2 pos, ImVec2 size, bool selected) {
    SDL_GPUTexture *tex = sSettingsPlate[selected ? 1 : 0];
    if (!tex || size.x <= 0 || size.y <= 0) return;
    ImTextureRef texture((ImTextureID)(intptr_t)tex);
    // The native 128x64 choice texture's visible plate is 119x45. Preserve both rounded caps.
    float cap = size.y * 24.0f / 45.0f;
    if (cap > size.x * 0.5f) cap = size.x * 0.5f;
    float x[] = {pos.x, pos.x + cap, pos.x + size.x - cap, pos.x + size.x};
    float u[] = {0.0f, 24.0f / 128.0f, 95.0f / 128.0f, 119.0f / 128.0f};
    ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, ImGui::GetStyle().Alpha));
    for (int i = 0; i < 3; i++) dl->AddImage(texture, ImVec2(x[i], pos.y), ImVec2(x[i+1], pos.y + size.y),
                                           ImVec2(u[i], 0), ImVec2(u[i+1], 45.0f / 64.0f), tint);
}

// Retain ImGui's input, accessibility, tooltips and labels; render the real plate behind its glyphs.
struct SettingsItemSkin {
    ImDrawList *dl = NULL;
    ImDrawListSplitter splitter;
    ImVec2 pos, size;
    bool selected;
    bool selectionOnly = false;
    SettingsItemSkin(ImVec2 dimensions, bool chosen = false) : size(dimensions), selected(chosen) {
        if (!sSettingsSkin || !sSettingsPlate[0] || !sSettingsPlate[1]) return;
        dl = ImGui::GetWindowDrawList();
        pos = ImGui::GetCursorScreenPos();
        splitter.Split(dl, 2);
        splitter.SetCurrentChannel(dl, 1);
        const ImGuiCol colors[] = {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
            ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive,
            ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive};
        for (ImGuiCol color : colors) ImGui::PushStyleColor(color, ImVec4(0, 0, 0, 0));
    }
    ~SettingsItemSkin() {
        if (!dl) return;
        bool pointed = ImGui::IsItemHovered() || ImGui::IsItemActive() || ImGui::IsItemFocused();
        bool highlighted = selected || (!selectionOnly && pointed);
        ImGui::PopStyleColor(9);
        splitter.SetCurrentChannel(dl, 0);
        settings_plate(dl, pos, size, highlighted);
        if (selectionOnly && pointed && !selected) {
            ImU32 outline = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.75f, 0.3f, ImGui::GetStyle().Alpha));
            dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), outline, size.y * 0.5f, 0, U(1.5f));
        }
        splitter.Merge(dl);
    }
};

static bool ui_button(const char *label, ImVec2 size = ImVec2(0, 0), bool selected = false) {
    ImVec2 actual = size;
    if (actual.x < 0) actual.x = ImGui::GetContentRegionAvail().x + actual.x;
    else if (actual.x == 0) actual.x = ImGui::CalcTextSize(label, NULL, true).x + ImGui::GetStyle().FramePadding.x * 2;
    if (actual.y == 0) actual.y = ImGui::GetFrameHeight();
    SettingsItemSkin skin(actual, selected);
    return ImGui::Button(label, size);
}
static bool ui_checkbox(const char *label, bool *value) {
    SettingsItemSkin skin(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()), *value);
    bool changed = ImGui::Checkbox(label, value);
    skin.selected = *value;
    return changed;
}
static bool ui_combo_begin(const char *label, const char *preview, ImGuiComboFlags flags = 0) {
    SettingsItemSkin skin(ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()));
    bool themed = sSettingsSkin && sSettingsPlate[0] && sSettingsPlate[1];
    if (themed) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(12.0f), U(12.0f)));
        // Size the popup for full-height textured rows, rather than ImGui's text-only rows.
        float rows = (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y) * 8.0f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, rows + U(24.0f)));
    }
    bool open = ImGui::BeginCombo(label, preview, flags);
    if (themed) ImGui::PopStyleVar();
    skin.selected = open;
    return open;
}
static bool ui_selectable(const char *label, bool selected = false, ImGuiSelectableFlags flags = 0, ImVec2 size = ImVec2(0, 0)) {
    if (!sSettingsSkin || !sSettingsPlate[0] || !sSettingsPlate[1])
        return ImGui::Selectable(label, selected, flags, size);
    ImVec2 actual(size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x,
                  size.y > 0 ? size.y : ImGui::GetFrameHeight());
    SettingsItemSkin skin(actual, selected);
    skin.selectionOnly = true;
    // Keep text clear of the rounded caps and vertically centered within the native plate.
    float inset = U(14.0f);
    float spare = actual.x - ImGui::CalcTextSize(label, NULL, true).x;
    float fraction = spare > inset ? inset / spare : 0.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(fraction, 0.5f));
    bool pressed = ImGui::Selectable(label, selected, flags, actual);
    ImGui::PopStyleVar();
    return pressed;
}
static bool ui_combo(const char *label, int *current, const char *const items[], int count, int height = -1) {
    if (!sSettingsSkin) return ImGui::Combo(label, current, items, count, height);
    bool changed = false;
    if (ui_combo_begin(label, *current >= 0 && *current < count ? items[*current] : "")) {
        for (int i = 0; i < count; i++) {
            ImGui::PushID(i);
            if (ui_selectable(items[i], *current == i)) { changed = *current != i; *current = i; }
            if (*current == i) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}
static bool ui_slider_int(const char *label, int *value, int minimum, int maximum, const char *format = "%d", ImGuiSliderFlags flags = 0) {
    SettingsItemSkin skin(ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight()));
    return ImGui::SliderInt(label, value, minimum, maximum, format, flags);
}

static bool ui_radio(const char *label, int *current, int value) {
    SettingsItemSkin skin(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()), *current == value);
    return ImGui::RadioButton(label, current, value);
}
static bool ui_header(const char *label, ImGuiTreeNodeFlags flags = 0) {
    SettingsItemSkin skin(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()));
    bool open = ImGui::CollapsingHeader(label, flags);
    skin.selected = open;
    return open;
}
static void ui_progress(float fraction, ImVec2 size, const char *label) {
    ImVec2 actual(size.x > 0 ? size.x : ImGui::GetContentRegionAvail().x,
                  size.y > 0 ? size.y : ImGui::GetFrameHeight());
    SettingsItemSkin skin(actual);
    ImGui::ProgressBar(fraction, size, label);
}

// The fight panels' two faces ("Name font", "Form font" in the settings): fonts of the game's own list (fonts.tsv,
// as the Menu font and the Dialogue / UI font), checked as those are (menu_text.c) and saved by their id; "Default"
// is the first of the list that can be used. A choice is loaded into the font atlas between two frames (fonts_apply).
struct PanelFont {
    const char *setting;
    ImFont *font;
    int choice;  // in use (0: Default)
    int pending; // chosen, loaded before the next frame (-1: nothing)
};
static PanelFont sPanelFont[2] = {{"panel_name_font_id", NULL, 0, -1}, {"panel_form_font_id", NULL, 0, -1}};
ImFont *Ui_FontName(void) { return sPanelFont[0].font; }
ImFont *Ui_FontForm(void) { return sPanelFont[1].font; }

#include "ui_font.h"

// A choice of the list as an ImGui font (NULL: none can be used).
static ImFont *panel_font_load(int choice) {
    char path[1400];
    for (int i = choice > 0 ? choice : 1; i < MenuText_FontCount(); i++) {
        if (MenuText_FontFile(i, path, sizeof(path))) {
            ImFont *font = ui_font_load(path, 48.0f);
            if (font) return font;
        }
        if (choice > 0) break;
    }
    return NULL;
}

static ImFont *settings_font_load(void) {
    char path[1400];
    int fallback = MenuText_FirstFontId(MENU_TEXT_DIALOGUE);
    int choice = MenuText_FontChoice(Port_Setting("text_font_id", fallback));
    if (!choice) choice = MenuText_FontChoice(fallback);
    if (!choice || !MenuText_FontFile(choice, path, sizeof(path))) return NULL;
    return ui_font_load(path, 18.0f);
}

static void fonts_apply(void) {
    for (PanelFont &pf : sPanelFont) {
        if (pf.pending < 0) {
            continue;
        }
        ImFont *f = panel_font_load(pf.pending);
        if (f != NULL) {
            if (pf.font != NULL) {
                ImGui::GetIO().Fonts->RemoveFont(pf.font);
            }
            pf.font = f;
            pf.choice = pf.pending;
            Port_SettingSave(pf.setting, MenuText_FontId(pf.choice));
            Port_SettingsWrite();
        }
        pf.pending = -1;
    }
}

// A combo of the game's fonts; the choice is loaded before the next frame.
static void font_picker(const char *label, PanelFont &pf, const char *tip) {
    if (ui_combo_begin(label, pf.choice > 0 ? MenuText_FontName(pf.choice) : "Default")) {
        if (ui_selectable("Default", pf.choice == 0)) {
            pf.pending = 0;
        }
        for (int i = 1; i < MenuText_FontCount(); i++) {
            ImGui::PushID(i);
            if (ui_selectable(MenuText_FontName(i), pf.choice == i)) {
                pf.pending = i;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
}

static void bars_draw(void) {
    if (!sBarsTried) {
        sBarsTried = 1;
        sBarTrim = bar_texture("bars/trim.png", &sBarTrimShape);
        sBarFill = bar_texture("bars/fill.png", NULL);
    }
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float y0 = (float)gUiPresentY, y1 = y0 + (float)gUiPresentH;
    float bar = (float)gUiPresentX; // the width of each side
    float trim = (float)gUiPresentH * sBarTrimShape;
    float u0 = 0.0f;
    if (trim > bar) { // a bar narrower than the border: the border's inner part
        u0 = 1.0f - bar / trim;
        trim = bar;
    }
    float lx = (float)gUiPresentX, rx = lx + (float)gUiPresentW; // the picture's two edges
    ImTextureRef tt((ImTextureID)(intptr_t)sBarTrim), ft((ImTextureID)(intptr_t)sBarFill);
    if (bar > trim) {
        if (sBarFill != NULL) {
            dl->AddImage(ft, ImVec2(lx - bar, y0), ImVec2(lx - trim, y1), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
            dl->AddImage(ft, ImVec2(rx + trim, y0), ImVec2(rx + bar, y1), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f));
        }
        // In a fight: each side's fighter over the plain panel, with the border along the picture at half its size
        // (in proportion, twice down the height) so the fighter has more room (asked for).
        const int tiles = 2;
        float thin = trim / tiles;
        if (sBars == 2 && FightPanels_Draw(dl, lx, rx, y0, y1, bar, thin)) {
            float th = (y1 - y0) / tiles;
            for (int i = 0; sBarTrim != NULL && i < tiles; i++) {
                float a = y0 + th * i, b = a + th;
                dl->AddImage(tt, ImVec2(lx - thin, a), ImVec2(lx, b), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
                dl->AddImage(tt, ImVec2(rx, a), ImVec2(rx + thin, b), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
            }
            return;
        }
    }
    if (sBarTrim != NULL) {
        dl->AddImage(tt, ImVec2(lx - trim, y0), ImVec2(lx, y1), ImVec2(u0, 0.0f), ImVec2(1.0f, 1.0f));
        dl->AddImage(tt, ImVec2(rx, y0), ImVec2(rx + trim, y1), ImVec2(1.0f, 0.0f), ImVec2(u0, 1.0f));
    }
}

static void load_strips(void) {
    char path[600];
    // The names of added stages and songs are drawn by this overlay, over the menu: from a strip of pre-rendered
    // names if the install script made one (names.rgba in the folder), else as text (frame_build). Either way the
    // game leaves the name to the overlay, so the flags are set whenever the overlay exists.
    snprintf(path, sizeof(path), "%s/names.rgba", PortStages_Dir());
    sNameTex = PortStages_Dir()[0] != '\0' ? load_strip("ZENKAIPOWER_STAGE_NAMES", path, &sNameW, &sNameH, &sNameCount, "stage-name") : NULL;
    snprintf(path, sizeof(path), "%s/names.rgba", PortSongs_Dir());
    sSongTex = PortSongs_Dir()[0] != '\0' ? load_strip("ZENKAIPOWER_SONG_NAMES", path, &sSongW, &sSongH, &sSongCount, "song-name") : NULL;
    if (sNameTex == NULL) { sNameCount = 0; }
    if (sSongTex == NULL) { sSongCount = 0; }
    gUiNameReady = 1;
    gUiSongReady = 1;
}

// The size of the windows drawn here: the style below and every width written in this file are for a picture 1080
// pixels high, and a taller one makes them that much larger (2880 x 1800: 1.67), times the player's "Menu size"
// (setting ui_scale, in percent, 100 by default). U() is a width or height of this file at that size.
static float sUiScale = 1.0f;
static int sUiScalePct = -1;  // (the setting, read once)
static ImGuiStyle sBaseStyle; // (as style() makes it)
static inline float U(float v) { return v * sUiScale; }

static void ui_scale_apply(void) {
    ImGuiIO &io = ImGui::GetIO();
    float s = io.DisplaySize.y > 1080.0f ? io.DisplaySize.y / 1080.0f : 1.0f;
    if (sUiScalePct < 0) {
        sUiScalePct = Port_Setting("ui_scale", 100);
    }
    s *= (float)sUiScalePct / 100.0f;
    s = s < 0.5f ? 0.5f : s > 4.0f ? 4.0f : s;
    if (s != sUiScale) {
        ImGuiStyle &st = ImGui::GetStyle();
        float base = st.FontSizeBase;
        sUiScale = s;
        st = sBaseStyle;
        st.FontSizeBase = base;
        st.ScaleAllSizes(s);
        st.FontScaleMain = s;
    }
}

static void style() {
    ImGuiStyle &st = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    st.WindowRounding = 10.0f;
    st.FrameRounding = 6.0f;
    st.GrabRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.PopupRounding = 6.0f;
    st.WindowPadding = ImVec2(18.0f, 16.0f);
    st.FramePadding = ImVec2(10.0f, 6.0f);
    st.ItemSpacing = ImVec2(10.0f, 9.0f);
    st.WindowBorderSize = 0.0f;
    st.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    ImVec4 *c = st.Colors;
    const ImVec4 accent(0.96f, 0.55f, 0.13f, 1.0f), accentDim(0.96f, 0.55f, 0.13f, 0.55f);
    c[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.11f, 0.96f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.17f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.17f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.24f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.29f, 0.38f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = accentDim;
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_Header] = ImVec4(0.96f, 0.55f, 0.13f, 0.30f);
    c[ImGuiCol_HeaderHovered] = accentDim;
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.20f, 1.0f);
    c[ImGuiCol_TabHovered] = accentDim;
    c[ImGuiCol_TabSelected] = ImVec4(0.96f, 0.55f, 0.13f, 0.80f);
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.14f, 0.98f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.03f);
    sBaseStyle = st;
    sUiScale = 1.0f;
}

static void language_init() {
    const char *override = getenv("ZENKAIPOWER_MENU_LANG");
    char saved[16];
    Port_SettingText("menu_lang", "", saved, sizeof(saved));
    sLanguageCount = MenuTranslation_Available(sLanguages, (int)(sizeof(sLanguages) / sizeof(sLanguages[0])));
    sLanguageSelectionAvailable = sLanguageCount > 1 && MenuTranslation_EuropeanAvailable();
    sLanguageChoice = 0;
    const char *preferred = override != NULL && override[0] != '\0' ? override : saved[0] != '\0' ? saved : "en";
    for (int i = 0; i < sLanguageCount; i++) if (strcmp(sLanguages[i].locale, preferred) == 0) sLanguageChoice = i;
}

extern "C" void Ui_LanguageChosen(const char *locale) {
    if (locale == NULL) return;
    for (int i = 0; i < sLanguageCount; i++) {
        if (strcmp(sLanguages[i].locale, locale) == 0) {
            sLanguageChoice = i;
            sLanguageRestartPending = false;
            break;
        }
    }
}

static void language_choose(int choice) {
    if (choice < 0 || choice >= sLanguageCount) return;
    MenuTranslation_SaveChoice(sLanguages[choice].locale);
    sLanguageChoice = choice;
    sLanguageRestartPending = true;
}

static bool settings_window_init(SDL_Window *game_window, SDL_GLContext game_gl) {
    static const char *fonts[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;
    bool platform_ready = false, renderer_ready = false;
    Uint64 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
    sGameGL = game_gl;
    if (sGL) {
        flags |= SDL_WINDOW_OPENGL;
    }
    sSettingsWindow = SDL_CreateWindow("Settings", 900, 820, flags);
    if (sSettingsWindow == NULL) return false;
    if (!sGL && !SDL_ClaimWindowForGPUDevice(sDevice, sSettingsWindow)) {
        fprintf(stderr, "zenkaipower: settings window: cannot use GPU: %s\n", SDL_GetError());
        SDL_DestroyWindow(sSettingsWindow);
        sSettingsWindow = NULL;
        return false;
    }
    if (sGL) {
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        sSettingsGL = SDL_GL_CreateContext(sSettingsWindow);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
        if (sSettingsGL == NULL || !SDL_GL_MakeCurrent(sSettingsWindow, sSettingsGL)) {
            fprintf(stderr, "zenkaipower: settings window: cannot create shared GL context: %s\n", SDL_GetError());
            if (sSettingsGL) SDL_GL_DestroyContext(sSettingsGL);
            sSettingsGL = NULL;
            SDL_DestroyWindow(sSettingsWindow);
            sSettingsWindow = NULL;
            SDL_GL_MakeCurrent(game_window, game_gl);
            return false;
        }
    }
    sGameContext = ImGui::GetCurrentContext();
    sSettingsContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(sSettingsContext);
    ImGui::GetIO().IniFilename = NULL;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        if (ui_font_load(fonts[i], 18.0f)) break;
    }
    if (ImGui::GetIO().Fonts->Fonts.Size == 0) ImGui::GetIO().Fonts->AddFontDefault();
    sSettingsWindowFont = settings_font_load();
    if (sGL) {
        platform_ready = ImGui_ImplSDL3_InitForOpenGL(sSettingsWindow, sSettingsGL);
        renderer_ready = platform_ready && ImGui_ImplOpenGL3_Init("#version 330 core");
        SDL_GL_MakeCurrent(game_window, game_gl);
    } else {
        platform_ready = ImGui_ImplSDL3_InitForSDLGPU(sSettingsWindow);
        if (platform_ready) {
            info.Device = sDevice;
            info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(sDevice, sSettingsWindow);
            info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
            renderer_ready = ImGui_ImplSDLGPU3_Init(&info);
        }
    }
    if (!platform_ready || !renderer_ready) {
        if (renderer_ready) {
            if (sGL) ImGui_ImplOpenGL3_Shutdown();
            else ImGui_ImplSDLGPU3_Shutdown();
        }
        if (platform_ready) ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(sSettingsContext);
        sSettingsContext = NULL;
        ImGui::SetCurrentContext(sGameContext);
        if (sGL && game_gl) SDL_GL_MakeCurrent(game_window, game_gl);
        if (!sGL) SDL_ReleaseWindowFromGPUDevice(sDevice, sSettingsWindow);
        SDL_DestroyWindow(sSettingsWindow);
        sSettingsWindow = NULL;
        return false;
    }
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll, NULL, 0);
    ImGui::SetCurrentContext(sGameContext);
    return true;
}

int Ui_Init(SDL_Window *window, SDL_GPUDevice *device, void *gl_context, const char *gpu_name) {
    static const char *fonts[] = { // a proportional system font if there is one; else the library's built-in font
        "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", "C:\\Windows\\Fonts\\segoeui.ttf",
    };
    ImGui_ImplSDLGPU3_InitInfo info;

    snprintf(sGpuName, sizeof(sGpuName), "%s", gpu_name && gpu_name[0] ? gpu_name : "Unavailable");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL; // no window-layout file next to the game
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad; // (a controller opens it too: START + SELECT)
    style();
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        if (ui_font_load(fonts[i], 18.0f)) break;
    }
    if (io.Fonts->Fonts.Size == 0) io.Fonts->AddFontDefault();
    sSettingsFont = settings_font_load();
    { // the fight panels' faces: the two chosen ones (sPanelFont), and the list's first where one cannot be had
        sFontBold = panel_font_load(0);
        for (PanelFont &pf : sPanelFont) {
            pf.choice = MenuText_FontChoice(Port_Setting(pf.setting, 0));
            pf.font = panel_font_load(pf.choice);
        }
    }
    if (device == NULL) { // the OpenGL back end (gs_gl.c), with its context current
        sGL = 1;
        if (!ImGui_ImplSDL3_InitForOpenGL(window, (SDL_GLContext)gl_context) || !ImGui_ImplOpenGL3_Init("#version 330 core")) {
            return 0;
        }
    } else {
        if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
            return 0;
        }
        info.Device = device;
        info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
        info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
        if (!ImGui_ImplSDLGPU3_Init(&info)) {
            return 0;
        }
    }
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll, NULL, 0); // any connected controller works the window
    sDevice = device;
    sGameContext = ImGui::GetCurrentContext();
    sReady = true;
#ifndef __ANDROID__
    if (Port_SettingsEnabled() && !settings_window_init(window, (SDL_GLContext)gl_context)) {
        fprintf(stderr, "zenkaipower: settings window unavailable; using the in-game overlay\n");
    }
#endif
    sMouseLastMoved = SDL_GetTicks();
    menu_mouse_apply();
    language_init();
    load_strips(); /* the stage- and song-name overlays; the ready flags stay 0 if there is no strip */
    if (getenv("ZENKAIPOWER_UI_OPEN") != NULL && !MenuTranslation_BootPending()) {
        sForceTab = atoi(getenv("ZENKAIPOWER_UI_OPEN"));
        if (sForceTab == 9) { // testing: the online screen
            sForceTab = -1;
            gPortNetMenuRequest = 1;
        }
        if (sForceTab == 8) { // testing: the exit question
            sForceTab = -1;
            sQuit = true;
            PortQuit_Request();
        }
        sOpen = !sQuit && Port_SettingsEnabled();
        if (!sOpen && !sQuit) sForceTab = -1;
        sSettingsTransition.set(sOpen);
        sSettingsFocus = sOpen;
        gPortOverlayOpen = sOpen || sQuit;
        if (sOpen && sSettingsWindow) SDL_ShowWindow(sSettingsWindow);
    }
    return 1;
}

// The game gets nothing from keyboard or controller while the window or the exit question is up; and the button
// that closed either is not the game's.
static void input_owner(void) {
    int up = sOpen || sQuit;
    if (gPortOverlayOpen && !up) {
        PortInput_Swallow();
    }
    gPortOverlayOpen = up;
}

static void set_open(bool open) {
    if (open && sChat) {
        chat_close();
    }
    if (open) {
        if (!sOpen || sSettingsTransition.closing()) PortSe_UiSound(4);
        sSettingsTransition.set(true);
        sSettingsFocus = true;
        sOpen = true;
        if (sSettingsWindow) {
            SDL_ShowWindow(sSettingsWindow);
            SDL_RaiseWindow(sSettingsWindow);
        }
        sNet = false;
    } else if (sOpen && !sNet) {
        if (sSettingsTransition.wanted) PortSe_UiSound(5);
        sSettingsTransition.set(false);
        sOpen = sSettingsTransition.active();
    } else {
        sSettingsTransition.reset();
        sOpen = false;
        sNet = false;
    }
    sCapKind = 0;
    gPortInputCapture = 0;
    input_owner();
}

static void capture(int kind, int player, int action) {
    sCapKind = kind; sCapPlayer = player; sCapAction = action; gPortInputCapture = 1;
    sCapUntil = SDL_GetTicks() + 5000;
    sPadHold = true;
}

// A controller button pressed in the window (not while it waits for a binding, a list is open or a field is edited).
static bool pad_pressed(ImGuiKey key) {
    return sCapKind == 0 && !sPadHold && !sQuit && !ImGui::IsAnyItemActive() &&
           !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) && ImGui::IsKeyPressed(key, false);
}

extern "C" void PortA11y_Say(const char *text, int flags);
extern "C" const char *PortA11y_Label(const char *label);
static void say_quit_button(int which, int flags);

int Ui_QuitAsk(void) {
    if (!sReady) {
        return 0;
    }
    if (sQuit) { return 1; }
    if (sOpen) { set_open(false); }
    sQuit = true;
    PortQuit_Request();
    input_owner();
    const char *translated = MenuTranslation_PortText(PORT_TEXT_QUIT_CONFIRM);
    PortA11y_Say(translated ? translated : PortA11y_Label("Close the game? Progress that was not saved is lost."), 1);
    say_quit_button(1, 0);
    PortA11y_Say(PortA11y_Label("Left and right choose, Enter confirms, Escape goes back to the game"), 0);
    return 1;
}

int Ui_QuitEvent(const SDL_Event *ev) {
    SDL_Window *window = GsDraw_Window();
    SDL_WindowID windowID = window != NULL ? SDL_GetWindowID(window) : 0;
    bool request = ev->type == SDL_EVENT_QUIT;

    if (ev->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && windowID != 0 && ev->window.windowID == windowID) {
        request = true;
    } else if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && windowID != 0 &&
               ev->key.windowID == windowID && ev->key.scancode == SDL_SCANCODE_F4 && (ev->key.mod & SDL_KMOD_ALT)) {
        request = true;
    }
    if (!request) return 0;
    Ui_QuitAsk();
    return 1;
}

// The screen reader (a11y.c): it cannot see this window's question, which is drawn and not one of the game's screens,
// so the question, the button under the keyboard's focus and the keys are said here. Nothing while it is off.
static void say_quit_button(int which, int flags) {
    char text[128], pos[32];
    snprintf(pos, sizeof(pos), PortA11y_Label(", %d of %d"), which + 1, 2);
    snprintf(text, sizeof(text), "%s%s", PortA11y_Label(which == 0 ? "Exit" : "Cancel"), pos);
    PortA11y_Say(text, flags);
}

static void build_quit(void) {
    static int said = -1; // the button last said: 0 Exit, 1 Cancel
    ImGuiIO &io = ImGui::GetIO();
    bool stay = sQuitNo;
    int focus = -1;

    sQuitNo = false;
    if (!ImGui::IsPopupOpen("Exit game")) {
        ImGui::OpenPopup("Exit game");
    }
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Exit game", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const char *translated = MenuTranslation_PortText(PORT_TEXT_QUIT_CONFIRM);
        const char *kAsk = translated ? translated : "Close the game?\nUnsaved progress will be lost.";
        // the two buttons under the middle of the question (the window is as wide as its text)
        float line = ImGui::CalcTextSize(kAsk).x, pair = U(140.0f) * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::TextUnformatted(kAsk);
        ImGui::Spacing();
        if (line > pair) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (line - pair) * 0.5f);
        }
        if (ui_button(option_text("quit/action/exit", "Exit"), ImVec2(U(140.0f), 0.0f))) {
            exit(0);
        }
        focus = ImGui::IsItemFocused() ? 0 : focus;
        ImGui::SameLine();
        if (ui_button(option_text("quit/action/cancel", "Cancel"), ImVec2(U(140.0f), 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            stay = true;
        }
        focus = ImGui::IsItemFocused() ? 1 : focus;
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetItemDefaultFocus(); // (on Cancel: a second press of the same button does not close the game)
            PortA11y_Say(translated ? translated : PortA11y_Label("Close the game? Progress that was not saved is lost."), 1);
            say_quit_button(1, 0);
            PortA11y_Say(PortA11y_Label("Left and right choose, Enter confirms, Escape goes back to the game"), 0);
            said = 1;
        } else if (focus >= 0 && focus != said) {
            say_quit_button(focus, 1);
            said = focus;
        }
        if (stay) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (stay) {
        sQuit = false;
        PortQuit_Dismiss();
        input_owner();
        PortA11y_Say(PortA11y_Label("Back to the game"), 1);
    }
}

// Dragon Net Battle: the screen behind the main menu's entry. For now only its shape: what a player fills in to
// host or join. Nothing is sent anywhere yet.
extern "C" {
extern volatile int gPortNetMenuRequest;
extern volatile int gPortNetWindowClose; // gs/state.c
}

static void net_open(void) {
    set_open(true);
    sNet = true;
    Port_LobbyHelloAsk(); // (once per opening: is there a newer release, and is this one still let in)
    gPortInputCapture = 1; // the game takes no input at all while this is open (it sits on the main menu behind it)
}

// Quick match (the third tab of the online screen): what stands under the queues.
static bool sNetBusy;   // the lobby is waiting or connecting
static int sQueue = -1; // the queue chosen: 0 single, 1 team, 2..4 DP battle of 10 / 15 / 20 DP

static void quick_status(int state, int &lastState, const char *name) {
    // A search ends by itself after three minutes: a window left open does not go on asking the service for hours,
    // and the player is told that nobody was there instead of being left to wonder. One press searches again.
    static const Uint64 kSearchMs = 180000;
    static Uint64 since;
    if (state == 3 && since != 0 && SDL_GetTicks() - since > kSearchMs) {
        Port_LobbyCancel();
        lastState = -5;
        state = 0;
    }
    if (state == 3 || state == 1) {
        ImGui::TextUnformatted(state == 1 ? "Found a player. Connecting..." : "Searching for a player in this queue...");
        if (state == 3 && since != 0) {
            Uint64 left = kSearchMs - (SDL_GetTicks() - since);
            ImGui::TextDisabled("Searching for %d:%02d more. Leave the window open.", (int)(left / 60000), (int)(left / 1000 % 60));
        }
        if (ui_button("Cancel", ImVec2(U(120.0f), 0.0f))) {
            Port_LobbyCancel();
            lastState = 0;
        }
        return;
    }
    if (lastState == -3) {
        ImGui::TextWrapped("%s", Port_LobbyError());
    } else if (lastState == -4) {
        ImGui::TextWrapped("The two games could not reach each other, directly or through the relay. Search again for another player.");
    } else if (lastState == -2) {
        ImGui::TextWrapped("The other player's game is a different version. Search again.");
    } else if (lastState == -5) {
        ImGui::TextWrapped("Nobody was searching this queue in the last three minutes. Search again, or try another queue.");
    }
    if (ui_button(lastState == -5 ? "Search again" : "Search for a match", ImVec2(U(200.0f), 0.0f))) {
        since = SDL_GetTicks() + 1;
        // The same for everyone in a quick match, whoever turns out to be the host: rollback of up to 8 frames and
        // the input delay measured from the connection. (The Host tab's own choices are for matches with someone
        // you can agree them with; a stranger's saved ones were not to be inherited unseen.)
        Port_NetOptions(Port_RollCan() ? 8 : 0, -2);
        Port_NetSpeedSet(Port_NetSpeedStandard()); // (every quick match: the competitive standard)
        Port_NetRulesSet(sQueue == 0 ? 0 : sQueue == 1 ? 1 : 2, sQueue >= 2 ? sQueue - 2 : 0, 4);
        Port_NetNameSet(name);
        Port_SettingTextSave("net_name", name);
        Port_SettingSave("net_queue", sQueue);
        Port_SettingsWrite();
        lastState = 0;
        Port_LobbyStartQuick(sQueue);
    }
}

static void build_net(void) {
    static char name[17] = "Player", address[64] = "", port[8] = "7000";
    static int tab, roll = -1, delay = 1, battle = -1, timeLimit = 3, speed = -1;
    static bool useCode = true;       // host with a room code (no port forwarding) or on a port
    static char roomCode[8] = "";     // the code to join with
    static int lastState = 0;         // what the lobby said last (for its messages after a failure)
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(U(560.0f), 0.0f), ImGuiCond_Appearing);
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window, as its close button
        open = false;
    }
    if (ImGui::Begin("Dragon Net Battle", &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        {
            int min = 0, latest = 0, mine = Port_LobbyRelease(&min, &latest);
            if (min > mine) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.55f, 0.35f, 1.0f));
                ImGui::TextWrapped("Update available. Online servers are unreachable until you update.");
                ImGui::PopStyleColor();
                ImGui::TextDisabled("You have 0.%d.%d; 0.%d.%d or newer is needed. github.com/ZenkaiPower/ZenkaiPower-Project/releases", mine / 100, mine % 100, min / 100, min % 100);
                ImGui::Separator();
            } else if (latest > mine) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Update available: release 0.%d.%d.", latest / 100, latest % 100);
                ImGui::TextDisabled("github.com/ZenkaiPower/ZenkaiPower-Project/releases");
                ImGui::Separator();
            }
        }
        ImGui::TextWrapped("Fight a player on another computer. One of you hosts, the other joins with the host's address.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(U(260.0f));
        if (ImGui::IsWindowAppearing())
            Port_SettingText("net_name", "Player", name, sizeof(name));
        ImGui::InputText("Player name", name, sizeof(name));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Port_SettingTextSave("net_name", name);
            Port_SettingsWrite();
        }
        ImGui::Spacing();
        if (ImGui::BeginTabBar("net")) {
            if (ImGui::BeginTabItem("Host")) {
                static const char *const kRoll[] = {"Off (wait for each other)", "Up to 2 frames", "Up to 4 frames", "Up to 6 frames", "Up to 8 frames"};
                tab = 0;
                ui_checkbox("Use a room code (no port forwarding needed)", &useCode);
                ImGui::SetItemTooltip("You get a short code to tell the other player, and the two games find each other through a\n"
                                      "small matchmaking service. Works with most home routers. Without it you host on a port,\n"
                                      "which must be reachable from the other player (port forwarding or a virtual network).");
                ImGui::BeginDisabled(useCode);
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText("Port", port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                if (roll < 0) { // the choices of last time (new names: the first ones' defaults were 4 frames and delay 1)
                    roll = Port_RollCan() ? Port_Setting("net_rollback2", 8) / 2 : 0;
                    delay = Port_Setting("net_delay2", -1) + 1; // 0 = automatic, else the delay + 1
                    if (roll < 0 || roll > 4) { roll = 4; }
                    if (delay < 0 || delay > 7) { delay = 0; }
                }
                ImGui::BeginDisabled(!Port_RollCan());
                ImGui::SetNextItemWidth(U(260.0f));
                ui_combo("Rollback", &roll, kRoll, 5);
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("With rollback your own moves come out at once and the game corrects itself when the\n"
                                      "other player's buttons arrive. More frames cope with a slower connection and cost\n"
                                      "more processor time. Off: both games wait for each other every frame.");
                }
                ImGui::SetNextItemWidth(U(260.0f));
                {
                    static const char *const kDelay[] = {"Automatic (from the connection)", "0 frames", "1 frame", "2 frames", "3 frames",
                                                         "4 frames", "5 frames", "6 frames"};
                    ui_combo("Input delay", &delay, kDelay, 8);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("How long after a press your own move comes out. A slow connection needs more, or the game\n"
                                      "has to guess too far ahead and stutters. Automatic measures the connection when the match\n"
                                      "starts: 1 frame on a good one, more on a slow one.");
                }
                {
                    // The match's rules: the game's own versus menu is not shown online, so they are chosen here.
                    static const char *const kBattle[] = {"Single battle", "Team battle", "DP battle (10 DP)", "DP battle (15 DP)", "DP battle (20 DP)"};
                    static const char *const kTime[] = {"60 seconds", "90 seconds", "180 seconds", "240 seconds", "No limit"};
                    if (battle < 0) {
                        battle = Port_Setting("net_battle", 0);
                        timeLimit = Port_Setting("net_time", 3);
                        if (battle < 0 || battle > 4) { battle = 0; }
                        if (timeLimit < 0 || timeLimit > 4) { timeLimit = 3; }
                    }
                    ImGui::Spacing();
                    ImGui::SetNextItemWidth(U(260.0f));
                    ui_combo("Battle", &battle, kBattle, 5);
                    ImGui::SetNextItemWidth(U(260.0f));
                    ui_combo("Time limit", &timeLimit, kTime, 5);
                    {
                        if (speed < 0) {
                            speed = Port_Setting("net_speed_host", Port_NetSpeedStandard()) /* (a new name with 0.1.31: the standard went from 120 to 114) */;
                        }
                        ImGui::SetNextItemWidth(U(260.0f));
                        ui_slider_int("Game speed", &speed, 100, 150, "%d%%", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::SetItemTooltip("How fast the match runs, for both players; the sound keeps its pitch.\n"
                                              "100%% is the game's own speed. %d%% is the competitive standard, and what\n"
                                              "quick matches run at.\n"
                                              "With a player on 0.1.29 only 100%% and 120%% are possible, with an older\n"
                                              "release only 100%%.", Port_NetSpeedStandard());
                    }
                }
                ImGui::TextDisabled("The host's choices apply to both players.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Quick match")) {
                static const char *const kQueue[] = {"Single battle", "Team battle", "DP battle (10 DP)", "DP battle (15 DP)", "DP battle (20 DP)"};
                static Uint64 asked;
                bool busy = sNetBusy; // (a search is under way: the queue is not changed under it)
                tab = 2;
                if (sQueue < 0) {
                    sQueue = Port_Setting("net_queue", 0);
                    if (sQueue < 0 || sQueue > 4) { sQueue = 0; }
                }
                {
                    // How many wait: asked when the tab is opened and then every thirty seconds, but only while the
                    // player is doing something here (a window left open all day asks nothing after two minutes),
                    // and not during a search (the search hears from the service anyway).
                    static Uint64 active;
                    bool moved = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f || ImGui::IsAnyItemActive();
                    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END && !moved; k++) {
                        moved = ImGui::IsKeyDown((ImGuiKey)k);
                    }
                    if (moved || active == 0) {
                        active = SDL_GetTicks() + 1;
                    }
                    if (!sNetBusy && (asked == 0 || (SDL_GetTicks() - asked > 30000 && SDL_GetTicks() - active < 120000))) {
                        asked = SDL_GetTicks() + 1;
                        Port_LobbyQueuesAsk();
                    }
                }
                ImGui::TextWrapped("Be matched with whoever is searching the same queue. All quick matches have no time limit and run at %d%% speed.", Port_NetSpeedStandard());
                ImGui::BeginDisabled(busy);
                for (int i = 0; i < 5; i++) {
                    char label[80];
                    int n = Port_LobbyQueueWaiting(i);
                    if (n < 0) {
                        snprintf(label, sizeof(label), "%s", kQueue[i]);
                    } else {
                        snprintf(label, sizeof(label), "%s   (%d searching)###q%d", kQueue[i], n, i);
                    }
                    ui_radio(label, &sQueue, i);
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled("The two games connect to each other directly when they can, as in most online fighting");
                ImGui::TextDisabled("games: a player who inspects their own network traffic can see the other's address.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Join")) {
                tab = 1;
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText("Room code", roomCode, sizeof(roomCode), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank);
                ImGui::SameLine();
                if (ui_button("Paste")) { // the letters and digits of what was copied, as capitals
                    char *clip = SDL_GetClipboardText();
                    size_t n = 0;
                    for (const char *c = clip; c != NULL && *c != '\0' && n + 1 < sizeof(roomCode); c++) {
                        if ((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z')) {
                            roomCode[n++] = (char)(*c >= 'a' ? *c - 32 : *c);
                        }
                    }
                    roomCode[n] = '\0';
                    SDL_free(clip);
                }
                ImGui::TextDisabled("or, without a code, the host's address and port:");
                ImGui::BeginDisabled(roomCode[0] != '\0');
                ImGui::SetNextItemWidth(U(260.0f));
                ImGui::InputText("Host's address", address, sizeof(address));
                ImGui::SetNextItemWidth(U(120.0f));
                ImGui::InputText("Port", port, sizeof(port), ImGuiInputTextFlags_CharsDecimal);
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Spacing();
        {
            int state = Port_LobbyPoll();
            sNetBusy = state == 1 || state == 3;
            if (state == 2) {
                Port_LobbyLaunch(); // the other player is there: the game turns into the session at its next blank
                set_open(false);
                sNet = false;
            }
            bool byCode = tab == 0 ? useCode : roomCode[0] != '\0';
            if (state != 0) {
                lastState = state;
            }
            if (tab == 2) {
                quick_status(state, lastState, name);
            } else if (state == 3 || (state == 1 && byCode)) { // a room code: asking the service, then reaching the other player
                if (tab == 0 && Port_LobbyCode()[0] != '\0') {
                    ImGui::TextUnformatted("Room code:");
                    ImGui::SameLine();
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", Port_LobbyCode());
                    ImGui::SetWindowFontScale(1.0f);
                    {
                        static Uint64 copiedAt;
                        ImGui::SameLine();
                        if (ui_button("Copy")) {
                            copiedAt = SDL_SetClipboardText(Port_LobbyCode()) ? SDL_GetTicks() : 0;
                        }
                        ImGui::SetItemTooltip("Copy the code, to paste it to the other player.");
                        if (copiedAt != 0 && SDL_GetTicks() - copiedAt < 2000) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("Copied");
                        }
                    }
                    ImGui::TextUnformatted(state == 3 ? "Tell the other player this code. Waiting for them to join..." : "The other player has joined. Connecting...");
                } else {
                    ImGui::TextUnformatted(state == 1 ? "Found the room. Connecting to the host..." : tab == 0 ? "Getting a room code..." : "Looking for the room...");
                }
                if (ui_button("Cancel", ImVec2(U(120.0f), 0.0f))) {
                    Port_LobbyCancel();
                    lastState = 0;
                }
            } else if (state == 1) {
                if (tab == 0) {
                    ImGui::Text("Waiting for the other player on port %s...", port);
                    if (Port_LobbyOther()) {
                        ImGui::TextWrapped("A player with a different version of the game is trying to join. Both need the same release.");
                    }
                } else {
                    ImGui::Text("Looking for %s...", address);
                }
                if (ui_button("Cancel", ImVec2(U(120.0f), 0.0f))) {
                    Port_LobbyCancel();
                }
            } else {
                if (lastState == -3) {
                    ImGui::TextWrapped("%s", Port_LobbyError());
                } else if (lastState == -4) {
                    ImGui::TextWrapped("The two games could not reach each other, directly or through the relay. "
                                       "Hosting on a port (with port forwarding or a virtual network such as Tailscale) still works.");
                }
                bool can = byCode ? (tab == 0 || strlen(roomCode) == 6) : atoi(port) > 0 && (tab == 0 || address[0] != '\0');
                ImGui::BeginDisabled(!can);
                if (ui_button(tab == 0 ? "Host a match" : "Join the match", ImVec2(U(200.0f), 0.0f))) {
                    if (tab == 0 && roll >= 0) {
                        Port_NetOptions(roll * 2, delay == 0 ? -2 : delay - 1);
                        if (battle >= 0) {
                            Port_NetSpeedSet(speed);
                            Port_SettingSave("net_speed_host", speed);
                            Port_NetRulesSet(battle == 0 ? 0 : battle == 1 ? 1 : 2, battle >= 2 ? battle - 2 : 0, timeLimit);
                            Port_SettingSave("net_battle", battle);
                            Port_SettingSave("net_time", timeLimit);
                        }
                        Port_SettingSave("net_rollback2", roll * 2);
                        Port_SettingSave("net_delay2", delay - 1);
                        Port_SettingsWrite();
                    } else {
                        Port_NetOptions(-1, -1); // joining: the host's choices arrive with its answer
                    }
                    Port_NetNameSet(name);
                    Port_SettingTextSave("net_name", name);
                    Port_SettingsWrite();
                    lastState = 0;
                    if (byCode) {
                        Port_LobbyStartCode(tab == 0, roomCode);
                    } else {
                        Port_LobbyStart(tab == 0, address, atoi(port));
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ui_button("Back", ImVec2(U(120.0f), 0.0f))) {
                    open = false;
                }
                if (state == -2) {
                    ImGui::TextWrapped("The host's game is a different version. Both players need the same release.");
                } else if (state < 0) {
                    ImGui::TextWrapped(tab == 0 ? "That port cannot be used (another program has it?)." : "That address is not known.");
                }
            }
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Characters follow each player's save. Shared stages and music require both players");
        ImGui::TextDisabled("to have unlocked them. Your save stays untouched; leaving brings you back to where");
        ImGui::TextDisabled("you were. The host's controls also work the menus.");
    }
    ImGui::End();
    if (!open) {
        Port_LobbyCancel();
        set_open(false);
    }
}

int Ui_ConsumesEscape(void) {
    return sReady && (sQuit || sChat || sCapKind != 0 || (sOpen &&
        (ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))));
}

void Ui_Toggle(void) {
    if (Port_SettingsEnabled() && sReady && !sQuit) {
        set_open(!sOpen || sSettingsTransition.closing());
    }
}

static void bind(int value) {
    if (sCapAction == PORT_ACTIONS) { // the row "Open this window"
        if (sCapKind == 1) {
            *PortInput_OverlayKey() = value > 0 ? value : SDL_SCANCODE_F1; // (cleared = F1 again: a keyboard always has a way in)
        } else if (value < PORT_PAD_LT) { // (a button, not a trigger)
            *PortInput_OverlayPad() = value;
        }
    } else if (sCapAction == PORT_ACTIONS + 1) { // the row "Open online chat"
        if (sCapKind == 1) {
            *PortInput_ChatKey() = value > 0 ? value : SDL_SCANCODE_TAB;
        }
    } else if (sCapKind == 1) {
        PortInput_Keys(sCapPlayer)[sCapAction] = value;
    } else {
        PortInput_PadButtons(sCapPlayer)[sCapAction] = value;
    }
    PortInput_Save();
}

static bool chat_can_open(void) {
    return sReady && !sOpen && !sQuit && PortChat_Enabled() && Port_NetActive();
}

static void chat_open(void) {
    sChat = true;
    sChatFocus = true;
    sChatKeyHeld = true;
    sChatSoon = false;
    gPortChatTyping = 1;
    ImGui::GetIO().ClearInputKeys(); // (it saw none of the keys while the line was put away: no key is down for it)
}

static void chat_close(void) {
    sChat = false;
    gPortChatTyping = 0;
    PortInput_SwallowKeys(); // (Enter, which sent the line, is START: not the game's until it is let go)
}

// The chat while a line is typed: the keyboard is the line's; Esc puts it away. 1 = the event was used.
static int chat_event(const SDL_Event *ev) {
    int key = *PortInput_ChatKey();
    if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !sChatKeyHeld && !ev->key.repeat && key > 0 && (int)ev->key.scancode == key) {
        chat_close();
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_UP && (int)ev->key.scancode == key) {
        sChatKeyHeld = false;
    }
    if (ev->type == SDL_EVENT_TEXT_INPUT && sChatKeyHeld) { // the letter of the key that opened the line
        return 1;
    }
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT || ev->type == SDL_EVENT_TEXT_EDITING ||
        ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL ||
        ev->type == SDL_EVENT_MOUSE_MOTION) {
        ImGui_ImplSDL3_ProcessEvent(ev);
    }
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

int Ui_Event(const SDL_Event *ev) {
    if (sSettingsWindow && sSettingsContext && ev->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        ev->window.windowID == SDL_GetWindowID(sSettingsWindow)) {
        set_open(false);
        return 1;
    }
    if (sSettingsContext && (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || ev->type == SDL_EVENT_GAMEPAD_BUTTON_UP ||
                             ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION || ev->type == SDL_EVENT_GAMEPAD_ADDED ||
                             ev->type == SDL_EVENT_GAMEPAD_REMOVED)) {
        ImGui::SetCurrentContext(sSettingsContext);
        ImGui_ImplSDL3_ProcessEvent(ev);
        ImGui::SetCurrentContext(sGameContext);
    }
    if (sSettingsContext && ev->type != SDL_EVENT_GAMEPAD_ADDED && ev->type != SDL_EVENT_GAMEPAD_REMOVED) {
        SDL_WindowID id = 0;
        switch (ev->type) {
        case SDL_EVENT_WINDOW_SHOWN: case SDL_EVENT_WINDOW_HIDDEN: case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOVED: case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_MINIMIZED: case SDL_EVENT_WINDOW_MAXIMIZED: case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MOUSE_ENTER: case SDL_EVENT_WINDOW_MOUSE_LEAVE: case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            id = ev->window.windowID; break;
        case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: id = ev->key.windowID; break;
        case SDL_EVENT_TEXT_INPUT: id = ev->text.windowID; break;
        case SDL_EVENT_TEXT_EDITING: id = ev->edit.windowID; break;
        case SDL_EVENT_MOUSE_MOTION: id = ev->motion.windowID; break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: id = ev->button.windowID; break;
        case SDL_EVENT_MOUSE_WHEEL: id = ev->wheel.windowID; break;
        default: break;
        }
        if (id == SDL_GetWindowID(sSettingsWindow)) {
            ImGui::SetCurrentContext(sSettingsContext);
            ImGui_ImplSDL3_ProcessEvent(ev);
            bool close = ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat &&
                         ev->key.scancode == SDL_SCANCODE_ESCAPE && !Ui_ConsumesEscape();
            ImGui::SetCurrentContext(sGameContext);
            if (close) set_open(false);
            return 1;
        }
    }
    if(PortInput_PcCaptureEvent(ev)) return 1;
    if (ev->type == SDL_EVENT_MOUSE_MOTION && (ev->motion.xrel != 0 || ev->motion.yrel != 0))
        sMouseLastMoved = SDL_GetTicks();
    if (!sReady) {
        return 0;
    }
    if (ev->type == SDL_EVENT_GAMEPAD_ADDED || ev->type == SDL_EVENT_GAMEPAD_REMOVED) { // (also while it is closed: its list of controllers)
        ImGui_ImplSDL3_ProcessEvent(ev);
        return 0;
    }
    if (sQuit && PortQuit_Active()) { // game dialog: keyboard and controller operate its native Yes / No plates
        if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat) {
            SDL_Scancode key = ev->key.scancode;
            if (key == SDL_SCANCODE_ESCAPE) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER || key == SDL_SCANCODE_SPACE) {
                PortQuit_Action(PORT_QUIT_CONFIRM);
            } else if (key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT ||
                       key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        } else if (ev->type == SDL_EVENT_MOUSE_MOTION || ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float x, y;
            bool motion = ev->type == SDL_EVENT_MOUSE_MOTION;
            int button = motion ? 0 : ev->button.button;
            if ((motion && (ev->motion.xrel != 0 || ev->motion.yrel != 0)) ||
                button == SDL_BUTTON_LEFT || button == SDL_BUTTON_RIGHT) {
                if (PortInput_MousePoint(motion ? ev->motion.x : ev->button.x,
                                         motion ? ev->motion.y : ev->button.y, &x, &y))
                    PortQuit_Mouse((int)(x * 16.0f), (int)(y * 16.0f), motion ? 0 : button == SDL_BUTTON_LEFT ? 1 : 2);
                else PortQuit_Mouse(-1, -1, 0);
            }
        } else if (ev->type == SDL_EVENT_WINDOW_FOCUS_LOST || ev->type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
            PortQuit_Action(0); /* discard pending pointer input */
        } else if (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            SDL_GamepadButton button = (SDL_GamepadButton)ev->gbutton.button;
            if (button == SDL_GAMEPAD_BUTTON_SOUTH) { PortQuit_Action(PORT_QUIT_CONFIRM); }
            else if (button == SDL_GAMEPAD_BUTTON_EAST) { PortQuit_Action(PORT_QUIT_CANCEL); }
            else if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                     button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                if (PortQuit_Ready()) { say_quit_button(1 - PortQuit_Cursor(), 1); }
                PortQuit_Action(PORT_QUIT_MOVE);
            }
        }
        return 1;
    }
    if (sChat && !sOpen && !sQuit) {
        return chat_event(ev);
    }
    if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && *PortInput_ChatKey() > 0 && (int)ev->key.scancode == *PortInput_ChatKey() &&
        !PortInput_NativeKeyboardActive() && chat_can_open()) {
        chat_open();
        return 1;
    }
    if (sQuit) { // fallback when the native dialog resource cannot be loaded
        if (ev->type == SDL_EVENT_KEY_DOWN && ev->key.scancode == SDL_SCANCODE_ESCAPE) {
            sQuitNo = sQuitNo || !ev->key.repeat; // (taken away where it is drawn)
            return 1;
        }
        ImGui_ImplSDL3_ProcessEvent(ev);
        return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
               ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
    }
    if (!sOpen) {
        return 0;
    }
    if (sCapKind != 0) { // the next key or button is the binding; Esc leaves it as it was, Delete clears it
        bool done = false;
        if (ev->type == SDL_EVENT_KEY_DOWN) {
            if (ev->key.scancode == SDL_SCANCODE_DELETE || ev->key.scancode == SDL_SCANCODE_BACKSPACE) {
                bind(sCapKind == 1 ? 0 : -1);
            } else if (ev->key.scancode != SDL_SCANCODE_ESCAPE && sCapKind == 1) {
                bind((int)ev->key.scancode);
            }
            done = ev->key.scancode == SDL_SCANCODE_ESCAPE || ev->key.scancode == SDL_SCANCODE_DELETE ||
                   ev->key.scancode == SDL_SCANCODE_BACKSPACE || sCapKind == 1;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            bind((int)ev->gbutton.button);
            done = true;
        } else if (sCapKind == 2 && ev->type == SDL_EVENT_GAMEPAD_AXIS_MOTION && ev->gaxis.value > 20000 &&
                   (ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || ev->gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            bind(ev->gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? PORT_PAD_LT : PORT_PAD_RT);
            done = true;
        }
        if (done) {
            sCapKind = 0;
            gPortInputCapture = 0;
        }
        if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
            return 1;
        }
    } else if (ev->type == SDL_EVENT_KEY_DOWN && !ev->key.repeat && ev->key.scancode == SDL_SCANCODE_ESCAPE && !Ui_ConsumesEscape()) {
        set_open(false);
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    return ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
           ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL;
}

static const char *pad_source_name(int src) {
    static const char *names[] = {"A / Cross", "B / Circle", "X / Square", "Y / Triangle", "Back / Select", "Guide", "Start",
                                  "Left stick click", "Right stick click", "Left shoulder", "Right shoulder", "D-pad up",
                                  "D-pad down", "D-pad left", "D-pad right", "Misc 1", "Right paddle 1", "Left paddle 1",
                                  "Right paddle 2", "Left paddle 2", "Touchpad"};
    static char other[24];
    if (src == PORT_PAD_LT) { return "Left trigger"; }
    if (src == PORT_PAD_RT) { return "Right trigger"; }
    if (src < 0) { return "-"; }
    if (src < (int)(sizeof(names) / sizeof(names[0]))) { return names[src]; }
    snprintf(other, sizeof(other), "Button %d", src);
    return other;
}

extern "C" int Port_CheatsEnabled(void);

static void settings_tabs(void) {
    const char *const names[] = {
        option_text("options/category/video", "Video Settings"),
        option_text("options/category/cheats", "Cheats")
    };
    static const int indices[] = {0, 5};
    if (!Port_CheatsEnabled() && sTab == 5) sTab = 0;
    for (int i = 0; i < 2; i++) if (sForceTab == indices[i] && (indices[i] != 5 || Port_CheatsEnabled())) sTab = sForceTab;
    int count = Port_CheatsEnabled() ? 2 : 1;
    float width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (count - 1)) / count;
    bool first = true;
    for (int i = 0; i < 2; i++) {
        if (indices[i] == 5 && !Port_CheatsEnabled()) continue;
        if (!first) ImGui::SameLine();
        first = false;
        if (ui_button(names[i], ImVec2(width, ImGui::GetFrameHeight() + U(6)), sTab == indices[i])) sTab = indices[i];
    }
    ImGui::Spacing();
}
static bool tab(const char *, int index) { return sTab == index; }

extern "C" int Port_TextureExportEnabled(void);
extern "C" void Port_TextureExportSet(int enabled);

// Native Options uses the same apply-and-save path as the external Video tab.
#include "../../../include/menu/port_video_options.h"
#include "../../../include/menu/port_options_save.h"
#include "../../../include/sys/port_zenkai.h"
static SDL_SpinLock sNativeVideoLock;
static int sNativeVideoCurrent[PORT_OPTION_SETTING_COUNT];
static int sNativeVideoReady;
static int sNativeVideoPending[PORT_OPTION_SETTING_COUNT]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
extern "C" int Port_WiiArtAvailable(void);
static int sNativeJapaneseAvailable,sNativeWiiAvailable;
extern "C" int Port_OptionsAvailable(int row) {
    if(row==18) return MenuTranslation_EuropeanAvailable();
    if(row==19) return __atomic_load_n(&sNativeJapaneseAvailable,__ATOMIC_ACQUIRE);
    if(row==20) return __atomic_load_n(&sNativeWiiAvailable,__ATOMIC_ACQUIRE);
    return 1;
}
extern "C" void Port_OptionsCategoryDefaults(int category) {
    if(category<0 || category>=PORT_OPTION_CATEGORY_COUNT) return;
    SDL_LockSpinlock(&sNativeVideoLock);
    for(int i=0;i<sPortOptionCounts[category];i++) {
        int row=Port_OptionsCategoryRow(category,i);
        if(row>=PORT_OPTION_SETTING_COUNT) continue; /* save-owned Classic Mode */
        if(Port_OptionsAvailable(row))
            sNativeVideoPending[row]=row==PORT_VISUAL_OPTION_OFFSET+2 ? 1 : Port_OptionsDescriptor(row)->defaultValue;
    }
    SDL_UnlockSpinlock(&sNativeVideoLock);
}
/* Staged separately from SaveData; persisted only after the main save succeeds. */
static unsigned char sOptionsSnapshot[PORT_OPTIONS_FILE_BYTES];
/* Only the game thread touches SaveData; the render thread owns application. */
extern "C" void Port_OptionsSaveLoad(void *save) {
    int values[PORT_OPTION_SETTING_COUNT];
    if(!save || !Port_ZenkaiEnabled()) return;
    unsigned char payload[PORT_OPTIONS_FILE_BYTES];
    int found=Port_OptionsFileRead(payload);
    /* A language choice can precede creation of the first game save. Do not
       apply its language-only companion as native settings or bindings. */
    if(found==1 && Port_OptionsFileLanguageOnly(payload)) found=0;
    memset(sOptionsSnapshot,0,sizeof(sOptionsSnapshot));
    if(found<0 || (found==1 ? !Port_OptionsFileDecode(payload,values):!Port_OptionsSaveDecode(save,values))) return;
    int bindings[PORT_PC_BINDINGS];
    if(found==1) {
        Port_OptionsFileApplyNative(payload,save);
        Port_ClashSquareApply(Port_OptionsFileClashMode(payload));
        Port_VirtualGamepadApply(Port_OptionsFileVirtualGamepad(payload));
        PortInput_GamepadSlotsSet(Port_OptionsFileGamepadSlots(payload));
        if(Port_OptionsFileDecodeBindings(payload,bindings)) PortInput_PcSet(bindings);
        int gamepad[PORT_GAMEPAD_BINDINGS];
        if(Port_OptionsFileDecodeGamepad(payload,gamepad)) PortInput_GamepadSet(gamepad);
        PortInput_PlayerDefaults(1,0);PortInput_PlayerDefaults(1,1);
        if(Port_OptionsFileDecodePlayer2(payload,bindings,gamepad)) PortInput_PlayerSet(1,bindings,gamepad);
        PortInput_PlayerDefaults(1,0);PortInput_PlayerDefaults(1,1);
        if(Port_OptionsFileDecodePlayer2(payload,bindings,gamepad)) PortInput_PlayerSet(1,bindings,gamepad);
    }
    Port_OptionsFileEncode(sOptionsSnapshot,save,values);
    Port_OptionsFileSetClashMode(sOptionsSnapshot,Port_ClashSquare());
    Port_OptionsFileSetVirtualGamepad(sOptionsSnapshot,Port_VirtualGamepad());
    Port_OptionsFileSetGamepadSlots(sOptionsSnapshot,PortInput_GamepadSlotsGet());
    if(found==1) {
        char playerName[PORT_OPTIONS_PLAYER_NAME_BYTES];
        if(Port_OptionsFileGetName(payload,playerName,sizeof(playerName)))
            Port_OptionsFileSetName(sOptionsSnapshot,playerName);
    }
    char locale[PORT_OPTIONS_LOCALE_BYTES];
    Port_SettingText("menu_lang","",locale,sizeof(locale));
    if(locale[0]) Port_OptionsFileSetLanguage(sOptionsSnapshot,locale);
    PortInput_PcGet(bindings);Port_OptionsFileEncodeBindings(sOptionsSnapshot,bindings);
    int gamepad[PORT_GAMEPAD_BINDINGS];PortInput_GamepadGet(gamepad);Port_OptionsFileEncodeGamepad(sOptionsSnapshot,gamepad);
    PortInput_PlayerGet(1,bindings,gamepad);Port_OptionsFileEncodePlayer2(sOptionsSnapshot,bindings,gamepad);
    PortInput_PlayerGet(1,bindings,gamepad);Port_OptionsFileEncodePlayer2(sOptionsSnapshot,bindings,gamepad);
    SDL_LockSpinlock(&sNativeVideoLock);
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) sNativeVideoPending[row]=values[row];
    SDL_UnlockSpinlock(&sNativeVideoLock);
}
extern "C" int Port_OptionsSaveSync(void *save) {
    int values[PORT_OPTION_SETTING_COUNT],old[PORT_OPTION_SETTING_COUNT],changed=0;
    if(!save || !Port_ZenkaiEnabled()) return 0;
    int valid=Port_OptionsFileDecode(sOptionsSnapshot,old);
    SDL_LockSpinlock(&sNativeVideoLock);
    if(!sNativeVideoReady) { SDL_UnlockSpinlock(&sNativeVideoLock);return 0; }
    /* Wait for application so the native save flow observes the new Autosave policy. */
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) if(sNativeVideoPending[row]>=0) {
        SDL_UnlockSpinlock(&sNativeVideoLock);return 0;
    }
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        int value=sNativeVideoPending[row]>=0 ? sNativeVideoPending[row]:sNativeVideoCurrent[row];
        const PortVideoOption *option=Port_OptionsDescriptor(row);
        values[row]=valid && !Port_OptionsAvailable(row) ? old[row]:option->values[Port_VideoOptionIndex(row,value)];
        if(valid && old[row]!=values[row]) changed=1;
    }
    SDL_UnlockSpinlock(&sNativeVideoLock);
    unsigned char snapshot[PORT_OPTIONS_FILE_BYTES];
    Port_OptionsFileEncode(snapshot,save,values);
    Port_OptionsFileSetClashMode(snapshot,Port_ClashSquare());
    Port_OptionsFileSetVirtualGamepad(snapshot,Port_VirtualGamepad());
    Port_OptionsFileSetGamepadSlots(snapshot,PortInput_GamepadSlotsGet());
    /* Rebuilding the native snapshot must not erase the online identity. */
    char playerName[PORT_OPTIONS_PLAYER_NAME_BYTES];
    if(Port_OptionsFileGetName(sOptionsSnapshot,playerName,sizeof(playerName)))
        Port_OptionsFileSetName(snapshot,playerName);
    char locale[PORT_OPTIONS_LOCALE_BYTES];
    Port_SettingText("menu_lang","",locale,sizeof(locale));
    if(locale[0]) Port_OptionsFileSetLanguage(snapshot,locale);
    int bindings[PORT_PC_BINDINGS];PortInput_PcGet(bindings);Port_OptionsFileEncodeBindings(snapshot,bindings);
    int gamepad[PORT_GAMEPAD_BINDINGS];PortInput_GamepadGet(gamepad);Port_OptionsFileEncodeGamepad(snapshot,gamepad);
    PortInput_PlayerGet(1,bindings,gamepad);Port_OptionsFileEncodePlayer2(snapshot,bindings,gamepad);
    PortInput_PlayerGet(1,bindings,gamepad);Port_OptionsFileEncodePlayer2(snapshot,bindings,gamepad);
    if(valid && memcmp(snapshot,sOptionsSnapshot,sizeof(snapshot))) changed=1;
    memcpy(sOptionsSnapshot,snapshot,sizeof(snapshot));
    /* Remove a recognized legacy block before the native checksum is made. */
    if(Port_OptionsSaveDecode(save,old))
        memset((unsigned char *)save+PORT_OPTIONS_SAVE_OFFSET,0,PORT_OPTIONS_SAVE_BYTES);
    return changed;
}
extern "C" int Port_OptionsSaveCommit(void) {
    int values[PORT_OPTION_SETTING_COUNT];
    if(!Port_ZenkaiEnabled()) return 1;
    if(!Port_OptionsFileDecode(sOptionsSnapshot,values)) return 0;
    /* A language change can arrive after the last Options sync. */
    char locale[PORT_OPTIONS_LOCALE_BYTES];
    Port_SettingText("menu_lang","",locale,sizeof(locale));
    if(locale[0]) Port_OptionsFileSetLanguage(sOptionsSnapshot,locale);
    /* Keep a recent Visual Settings change even if this save reached commit
       without another render-thread sync of the native options. */
    const int promptRow=PORT_VISUAL_OPTION_OFFSET+2;
    unsigned char *zop=sOptionsSnapshot+8;
    int prompt=Port_Setting("language_prompt_always",1)!=0;
    zop[8+2*promptRow]=(unsigned char)(zop[3]=='1' ? !prompt:prompt);
    zop[9+2*promptRow]=0;
    sOptionsSnapshot[7]=PORT_OPTIONS_PROMPT_SET|(prompt ? PORT_OPTIONS_PROMPT_ON:0);
    return Port_OptionsFileWrite(sOptionsSnapshot);
}
extern "C" int Port_OptionsVideoGet(int row) {
    if(row<0 || row>=PORT_OPTION_SETTING_COUNT) return 0;
    SDL_LockSpinlock(&sNativeVideoLock);
    int value=sNativeVideoPending[row]>=0 ? sNativeVideoPending[row]:sNativeVideoCurrent[row];
    SDL_UnlockSpinlock(&sNativeVideoLock);
    return Port_VideoOptionIndex(row,value);
}
extern "C" void Port_OptionsVideoChange(int row,int direction) {
    if(row<0 || row>=PORT_OPTION_SETTING_COUNT || !direction || !Port_OptionsAvailable(row)) return;
    SDL_LockSpinlock(&sNativeVideoLock);
    const PortVideoOption &option=*Port_OptionsDescriptor(row);
    int value=sNativeVideoPending[row]>=0 ? sNativeVideoPending[row]:sNativeVideoCurrent[row];
    int index=Port_VideoOptionIndex(row,value);
    index=(index+(direction>0 ? 1:option.count-1))%option.count;
    sNativeVideoPending[row]=option.values[index];
    SDL_UnlockSpinlock(&sNativeVideoLock);
}
extern "C" void Port_OptionsVideoDefaults(void) {
    SDL_LockSpinlock(&sNativeVideoLock);
    for(int row=0;row<PORT_VIDEO_OPTION_COUNT;row++) sNativeVideoPending[row]=sPortVideoOptions[row].defaultValue;
    sNativeVideoPending[PORT_FPS_OPTION_ROW]=0;
    SDL_UnlockSpinlock(&sNativeVideoLock);
}
extern "C" void Port_OptionsBattleEffectsDefaults(void) {
    SDL_LockSpinlock(&sNativeVideoLock);
    for(int row=0;row<PORT_BATTLE_EFFECT_COUNT;row++)
        sNativeVideoPending[PORT_VIDEO_OPTION_COUNT+row]=sPortBattleEffects[row].defaultValue;
    SDL_UnlockSpinlock(&sNativeVideoLock);
}
/* Window/device changes and settings-file writes belong to the render thread. */
static void native_video_apply(void) {
    PortVideo video;
    __atomic_store_n(&sNativeJapaneseAvailable,PortSongs_SoundtrackAvailable(),__ATOMIC_RELEASE);
    __atomic_store_n(&sNativeWiiAvailable,Port_WiiArtAvailable(),__ATOMIC_RELEASE);
    SDL_LockSpinlock(&sNativeVideoLock);
    GsGpu_GetSettings(&video);
    bool changed=false,extraChanged=false;
    if(Port_OptionsAvailable(20) && Port_Setting("dragon_net_mode",1)==0) {
        Port_SettingSave("dragon_net_mode",1);extraChanged=true;
    }
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        int value=sNativeVideoPending[row];
        if(value<0) continue;
        if(row>=PORT_VISUAL_OPTION_OFFSET) {
            if(!Port_OptionsAvailable(row)) continue;
            switch(row) {
            case 16: Port_SettingSave("skip_logos",!value);Port_SettingSave("skip_copyright",1);break;
            case 17: Port_SettingSave("skip_intro",!value);break;
            case 18: Port_SettingSave("language_prompt_always",value);break;
            case 19: Port_SettingSave("game_version",value);break;
            case 20: Port_SettingSave("dragon_net_mode",value);break;
            case 21: Port_SettingSave("save_quick",value);break;
            case PORT_FPS_OPTION_ROW: Port_SettingSave("show_fps",value);break;
            }
            extraChanged=true;continue;
        }
        if(row>=PORT_VIDEO_OPTION_COUNT) {
            int effect=row-PORT_VIDEO_OPTION_COUNT,bit=sPortBattleEffectBits[effect];
            if(bit) video.fxOff=(video.fxOff & ~bit) | (value ? 0:bit);
            else video.sharpBlur=value;
            changed=true;continue;
        }
        switch(row) {
        case 0: video.scale=value;break;
        case 1: video.aspectMilli=value;break;
        case 2: video.smooth2d=value;break;
        case 3: video.fullscreen=value;break;
        case 4: video.vsync=value;break;
        case 5: video.frameSkip=value;break;
        case 6: Port_SettingSave("gpu_api",value);break;
        case 7: gsInterp=value;Port_SettingSave("interp",value);break;
        }
        if(row==6 || row==7) extraChanged=true;
        else changed=true;
    }
    if(changed) GsGpu_SetSettings(&video);
    else if(extraChanged) Port_SettingsWrite();
    const int current[]={video.scale,video.aspectMilli,video.smooth2d,video.fullscreen,video.vsync,
                         video.frameSkip,Port_Setting("gpu_api",0)==1 ? 1:0,gsInterp<0 ? 0:gsInterp>3 ? 3:gsInterp};
    for(int row=0;row<PORT_OPTION_SETTING_COUNT;row++) {
        if(row<PORT_VIDEO_OPTION_COUNT) sNativeVideoCurrent[row]=current[row];
        else if(row>=PORT_VISUAL_OPTION_OFFSET) {
            const int values[]={!Port_Setting("skip_logos",0),
                !Port_Setting("skip_intro",0),Port_Setting("language_prompt_always",1),
                Port_Setting("game_version",0),Port_Setting("dragon_net_mode",1),Port_Setting("save_quick",0),Port_Setting("show_fps",0)};
            sNativeVideoCurrent[row]=values[row-PORT_VISUAL_OPTION_OFFSET];
        } else {
            int bit=sPortBattleEffectBits[row-PORT_VIDEO_OPTION_COUNT];
            sNativeVideoCurrent[row]=bit ? !(video.fxOff & bit):video.sharpBlur;
        }
        sNativeVideoPending[row]=-1;
    }
    sNativeVideoReady=1;
    SDL_UnlockSpinlock(&sNativeVideoLock);
}

static void video_tab(void) {
    for (int row = 0; row < PORT_VIDEO_OPTION_COUNT; row++) {
        const PortVideoOption *option = Port_OptionsDescriptor(row);
        const char *values[9];
        int selected = Port_OptionsVideoGet(row);
        for (int i = 0; i < option->count; i++) {
            values[i] = option_text(option->nameIds[i], option->names[i]);
        }
        if (selected < 0 || selected >= option->count) selected = 0;
        ImGui::PushID(row);
        ImGui::SetNextItemWidth(-1.0f);
        if (ui_combo(option_text(option->labelId, option->label), &selected, values, option->count)) {
            int current = Port_OptionsVideoGet(row);
            int steps = (selected - current + option->count) % option->count;
            while (steps-- > 0) Port_OptionsVideoChange(row, 1);
        }
        ImGui::SetItemTooltip("%s", option_text(option->helpId, option->help));
        ImGui::PopID();
    }
}

static void effects_tab(PortVideo &v) {
    static const struct { const char *id, *name, *tip; } fx[7] = {
        {"character-outlines", "Outline", "The black line around the fighters."},
        {"occlusion-tint", "See-through tint", "The tint that shows a fighter behind scenery."},
        {"depth-haze", "Depth tint", "The haze that colours distant things."},
        {"bloom", "Glare and glow", "The bloom around bright things and the sky's glare."},
        {"depth-of-field", "Distance blur", "The soft focus on distant scenery."},
        {"motion-blur", "Movement blur", "The blur of the scenery while the view turns or moves fast."},
        {"weather", "Weather", "The haze of dust or mist that drifts over some stages (Desert and Rocky Area at noon, Rocky Area\n"
                    "in the evening, Kami's Lookout; Glacier, Muscle Tower and the Time Chamber have a little of it too).\n"
                    "A faint tint, made of big soft sprites over the whole picture: off, those stages run as fast as the\n"
                    "others (a fifth faster at 3x)."},
    };
    for (int i = 0; i < 7; i++) {
        bool on = !((v.fxOff >> i) & 1);
        char id[64];
        snprintf(id, sizeof(id), "options/setting/%s", fx[i].id);
        if (ui_checkbox(option_text(id, fx[i].name), &on)) { v.fxOff = (v.fxOff & ~(1 << i)) | (on ? 0 : 1 << i); }
        if (i == 6 && on) { /* Weather on: how much of it (fx_off bit 128: half the sprites) */
            static const char *const amount[2] = {"Full", "Light"};
            int light = (v.fxOff >> 7) & 1;
            ImGui::Indent();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
            if (ui_combo("Weather amount", &light, amount, 2)) { v.fxOff = (v.fxOff & ~128) | (light ? 128 : 0); }
            ImGui::Unindent();
        }
    }
    ImGui::Spacing();
    ui_slider_int("Glow strength", &v.glow, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ui_slider_int("Glare strength", &v.glare, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    bool sharp = v.sharpBlur != 0;
    if (ui_checkbox(option_text("options/setting/sharp-stage-blur", "Sharp stage blur"), &sharp)) { v.sharpBlur = sharp; }
}

// ---- "an update is available" ----
// Once per run the newest published release's number is asked of GitHub (net_match.c), unless "Check for updates" is
// off. If it is newer than this program: a line over the picture for ten seconds, and a line with a button to the
// release page at the top of this window for the rest of the run. Nothing is downloaded by the game.
// Not when nobody is looking (the offscreen and dummy video drivers); ZENKAIPOWER_UPDATE_CHECK=1 / 0 says otherwise, and
// ZENKAIPOWER_UPDATE_LATEST=<number> is taken as the answer without asking (testing).
static int sUpdCheck = -1; // the setting; -1 not read yet
static bool sUpdAsked, sUpdTold;

static int update_latest(void) {
    const char *fake = SDL_getenv("ZENKAIPOWER_UPDATE_LATEST");
    if (sUpdCheck < 0) {
        sUpdCheck = Port_Setting("update_check", 0) != 0;
    }
    if (fake != NULL) {
        return SDL_atoi(fake);
    }
    if (!sUpdAsked) {
        const char *drv = SDL_GetCurrentVideoDriver(), *env = SDL_getenv("ZENKAIPOWER_UPDATE_CHECK");
        bool seen = drv == NULL || (SDL_strcmp(drv, "offscreen") != 0 && SDL_strcmp(drv, "dummy") != 0);
        sUpdAsked = true;
        if (env != NULL ? SDL_atoi(env) != 0 : (sUpdCheck != 0 && seen)) {
            Match_UpdateAsk();
        }
    }
    return sUpdCheck != 0 || SDL_getenv("ZENKAIPOWER_UPDATE_CHECK") != NULL ? Match_UpdateLatest() : 0;
}

static void misc_tab(void) {
    {
        const char *const modes[] = {
            option_text("options/value/online", "Online"),
            option_text("options/value/lan", "LAN")
        };
        int mode = Port_Setting("dragon_net_mode", 1)==2 ? 1:0;
        ImGui::BeginDisabled(!Port_OptionsAvailable(20));
        if (ui_combo(option_text("options/setting/network-mode", "Network Mode"), &mode, modes, 2)) {
            Port_SettingSave("dragon_net_mode", mode+1);
            Port_SettingsWrite();
        }
        if (mode == 1) {
            char address[128];
            Port_SettingText("lan_address", "", address, sizeof(address));
            if (ImGui::InputText("LAN host address", address, sizeof(address))) {
                Port_SettingTextSave("lan_address", address);
                Port_SettingsWrite();
            }
            ImGui::TextDisabled("Leave empty to host; enter the host IP to join.");
        }
        ImGui::EndDisabled();
        ImGui::Separator();
    }
    if (PortSongs_SoundtrackAvailable()) {
        const char *const versions[] = {
            option_text("options/value/sparking-meteor", "Sparking! METEOR"),
            option_text("options/value/budokai-tenkaichi-3", "Budokai Tenkaichi 3")
        };
        int version = Port_Setting("game_version", 0) == 0 ? 0 : 1;
        if (ui_combo(option_text("options/setting/game-edition", "Game Edition"), &version, versions, 2)) {
            Port_SettingSave("game_version", version);
            Port_SettingsWrite();
        }
        ImGui::Separator();
    }
    bool zenkaiPower = Port_Setting("zenkai_power", 1) != 0;
    if (ui_checkbox("ZENKAI Power!", &zenkaiPower)) {
        Port_SettingSave("zenkai_power", zenkaiPower);
        Port_SettingsWrite();
    }
    ImGui::TextDisabled("Applies when starting or loading a game. Uses a separate save.");
    bool autoSave = Port_Setting("save_quick", 0) != 0;
    if (ui_checkbox(option_text("options/setting/autosave", "Autosave"), &autoSave)) {
        Port_SettingSave("save_quick", autoSave);
        Port_SettingsWrite();
    }
    // the start of the game (PR #50): read by FirstRun_Main, so they count from the next start
    int sSkipLogos = Port_Setting("skip_logos", 0) != 0;
    int sSkipIntro = Port_Setting("skip_intro", 0) != 0;
    bool logos = sSkipLogos == 0, intro = sSkipIntro == 0;
    ImGui::Spacing();
    if (ui_checkbox(option_text("options/setting/startup-logos", "Startup Logos"), &logos)) {
        sSkipLogos = !logos;
        Port_SettingSave("skip_logos", sSkipLogos);
        Port_SettingsWrite();
    }
    if (ui_checkbox(option_text("options/setting/intro-movie", "Intro Movie"), &intro)) {
        sSkipIntro = !intro;
        Port_SettingSave("skip_intro", sSkipIntro);
        Port_SettingsWrite();
    }
    bool hidePointer = Port_Setting("hide_pointer", 0) != 0;
    if (ui_checkbox("Hide the mouse pointer during play", &hidePointer)) {
        Port_SettingSave("hide_pointer", hidePointer);
        Port_SettingsWrite();
    }
}

static void ui_tab(PortVideo &v) {
#ifndef __ANDROID__
    bool nativeKeyboard = Port_Setting("native_keyboard", 1) != 0;
    if (ui_checkbox("Native Keyboard (Work in Progress)", &nativeKeyboard)) {
        Port_SettingSave("native_keyboard", nativeKeyboard);
        Port_SettingsWrite();
    }
#endif
    bool mouseSupport = Port_Setting("mouse_support", 1) != 0;
    if (ui_checkbox("Mouse Support (Work in Progress)", &mouseSupport)) {
        Port_SettingSave("mouse_support", mouseSupport);
        Port_SettingsWrite();
    }
    if (sLanguageSelectionAvailable) {
        bool overridden = getenv("ZENKAIPOWER_MENU_LANG") != NULL && getenv("ZENKAIPOWER_MENU_LANG")[0] != '\0';
        ImGui::BeginDisabled(overridden || Port_NetActive());
        if (ui_combo_begin(option_text("options/setting/language-selection", "Language"), sLanguages[sLanguageChoice].name)) {
            for (int i = 0; i < sLanguageCount; i++) {
                if (ui_selectable(sLanguages[i].name, i == sLanguageChoice) && i != sLanguageChoice) language_choose(i);
                if (i == sLanguageChoice) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        bool alwaysPrompt = Port_Setting("language_prompt_always", 1) != 0;
        if (ui_checkbox(option_text("options/help/language-selection", "Show language selector at startup"), &alwaysPrompt)) {
            Port_SettingSave("language_prompt_always", alwaysPrompt);
            Port_SettingsWrite();
        }
        if (sLanguageRestartPending) {
            ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.35f, 1.0f),
                "%s", option_text("options/dialog/language-restart", "Language saved. It will apply on the next launch."));
        }
        ImGui::Separator();
    }
    static const char *const kHud[] = {"At the screen's edges", "At the edges, long health bars", "In the middle, as in 4:3"};
    ui_combo("HUD Style", &v.hudLayout, kHud, 3);
    { // Text and artwork proportion
        bool characters43 = Port_TitleMainChars43() != 0;
        if (ui_checkbox("Menu art and dialogs in 4:3 (Work in Progress)", &characters43)) {
            Port_TitleMainChars43Set(characters43);
            Port_SettingSave("title_main_chars_43", characters43);
            Port_SettingsWrite();
        }
    }
    {
        bool automatic43 = Port_TitleMainChars43() != 0;
        bool m43 = automatic43 || Port_Menus43Configured() != 0;
        ImGui::BeginDisabled(automatic43);
        if (ui_checkbox("Menus in 4:3", &m43)) {
            Port_Menus43Set(m43);
            Port_SettingSave("menus_43", m43);
            Port_SettingsWrite();
        }
        ImGui::EndDisabled();
    }
}

extern "C" int gPortMusicFollows; // snd_adx.c: 1 = looping music is sped up or slowed down with the game

static void audio_tab(PortVideo &v) {
    ui_slider_int("Music", &v.music, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ui_slider_int("Sound effects", &v.effects, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ui_slider_int("Voices", &v.voices, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    ui_slider_int("Stage sounds", &v.stage, 0, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    {
        bool follows = gPortMusicFollows > 0;
        ImGui::Spacing();
        if (ui_checkbox("Music follows game speed", &follows)) {
            gPortMusicFollows = follows;
            Port_SettingSave("music_follows_speed", follows);
        }
    }
}

// Cheats. "Unlock everything" is the game's own leftover debug function (Save_UnlockAll, which nothing in the game
// calls): the request is set here and carried out on the game's side at the next vertical blank (headless.c).
extern "C" {
extern volatile int gPortUnlockAll, gPortUnlockDone;
extern volatile int gPortDragonBalls, gPortDragonBallsDone;
}

extern "C" int Port_GameSpeed(void);          // plat_stub.c: the player's own game speed, percent; never used online
extern "C" void Port_GameSpeedSet(int percent);
extern "C" void Port_AudioRefresh(void);

extern "C" int Port_OnePunchEnabled(void);
extern "C" void Port_OnePunchSet(int enabled);
extern "C" int Port_SimFightDayEnabled(void);
extern "C" void Port_SimFightDaySet(int enabled);

extern "C" int Port_TournamentFinalsEnabled(void);
extern "C" void Port_TournamentFinalsSet(int enabled);

extern "C" int Port_StoryNextCueEnabled(void);
extern "C" void Port_StoryNextCueSet(int enabled);

static void cheats_tab(void) {
    if (ui_button("Get all 7 Dragon Balls")) {
        gPortDragonBallsDone = 0;
        gPortDragonBalls = 1;
    }
    if (gPortDragonBallsDone) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Done.");
    }
    ImGui::Separator();
    bool nextCue = Port_StoryNextCueEnabled() != 0;
    if (ui_checkbox("Dragon History: Automatic Progress", &nextCue)) {
        Port_StoryNextCueSet(nextCue);
    }
    bool onePunch = Port_OnePunchEnabled() != 0;
    if (ui_checkbox("One Punch-Man", &onePunch)) {
        Port_OnePunchSet(onePunch);
    }
    bool fightDay = Port_SimFightDayEnabled() != 0;
    if (ui_checkbox("Sim Dragon: Always fight day", &fightDay)) {
        Port_SimFightDaySet(fightDay);
    }
    bool finals = Port_TournamentFinalsEnabled() != 0;
    if (ui_checkbox("Tournament: Start in finals", &finals)) {
        Port_TournamentFinalsSet(finals);
    }
    ImGui::Separator();
    {
        int speed = Port_GameSpeed();
        if (ui_slider_int("Game speed", &speed, 50, 200, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
            Port_GameSpeedSet(speed);
            Port_AudioRefresh();
        }
        ImGui::SameLine();
        if (ui_button("Reset")) {
            Port_GameSpeedSet(100);
            Port_AudioRefresh();
        }
        ImGui::Separator();
        ImGui::Spacing();
    }
    ImGui::TextWrapped("Unlock everything: all characters, stages, music and items, and the largest amount of Zenni. "
                       "This is a debug function the game's developers left in.");
    ImGui::Spacing();
    ImGui::TextWrapped("It also empties your records list, and it becomes permanent the next time the game saves. "
                       "Use it after your save has been loaded (from the main menu on).");
    ImGui::Spacing();
    if (ui_button("Unlock everything...")) {
        ImGui::OpenPopup("Unlock everything?");
    }
    if (gPortUnlockDone) {
        ImGui::SameLine();
        ImGui::TextUnformatted("Done.");
    }
    if (ImGui::BeginPopupModal("Unlock everything?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Your records list will be emptied. This cannot be undone once the game saves.");
        ImGui::Spacing();
        if (ui_button("Unlock", ImVec2(U(140.0f), 0.0f))) {
            gPortUnlockAll = 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui_button("Cancel", ImVec2(U(140.0f), 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// The Controls tab's part for the sticks: each stick of the player's controller as it is now (the dot), its deadzone
// (the shaded middle: a circle, or the game's own square), what the game is given for it (the ring, and in numbers),
// the triggers, and the settings for these and for vibration. For seeing a stick that drifts and choosing a deadzone
// that covers it.
static void sticks_panel(int player) {
    const char *const kStick[2] = {option_text("controls/stick/left", "Left stick"), option_text("controls/stick/right", "Right stick")};
    const char *const kTrigger[2] = {option_text("controls/trigger/left", "Left trigger"), option_text("controls/trigger/right", "Right trigger")};
    static const char *const kDir[5] = {"", "up", "down", "left", "right"};
    const float R = U(42.0f), box = 2.0f * R + U(8.0f);
    const ImU32 colZone = ImGui::GetColorU32(ImGuiCol_Text, 0.22f);
    const ImU32 colRaw = ImGui::GetColorU32(ImGuiCol_Text), colGame = ImGui::GetColorU32(ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
    int *dead = PortInput_Deadzone(player), *vib = PortInput_Vibration(player);
    PortStickView view;
    char label[64];

    PortInput_StickView(player, &view);
    if (!ImGui::BeginTable("sticks", 2)) {
        return;
    }
    float top = 0.0f;
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        if (s == 0) {
            top = ImGui::GetCursorScreenPos().y;
        } else { // (the second cell would start lower by the text offset of the first one's last line)
            ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        }
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 at = ImGui::GetCursorScreenPos(), c(at.x + box * 0.5f, at.y + box * 0.5f);
        ImGui::Dummy(ImVec2(box, box));
        dl->AddCircleFilled(c, R, ImGui::GetColorU32(ImGuiCol_FrameBg), 48);
        if (dead[s] < 0) { // the game's own: each axis by itself, 50 of 127.5
            float h = R * 50.0f / 127.5f;
            dl->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), colZone);
        } else if (dead[s] > 0) {
            dl->AddCircleFilled(c, R * (float)dead[s] / 100.0f, colZone, 48);
        }
        dl->AddLine(ImVec2(c.x - R, c.y), ImVec2(c.x + R, c.y), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddLine(ImVec2(c.x, c.y - R), ImVec2(c.x, c.y + R), ImGui::GetColorU32(ImGuiCol_Border));
        dl->AddCircle(c, R, ImGui::GetColorU32(ImGuiCol_Border), 48);
        if (view.connected) {
            dl->AddCircle(ImVec2(c.x + view.game[s][0] * R, c.y + view.game[s][1] * R), 5.5f, colGame, 16, 1.5f);
            dl->AddCircleFilled(ImVec2(c.x + view.raw[s][0] * R, c.y + view.raw[s][1] * R), 3.0f, colRaw, 12);
        }
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, top));
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding(); // (in both cells: the second one's lines start that much lower anyway)
        if (view.connected) {
            ImGui::Text(view.dir[s] != 0 ? "%s: %s" : "%s", kStick[s], kDir[view.dir[s]]);
            ImGui::Text("stick  %+.2f %+.2f", view.raw[s][0], view.raw[s][1]);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colGame), "game   %+.2f %+.2f", view.game[s][0], view.game[s][1]);
        } else {
            ImGui::TextUnformatted(kStick[s]);
            ImGui::TextDisabled("%s", option_text("controls/no-controller", "no controller"));
            ImGui::TextDisabled(" ");
        }
        ImGui::SetNextItemWidth(U(100.0f));
        snprintf(label, sizeof(label), "%s##%d", option_text("controls/deadzone", "Deadzone"), s);
        ui_slider_int(label, &dead[s], -1, PORT_DEADZONE_MAX,
                      dead[s] < 0 ? option_text("controls/deadzone/original", "Original") : "%d%%", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            PortInput_Save();
        }
        ImGui::EndGroup();
    }
    for (int s = 0; s < 2; s++) {
        ImGui::TableNextColumn();
        if (view.triggerHeld[s]) {
            snprintf(label, sizeof(label), "%d%% %s", (int)(view.trigger[s] * 100.0f + 0.5f),
                     option_text("controls/trigger/pressed", "pressed"));
        } else {
            snprintf(label, sizeof(label), "%d%%", (int)(view.trigger[s] * 100.0f + 0.5f));
        }
        ui_progress(view.trigger[s], ImVec2(U(144.0f), 0.0f), label);
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(kTrigger[s]);
    }
    ImGui::EndTable();
    bool on = vib[0] != 0;
    if (ui_checkbox(option_text("controls/vibration", "Vibration"), &on)) {
        vib[0] = on;
        PortInput_Save();
    }
    ImGui::BeginDisabled(!on);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(U(160.0f));
    ui_slider_int(option_text("controls/strength", "Strength"), &vib[1], 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        PortInput_Save();
    }
    ImGui::SameLine();
    if (ui_button(option_text("controls/test", "Test"))) {
        PortInput_VibrationTest(player);
    }
    ImGui::EndDisabled();
    if (view.connected && !view.canVibrate) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", option_text("controls/no-vibration", "(this controller has none)"));
    }
}

static const char *control_action_label(int action) {
    static const char *const ids[PORT_ACTIONS] = {
        "up", "down", "left", "right", "cross", "circle", "square", "triangle",
        "l1", "r1", "l2", "r2", "l3", "r3", "start", "select",
        "left-stick-up", "left-stick-down", "left-stick-left", "left-stick-right",
        "right-stick-up", "right-stick-down", "right-stick-left", "right-stick-right"
    };
    char id[64];
    if (action < 0 || action >= PORT_ACTIONS) return PortInput_ActionLabel(action);
    snprintf(id, sizeof(id), "controls/action/%s", ids[action]);
    return option_text(id, PortInput_ActionLabel(action));
}

static void controls_tab(void) {
    static int player;
    char label[96];
    int count = 0;

    ui_radio(option_text("controls/player/1", "Player 1"), &player, 0);
    ImGui::SameLine();
    ui_radio(option_text("controls/player/2", "Player 2"), &player, 1);
    int *slot = PortInput_PadSlot(player), *keys = PortInput_Keys(player), *pads = PortInput_PadButtons(player);
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    if (*slot < 0) {
        snprintf(label, sizeof(label), "None");
    } else if (*slot < count) {
        snprintf(label, sizeof(label), "%d: %s", *slot + 1, SDL_GetGamepadNameForID(ids[*slot]));
    } else {
        snprintf(label, sizeof(label), option_text("controls/device/not-connected", "Controller %d (not connected)"), *slot + 1);
    }
    ImGui::SetNextItemWidth(U(300.0f));
    if (ui_combo_begin(option_text("controls/device/controller", "Controller"), label)) {
        if (ui_selectable(option_text("controls/device/none", "None"), *slot < 0)) { *slot = -1; PortInput_Save(); }
        for (int i = 0; i < (count > 4 ? count : 4); i++) {
            if (i < count) {
                snprintf(label, sizeof(label), "%d: %s", i + 1, SDL_GetGamepadNameForID(ids[i]));
            } else {
                snprintf(label, sizeof(label), option_text("controls/device/not-connected", "Controller %d (not connected)"), i + 1);
            }
            if (ui_selectable(label, *slot == i)) { *slot = i; PortInput_Save(); }
        }
        ImGui::EndCombo();
    }
    SDL_free(ids);
    ImGui::SameLine();
    if (ui_button(option_text("controls/reset-defaults", "Reset to defaults"))) {
        PortInput_ResetDefaults(player);
        PortInput_Save();
    }
    if (ui_header(option_text("controls/sticks-header", "Sticks, triggers and vibration"), ImGuiTreeNodeFlags_DefaultOpen)) {
        sticks_panel(player);
    }
    ImGui::TextDisabled("%s", option_text("controls/instructions/keyboard", "Choose a binding, then press the new key or button. Esc keeps it, Delete clears it;"));
    ImGui::TextDisabled("%s", option_text("controls/instructions/gamepad", "with a controller, press nothing for five seconds to keep it."));
    // (never lower than a few rows: in a small window the tab scrolls as a whole instead)
    if (ImGui::BeginTable("bindings", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y > U(140.0f) ? ImGui::GetContentRegionAvail().y : U(140.0f)))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(option_text("controls/column/action", "Action"));
        ImGui::TableSetupColumn(option_text("controls/column/mouse-keyboard", "Mouse/Keyboard"));
        ImGui::TableSetupColumn(option_text("controls/column/gamepad", "Gamepad"));
        ImGui::TableHeadersRow();
        {   // what opens and closes this window (not a player's: one for the program)
            const int a = PORT_ACTIONS;
            bool waitKey = sCapKind == 1 && sCapAction == a, waitPad = sCapKind == 2 && sCapAction == a;
            int key = *PortInput_OverlayKey(), pad = *PortInput_OverlayPad();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(option_text("controls/action/open-settings", "Open this window"));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? option_text("controls/capture/keyboard", "press a key...") : key > 0 ? SDL_GetScancodeName((SDL_Scancode)key) : "-", a);
            if (ui_button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##p%d", waitPad ? option_text("controls/capture/gamepad", "press a button...") : pad >= 0 ? pad_source_name(pad) : option_text("controls/capture/start-select", "START + SELECT"), a);
            if (ui_button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(2, player, a);
            }
        }
        {   // what opens the online chat (not a player's: one for the program)
            const int a = PORT_ACTIONS + 1;
            bool waitKey = sCapKind == 1 && sCapAction == a;
            int key = *PortInput_ChatKey();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(option_text("controls/action/open-chat", "Open online chat"));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? option_text("controls/capture/keyboard", "press a key...") : key > 0 ? SDL_GetScancodeName((SDL_Scancode)key) : "Tab", a);
            if (ui_button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
        }
        for (int a = 0; a < PORT_ACTIONS; a++) {
            bool waitKey = sCapKind == 1 && sCapPlayer == player && sCapAction == a;
            bool waitPad = sCapKind == 2 && sCapPlayer == player && sCapAction == a;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(control_action_label(a));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "%s##k%d", waitKey ? option_text("controls/capture/keyboard", "press a key...") : keys[a] > 0 ? SDL_GetScancodeName((SDL_Scancode)keys[a]) : "-", a);
            if (ui_button(label, ImVec2(-FLT_MIN, 0.0f))) {
                capture(1, player, a);
            }
            ImGui::TableNextColumn();
            if (a < PORT_PAD_BUTTONS) {
                snprintf(label, sizeof(label), "%s##p%d", waitPad ? option_text("controls/capture/gamepad", "press a button...") : pad_source_name(pads[a]), a);
                if (ui_button(label, ImVec2(-FLT_MIN, 0.0f))) {
                    capture(2, player, a);
                }
            } else {
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(a < 20 ? "Left stick" : "Right stick");
            }
        }
        ImGui::EndTable();
    }
}

// Metadata is generated from game-info.json, alongside the executable like the server package.
static const char *game_info_field(int index) {
    static bool loaded;
    static char values[5][1024];
    static const char *const keys[] = {"title", "subtitle", "version", "source", "original_source"};
    if (!loaded) {
        loaded = true;
        for (int i = 0; i < 5; i++) {
            char path[4096];
            FILE *file = NULL;
            const char *base = SDL_GetBasePath();
            if (base) {
                snprintf(path, sizeof(path), "%sgame-info/%s.txt", base, keys[i]);
                file = Port_PackFopen(path, "rb");
            }
            if (!file) {
                snprintf(path, sizeof(path), "./game-info/%s.txt", keys[i]);
                file = Port_PackFopen(path, "rb");
            }
            if (file) {
                size_t n = fread(values[i], 1, sizeof(values[i]) - 1, file);
                values[i][n] = 0;
                fclose(file);
            }
        }
    }
    return values[index];
}

extern "C" const char *Ui_GameTitle(void) {
    const char *title = game_info_field(0);
    return title[0] ? title : "ZENKAI Power!";
}

static void game_info_footer(void) {
    if (!game_info_field(0)[0]) return;
    ImGui::TextDisabled("%s", game_info_field(0));
    ImGui::TextDisabled("%s%s%s", game_info_field(1),
                        game_info_field(1)[0] && game_info_field(2)[0] ? "  " : "", game_info_field(2));
    if (game_info_field(3)[0]) ImGui::TextLinkOpenURL("Source", game_info_field(3));
    if (game_info_field(4)[0]) {
        if (game_info_field(3)[0]) ImGui::SameLine();
        ImGui::TextLinkOpenURL("Original source", game_info_field(4));
    }
}

static void build(void) {
    PortVideo v, was;
    ImGuiIO &io = ImGui::GetIO();
    bool open = true;

    GsGpu_GetSettings(&v);
    was = v;
    if (sCapKind == 2 && SDL_GetTicks() > sCapUntil) { // nothing was pressed: the binding stays
        sCapKind = 0;
        gPortInputCapture = 0;
    }
    if (pad_pressed(ImGuiKey_GamepadFaceRight)) { // Circle / B: out of the window
        open = false;
    }
    if (sForceTab < 0 && (pad_pressed(ImGuiKey_GamepadL1) || pad_pressed(ImGuiKey_GamepadR1))) {
        const int withCheats[] = {0, 5};
        const int withoutCheats[] = {0};
        const int *visibleTabs = Port_CheatsEnabled() ? withCheats : withoutCheats;
        int count = Port_CheatsEnabled() ? 2 : 1;
        int current = 0;
        for (int i = 0; i < count; i++) if (visibleTabs[i] == sTab) current = i;
        sForceTab = visibleTabs[(current + (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) ? count - 1 : 1)) % count];
        sPadTab = true;
    }
    float reveal = sSettingsTransition.amount();
    if (sBuildingSettingsWindow) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 255));
    } else {
        ImVec4 dim(0.0f, 0.0f, 0.0f, 0.45f * reveal);
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, ImGui::ColorConvertFloat4ToU32(dim));
    }
    if (sSettingsFont) ImGui::PushFont(sSettingsFont, 18.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.13f, 0.28f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
    float sw, sh;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;
    if (sBuildingSettingsWindow) {
        // The SDL window is the frame: fill its client area with the settings panel and use its native title bar.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        sw = io.DisplaySize.x;
        sh = io.DisplaySize.y;
        ImGui::SetNextWindowSize(ImVec2(sw, sh), ImGuiCond_Always);
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    } else {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        sw = U(640.0f);
        sh = io.DisplaySize.y >= U(800.0f) ? U(760.0f) : U(600.0f);
        if (sw > io.DisplaySize.x * 0.96f) sw = io.DisplaySize.x * 0.96f;
        if (sh > io.DisplaySize.y * 0.94f) sh = io.DisplaySize.y * 0.94f;
        float folded = sh < U(64.0f) ? sh : U(64.0f);
        ImGui::SetNextWindowSize(ImVec2(sw * (0.85f + 0.15f * reveal), folded + (sh - folded) * reveal), ImGuiCond_Always);
    }
    if (sSettingsTransition.progress < 1.0f || sSettingsTransition.closing()) flags |= ImGuiWindowFlags_NoInputs;
    else if (sSettingsFocus) {
        ImGui::SetNextWindowFocus();
        sSettingsFocus = false;
    }
    bool drawFrame = settings_frame_load();
    if (drawFrame) flags |= ImGuiWindowFlags_NoBackground;
    // Keep the in-game close button inside its ornamental frame; the detached window uses the OS title bar.
    float framePaddingX = sBuildingSettingsWindow ? ImGui::GetStyle().FramePadding.x : U(32.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(framePaddingX, ImGui::GetStyle().FramePadding.y));
    bool visible = ImGui::Begin("Settings", &open, flags);
    ImGui::PopStyleVar();
    if (drawFrame) {
        settings_frame(ImGui::GetBackgroundDrawList(), ImGui::GetWindowPos(), ImGui::GetWindowSize(), reveal);
    }
    if (visible) {
        {
            int latest = update_latest();
            if (latest > PORT_RELEASE) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Update available: 0.%d.%d (this is 0.%d.%d).", latest / 100, latest % 100,
                                   PORT_RELEASE / 100, PORT_RELEASE % 100);
                ImGui::SameLine();
                if (ui_button("Download page")) {
                    SDL_OpenURL("https://github.com/" PORT_UPDATE_REPO "/releases/latest");
                }
                ImGui::Separator();
            }
        }
        // the tabs in a region of their own, above a last line that is always there
        int footerLines = game_info_field(0)[0] ? 3 : 0;
        if ((io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0) footerLines++;
        float footerHeight = footerLines * ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("body", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_NavFlattened);
        sSettingsSkin = true;
        settings_tabs();
        {
            if (tab("Video Settings", 0)) { video_tab(); }
            if (Port_CheatsEnabled() && tab("Cheats", 5)) { cheats_tab(); }
        }
        sSettingsSkin = false;
        ImGui::EndChild();
        if (sPadTab) { // (a tab chosen with the shoulder buttons)
            sPadTab = false;
            sTab = sForceTab;
            sForceTab = -1;
        } else if (ImGui::GetFrameCount() > 3) {
            sForceTab = -1;
        }
        ImGui::Separator();
        game_info_footer();
        if ((io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0) {
            ImGui::TextDisabled("D-pad: move   Cross / A: choose   Circle / B: back   L1 / R1: tab");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(3);
    if (sSettingsFont) ImGui::PopFont();
    if (memcmp(&v, &was, sizeof(v)) != 0) {
        GsGpu_SetSettings(&v);
    }
    if (!open) {
        set_open(false);
    }
}

static bool sBuilt; // this frame has something to draw: the window, or the line over the picture

static void settings_window_frame(void) {
    if (!sSettingsWindow || !sSettingsContext || !sOpen) {
        if (sSettingsWindow) SDL_HideWindow(sSettingsWindow);
        ImGui::SetCurrentContext(sGameContext);
        return;
    }
    SDL_ShowWindow(sSettingsWindow);
    if (sGL && !SDL_GL_MakeCurrent(sSettingsWindow, sSettingsGL)) {
        ImGui::SetCurrentContext(sGameContext);
        return;
    }
    ImGui::SetCurrentContext(sSettingsContext);
    ImFont *gameSettingsFont = sSettingsFont;
    sSettingsFont = sSettingsWindowFont;
    if (sGL) ImGui_ImplOpenGL3_NewFrame();
    else ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    float gameUiScale = sUiScale;
    sUiScale = 0.0f;
    ui_scale_apply();
    sUiScale = gameUiScale;
    ImGui::NewFrame();
    sBuildingSettingsWindow = true;
    build();
    sBuildingSettingsWindow = false;
    ImGui::Render();
    sSettingsFont = gameSettingsFont;
    sSettingsDrawn = ImGui::GetDrawData()->CmdListsCount != 0;
    if (sGL) {
        if (sSettingsDrawn) ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(sSettingsWindow);
        SDL_GL_MakeCurrent(GsDraw_Window(), sGameGL);
    } else {
        SDL_GPUCommandBuffer *ext_cmd = SDL_AcquireGPUCommandBuffer(sDevice);
        SDL_GPUTexture *target = NULL;
        Uint32 width = 0, height = 0;
        SDL_GPURenderPass *pass;
        SDL_GPUColorTargetInfo ti;
        if (ext_cmd && SDL_WaitAndAcquireGPUSwapchainTexture(ext_cmd, sSettingsWindow, &target, &width, &height) &&
            target != NULL && sSettingsDrawn) {
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), ext_cmd);
            SDL_zero(ti);
            ti.texture = target;
            ti.load_op = SDL_GPU_LOADOP_CLEAR;
            ti.store_op = SDL_GPU_STOREOP_STORE;
            ti.clear_color.a = 1.0f;
            pass = SDL_BeginGPURenderPass(ext_cmd, &ti, 1, NULL);
            if (pass) {
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), ext_cmd, pass);
                SDL_EndGPURenderPass(pass);
            }
        }
        if (ext_cmd) SDL_SubmitGPUCommandBuffer(ext_cmd);
    }
    ImGui::SetCurrentContext(sGameContext);
}

void Ui_DrawAgain(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    ImDrawData *dd = sReady && sBuilt ? ImGui::GetDrawData() : NULL; // (sBuilt: Ui_Draw made a picture this frame)
    SDL_GPUColorTargetInfo ti;
    SDL_GPURenderPass *pass;

    if (dd == NULL || dd->CmdListsCount == 0 || dd->DisplaySize.x <= 0.0f || dd->DisplaySize.y <= 0.0f) {
        return;
    }
    ImGui_ImplSDLGPU3_PrepareDrawData(dd, cmd);
    SDL_zero(ti);
    ti.texture = target;
    ti.load_op = SDL_GPU_LOADOP_LOAD;
    ti.store_op = SDL_GPU_STOREOP_STORE;
    pass = SDL_BeginGPURenderPass(cmd, &ti, 1, NULL);
    ImGui_ImplSDLGPU3_RenderDrawData(dd, cmd, pass);
    SDL_EndGPURenderPass(pass);
}

static char sNotice[160];
static Uint64 sNoticeUntil;
static volatile int sNoticeNew;

extern "C" void Port_UiNotice(const char *text) {
    SDL_strlcpy(sNotice, text != NULL ? text : "", sizeof(sNotice));
    sNoticeNew = 1;
}

// Autosave uses the main menu Options animation in the bottom right corner of the picture
// while the game saves, at least a second, then it fades. Set from the game's thread: 1 saving, 0 saved, -1 the
// game's own window took over (an error: the icon goes at once).
static volatile int sSaving;
static Uint64 sSaveFrom, sSaveEnd; // (0: not shown; the end, 0 while it saves)
extern "C" void Port_UiSaving(int state) {
    sSaving = state;
}

static bool save_icon_on(Uint64 now) {
    int state = sSaving;
    if (state > 0 && (sSaveFrom == 0 || sSaveEnd != 0)) {
        sSaveFrom = now;
        sSaveEnd = 0;
    }
    if (sSaveFrom != 0 && sSaveEnd == 0 && (state <= 0 || now - sSaveFrom > 15000)) { // (or a flow left half-way)
        sSaveEnd = state < 0 || now > sSaveFrom + 1000 ? now : sSaveFrom + 1000;
        if (state == 0) {
            PortA11y_Say(PortA11y_Label("Saved"), 0);
        }
    }
    if (sSaveEnd != 0 && now >= sSaveEnd + 400) {
        sSaveFrom = 0;
    }
    return sSaveFrom != 0;
}

static void save_icon_draw(Uint64 now) {
    static SDL_GPUTexture *texture;
    static bool tried;
    if (!tried) {
        tried = true;
        int width, height;
        unsigned char *pixels = MenuLabels_SaveIcon(Port_FileRoot(), &width, &height);
        if (pixels) texture = Ui_Texture(pixels, (uint32_t)width, (uint32_t)height);
    }
    if (!texture) return;
    float h = (float)gUiPresentH * 0.055f * 1.5f;
    float margin = (float)gUiPresentH * 0.055f * 0.6f;
    float x1 = (float)(gUiPresentX + gUiPresentW) - margin;
    float y1 = (float)(gUiPresentY + gUiPresentH) - margin;
    // MainMenu_Draw: five cells, advancing every six ticks at 60Hz (100ms).
    int frame = (int)(((now - sSaveFrom) / 100) % 5);
    float alpha = sSaveEnd != 0 && now > sSaveEnd ? 1.0f - (float)(now - sSaveEnd) / 400.0f : 1.0f;
    alpha = alpha < 0.0f ? 0.0f : alpha;
    ImGui::GetForegroundDrawList()->AddImage(ImTextureRef((ImTextureID)(intptr_t)texture),
        ImVec2(x1 - h, y1 - h), ImVec2(x1, y1),
        ImVec2(frame / 5.0f, 0), ImVec2((frame + 1) / 5.0f, 1),
        IM_COL32(255, 255, 255, (int)(255.0f * alpha)));
}

// The meter: frames shown and the game's speed, and in an online match the connection. A setting (Video tab).
static bool meter_on(void) {
    if (sMeter < 0) {
        sMeter = getenv("ZENKAIPOWER_METER") != NULL ? atoi(getenv("ZENKAIPOWER_METER")) != 0 : Port_Setting("meter", 0) != 0;
    }
    return sMeter != 0;
}
static int sBattleFps=-1;
extern "C" int Port_BattleFps(void) {
    return __atomic_load_n(&sBattleFps,__ATOMIC_ACQUIRE);
}
static float sFps, sSpeed; // frames shown per second; the game's vertical blanks per second as a share of 60

static void meter_count(void) { // once per frame shown
    static Uint64 t0;
    static unsigned frames, blanks0;
    Uint64 now = SDL_GetTicks();
    frames++;
    if (t0 == 0) {
        t0 = now;
        blanks0 = gPortLiveBlanks;
    } else if (now - t0 >= 500) {
        sFps = (float)frames * 1000.0f / (float)(now - t0);
        __atomic_store_n(&sBattleFps,Port_Setting("show_fps",0) ? (int)(sFps+0.5f):-1,__ATOMIC_RELEASE);
        sSpeed = (float)(gPortLiveBlanks - blanks0) * 1000.0f / (float)(now - t0) / 60.0f * 100.0f;
        frames = 0;
        blanks0 = gPortLiveBlanks;
        t0 = now;
    }
}

static void meter_draw(void) {
    int n[10];
    ImGui::SetNextWindowPos(ImVec2(U(8.0f), U(8.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##meter", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
        // a fight shows 30 frames a second and the menus 60, as on the console; the speed is what should stay at 100%
        ImGui::Text("%.0f fps   speed %.0f%%", sFps, sSpeed);
        if (Port_NetStats(n)) {
            if (n[0] >= 0) {
                ImGui::Text("ping %d ms%s", n[0], Port_LobbyRelayed() ? " (relay)" : "");
            } else {
                ImGui::TextUnformatted("ping ...");
            }
            if (n[5] > 0) {
                ImGui::Text("rollbacks %d/s, %.1f frames", n[1], (float)n[2] / 10.0f);
            } else {
                ImGui::TextUnformatted("rollback off");
            }
            ImGui::Text("waited %d ms/s (%d times)", n[4], n[3]);
            if (n[8] == 1) { // gs/net.c wait_end: input packets came, without the frame this copy needed
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  the other player's game is behind");
            } else if (n[8] == 2) { // nothing came for most of the wait
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  nothing arrived from the other player");
            }
            ImGui::Text("delay %d%s, rollback up to %d", n[6], n[7] ? " (auto)" : "", n[5]);
            if (n[9] != 0) { // the desync check found the two games apart (the match goes on)
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "desync since frame %d", n[9]);
            }
        }
    }
    ImGui::End();
}

// The name of an added stage or song as text, in the rectangle the menu gives (the strip images' place): cream
// letters with a dark outline, as tall as the rectangle allows, centred for a stage and from the left for a song.
static void name_text(ImDrawList *dl, const char *text, float x, float y, float w, float h, bool centre) {
    ImFont *font = ImGui::GetFont();
    float size = h * (centre ? 0.42f : 0.72f);
    ImVec2 ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    if (ext.x > w * 0.96f && ext.x > 0.0f) { // a long name: smaller, to fit
        size *= w * 0.96f / ext.x;
        ext = font->CalcTextSizeA(size, 10000.0f, 0.0f, text);
    }
    float tx = centre ? x + (w - ext.x) * 0.5f : x + h * 0.15f, ty = y + (h - ext.y) * 0.5f, o = size * 0.07f + 1.0f;
    for (int k = 0; k < 8; k++) {
        static const float dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1}, dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        dl->AddText(font, size, ImVec2(tx + dx[k] * o, ty + dy[k] * o), IM_COL32(40, 20, 0, 255), text);
    }
    dl->AddText(font, size, ImVec2(tx, ty), IM_COL32(255, 236, 170, 255), text);
}

/* Names in the game's own lettering (namefont.c): a name with no picture in a strip is put together from the letters
   of the disc's name pictures, once, and kept as a texture while it is the one shown (a few are kept, for going up
   and down a list). */
struct FontName {
    SDL_GPUTexture *tex; // NULL: this name cannot be drawn that way (a character the disc's names do not have)
    int style, w, h, choice, resolution;
    char text[64];
    bool used;
};
static FontName sFontNames[12];
static unsigned sFontNext;

static const FontName *font_name(int style, const char *text) {
    int choice = MenuText_GetFont();
    if (choice == 0) {
        for (int i = 1; i < MenuText_FontCount(); i++) {
            if (MenuText_FontAvailable(i, MENU_TEXT_MENU)) { choice = i; break; }
        }
    }
    int resolution = choice ? 4 : 1; // (a font's strip: 4 times the disc's, for a window larger than the game's picture)
    if (text == NULL || text[0] == '\0' || (choice == 0 && !NameFont_Ready())) { // (not ready: asked again next frame)
        return NULL;
    }
    for (FontName &n : sFontNames) {
        if (n.used && n.style == style && n.choice == choice && n.resolution == resolution && strncmp(n.text, text, sizeof(n.text) - 1) == 0) {
            return n.tex != NULL ? &n : NULL;
        }
    }
    FontName &n = sFontNames[sFontNext++ % (sizeof(sFontNames) / sizeof(sFontNames[0]))];
    if (n.used && n.tex != NULL) {
        if (sGL) {
            GsGl_StripTextureFree((unsigned)(intptr_t)n.tex);
        } else {
            SDL_ReleaseGPUTexture(sDevice, n.tex);
        }
    }
    n.tex = NULL;
    n.w = n.h = 0;
    n.used = true;
    n.style = style;
    n.choice = choice; n.resolution = resolution;
    SDL_strlcpy(n.text, text, sizeof(n.text));
    unsigned char *pix;
    if (choice) {
        pix = style == NF_STAGE ? MenuText_RenderStage(n.text, resolution, &n.w, &n.h) :
                                  MenuText_RenderSong(n.text, style == NF_SONG_LIT, resolution, &n.w, &n.h);
    } else {
        pix = NameFont_Compose(style, n.text, &n.w, &n.h);
    }
    if (pix != NULL) {
        n.tex = make_strip(pix, (size_t)n.w * n.h * 4, (uint32_t)n.w, (uint32_t)n.h, 1); // (frees pix)
    }
    if (choice) { n.w /= resolution; n.h /= resolution; }
    return n.tex != NULL ? &n : NULL;
}

/* Draws such a name over the place of the disc's name picture: (x, y) is the corner of that picture's row in the
   window, kx and ky are window pixels per pixel of the row (512 wide). A name longer than the row is made smaller
   to fit, about the row's middle line; a stage name stays in the middle, a song name keeps its left edge. */
static void font_draw(ImDrawList *dl, const FontName *n, float x, float y, float kx, float ky, bool centre) {
    float k = n->w > 512 ? 512.0f / (float)n->w : 1.0f;
    float w = (float)n->w * kx * k, h = (float)n->h * ky * k;
    float x0 = centre ? x + (512.0f * kx - w) * 0.5f : x;
    float y0 = y + ((float)n->h * ky - h) * 0.5f;
    dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)n->tex), ImVec2(x0, y0), ImVec2(x0 + w, y0 + h));
}

// The online chat, in the lower left corner. Put away, it shows the newest lines for a few seconds each in a dark
// rounded card, fading out, and takes no input. While a line is typed it shows the last ones, input field and
// allows selecting/copying text with mouse.
enum { CHAT_SHOWN_MS = 8000, CHAT_FADE_MS = 2000 };

static bool chat_recent(void) {
    PortChatLine l;
    unsigned n = PortChat_Count();
    return n > 0 && PortChat_Line(n - 1, &l) && SDL_GetTicks() - l.atMs < CHAT_SHOWN_MS;
}

static void chat_draw(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (sChatSize < 0) {
        sChatSize = Port_Setting("chat_size", 1);
        if (sChatSize < 0 || sChatSize > 2) sChatSize = 1;
    }
    int rowsTyping = 6, rowsPassive = 4; // Normal: as the chat always was
    float wFactor = 0.42f, minW = 320.0f, maxW = 100000.0f;
    if (sChatSize == 0) { // Compact
        rowsTyping = 4;
        rowsPassive = 2;
        wFactor = 0.28f;
        minW = 240.0f;
        maxW = 380.0f;
    } else if (sChatSize == 2) { // Large
        rowsTyping = 8;
        rowsPassive = 6;
        wFactor = 0.52f;
        minW = 400.0f;
    }
    float w = io.DisplaySize.x * wFactor;
    if (w < U(minW)) w = U(minW);
    if (w > U(maxW)) w = U(maxW);
    unsigned n = PortChat_Count(), rows = sChat ? rowsTyping : rowsPassive, first = n > rows ? n - rows : 0;
    Uint64 now = SDL_GetTicks();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
    if (!sChat) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    // In the lower left corner (growing upwards) until it is dragged; then where it was left (off the screen: back).
    static ImVec2 sChatPos;
    static bool sChatMoved;
    if (sChatMoved && (sChatPos.x < 0.0f || sChatPos.y < 0.0f || sChatPos.x > io.DisplaySize.x - 50.0f ||
                       sChatPos.y > io.DisplaySize.y - 50.0f)) {
        sChatMoved = false;
    }
    if (!sChatMoved) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.02f, io.DisplaySize.y * 0.86f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    } else if (!sChat) {
        ImGui::SetNextWindowPos(sChatPos, ImGuiCond_Always);
    }

    float bgAlpha = 0.85f;
    if (!sChat) {
        float newestAlpha = 1.0f;
        PortChatLine lastLine;
        if (n > 0 && PortChat_Line(n - 1, &lastLine)) {
            Uint64 age = now - lastLine.atMs;
            if (age >= CHAT_SHOWN_MS) {
                return;
            }
            if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                newestAlpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
            }
        }
        bgAlpha = 0.65f * newestAlpha;
    }
    ImGui::SetNextWindowBgAlpha(bgAlpha);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(8.0f), U(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, sChat ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(U(4.0f), U(2.0f)));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.75f, 0.2f, 0.5f));

    if (sChat && sChatFocus) {
        ImGui::SetNextWindowFocus();
    }
    if (ImGui::Begin("##chat", NULL, flags)) {
        if (sChat && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            sChatMoved = true;
        }
        if (sChat && sChatMoved) {
            sChatPos = ImGui::GetWindowPos();
        }
        const char *names[2] = {Port_NetName(Port_NetMe()), Port_NetName(!Port_NetMe())};
        if (sChat) {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.28f, 1.0f), "Chat (Tab)");
            ImGui::SameLine();
            ImGui::TextDisabled("::");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Drag anywhere to move the chat window. Click a line to copy it.");
            }
            ImGui::SameLine(w - 70.0f);
            bool mute = PortChat_Muted() != 0;
            if (ui_checkbox("Mute", &mute)) {
                PortChat_MutedSet(mute);
                sChatFocus = true;
            }
            ImGui::Separator();
        }

        for (unsigned i = first; i < n; i++) {
            PortChatLine l;
            float alpha = 1.0f;
            if (!PortChat_Line(i, &l)) {
                continue;
            }
            if (!sChat) {
                Uint64 age = now - l.atMs;
                if (age >= CHAT_SHOWN_MS) {
                    continue;
                }
                if (age > CHAT_SHOWN_MS - CHAT_FADE_MS) {
                    alpha = (float)(CHAT_SHOWN_MS - age) / (float)CHAT_FADE_MS;
                }
            }
            const char *sender = l.sender[0] != '\0' ? l.sender : (names[l.mine ? 0 : 1][0] != '\0' ? names[l.mine ? 0 : 1] : (l.mine ? "You" : "Rival"));
            ImGui::PushID((int)i);
            ImVec4 nameCol = l.mine ? ImVec4(0.35f, 0.85f, 1.0f, alpha) : ImVec4(1.0f, 0.82f, 0.28f, alpha);
            ImGui::TextColored(nameCol, "%s:", sender);
            ImGui::SameLine(0.0f, U(4.0f));

            ImVec2 at = ImGui::GetCursorScreenPos();
            if (!sChat) {
                ImGui::GetWindowDrawList()->AddText(NULL, 0.0f, ImVec2(at.x + 1.0f, at.y + 1.0f), IM_COL32(0, 0, 0, (int)(230.0f * alpha)), l.text, NULL, w - (at.x - ImGui::GetWindowPos().x));
            }
            ImGui::PushTextWrapPos(w);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", l.text);
            ImGui::PopTextWrapPos();
            if (sChat) { // a click copies the line (a room code or an address is passed on this way); a drag does not
                static unsigned sCopiedIdx;
                static Uint64 sCopiedMs;
                bool copied = sCopiedIdx == i + 1 && now - sCopiedMs < 1500;
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(copied ? "Copied" : "Click to copy");
                    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16.0f) {
                        SDL_SetClipboardText(l.text);
                        sCopiedIdx = i + 1;
                        sCopiedMs = now;
                        sChatFocus = true;
                    }
                }
            }
            ImGui::PopID();
        }

        if (sChat) {
            if (sChatFocus) {
                ImGui::SetKeyboardFocusHere();
                sChatFocus = false;
            }
            ImGui::SetNextItemWidth(w);
            if (ImGui::InputText("##chatline", sChatBuf, sizeof(sChatBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                const char *p = sChatBuf;
                while (*p == ' ') {
                    p++;
                }
                if (*p == '\0') {
                    chat_close();
                } else if (PortChat_Say(sChatBuf)) {
                    sChatBuf[0] = '\0';
                    chat_close();
                } else { // (said within a second of the line before: kept, to be sent again)
                    sChatSoon = true;
                    sChatFocus = true;
                }
            }
            ImGui::TextDisabled(sChatSoon ? "1 line/s: Enter to send" : "Enter: send   Esc/Tab: close   Ctrl+V: paste");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// Builds this frame's picture of the overlay (the settings or the online window, the line over the picture).
// false: there is nothing to draw. The same for both back ends; each then draws ImGui's data its own way.
// A native SDL color cursor works with both renderers on Windows and Linux.
// Purple normally; orange over clickable controls. Pressing preserves the palette.
static void menu_mouse_apply(void) {
    static SDL_Cursor *cursors[4];
    static float scales[4];
    static bool wasPressed, orange;
    if (ImGui::GetMouseCursor() != ImGuiMouseCursor_Arrow) return;
    int pressed = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
    bool hovered = sQuit ? PortQuit_MouseHovered() != 0 : PortMenuMouse_Hovered() != 0;
    // A click can clear hover by activating a control or changing menus.
    // Retain the displayed palette until release, including that first frame.
    if (!pressed) orange = hovered;
    else if (!wasPressed) orange = orange || hovered;
    wasPressed = pressed != 0;
    int style = pressed * 2 + (orange ? 1 : 0);
    SDL_Cursor *cursor = cursors[style];
    if (!cursor || scales[style] != sUiScale) {
        const ImVec2 shape[] = {{0, 0}, {0, 25}, {6, 19}, {11, 29}, {16, 26}, {11, 17}, {20, 17}};
        int width = (int)ceilf(U(28)), height = (int)ceilf(U(37));
        SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        if (!surface) return;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            float red = 0, green = 0, blue = 0, alpha = 0;
            // Supersampling keeps the contour smooth at small and fractional scales.
            for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
                float px = (x + (sx + 0.5f) / 4) / sUiScale - 4;
                float py = (y + (sy + 0.5f) / 4) / sUiScale - 4;
                // Compress toward the tip when pressed; the click hotspot stays fixed.
                if (pressed) { px /= 0.9f; py /= 0.9f; }
                bool inside = false;
                float distance = 1000;
                for (int i = 0, j = IM_ARRAYSIZE(shape) - 1; i < IM_ARRAYSIZE(shape); j = i++) {
                    const ImVec2 &a = shape[j], &b = shape[i];
                    if ((a.y > py) != (b.y > py) && px < (b.x-a.x)*(py-a.y)/(b.y-a.y)+a.x) inside = !inside;
                    float dx=b.x-a.x, dy=b.y-a.y;
                    float t=((px-a.x)*dx+(py-a.y)*dy)/(dx*dx+dy*dy);
                    t=t < 0 ? 0 : t > 1 ? 1 : t;
                    float ex=px-a.x-t*dx, ey=py-a.y-t*dy;
                    float d=sqrtf(ex*ex+ey*ey);
                    if (d < distance) distance=d;
                }
                if (!inside && distance > 3) continue;
                float r=orange ? 44 : 70, g=orange ? 21 : 0, b=orange ? 27 : 89;
                if (inside && distance > 1.5f) {
                    if (orange) {
                        float t=py / 29;
                        r=244-34*t; g=239-135*t; b=137-62*t;
                    } else {
                        r=g=b=203;
                    }
                }
                red+=r; green+=g; blue+=b; alpha+=1;
            }
            Uint8 *pixel=(Uint8 *)surface->pixels+y*surface->pitch+x*4;
            pixel[0]=alpha ? (Uint8)(red/alpha) : 0;
            pixel[1]=alpha ? (Uint8)(green/alpha) : 0;
            pixel[2]=alpha ? (Uint8)(blue/alpha) : 0;
            pixel[3]=(Uint8)(255*alpha/16);
        }
        SDL_Cursor *next=SDL_CreateColorCursor(surface, (int)U(4), (int)U(4));
        SDL_DestroySurface(surface);
        if (!next) return;
        SDL_SetCursor(next);
        if (cursor) SDL_DestroyCursor(cursor);
        cursors[style]=next;
        scales[style]=sUiScale;
    } else if (SDL_GetCursor() != cursor) {
        SDL_SetCursor(cursor);
    }
}

static bool frame_build(void) {
    native_video_apply();
    static Uint64 lastSettingsFrame;
    Uint64 settingsNow = SDL_GetTicks();
    float settingsDelta = lastSettingsFrame ? (float)(settingsNow - lastSettingsFrame) * 0.001f : 0.0f;
    lastSettingsFrame = settingsNow;
    if (sOpen && !sNet) {
        sSettingsTransition.advance(settingsDelta);
        if (!sSettingsTransition.active()) {
            sOpen = false;
            input_owner();
        }
    }
    if (sQuit && !PortQuit_Active() && !PortQuit_Failed()) {
        sQuit = false;
        input_owner();
        PortA11y_Say(PortA11y_Label("Back to the game"), 1);
    }
    if (sReady && gPortNetWindowClose) { // back from an online session: the window it was started from closes
        gPortNetWindowClose = 0;
        gPortNetMenuRequest = 0;
        set_open(false);
    } else if (sReady && gPortNetMenuRequest) { // Dragon Net Battle was chosen in the game's main menu
        gPortNetMenuRequest = 0;
        net_open();
    }
    if (sReady && !sUpdTold && update_latest() > PORT_RELEASE && SDL_GetTicks() >= sNoticeUntil) {
        int latest = update_latest(); // (told once, and not over another notice)
        sUpdTold = true;
        SDL_snprintf(sNotice, sizeof(sNotice), "Update available: 0.%d.%d. Open settings for the download page.", latest / 100, latest % 100);
        sNoticeUntil = SDL_GetTicks() + 10000;
    }
    bool notice = sNotice[0] != '\0' && SDL_GetTicks() < sNoticeUntil;
    if (sNoticeNew) { // (set from the game's side: the time starts when it is first drawn)
        sNoticeNew = 0;
        sNoticeUntil = SDL_GetTicks() + 6000;
        notice = sNotice[0] != '\0';
    }
    // the name of a stage added from outside the disc, over the stage select (a strip of pre-rendered names)
    bool nameOn = gUiNameReady != 0 && gUiNameIdx >= 0 && gUiNameIdx < PortStages_Count();
    bool songOn = gUiSongReady != 0 && gUiSongIdx >= 0 && gUiSongIdx < PortSongs_Count(); // (the same for an added song)
    bool overlay = nameOn || songOn;
    bool meter = sReady && meter_on();

    bool bars = sReady && bars_on();
    bool online = sReady && Port_NetActive() && PortChat_Enabled();
    if (sChat && !online) { // (the match ended while a line was typed)
        chat_close();
    }
    bool chat = online && (sChat || chat_recent());
    Uint64 now = SDL_GetTicks();
    bool saving = save_icon_on(now) && sReady && gUiPresentW > 0 && gUiPresentH > 0;
    bool inBattle = gBattleWork.running != 0;
    bool battlePause = inBattle && (gBattleWork.flags & 0x4000) &&
                       (!Port_NetActive() || BtlPause_GetMenuPad() == Port_NetMe());
    // A hidden cursor must stay inside the fight window, otherwise its clicks
    // go to another desktop window while keyboard attacks keep working.
    SDL_Window *mouseWindow = GsDraw_Window();
    bool grabMouse = inBattle && !battlePause && !sOpen && !sQuit && !sChat &&
        !gPortInputCapture && Port_Setting("mouse_support", 1) && mouseWindow &&
        (SDL_GetWindowFlags(mouseWindow) & SDL_WINDOW_INPUT_FOCUS);
    if (mouseWindow && SDL_GetWindowMouseGrab(mouseWindow) != grabMouse)
        SDL_SetWindowMouseGrab(mouseWindow, grabMouse);
    // External menus take precedence over battle and idle cursor hiding.
    bool cursorHidden = !sOpen && !sQuit && !sChat &&
        (now - sMouseLastMoved >= 10000 || (inBattle && !battlePause));
    // Apply even when no ImGui frame is needed (normal gameplay).
    if (cursorHidden) SDL_HideCursor();
    bool menuMouse = PortMenuMouse_Visible() != 0;
    static bool previousMenuMouse;
    bool updateMouse = menuMouse || previousMenuMouse || battlePause;
    previousMenuMouse = menuMouse;
    meter_count();
    sBuilt = false;
    if (!sReady || (!sOpen && !sQuit && !notice && !overlay && !meter && !bars && !chat && !saving && !updateMouse)) {
        return false;
    }
    if (sPadHold && sCapKind != 2) { // a binding was taken: the controller works the window again once nothing is held
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        bool held = false;
        for (int i = 0; i < count; i++) {
            SDL_Gamepad *g = SDL_GetGamepadFromID(ids[i]);
            for (int b = 0; g != NULL && b < SDL_GAMEPAD_BUTTON_COUNT; b++) {
                held = held || SDL_GetGamepadButton(g, (SDL_GamepadButton)b);
            }
        }
        SDL_free(ids);
        sPadHold = held;
    }
    // (the button that becomes the binding would also press the row it is bound in; while a chat line is typed the
    // controller is the game's)
    if (sPadHold || (sChat && !sOpen)) {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    } else {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    }
    sBuilt = true;
    if (sGL) {
        ImGui_ImplOpenGL3_NewFrame();
    } else {
        ImGui_ImplSDLGPU3_NewFrame();
    }
    ImGui_ImplSDL3_NewFrame();
    ui_scale_apply(); // (the window's size is known from here)
    fonts_apply(); // (a face chosen for the fight panels, between two frames)
    ImGui::NewFrame();
    // The mouse pointer is hidden over the game's picture and shown with a window of this file that takes the mouse:
    // the settings, the online windows, the question before the game closes, a chat line being typed, or a mouse menu
    // (setting hide_pointer, off unless enabled). The SDL back end hides it for this cursor at the next frame.
    if (cursorHidden || (!sOpen && !sQuit && !sChat && !menuMouse && !battlePause && Port_Setting("hide_pointer", 0) != 0)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    }
    if (bars) {
        bars_draw();
    }
    if (sOpen && !sSettingsContext) {
        if (sNet) {
            build_net();
        } else {
            build();
        }
    }
    if (sQuit && PortQuit_Failed()) {
        build_quit();
    }
    if (meter) {
        meter_draw();
    }
    if (chat) {
        chat_draw();
    }
    if (saving) {
        save_icon_draw(now);
    }
    if (notice) { // a line over the picture for a few seconds (the end of an online match)
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.12f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("##notice", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(sNotice);
        }
        ImGui::End();
    }
    if (overlay && gUiPresentW > 0 && gUiPresentH > 0) {
        /* Each name's rectangle is in the game's 512x448 pixels: map it through the picture's rectangle in the
           window (the letterbox gs_gpu.c just blitted into). */
        float sx = (float)gUiPresentW / 512.0f;
        float sy = (float)gUiPresentH / 448.0f;
        // Stage/song names belong to the game picture, behind settings and other windows,
        // including added names rendered while Menu font is Original.
        ImDrawList *dl = ImGui::GetBackgroundDrawList();

        if (nameOn) {
            const FontName *fn = font_name(NF_STAGE, PortStages_Name(gUiNameIdx));
            float x = (float)gUiPresentX + (float)gUiNameX * sx, y = (float)gUiPresentY + (float)gUiNameY * sy;
            if (fn != NULL) {
                float kx = (float)gUiNameW * sx / 384.0f, ky = (float)gUiNameH * sy / 48.0f;
                font_draw(dl, fn, x - 64.0f * kx, y + (1.5f - 14.0f) * ky, kx, ky, true);
            } else {
                name_text(dl, PortStages_Name(gUiNameIdx), x, y, (float)gUiNameW * sx, (float)gUiNameH * sy, true);
            }
        }
        if (songOn) {
            const FontName *fn = font_name(gUiSongLit ? NF_SONG_LIT : NF_SONG, PortSongs_Name(gUiSongIdx));
            float x = (float)gUiPresentX + (float)gUiSongX * sx, y = (float)gUiPresentY + (float)gUiSongY * sy;
            if (fn != NULL) {
                font_draw(dl, fn, x, y, (float)gUiSongW * sx / 512.0f, (float)gUiSongH * sy / 32.0f, false);
            } else {
                name_text(dl, PortSongs_Name(gUiSongIdx), x, y, (float)gUiSongW * sx, (float)gUiSongH * sy, false);
            }
        }
    }
    if (cursorHidden) ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    menu_mouse_apply();
    // The SDL backend updates visibility at NewFrame; apply this frame's decision immediately.
    if (ImGui::GetMouseCursor() == ImGuiMouseCursor_None) SDL_HideCursor();
    else SDL_ShowCursor();
    ImGui::Render();
    return true;
}

void Ui_Draw(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target) {
    if (!sGL && frame_build()) {
        Ui_DrawAgain(cmd, target);
    }
    if (!sGL) settings_window_frame();
}

// The OpenGL back end's turn: over the presented picture, just before the buffers are swapped.
void Ui_DrawGL(void) {
    if (sGL && frame_build()) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    if (sGL) settings_window_frame();
}
