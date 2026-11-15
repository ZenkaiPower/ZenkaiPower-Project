#include "common.h"
#include "menu/menu_e.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/dialog.h"

#ifdef PORT
#include <stdlib.h>
#include "port_account.h"
#include "sys/port_zenkai.h"
#include "menu/port_native_keyboard.h"
#include "port_username.h"
#include "port_ranking.h"
#include "battle/col_c.h"
#include "menu/tbo_art.h"
#include "menu/tbo_sizes.h"
#include "menu/port_screen.h"
#include "menu/port_selection_mouse.h"
extern s32 Port_PageAspectMilli(void), Port_TitleMainChars43(void), Port_Menus43(void);
extern s32 File_IsPartitionLoaded(s32 pt);
static void Tb_DisconnectDialogTerm(void);
static s32 Tb_DisconnectDialogStartClose(void);
static s32 Tb_DisconnectDialogIsClosed(void);
static s32 Tb_DisconnectDialogIsOpen(void);
static s32 Tb_AccountConfirmInput(void);
extern void Port_NetRewardBegin(void);
extern void Port_NetRewardRecord(s32 change);
extern s32 Port_NetRewardTake(void);
static void Tb_AccountUpdate(void);
/* Dragon Net Battle: menus and animation reconstructed from the original Wii assets. */

/* The game's windows of every menu (battle/view_a_b.c, view_a_c.c), made from the Duel menu's data as
   DuelMenu_Init makes them: the heading's window (IconWin: the cloud swirls in the corners and the line under the
   heading) and the message window (MsgWin: the teal box with the meander corners). */
extern void MsgWin_SetText(void *text); /* (the rest: menu/menu_a.h) */
extern void IconWin_PortHideWords(void);
extern s32 IconWin_PortHeadingMotion(s32 frame, f32 *iconDx, f32 *iconDy, s32 *iconAlpha,
                                     f32 *iconScaleX, f32 *iconScaleY, f32 *titleDx, f32 *titleDy,
                                     s32 *titleAlpha, f32 *titleScaleX, f32 *titleScaleY);
extern void MsgWin_PortDrawString(u16 *str); /* (battle/view_a_b.c) the box with a line of ours, in the game's font */
extern void MsgWin_PortPrepareNativeNames(void);
extern void MsgWin_PortDrawNativeName(const char *name, s32 x0, s32 y0, s32 x1, s32 y1);
extern s32 MsgWin_PortLineRoom(s32 right);
extern void MsgWin_PortSameTop(void);
extern s32 Port_WiiArtAvailable(void);
extern s32 PortTbo_ResultTextCopy(s32 side, char *out, s32 capacity);
extern s32 PortTbo_WiiTextCopy(s32 index, char *out, s32 capacity);
extern s32 PortTbo_DisconnectTextCopy(s32 reason, char *out, s32 capacity);
extern s32 PortTbo_WiiUseFont(void);
extern s32 PortTbo_WiiUseFontForArtwork(s32 textKind);
extern s32 PortTbo_TextId(const char *text, s32 kind);
extern s32 PortTbo_TextInfo(s32 id, s32 what);
extern s32 Port_DcNameOpen(void);
extern void Port_DcNameClose(void);
extern s32 Port_DcNameReady(void);
extern void Port_DcNameStartClose(void);
extern s32 Port_DcNameIsClosed(void);
extern s32 Port_DcNameKey(s32 page, s32 row, s32 col);
extern void Port_DcNamePress(s32 page, s32 row, s32 col);
extern void Port_DcNameDraw(s32 page, s32 row, s32 col, s32 rise);
extern s32 Port_WiiDialogText(s32 line, char *out, s32 capacity);
extern s32 Port_WiiMenuText(s32 line, char *out, s32 capacity);
extern s32 Port_WiiDialogDuration(s32 line, s32 japanese);
extern s32 Port_WiiDialogMouth(s32 line, s32 frame, s32 japanese);
extern s32 Port_WiiVoicePlay(s32 line, s32 japanese);
extern void Voice_Stop(s32 voice);
extern void Adx_SetVolume(s32 ch, s32 vol);
extern void Adx_Resume(s32 ch);
extern s32 Font_GetWidth(u16 *str); /* (battle/col_c.c) a line's width in the game's font */

extern void Job_Run(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 arg);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void File_Stub264D90(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern s32 ColorFade_IsOutDone(void);
extern s32 sceGsSyncPath(s32 mode, u16 timeout);

/* port/src/gs/net.c, state.c, plat_settings.c */
extern void Port_NetOptions(int rollback, int delay);
extern int Port_RollCan(void);
extern void Port_NetRulesSet(int type, int dp, int time);
extern const char *Port_NetNameGet(void);
extern int Port_LobbyStartCode(int host, const char *code);
extern int Port_LobbyStartLan(int host, const char *address, int port, int queue);
extern int Port_LanChoice(char *address, int capacity);
extern const char *Port_LobbyPeerName(void);
extern const char *Port_LobbyName(int player);
extern int Port_LobbyPoll(void);
extern void Port_LobbyCancel(void);
extern void Port_LobbyLaunch(void);
extern int Port_NetBattleMenuStyle(int style);
extern int Port_NetMe(void);
extern int Port_NetSessionsEnded(void); /* gs/net.c */
extern void Match_DataAsk(void);
extern int Match_DataState(void); /* 0 checking, 1 accepted by server, -1 failed */
extern int Port_NetTakeEndReason(void); /* 1: peer left; 2: connection lost; 4: desync */
extern const char *Port_LobbyCode(void);
extern const char *Port_LobbyError(void);
extern int Port_LobbyOther(void);
extern int Port_LobbyStartQuick(int queue);   /* S, T, D10, D15, D20 matchmaking queues */
extern void Port_NetSpeedSet(int percent);
extern int Port_NetSpeedStandard(void);
extern void Port_NetNameSet(const char *name);
extern int Port_Setting(const char *name, int def);
extern void Port_SettingSave(const char *name, int value);
extern void Port_SettingTextSave(const char *name, const char *value);
extern void Port_SettingsWrite(void);

/* the screens */
#define TB_WII_REWARD 20
#define TB_WII_HOME 9
#define TB_WII_BATTLE 10
#define TB_WII_FRIENDS 11
#define TB_WII_SETTINGS 12
#define TB_WII_RANKING 13
#define TB_WII_NAME 14
#define TB_WII_CUSTOM 15
#define TB_WII_VS 16
#define TB_WII_FRIEND_MENU 17
#define TB_WII_FRIEND_FORMAT 18
#define TB_WII_FRIEND_CODE 19
#define CODE_LEN 6

typedef struct TboScreen {
    s32 frames;
    s32 mode;
    s32 wiiCur;
    s32 wiiPrevCur;
    s32 wiiSelectT;
    s32 wiiFocusFrame;
    s32 wiiHomeT;      /* Wii FOD entrance frame; begins after the screen fade */
    s32 wiiDialogLine;
    s32 wiiDialogT;
    s32 wiiDialogStarted;
    s32 wiiDialogTouched;
    s32 wiiPanPointing;
    s32 wiiBattleIntro;
    s32 wiiIdleT;
    s32 wiiIdleUsed;
    s32 wiiIdleLast; /* line ID; zero before the first idle remark */
    s32 wiiDialogJapanese;
    s32 wiiDialogVoiceStarted;
    char wiiDialogText[256];
    s32 wiiBattleCur;
    s32 wiiCustomCur;
    s32 wiiNormalBattle; /* format page opened from Normal Battle, rather than Custom Battle */
    s32 wiiFriend;       /* room-code battle, rather than a public/LAN search */
    s32 wiiBattleT;     /* battle-page entrance, after its fade in */
    s32 wiiNextMode;    /* pending Wii page switch, after fade out */
    s32 wiiNameExit;    /* target page after the keyboard's native exit */
    s32 wiiNameExitT;
    s32 wiiNameJoin;
    s32 wiiRanked;
    s32 resultPoints[2], resultPointDelta[2], resultHasPoints;
    s32 rewardZPoints;
    s32 rewardSavePending; /* This menu visit changed local Z Points. */
    s32 rankingOffset, rankingRefresh;
    PortRankingPage ranking;
    s32 wiiSubCur;
    s32 modeT;
    s32 leaving;
    s32 note;
    char noteLine[96];
    /* the keypad */
    char code[CODE_LEN + 1];
    s32 keyCol;
    s32 keyRow;         /* WII_KEY_ROWS: the bottom action row */
    /* the search */
    s32 host;           /* this side opened the room */
    s32 found;          /* the other player is there: the match is starting */
    s32 foundAt;        /* Wii VS: keep both names on screen before launching */
    s32 ended;          /* Port_NetSessionsEnded when it started: a change says the match has been played */
    s32 disconnectReason; /* dismissible message after an interrupted Wii match */
    s32 disconnectClosing;
    s32 disconnectCloseFrames;
    s32 disconnectOpenFrames;
    char disconnectLine[256];
    s32 waitBack;       /* the screen a cancelled search goes back to */
    s32 lower;          /* the name pad shows small letters */
    s32 accountWaiting, accountPrompt, accountCode, accountSubmit;
    s32 nameCursor;     /* insertion point in the Wii name editor */
    /* typing the name */
    char typed[20];
    s32 typedLen;
} TboScreen;

static TboScreen *sTb;
static s32 sRankingRefresh;

static char *sTbWiiHomeName[4] = {"Nintendo WFC Battle", "Manage Friends", "User Settings", "Dragon Net Battle"};
/* Set the first visible entry back to 0 to restore Nintendo WFC Battle. */
static const s32 sTbWiiHomeVisible[3] = {3, 1, 2};
static char *sTbWiiBattleName[6] = {"Custom Battle", "Normal Battle", "Friend Battle", "Ranking Battle", "View Ranking", "Battle Record"};
#define TB_WII_CUSTOM_COUNT 5
#define TB_WII_BATTLE_ROW_STEP_X 10.5f
#define TB_WII_MODE_ROW_OFFSET_X 120.0f
static char *sTbWiiCustomName[TB_WII_CUSTOM_COUNT] = {
    "1 VS 1 Battle", "Team Battle", "10 DP Team Battle", "15 DP Team Battle", "20 DP Team Battle"
};
static char *sTbWiiFriendAction[2] = {"Host Battle", "Join Battle"};
static const char *const sTbWiiBattleHelpDefault[6] = {
    "Search for opponents nationwide and fight in a single battle. (Custom characters OK)",
    "Search for opponents nationwide and fight in a single battle. (Normal characters only)",
    "Search for opponents from your Friend Roster. (Custom characters OK)",
    "A ranking battle; Dragon Net Battle fight. (Custom characters OK)",
    "View Rankings.",
    "View Battle History."
};
static char *sTbWiiBattleHelp[6];
static void Tb_SetWiiTitles(void);
static char *sTbWiiFriendsName[3] = {"Friend List", "Add Friend", "My Friend Code"};
static char *sTbWiiSettingsName[4] = {"Create Player Name", "Initialize Wi-Fi Data", "Initialize Data", "Player Name"};
/* Visible rows map to separate entries; the original Wii settings entries are hidden. */
static const s32 sTbWiiSettingsVisible[2] = {3, 2};
static char *sTbWiiRankingName[2] = {"My Area", "Top 10"};

/* Keep the selected catalog's words in game memory for drawing, help text,
   and the screen reader. Art-backed labels use the font only when edited. */
enum { TB_WII_TEXT_COUNT = 40, TB_WII_TEXT_SIZE = 256 };
static char sTbWiiText[TB_WII_TEXT_COUNT][TB_WII_TEXT_SIZE];
static u8 sTbWiiTextEdited[TB_WII_TEXT_COUNT];
#define TB_WII_TX_HOME_TITLE 0
#define TB_WII_TX_HOME_ROW 1
#define TB_WII_TX_BATTLE_TITLE 4
#define TB_WII_TX_BATTLE_ROW 5
#define TB_WII_TX_NORMAL_TITLE 11
#define TB_WII_TX_CUSTOM_ROW 12
#define TB_WII_TX_FRIENDS_TITLE 17
#define TB_WII_TX_FRIENDS_ROW 18
#define TB_WII_TX_SETTINGS_TITLE 21
#define TB_WII_TX_SETTINGS_ROW 22
#define TB_WII_TX_RANKING_TITLE 24
#define TB_WII_TX_RANKING_ROW 25
#define TB_WII_TX_NAME_TITLE 27
#define TB_WII_TX_CUSTOM_TITLE 28
#define TB_WII_TX_FRIEND_HOST 29
#define TB_WII_TX_FRIEND_JOIN 30
#define TB_WII_TX_SETTINGS_NET 31
#define TB_WII_TX_SETTINGS_PLAYER_NAME 32
#define TB_WII_TX_HOME_DRAGON_NET 33
#define TB_WII_TX_BATTLE_HELP 34
/* Use TB_WII_TX_BATTLE_TITLE here to restore the original battle-page header. */
static const s32 sTbWiiBattleHeader = TB_WII_TX_HOME_TITLE;
static void Tb_LoadWiiText(void) {
    s32 i;
    for (i = 0; i < TB_WII_TX_BATTLE_HELP; i++)
        sTbWiiTextEdited[i] = PortTbo_WiiTextCopy(i, sTbWiiText[i], TB_WII_TEXT_SIZE);
    for (i = 0; i < 3; i++) {
        sTbWiiHomeName[i] = sTbWiiText[TB_WII_TX_HOME_ROW + i];
        sTbWiiFriendsName[i] = sTbWiiText[TB_WII_TX_FRIENDS_ROW + i];
    }
    for (i = 0; i < 6; i++) {
        sTbWiiBattleName[i] = sTbWiiText[TB_WII_TX_BATTLE_ROW + i];
        Ps_Copy(sTbWiiText[TB_WII_TX_BATTLE_HELP + i], sTbWiiBattleHelpDefault[i], TB_WII_TEXT_SIZE);
        Port_WiiMenuText(47 + i, sTbWiiText[TB_WII_TX_BATTLE_HELP + i], TB_WII_TEXT_SIZE);
        sTbWiiBattleHelp[i] = sTbWiiText[TB_WII_TX_BATTLE_HELP + i];
    }
    for (i = 0; i < TB_WII_CUSTOM_COUNT; i++) sTbWiiCustomName[i] = sTbWiiText[TB_WII_TX_CUSTOM_ROW + i];
    sTbWiiHomeName[3] = sTbWiiText[TB_WII_TX_HOME_DRAGON_NET];
    sTbWiiSettingsName[2] = sTbWiiText[TB_WII_TX_SETTINGS_NET];
    sTbWiiSettingsName[3] = sTbWiiText[TB_WII_TX_SETTINGS_PLAYER_NAME];
    sTbWiiFriendAction[0] = sTbWiiText[TB_WII_TX_FRIEND_HOST];
    sTbWiiFriendAction[1] = sTbWiiText[TB_WII_TX_FRIEND_JOIN];
    for (i = 0; i < 2; i++) {
        sTbWiiSettingsName[i] = sTbWiiText[TB_WII_TX_SETTINGS_ROW + i];
        sTbWiiRankingName[i] = sTbWiiText[TB_WII_TX_RANKING_ROW + i];
    }
}

/* Apply the same GS tint used by the disabled Wii bar artwork to its text. */
static u32 Tb_WiiTintText(u32 color, u32 tint) {
    return Ps_Rgba(((color & 0xff) * (tint & 0xff)) / 0x80,
                   (((color >> 8) & 0xff) * ((tint >> 8) & 0xff)) / 0x80,
                   (((color >> 16) & 0xff) * ((tint >> 16) & 0xff)) / 0x80,
                   (color >> 24) & 0xff);
}

static s32 Tb_WiiFocusAlpha(s32 row, s32 current) {
    if (row == current) return sTb->wiiSelectT >= 4 ? 0x80 : sTb->wiiSelectT * 0x80 / 4;
    if (row == sTb->wiiPrevCur && sTb->wiiSelectT < 4)
        return (4 - sTb->wiiSelectT) * 0x80 / 4;
    return 0;
}

/* Evolution Z's mc_menu_text_on track in the PS2 movie (00452.bin):
   fl_on_loop has 49 frames, rising by 1/96 to 1.25 then falling by 1/100.
   Only the vertical component is used for the Wii caption. */
static f32 Tb_WiiFocusStretch(void) {
    s32 frame = sTb->wiiFocusFrame % 49;
    if (frame <= 24) return 1.0f + (f32)frame / 96.0f;
    return 1.0f + (f32)(49 - frame) / 100.0f;
}

/* The Wii texture labels switch from their idle sheet to the selected sheet,
   whose vertical scale follows Evolution Z's focused text loop. */
/* Edge-bound artwork must keep its original edge during entry, idle and exit. */
static void Tb_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1,
                            f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha) {
    s32 edge = 0;
    if ((id >= TBO_WII_PAN && id <= TBO_WII_PAN_POINT_MOUTH_CLOSED) ||
        (id >= TBO_WII_BATTLE_GIRU && id <= TBO_WII_BATTLE_GIRU_EYE_DIM) ||
        id == TBO_WII_CREST || id == TBO_WII_TITLE || id == TBO_WII_SETTINGS_TITLE ||
        id == TBO_WII_BATTLE_TITLE || id == TBO_WII_CUSTOM_TITLE ||
        id == TBO_WII_NORMAL_TITLE || id == TBO_WII_RANK_TITLE ||
        ((sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM) &&
         (id == TBO_WII_BATTLE_POINT_PANEL || id == TBO_WII_BATTLE_POINT_LABEL ||
          id == TBO_WII_BATTLE_POINT_DIGITS))) edge = 1;
    else if (id >= TBO_WII_GIRU && id <= TBO_WII_GIRU_EYE_DIM)
        edge = sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE ? 1 : 2;
    if (edge) Ps_Foreground43Anchor(edge);
    Ps_SpriteScreen(id, x0, y0, x1, y1, u0, v0, u1, v1, tint, alpha);
    if (edge) Ps_Foreground43Anchor(0);
}
#define Ps_SpriteScreen Tb_SpriteScreen

static void Tb_Heading(f32 x, f32 y, char *text, f32 scale, f32 width, s32 alpha) {
    Ps_Foreground43Anchor(sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE ? 0 : 1);
    Ps_Heading(x, y, text, scale, width, alpha);
    Ps_Foreground43Anchor(0);
}
#define Ps_Heading Tb_Heading

static void Tb_WiiTextureLabel(s32 idle, s32 selected, f32 x, f32 y,
                               f32 v0, f32 v1, s32 focusAlpha, u32 tint, u16 capHeight) {
    /* EvoTop's normal and selected labels have a measured 19-pixel letter fill. */
    const f32 size = 19.0f / (f32)capHeight;
    const f32 middle = y + 16.0f;
    if (focusAlpha < 0x80)
        Ps_SpriteScreen(idle, x, middle - 16.0f * size,
                        x + 512.0f * size, middle + 16.0f * size,
                        0.0f, v0, 1.0f, v1, tint, 0x80 - focusAlpha);
    if (focusAlpha > 0) {
        f32 stretch = size * Tb_WiiFocusStretch();
        f32 top = middle - 16.0f * stretch;
        Ps_SpriteScreen(selected, x, top, x + 512.0f * size, top + 32.0f * stretch,
                        0.0f, v0, 1.0f, v1, tint, focusAlpha);
    }
}

/* Wii coordinates are screen pixels; menu text uses page coordinates. The
   selected layer replaces the idle layer as the Evolution Z clip switches. */
static void Tb_WiiLabelText(char *label, f32 x, f32 y, s32 focusAlpha, u32 tint) {
    s32 layer;
    y -= 3.0f;
    if (PortTbo_WiiUseFont() ||
        (sTb->mode == TB_WII_HOME && label == sTbWiiHomeName[3]) ||
        (sTb->mode == TB_WII_SETTINGS &&
         (label == sTbWiiSettingsName[2] || label == sTbWiiSettingsName[3])) ||
        sTb->mode == TB_WII_CUSTOM ||
        sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT) {
        for (layer = 0; layer < 2; layer++) {
            s32 opacity = layer ? focusAlpha : 0x80 - focusAlpha;
            s32 id;
            if (opacity <= 0) continue;
            id = PortTbo_TextId(label, layer ? 4 : 3);
            if (id >= 0) {
                f32 stretch = layer ? Tb_WiiFocusStretch() : 1.0f;
                f32 top = y + 16.0f - 16.0f * stretch;
                Ps_SpriteScreen(id, x, top, x + 512.0f, top + 32.0f * stretch,
                                0.0f, 0.0f, 1.0f, 1.0f, tint, opacity);
            }
        }
        return;
    }
    {
        f32 sx = 1.0f / (Ps_X(1.0f) - Ps_X(0.0f));
        f32 sy = 1.0f / (Ps_Y(1.0f) - Ps_Y(0.0f));
        f32 scaleX = (19.0f / (PS_CELL_UNITS * PS_CAP_OF_CELL)) * sx;
        f32 scaleY = (19.0f / (PS_CELL_UNITS * PS_CAP_OF_CELL)) * sy;
        f32 tx = (x - Ps_X(0.0f)) * sx;
        f32 textMiddle = (y + 16.0f - Ps_Y(0.0f)) * sy;
        Ps_Align(0);
        for (layer = 0; layer < 2; layer++) {
            s32 opacity = layer ? focusAlpha : 0x80 - focusAlpha;
            f32 stretch = layer ? Tb_WiiFocusStretch() : 1.0f;
            f32 textY = Ps_TextTop(textMiddle, scaleY * stretch);
            if (opacity <= 0) continue;
            Ps_TextExOuterXY(tx, textY, label, scaleX, scaleY * stretch, 230.0f * sx,
                             Tb_WiiTintText(Ps_Rgba(layer ? 0xF4 : 0xCB, layer ? 0xEF : 0xCB,
                                                    layer ? 0x89 : 0xCB, opacity), tint),
                             Tb_WiiTintText(Ps_Rgba(layer ? 0xD2 : 0xCB, layer ? 0x68 : 0xCB,
                                                    layer ? 0x4B : 0xCB, opacity), tint),
                             Tb_WiiTintText(Ps_Rgba(layer ? 0x2C : 0x46, layer ? 0x15 : 0x00,
                                                    layer ? 0x1B : 0x59, opacity), tint),
                             Tb_WiiTintText(Ps_Rgba(layer ? 0x2C : 0x46, layer ? 0x15 : 0x00,
                                                    layer ? 0x1B : 0x59, opacity), tint), sy);
        }
    }
}
static void Tb_WiiTitleText(s32 index, f32 x, f32 y, f32 scaleX, f32 scaleY, s32 alpha) {
    Ps_Foreground43Anchor(sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE ? 0 : 1);
    /* Friend Battle has no dedicated Wii title sheet. Match the converted
       Normal/Custom heading even when Menu font is set to Original. */
    if (PortTbo_WiiUseFont() || index == TB_WII_TX_BATTLE_ROW + 2 || index == TB_WII_TX_BATTLE_ROW + 3) {
        s32 id = PortTbo_TextId(sTbWiiText[index], 5);
        if (id >= 0)
            Ps_SpriteScreen(id, x, y, x + 512.0f * scaleX, y + 64.0f * scaleY,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
        Ps_Foreground43Anchor(0);
        return;
    }
    f32 sx = 1.0f / (Ps_X(1.0f) - Ps_X(0.0f));
    f32 sy = 1.0f / (Ps_Y(1.0f) - Ps_Y(0.0f));
    /* Evolution Z's heading face is 21 screen pixels high. Keep the Wii
       title motion while matching that measured letter height. */
    f32 textScale = (21.0f / (PS_CELL_UNITS * PS_CAP_OF_CELL)) * scaleY * sy;
    Ps_Heading((x - Ps_X(0.0f)) * sx,
               Ps_TextTop((y + 32.0f * scaleY - Ps_Y(0.0f)) * sy, textScale),
               sTbWiiText[index], textScale, 470.0f * scaleX * sx, alpha);
    Ps_Foreground43Anchor(0);
}

/* Data Center's password keyboard. The last two columns on rows 1/2 move
   the text cursor; the last two on row 3 delete. */
#define WII_KEY_COLS 14
#define WII_KEY_ROWS 4

static s32 sTbLanChoice;
static char sTbLanAddress[128];

/* ---------------------------------------------------------------------------------------------- small things */

static f32 Tb_In(s32 delay) {
    f32 t = (f32)(((sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM ||
                    sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT) ? sTb->wiiBattleT : sTb->modeT) - delay) / 16.0f;

    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }
    t = 1.0f - t;
    return 1.0f - t * t * t;
}

static void Tb_Note(char *line) {
    Ps_Copy(sTb->noteLine, line, 96);
    sTb->note = 150;
}

static void Tb_ShowMessage(const char *line) {
    sTb->accountPrompt = 0;
    sTb->disconnectReason = 3;
    sTb->disconnectClosing = 0;
    sTb->disconnectCloseFrames = 0;
    sTb->disconnectOpenFrames = 0;
    Ps_Copy(sTb->disconnectLine, (char *)line, sizeof(sTb->disconnectLine));
}

static void Tb_ShowConnectionError(const char *line) {
    Tb_ShowMessage(line);
}

static void Tb_SetMode(s32 mode) {
    s32 previous = sTb->mode;
    if ((sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE) &&
        mode != TB_WII_NAME && mode != TB_WII_FRIEND_CODE) Port_DcNameClose();
    if (sTb->mode != mode) { /* (the page that is up again, as when the rival comes on the search: it does not come in anew) */
        if (mode == TB_WII_RANKING) { sTb->rankingOffset = 0; sTb->rankingRefresh = ++sRankingRefresh; }
        sTb->wiiPrevCur = -1;
        sTb->wiiSelectT = mode >= TB_WII_HOME ? 0 : 4;
        sTb->wiiFocusFrame = 0;
        if (sTb->wiiDialogStarted) Voice_Stop(0);
        sTb->wiiDialogStarted = 0;
        sTb->wiiDialogText[0] = 0;
        sTb->modeT = 0;
        if (previous == TB_WII_NAME && mode == TB_WII_SETTINGS)
            sTb->modeT = 24; /* return with the settings page already settled */
        sTb->wiiBattleIntro = 0;
        sTb->wiiNameExit = 0;
        sTb->wiiNameExitT = 0;
        sTb->wiiNameJoin = 0;
        sTb->wiiIdleT = 0;
        if (mode == TB_WII_HOME) {
            sTb->wiiPanPointing = 0;
            if (previous != TB_WII_SETTINGS && previous != TB_WII_BATTLE) {
                sTb->wiiHomeT = 0;
                sTb->wiiDialogTouched = 0;
            }
            Port_WiiDialogText(0, sTb->wiiDialogText, sizeof(sTb->wiiDialogText));
        }
        if (mode == TB_WII_BATTLE || mode == TB_WII_CUSTOM || mode == TB_WII_VS ||
            mode == TB_WII_FRIEND_MENU || mode == TB_WII_FRIEND_FORMAT || mode == TB_WII_FRIEND_CODE || mode == TB_WII_RANKING)
            sTb->wiiBattleT = 0;
    }
    sTb->mode = mode;
}

/* Resource 7's PAL text and resource 6's paired Japanese/English lip tracks
   share a line index with both sets of Wii ADX streams. */
static s32 Tb_WiiLine(void) {
    static const s32 battle[6] = {32, 32, 33, 34, 35, 36};
    static const s32 friends[3] = {19, 20, 21};
    static const s32 home[4] = {6, 7, 8, 6};
    /* Added entries reuse the original settings dialogue and both voice tracks. */
    static const s32 settings[4] = {30, 31, 31, 30};
    switch (sTb->mode) {
    case TB_WII_HOME: return home[sTbWiiHomeVisible[sTb->wiiCur]];
    case TB_WII_BATTLE: return battle[sTb->wiiBattleCur];
    case TB_WII_FRIENDS: return friends[sTb->wiiSubCur];
    case TB_WII_SETTINGS: return settings[sTbWiiSettingsVisible[sTb->wiiSubCur]];
    case TB_WII_RANKING: return 35;
    }
    return -1;
}

static void Tb_WiiDialogSet(s32 line) {
    s32 played;
    if (sTb->wiiDialogStarted) Voice_Stop(0);
    sTb->wiiDialogLine = line;
    sTb->wiiDialogT = 0;
    sTb->wiiDialogStarted = 1;
    sTb->wiiDialogVoiceStarted = 0;
    /* The game's Sound Options use flag 1 for English, clear for Japanese. */
    sTb->wiiDialogJapanese = gSaveData != NULL && !(gSaveData->flags & 1);
    sTb->wiiDialogText[0] = 0;
    if (!Port_WiiDialogText(line, sTb->wiiDialogText, sizeof(sTb->wiiDialogText))) return;
    Adx_SetVolume(4, 0x80); /* the game's voice 0 ADX channel */
    Adx_Resume(4);
    played = Port_WiiVoicePlay(line, sTb->wiiDialogJapanese);
    sTb->wiiDialogVoiceStarted = played != 0;
    if (played) sTb->wiiDialogJapanese = played == 2;
}

static s32 Tb_DataState(void);

/* Use the same build verification as the online page and matchmaking requests.
   An unavailable legacy gameplay fingerprint is not evidence of modified data. */
static s32 Tb_Verified(void) {
    s32 state = Tb_DataState();

    if (state == 0) {
        Tb_Note("CHECKING BUILD COMPATIBILITY... TRY AGAIN IN A MOMENT");
        return 0;
    }
    if (state == 1) {
        return 1;
    }
    Snd_PlaySe(1, 2);
    Tb_ShowMessage("\nCannot connect.\nBuild compatibility check failed.\nCheck your connection and update the game.");
    return 0;
}

static void Tb_ShowDisconnect(s32 reason) {
    sTb->disconnectReason = reason;
    sTb->disconnectClosing = 0;
    sTb->disconnectCloseFrames = 0;
    sTb->disconnectOpenFrames = 0;
    if (!PortTbo_DisconnectTextCopy(reason, sTb->disconnectLine, sizeof(sTb->disconnectLine)))
        Ps_Copy(sTb->disconnectLine, reason == 4 ? "Synchronization lost." : reason == 1 ? "The other player left the match."
                                                : "Connection lost.", sizeof(sTb->disconnectLine));
}

static void Tb_ShowRoomCodeMessage(void) {
    Tb_ShowMessage("A Room Code has 6 letters and numbers.");
}

static void Tb_ShowLobbyError(void) {
    Tb_ShowMessage(Port_LobbyError());
}

/* What the search says (each frame on the screens that wait): the rival came (the match starts), or it failed.
   Returns 1 when the screen that called has nothing more to do this frame. */

/* ---------------------------------------------------------------------------------------------- Quick Match */

/* Both Wii format pages share the PC quick-match queues, in their original order. */
static void Tb_WiiCustomSearchStart(void) {
    s32 queue = sTb->wiiFriend ? sTb->wiiCustomCur :
                sTb->wiiCustomCur + (sTb->wiiNormalBattle ? 2 : 0);
    char address[128];
    s32 lan = sTbLanChoice;
    Ps_Copy(address, sTbLanAddress, sizeof(address));
    s32 state;

    Port_NetSpeedSet(Port_NetSpeedStandard());
    Port_NetRulesSet(queue == 0 ? 0 : queue == 1 ? 1 : 2,
                     queue >= 2 ? queue - 2 : 0, 4);
    Port_NetOptions(Port_RollCan() ? 8 : 0, -2);
    Port_NetNameSet(Port_NetNameGet());
    if (sTb->wiiFriend) {
        state = Port_LobbyStartCode(1, "");
        sTb->host = 1;
    } else if (lan == 3 || sTb->wiiRanked) {
        state = Port_LobbyStartQuick(sTb->wiiRanked ? 5 : queue);
        sTb->host = 2;
    } else {
        /* Direct LAN play is the default for this Wii page. The first local copy hosts; a second copy
           whose bind finds that port occupied joins it over loopback. On separate PCs, give the host IP. */
        if (lan == 0) {
            state = Port_LobbyStartLan(1, "", 25443, queue);
            sTb->host = 1;
            if (state < 0) {
                state = Port_LobbyStartLan(0, "127.0.0.1", 25443, queue);
                sTb->host = 0;
            }
        } else if (lan == 1) {
            state = Port_LobbyStartLan(1, "", 25443, queue);
            sTb->host = 1;
        } else if (lan == 2) {
            state = Port_LobbyStartLan(0, address, 25443, queue);
            sTb->host = 0;
        } else {
            Tb_ShowConnectionError("ZENKAIPOWER_LAN MUST BE auto, host, join:<HOST IP>, OR online");
            Tb_SetMode(sTb->wiiFriend ? TB_WII_FRIEND_FORMAT : TB_WII_CUSTOM);
            return;
        }
    }
    if (state < 0) {
        Tb_ShowConnectionError("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        Tb_SetMode(sTb->wiiFriend ? TB_WII_FRIEND_FORMAT : TB_WII_CUSTOM);
        return;
    }
    sTb->found = 0;
    sTb->waitBack = sTb->wiiFriend ? TB_WII_FRIEND_FORMAT : TB_WII_CUSTOM;
}

static void Tb_WiiFriendJoinStart(void) {
    s32 state;
    if (!Tb_Verified()) return;
    Port_NetNameSet(Port_NetNameGet());
    Port_NetOptions(-1, -1); /* the host supplies the battle rules */
    state = Port_LobbyStartCode(0, sTb->code);
    if (state < 0) {
        Tb_ShowConnectionError("THE NETWORK COULD NOT BE OPENED ON THIS PC");
        Tb_SetMode(TB_WII_FRIEND_MENU);
        return;
    }
    sTb->host = 0;
    sTb->found = 0;
    sTb->waitBack = TB_WII_FRIEND_MENU;
    sTb->wiiNextMode = TB_WII_VS;
    ColorFade_StartOut(0, 0, 0, 0x14);
}

static s32 sTbLan;
static s32 Tb_DataState(void) { return sTbLan ? 1 : Match_DataState(); }

static s32 Tb_HomeDisabled(s32 entry) {
    return Tb_DataState() != 1 || entry == 1 || ((entry == 0 || entry == 3) && !sTbLan && !Port_AccountLogged(Port_NetNameGet()));
}

static s32 Tb_SettingsDisabled(s32 entry) {
    return Tb_DataState() != 1 || entry == 1 || (entry == 2 && Port_AccountLogged(Port_NetNameGet()));
}

static void Tb_AccountMessage(const char *id) {
    char line[256];
    Port_AccountUiText(id, line, sizeof(line));
    Tb_ShowMessage(line);
}

static void Tb_AccountFormat(char *out, s32 capacity, const char *format, const char *code) {
    s32 used = 0;
    while (*format && used + 1 < capacity) {
        if (format[0] == '%' && format[1] == 's') {
            s32 i;
            for (i = 0; code[i] && used + 1 < capacity; i++) out[used++] = code[i];
            format += 2;
        } else out[used++] = *format++;
    }
    out[used] = 0;
}

static void Tb_AccountUpdate(void) {
    char text[256], format[256], line[256];
    s32 state = Port_AccountPoll(text, sizeof(text));
    if (state == 1) return;
    if (sTb->accountWaiting == 2) {
        sTb->accountWaiting = 0;
        if (Port_AccountLogged(Port_NetNameGet())) return;
        if (Port_AccountBegin(Port_NetNameGet(), NULL)) { sTb->accountWaiting = 1; return; }
    }
    sTb->accountWaiting = 0;
    if (state == 2) {
        if (!Port_DcNameOpen()) { Tb_AccountMessage("account/unavailable"); return; }
        sTb->accountCode = 1;
        sTb->typed[0] = 0;
        sTb->typedLen = sTb->nameCursor = 0;
        sTb->lower = 0;
        sTb->keyCol = sTb->keyRow = 0;
        Tb_SetMode(TB_WII_NAME);
    } else if (state == 3) {
        Port_AccountUiText("account/save-code", format, sizeof(format));
        Tb_AccountFormat(line, sizeof(line), format, text);
        Tb_ShowMessage(line);
        sTb->accountPrompt = 1;
    } else if (state == 4) {
        Tb_AccountMessage("account/success");
    } else {
        if (text[0]) Tb_ShowMessage(text);
        else Tb_AccountMessage("account/unavailable");
    }
}

/* Dragon History transfers 0x21 Z Points per 60 Hz frame. Confirm finishes
   the transfer immediately; a fresh confirm afterwards leaves the reward page. */
static void Tb_WiiRewardUpdate(s32 live) {
    s32 confirm = gPad[0].gamePressed & (PADG_CROSS | PADG_TRIANGLE);
    if (!live || sTb->modeT < 30 || sTb->wiiNextMode) return;
    if (sTb->rewardZPoints > 0) {
        s32 amount = confirm || sTb->rewardZPoints <= 0x21 ? sTb->rewardZPoints : 0x21;
        if (gSaveData == NULL) return;
        Save_AddMoney(amount);
        sTb->rewardSavePending = 1;
        sTb->rewardZPoints -= amount;
        if (confirm) Snd_PlaySe(1, 1);
        return;
    }
    if (confirm) {
        Snd_PlaySe(1, 1);
        sTb->wiiNextMode = TB_WII_HOME;
        ColorFade_StartOut(0, 0, 0, 0x14);
    }
}

/* The Wii home menu opens its battle or settings page. */
/* Rectangles are recorded at draw time, after each row's entrance offset.
   Match the foreground anchor so artwork and pointer use identical coordinates. */
typedef struct TbMouseRow { s32 mode,index; f32 x0,y0,x1,y1; } TbMouseRow;
static TbMouseRow sTbMouseRows[16];
static s32 sTbMouseCount;
extern s32 Port_DcNameMouse(s32 page,s32 *row,s32 *col,PortMenuMouse *mouse);
static void Tb_MouseRow(s32 index,f32 x,f32 y,f32 w,f32 h,s32 anchor,s32 page) {
    TbMouseRow *r;
    f32 pivot=anchor==1 ? 0:anchor==2 ? 512:256;
    f32 scale=1333.333f/(f32)Port_PageAspectMilli();
    if(sTbMouseCount>=16) return;
    r=&sTbMouseRows[sTbMouseCount++];r->mode=sTb->mode;r->index=index;
    if(Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli()>1340) {
        r->x0=pivot+(x-pivot)*scale;r->x1=r->x0+w*scale;r->y0=y;r->y1=y+h;
    } else if(page) {
        r->x0=Ps_X(x);r->x1=Ps_X(x+w);r->y0=Ps_Y(y);r->y1=Ps_Y(y+h);
    } else {r->x0=x;r->x1=x+w;r->y0=y;r->y1=y+h;}
}
static void Tb_MouseInput(s32 live) {
    PortMenuMouse mouse;
    s32 i,hit=-1,*cur;
    s32 ready=live && !sTb->wiiNextMode && !sTb->accountWaiting && !sTb->wiiBattleIntro;
    if(Port_WiiArtAvailable() && ((sTb->mode==TB_WII_HOME && sTb->wiiHomeT<23) ||
       ((sTb->mode==TB_WII_BATTLE || sTb->mode==TB_WII_CUSTOM ||
         sTb->mode==TB_WII_FRIEND_MENU || sTb->mode==TB_WII_FRIEND_FORMAT) && sTb->wiiBattleT<30))) ready=0;
    if(sTb->mode==TB_WII_NAME || sTb->mode==TB_WII_FRIEND_CODE)
        ready=ready && Port_DcNameReady() && !sTb->wiiNameExit;
    /* The native account dialog consumes its own mouse edges. */
    if(sTb->disconnectReason && sTb->accountPrompt) return;
    PortMenuMouse_Ready(ready);
    PortMenuMouse_Read(&mouse);
    if(!ready) return;
    if(sTb->mode==TB_WII_NAME || sTb->mode==TB_WII_FRIEND_CODE) {
        hit=Port_DcNameMouse(sTb->lower,&sTb->keyRow,&sTb->keyCol,&mouse);
        if(mouse.wheel) {gPad[0].gamePressed|=PADG_CIRCLE;mouse.wheel=0;}
    } else {
        for(i=sTbMouseCount-1;i>=0;i--) {
            TbMouseRow *r=&sTbMouseRows[i];
            if(r->mode==sTb->mode && mouse.x>=r->x0 && mouse.x<r->x1 && mouse.y>=r->y0 && mouse.y<r->y1) {
                hit=r->index;PortMenuMouse_SetHover(1);break;
            }
        }
        cur=sTb->mode==TB_WII_HOME ? &sTb->wiiCur : sTb->mode==TB_WII_BATTLE ? &sTb->wiiBattleCur :
            (sTb->mode==TB_WII_CUSTOM || sTb->mode==TB_WII_FRIEND_FORMAT) ? &sTb->wiiCustomCur:&sTb->wiiSubCur;
        if(hit>=0 && (mouse.moved || mouse.confirm) && !mouse.wheel && *cur!=hit) {
            sTb->wiiPrevCur=*cur;*cur=hit;sTb->wiiSelectT=0;sTb->wiiFocusFrame=0;sTb->wiiIdleT=0;
            if(sTb->mode==TB_WII_HOME) sTb->wiiDialogTouched=1;
            if(sTb->mode!=TB_WII_BATTLE && sTb->mode!=TB_WII_CUSTOM &&
               sTb->mode!=TB_WII_FRIEND_MENU && sTb->mode!=TB_WII_FRIEND_FORMAT) Tb_WiiDialogSet(Tb_WiiLine());
            Snd_PlaySe(1,0);
        }
        if(sTb->disconnectReason || sTb->mode==TB_WII_REWARD) hit=0;
    }
    Port_SelectionMouseButtons(&mouse,0,hit>=0);
}

static void Tb_WiiUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 *cur = sTb->mode == TB_WII_HOME ? &sTb->wiiCur
             : sTb->mode == TB_WII_BATTLE ? &sTb->wiiBattleCur
             : (sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_FRIEND_FORMAT) ? &sTb->wiiCustomCur : &sTb->wiiSubCur;
    s32 count = sTb->mode == TB_WII_HOME || sTb->mode == TB_WII_FRIENDS ? 3
              : sTb->mode == TB_WII_BATTLE ? 6
              : sTb->mode == TB_WII_FRIEND_FORMAT ? 5
              : sTb->mode == TB_WII_CUSTOM ? (sTb->wiiRanked ? 1 : sTb->wiiNormalBattle ? 3 : 2) : 2;

    if (sTb->mode == TB_WII_REWARD) { Tb_WiiRewardUpdate(live); return; }
    if (sTb->accountWaiting) { Tb_AccountUpdate(); return; }
    if (sTb->disconnectReason) {
        if (sTb->accountPrompt && !sTb->disconnectClosing) {
            if (live && Tb_DisconnectDialogIsOpen()) Tb_AccountConfirmInput();
            return;
        }
        if (sTb->disconnectClosing && Tb_DisconnectDialogIsClosed()) {
            s32 initialized = sTb->accountPrompt == 2;
            Tb_DisconnectDialogTerm();
            sTb->disconnectReason = 0;
            sTb->accountPrompt = 0;
            sTb->disconnectClosing = 0;
            if (initialized) sTb->accountWaiting = 1;
        } else if (!sTb->disconnectClosing && live && Tb_DisconnectDialogIsOpen() &&
                   (pad->gamePressed & (PADG_CROSS | PADG_TRIANGLE))) {
            if (Tb_DisconnectDialogStartClose()) {
                sTb->disconnectClosing = 1;
                sTb->disconnectCloseFrames = 0;
            } else {
                Tb_DisconnectDialogTerm();
                sTb->disconnectReason = 0;
            }
        }
        return;
    }

    if (sTb->mode == TB_WII_VS) {
        if (sTb->found && Port_NetSessionsEnded() != sTb->ended) {
            s32 reason = Port_NetTakeEndReason();
            sTb->found = 0;
            if (reason) Tb_ShowDisconnect(reason);
            Tb_SetMode(sTb->waitBack);
            return;
        }
        if (sTb->wiiNextMode) return;
        if (!sTb->found) {
            s32 state = Port_LobbyPoll();
            if (state == 2) {
                sTb->found = 1;
                sTb->foundAt = live ? sTb->frames : -1;
                sTb->ended = Port_NetSessionsEnded();
                Snd_PlaySe(1, 1);
            } else if (state <= 0) {
                if (state == -3) Tb_ShowLobbyError();
                else if (state == -4) Tb_ShowConnectionError("Players could not reach each other.");
                else if (state < 0) Tb_ShowConnectionError("The room could not be opened.");
                Port_LobbyCancel();
                Tb_SetMode(sTb->waitBack);
                return;
            }
        } else if (sTb->foundAt < 0 && live) {
            sTb->foundAt = sTb->frames;
        } else if (sTb->foundAt >= 0 && sTb->frames - sTb->foundAt >= 180) {
            Port_NetBattleMenuStyle(sTb->wiiNormalBattle | (sTb->wiiFriend << 1) | (sTb->wiiRanked << 2));
            Port_LobbyLaunch();
            return;
        }
        /* Once the peer handshake completes, only disconnect handling or the
           automatic battle launch can leave this screen. Ignore menu input. */
        if (sTb->found) return;
        if (live && (pad->gamePressed & PADG_TRIANGLE)) {
            Snd_PlaySe(1, 2);
            Port_LobbyCancel();
            sTb->wiiNextMode = sTb->waitBack;
            ColorFade_StartOut(0, 0, 0, 0x14);
        }
        return;
    }

    if (sTb->mode == TB_WII_RANKING) {
        Port_RankingRead(sTb->rankingOffset, sTb->rankingRefresh, &sTb->ranking);
        if (!live) return;
        if (pad->gamePressed & PADG_TRIANGLE) {
            Snd_PlaySe(1, 2);
            sTb->wiiNextMode = TB_WII_BATTLE;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else if (sTb->ranking.state == 1) {
            if ((pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) && sTb->rankingOffset + 3 < sTb->ranking.total) {
                sTb->rankingOffset += 3; Snd_PlaySe(1, 0);
            } else if ((pad->gameRepeat & (PADG_UP | PADG_UP2)) && sTb->rankingOffset > 0) {
                sTb->rankingOffset -= 3; Snd_PlaySe(1, 0);
            }
        }
        return;
    }
    if (!live) {
        return;
    }
    if (sTb->mode == TB_WII_HOME && sTb->wiiHomeT < 23 && Port_WiiArtAvailable()) {
        return; /* the Wii movie enables its buttons after fl_btn_in_1 */
    }
    if ((sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM ||
         sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT) &&
        sTb->wiiBattleT < 30 && Port_WiiArtAvailable()) {
        return;
    }
    if (sTb->mode != TB_WII_BATTLE && sTb->mode != TB_WII_CUSTOM &&
        sTb->mode != TB_WII_FRIEND_MENU && sTb->mode != TB_WII_FRIEND_FORMAT && !sTb->wiiDialogStarted)
        Tb_WiiDialogSet(sTb->mode == TB_WII_HOME ? 0 : Tb_WiiLine());
    if (sTb->mode == TB_WII_HOME && !sTb->wiiDialogTouched && sTb->wiiDialogLine == 0 &&
        sTb->wiiDialogT > Port_WiiDialogDuration(0, sTb->wiiDialogJapanese) + 12 &&
        Port_WiiDialogDuration(0, sTb->wiiDialogJapanese) > 0)
        Tb_WiiDialogSet(1); /* Giru answers Pan's opening line. */
    else if (sTb->mode == TB_WII_HOME && !sTb->wiiDialogTouched && sTb->wiiDialogLine == 1 &&
             sTb->wiiDialogT > Port_WiiDialogDuration(1, sTb->wiiDialogJapanese) + 12 &&
             Port_WiiDialogDuration(1, sTb->wiiDialogJapanese) > 0)
        Tb_WiiDialogSet(Tb_WiiLine()); /* Pan describes the initially selected entry. */
    if (sTb->mode == TB_WII_HOME && sTb->wiiBattleIntro) {
        s32 duration = Port_WiiDialogDuration(sTb->wiiDialogLine, sTb->wiiDialogJapanese);
        /* Missing lip assets still leave time to read the translated subtitle. */
        if (duration <= 0) duration = 180;
        if (sTb->wiiDialogT > duration + 12) {
            if (sTb->wiiDialogLine == 9) {
                Tb_WiiDialogSet(10); /* Giru answers Pan in the selected voice language. */
            } else {
                sTb->wiiNextMode = TB_WII_BATTLE;
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
        return; /* Keep selection input from interrupting the exchange. */
    }
    if (sTb->mode == TB_WII_HOME) {
        if (pad->gameHeld || pad->gamePressed || sTb->wiiNextMode) {
            sTb->wiiIdleT = 0;
        } else if (++sTb->wiiIdleT >= 30 * 60) {
            s32 choices[4], count = 0, i, line;
            if (sTb->wiiIdleUsed == 15) sTb->wiiIdleUsed = 0;
            for (i = 0; i < 4; i++) {
                if (!(sTb->wiiIdleUsed & (1 << i)) && i + 14 != sTb->wiiIdleLast)
                    choices[count++] = i;
            }
            /* Shuffle each group of four, avoiding a repeat across groups. */
            line = choices[Rand_Range(count)];
            sTb->wiiIdleUsed |= 1 << line;
            sTb->wiiIdleLast = line + 14;
            sTb->wiiIdleT = 0;
            sTb->wiiDialogTouched = 1;
            sTb->wiiPanPointing = !sTb->wiiPanPointing;
            Tb_WiiDialogSet(sTb->wiiIdleLast);
        }
    }
    {
        s32 previous = *cur;
        Ps_Move(cur, count);
        if (*cur != previous) {
            sTb->wiiPrevCur = previous;
            sTb->wiiSelectT = 0;
            sTb->wiiFocusFrame = 0;
            if (sTb->mode == TB_WII_HOME) sTb->wiiDialogTouched = 1;
        }
        if (*cur != previous && sTb->mode != TB_WII_BATTLE && sTb->mode != TB_WII_CUSTOM &&
            sTb->mode != TB_WII_FRIEND_MENU && sTb->mode != TB_WII_FRIEND_FORMAT)
            Tb_WiiDialogSet(Tb_WiiLine());
    }
    /* Verification locks activation only: cursor movement and Back remain available. */
    if ((pad->gamePressed & PADG_CROSS) && Tb_DataState() != 1) {
        Snd_PlaySe(1, 1);
        return;
    }
    if ((pad->gamePressed & PADG_CROSS) && Tb_DataState() == 1) {
        if (sTbLan && sTb->mode == TB_WII_BATTLE && *cur >= 2) return;
        if (sTb->mode == TB_WII_HOME) {
            if (*cur == 0) {
                if (Tb_HomeDisabled(sTbWiiHomeVisible[*cur])) return;
                Snd_PlaySe(1, 1);
                sTb->wiiBattleCur = 0;
                sTb->wiiDialogTouched = 1;
                sTb->wiiPanPointing = 1;
                Port_NetRewardBegin();
                sTb->wiiBattleIntro = 1;
                Tb_WiiDialogSet(9);
            } else if (*cur == 1) {
                /* Manage Friends responds to focus and selection, but has no PC service. */
                Snd_PlaySe(1, 1);
            } else if (*cur == 2) {
                Snd_PlaySe(1, 1);
                sTb->wiiSubCur = 0;
                Tb_SetMode(TB_WII_SETTINGS);
            }
        } else {
            Snd_PlaySe(1, 1);
            if (sTb->mode == TB_WII_BATTLE && *cur == 2) {
                sTb->wiiRanked = 0;
                sTb->wiiFriend = 1;
                Tb_SetWiiTitles();
                sTb->wiiSubCur = 0;
                sTb->wiiNextMode = TB_WII_FRIEND_MENU;
                ColorFade_StartOut(0, 0, 0, 0x14);
            } else if (sTb->mode == TB_WII_FRIEND_MENU) {
                if (*cur == 0) {
                    sTb->host = 1;
                    sTb->wiiCustomCur = 0;
                    sTb->wiiNextMode = TB_WII_FRIEND_FORMAT;
                    ColorFade_StartOut(0, 0, 0, 0x14);
                } else {
                    if (!Port_DcNameOpen()) return;
                    sTb->typed[0] = 0;
                    sTb->typedLen = 0;
                    sTb->nameCursor = 0;
                    sTb->lower = 0;
                    sTb->keyCol = sTb->keyRow = 0;
                    sTb->wiiNextMode = TB_WII_FRIEND_CODE;
                    ColorFade_StartOut(0, 0, 0, 0x14);
                }
            } else if (sTb->mode == TB_WII_BATTLE && (*cur == 0 || *cur == 1 || *cur == 3)) {
                sTb->wiiFriend = 0;
                sTb->wiiRanked = *cur == 3;
                sTb->wiiNormalBattle = *cur == 1;
                Tb_SetWiiTitles();
                sTb->wiiCustomCur = 0;
                sTb->wiiNextMode = TB_WII_CUSTOM;
                ColorFade_StartOut(0, 0, 0, 0x14);
            } else if (sTb->mode == TB_WII_BATTLE && *cur == 4) {
                sTb->wiiNextMode = TB_WII_RANKING;
                ColorFade_StartOut(0, 0, 0, 0x14);
            } else if (sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_FRIEND_FORMAT) {
                if (Tb_Verified()) {
                    sTb->wiiNextMode = TB_WII_VS;
                    ColorFade_StartOut(0, 0, 0, 0x14);
                }
            } else if (sTb->mode == TB_WII_SETTINGS &&
                       (sTbWiiSettingsVisible[*cur] == 0 || sTbWiiSettingsVisible[*cur] == 3)) {
                if (!Port_DcNameOpen()) {
                    return;
                }
                sTb->accountCode = 0;
                Ps_Copy(sTb->typed, Port_NetNameGet(), sizeof(sTb->typed));
                sTb->typed[16] = 0; /* matches the online window's 16-character field */
                sTb->typedLen = Ps_Len(sTb->typed);
                sTb->nameCursor = sTb->typedLen;
                sTb->keyRow = 0;
                sTb->keyCol = 0;
                sTb->lower = 0;
                Tb_SetMode(TB_WII_NAME);
            } else if (sTb->mode == TB_WII_SETTINGS && sTbWiiSettingsVisible[*cur] == 2) {
                if (!Port_AccountLogged(Port_NetNameGet())) {
                    if (Port_AccountBegin(Port_NetNameGet(), NULL)) sTb->accountWaiting = 1;
                    else if (Port_AccountPoll(NULL, 0) == 1) sTb->accountWaiting = 2;
                    else Tb_AccountMessage("account/unavailable");
                }
            } else {
                /* Leave unsupported Wii options without a port-specific dialog. */
            }
        }
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        Snd_PlaySe(1, 2);
        if (sTb->mode == TB_WII_FRIEND_FORMAT) {
            sTb->wiiNextMode = TB_WII_FRIEND_MENU;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else if (sTb->mode == TB_WII_FRIEND_MENU) {
            sTb->wiiNextMode = TB_WII_BATTLE;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else if (sTb->mode == TB_WII_CUSTOM) {
            sTb->wiiNextMode = TB_WII_BATTLE;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else if (sTb->mode == TB_WII_BATTLE) {
            s32 gained = Port_NetRewardTake();
            sTb->rewardZPoints = gained * 10;
            sTb->wiiNextMode = gained > 0 ? TB_WII_REWARD : TB_WII_HOME;
            ColorFade_StartOut(0, 0, 0, 0x14);
        } else if (sTb->mode == TB_WII_FRIENDS || sTb->mode == TB_WII_SETTINGS) {
            Tb_SetMode(TB_WII_HOME);
        } else if (sTb->mode == TB_WII_RANKING) {
            Tb_SetMode(TB_WII_BATTLE);
        } else {
            sTb->leaving = 1;
            ColorFade_StartOut(0, 0, 0, 0x14);
        }
    }
}

static s32 Tb_NameKey(s32 c) {
    s32 row, col, page;
    for (page = 0; page < 2; page++)
        for (row = 0; row < WII_KEY_ROWS; row++)
            for (col = 0; col < WII_KEY_COLS; col++)
                if (Port_DcNameKey(page, row, col) == c) return 1;
    return 0;
}

static void Tb_NameAdd(s32 c) {
    if (sTb->mode == TB_WII_FRIEND_CODE || sTb->accountCode) {
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return;
    }
    if (Tb_NameKey(c) && sTb->typedLen < ((sTb->mode == TB_WII_FRIEND_CODE || sTb->accountCode) ? CODE_LEN : 16)) {
        s32 i;
        for (i = sTb->typedLen; i >= sTb->nameCursor; i--)
            sTb->typed[i + 1] = sTb->typed[i];
        sTb->typed[sTb->nameCursor++] = (char)c;
        sTb->typedLen++;
        Snd_PlaySe(1, 0);
    }
}

static void Tb_NameBack(void) {
    if (sTb->typedLen > 0) {
        s32 i;
        if (sTb->nameCursor > 0) sTb->nameCursor--;
        for (i = sTb->nameCursor + 1; i <= sTb->typedLen; i++)
            sTb->typed[i - 1] = sTb->typed[i];
        sTb->typedLen--;
        Snd_PlaySe(1, 2);
    }
}

/* The Data Center password keyboard has 14 columns, four rows, and three wide
   buttons underneath: Quit, Case, Decide. Its side cells move the text cursor. */
static s32 Tb_WiiNameKey(void) {
    Pad *pad = &gPad[0];
    s32 col = sTb->keyCol;
    s32 row = sTb->keyRow;
    s32 next;

    /* Several cells belong to one wide key (including three cells for '0').
       Match Data Center by skipping cells with the same key value. */
    if (pad->gameRepeat & (PADG_RIGHT | PADG_RIGHT2)) {
        next = Port_DcNameKey(sTb->lower, row, col);
        do { col = (col + 1) % WII_KEY_COLS; }
        while (Port_DcNameKey(sTb->lower, row, col) == next);
    } else if (pad->gameRepeat & (PADG_LEFT | PADG_LEFT2)) {
        next = Port_DcNameKey(sTb->lower, row, col);
        do { col = (col + WII_KEY_COLS - 1) % WII_KEY_COLS; }
        while (Port_DcNameKey(sTb->lower, row, col) == next);
    }
    if (pad->gameRepeat & (PADG_DOWN | PADG_DOWN2)) {
        next = Port_DcNameKey(sTb->lower, row, col);
        do { row = (row + 1) % (WII_KEY_ROWS + 1); }
        while (Port_DcNameKey(sTb->lower, row, col) == next);
    } else if (pad->gameRepeat & (PADG_UP | PADG_UP2)) {
        next = Port_DcNameKey(sTb->lower, row, col);
        do { row = (row + WII_KEY_ROWS) % (WII_KEY_ROWS + 1); }
        while (Port_DcNameKey(sTb->lower, row, col) == next);
    }
    if (row == WII_KEY_ROWS) col = col < 4 ? 0 : col < 10 ? 4 : 10;
    if (row == 3 && col < 3) col = 0;
    if (row == 3 && col == 13) col = 12;
    if (row != sTb->keyRow || col != sTb->keyCol) {
        sTb->keyRow = row;
        sTb->keyCol = col;
        Snd_PlaySe(1, 0);
        return 0;
    }
    sTb->keyRow = row;
    sTb->keyCol = col;
    if (!(pad->gamePressed & PADG_CROSS)) return 0;
    Port_DcNamePress(sTb->lower, row, col);
    if (row == WII_KEY_ROWS) return col == 0 ? -1 : col == 4 ? -2 : -3;
    if ((row == 1 || row == 2) && col >= 12) return col == 12 ? -4 : -5;
    if (row == 3 && col >= 12) return -6;
    return Port_DcNameKey(sTb->lower, row, col);
}

static void Tb_WiiNameUpdate(s32 live) {
    Pad *pad = &gPad[0];
    s32 c, done = 0;

    Port_NativeKeyboardSet(live && !sTb->wiiNameExit && Port_DcNameReady());
    if (!live) return;
    if (sTb->wiiNameExit) {
        sTb->wiiNameExitT++;
        if (Port_DcNameIsClosed() && sTb->wiiNameExitT > 29) {
            s32 next = sTb->wiiNameExit;
            s32 join = sTb->wiiNameJoin;
            if (sTb->accountSubmit) {
                sTb->accountSubmit = 0;
                sTb->accountCode = 0;
                Tb_SetMode(TB_WII_SETTINGS);
                if (Port_AccountBegin(Port_NetNameGet(), sTb->typed)) sTb->accountWaiting = 1;
                else Tb_AccountMessage("account/unavailable");
            } else if (join) Tb_WiiFriendJoinStart();
            else { sTb->accountCode = 0; Tb_SetMode(next); }
        }
        return;
    }
    if (!Port_DcNameReady()) return;
    /* Native characters share the same validation and insertion path as the
       on-screen keyboard. Enter/Backspace preserve confirm/delete behavior. */
    c = Port_NativeKeyboardPage();
    if (c >= 0) sTb->lower = c;
    c = Port_NativeKeyboardPop();
    if (c > 0) { Tb_NameAdd(c); return; }
    if (c == PORT_KEYBOARD_CONFIRM) pad->gamePressed |= PADG_CROSS;
    else if (c == PORT_KEYBOARD_DELETE) pad->gamePressed |= PADG_TRIANGLE;
    if (pad->gameRepeat & (PADG_L1 | PADG_L2)) {
        if (sTb->nameCursor > 0) sTb->nameCursor--;
    } else if (pad->gameRepeat & (PADG_R1 | PADG_R2)) {
        if (sTb->nameCursor < sTb->typedLen) sTb->nameCursor++;
    } else if (pad->gamePressed & PADG_CIRCLE) {
        sTb->lower = !sTb->lower;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_START) {
        sTb->keyRow = WII_KEY_ROWS;
        sTb->keyCol = 10;
        Snd_PlaySe(1, 1);
    } else if (pad->gamePressed & PADG_TRIANGLE) {
        if (sTb->typedLen == 0) done = -1;
        else Tb_NameBack();
    } else {
        c = Tb_WiiNameKey();
        if (c > 0) Tb_NameAdd(c);
        else if (c == -1) done = -1;
        else if (c == -2) {
            sTb->lower = !sTb->lower;
            Snd_PlaySe(1, 1);
        } else if (c == -3) done = 1;
        else if (c == -4 && sTb->nameCursor > 0) sTb->nameCursor--;
        else if (c == -5 && sTb->nameCursor < sTb->typedLen) sTb->nameCursor++;
        else if (c == -6) Tb_NameBack();
    }
    if (done == 1) {
        if (sTb->accountCode) {
            if (sTb->typedLen != CODE_LEN) { Tb_AccountMessage("account/code-length"); return; }
            sTb->accountSubmit = 1;
            sTb->wiiNameExit = TB_WII_SETTINGS;
            Port_DcNameStartClose();
            return;
        }
        if (sTb->mode == TB_WII_FRIEND_CODE) {
            if (sTb->typedLen != CODE_LEN) {
                Tb_ShowRoomCodeMessage();
                Snd_PlaySe(1, 2);
                return;
            }
            if (!Tb_Verified()) return;
            Ps_Copy(sTb->code, sTb->typed, sizeof(sTb->code));
            sTb->wiiNameExit = TB_WII_VS;
            sTb->wiiNameJoin = 1;
            Port_DcNameStartClose();
            return;
        }
        {
            const char *error = Port_UsernameError(sTb->typed);
            if (error) { Tb_AccountMessage(error); return; }
        }
        Port_NetNameSet(sTb->typed);
        Port_AccountRestore(Port_NetNameGet());
        Port_SettingTextSave("net_name", sTb->typed);
        Port_SettingsWrite();
        Tb_Note("PLAYER NAME SAVED");
        Snd_PlaySe(1, 1);
        sTb->wiiNameExit = TB_WII_SETTINGS;
        Port_DcNameStartClose();
    } else if (done == -1) {
        Snd_PlaySe(1, 2);
        sTb->wiiNameExit = sTb->mode == TB_WII_FRIEND_CODE ? TB_WII_FRIEND_MENU : TB_WII_SETTINGS;
        Port_DcNameStartClose();
    }
}

/* ---------------------------------------------------------------------------------------------- what is drawn */
/* ---------------------------------------------------------------------------------------------- the pictures */
/* (how a screen draws: menu/port_screen.c) */
static f32 Tb_TitleLeft(void) {
    return Ps_Left() + 6.0f;
}

extern s32 Port_WiiHomeMotionQuarterMilli(s32 track, s32 quarterFrame);

static f32 Tb_WiiMotionQuarter(s32 track, s32 quarterFrame) {
    return (f32)Port_WiiHomeMotionQuarterMilli(track, quarterFrame) / 1000.0f;
}
static u16 *Tb_BoxLine(void);

/* Original Wii landscape or battle-space background. */
static void Tb_Backdrop(void) {
    s32 i;

    if (Port_WiiArtAvailable()) {
        if (sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_VS ||
            sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT ||
            sTb->mode == TB_WII_FRIEND_CODE || sTb->mode == TB_WII_RANKING) {
            f32 nebulaPulse = (1.0f + cosf((f32)sTb->frames * 0.025f)) * 0.5f;
            for (i = 0; i < 8; i++) {
                f32 x = (f32)(i % 4) * 128.0f;
                f32 y = (f32)(i / 4) * 224.0f;
                Ps_SpriteScreen(TBO_WII_BATTLE_BG_0 + i, x, y, x + 128.0f, y + 224.0f,
                                0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
            }
            /* The extracted space layer belongs inside the clock's circular
               window, behind the rows and the VS panels. */
            Ps_SpriteScreen(TBO_WII_BATTLE_NEBULA, 101.0f, 43.0f, 411.0f, 401.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN,
                            0x58 + (s32)(nebulaPulse * 0x28));
            for (i = 0; i < 2; i++) {
                /* The glowing large stars sit around the source sheet's rim.
                   Keep the small central stars at their original screen size. */
                f32 starPulse = (1.0f + cosf((f32)sTb->frames * 0.041f + (f32)i * 1.7f)) * 0.5f;
                f32 srcX0 = i ? 256.0f : 120.0f;
                f32 srcX1 = i ? 390.0f : 256.0f;
                f32 x0 = 101.0f + srcX0 * (310.0f / 512.0f);
                f32 x1 = 101.0f + srcX1 * (310.0f / 512.0f);
                Ps_SpriteScreen(TBO_WII_BATTLE_STARS_0 + i,
                                x0, 43.0f + 110.0f * (358.0f / 512.0f),
                                x1, 43.0f + 390.0f * (358.0f / 512.0f),
                                (srcX0 - (f32)i * 256.0f) / 256.0f, 110.0f / 512.0f,
                                (srcX1 - (f32)i * 256.0f) / 256.0f, 390.0f / 512.0f, PS_PLAIN,
                                0x60 + (s32)(starPulse * 0x20));
            }
            return;
        }
        f32 cloudPhase = (f32)(sTb->frames % 1280) * 0.4f;
        /* Resource 1's eight 128x256 strips form the original 512x512 landscape. */
        for (i = 0; i < 8; i++) {
            f32 x = (f32)(i % 4) * 128.0f;
            f32 y = (f32)(i / 4) * 224.0f;
            Ps_SpriteScreen(TBO_WII_BG_0 + i, x, y, x + 128.0f, y + 224.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        }
        /* Start at the Wii movie's (0, -10) placement. Adjacent copies make
           the cloud sheet travel left at constant speed and wrap in a loop. */
        for (i = -1; i <= 1; i++) {
            f32 x = (f32)i * 512.0f - cloudPhase;
            Ps_SpriteScreen(TBO_WII_CLOUDS, x, -10.0f, x + 512.0f, 118.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        }
        for (i = 0; i < 2; i++) {
            f32 glow = Tb_WiiMotionQuarter(TBO_WII_MOTION_SPARKLE_0 + i, sTb->frames * 3);
            s32 alpha = (s32)(glow * 128.0f);
            Ps_SpriteScreen(TBO_WII_SPARKLE_0 + i, 36.0f, 115.0f, 292.0f, 179.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
        }
        return;
    }
    if (!Port_WiiArtAvailable()) {
        Ps_Rect(0.0f, 0.0f, 512.0f, 448.0f, Ps_Rgba(0x20, 0x27, 0x2C, 0x80), Ps_Rgba(0x08, 0x15, 0x1A, 0x80));
        return;
    }

}

/* Home artwork decoded from DragonNe section 0. Each option has its own
   unselected and selected label in resource 5, so moving the cursor uses
   the Wii artwork instead of recoloring a captured frame. */
/* Five complete, smooth up/down cycles over the Wii movie's 500-frame idle
   span. The cosine eases into each turn and joins the next cycle seamlessly. */
static f32 Tb_WiiIdleBob(s32 quarter, f32 height) {
    s32 phase = quarter % 400;
    return -cosf((f32)phase * (6.283185307f / 400.0f)) * height;
}

#define TB_WII_NATIVE_GIRU_W 256.0f
#define TB_WII_NATIVE_GIRU_H 128.0f

/* The documented MsgWin policy: wide backing, left-anchored proportional glyphs. */
static void Tb_DrawMessage(void) {
    extern void Port_MsgWinDrawString43(u16 *str);
    Ps_Foreground43End();
    Port_MsgWinDrawString43(Tb_BoxLine());
    Ps_Foreground43Begin();
}

/* Leave room at the right without exposing a parked row after left anchoring. */
static s32 Tb_RowProportional(void) {
    extern s32 Port_TitleMainChars43(void), Port_Menus43(void), Port_PageAspectMilli(void);
    return Port_TitleMainChars43() && !Port_Menus43() && Port_PageAspectMilli() > 1340;
}
static f32 Tb_RowInset(void) {
    extern s32 Port_PageAspectMilli(void);
    if (!Tb_RowProportional()) return 0.0f;
    f32 narrow = 1333.333f / (f32)Port_PageAspectMilli();
    f32 right = sTb->mode == TB_WII_HOME || sTb->mode == TB_WII_REWARD ?
        Tb_WiiMotionQuarter(TBO_WII_MOTION_BARS_X, 120) + 461.0f :
        170.0f + 5.0f * TB_WII_BATTLE_ROW_STEP_X + 512.0f;
    f32 inset = right - 496.0f / narrow; /* At least 16 screen units of right-side space. */
    return inset > 0.0f ? inset : 0.0f;
}
/* This correction is for wider pages only. On a 4:3 page it shifts
   every Battle / Custom / Friend row and its selection radar 120px right. */
static f32 Tb_RowOffset(void) {
    return Port_PageAspectMilli() > 1340 ? TB_WII_MODE_ROW_OFFSET_X : 0.0f;
}
static f32 Tb_RowX(f32 animated, f32 resting) {
    extern s32 Port_PageAspectMilli(void);
    f32 offset = Tb_RowOffset();
    if (!Tb_RowProportional())
        return animated + offset * (512.0f - animated) / (512.0f - resting);
    f32 narrow = 1333.333f / (f32)Port_PageAspectMilli();
    f32 destination = resting - Tb_RowInset() + offset;
    /* At animated=512, transformed x is still exactly the right screen edge. */
    return destination + (animated - resting) * (512.0f / narrow - destination) / (512.0f - resting);
}

/* Match User Settings' original resting row coordinates, with identical centre anchor. */
static f32 Tb_HomeRowX(f32 animated) {
    extern s32 Port_PageAspectMilli(void);
    f32 resting = Tb_WiiMotionQuarter(TBO_WII_MOTION_BARS_X, 120);
    f32 boundary = 512.0f;
    if (Tb_RowProportional()) {
        f32 narrow = 1333.333f / (f32)Port_PageAspectMilli();
        boundary = 256.0f + 256.0f / narrow;
    }
    return 162.0f + (animated - resting) * (boundary - 162.0f) / (512.0f - resting);
}

static void Tb_DrawWiiHomeArt(void) {
    s32 i;
    s32 idleQuarter = (sTb->frames * 3) % 2000;
    s32 idleFrame = idleQuarter / 4;
    s32 giruIdleQuarter = (idleQuarter + 500) % 2000;
    s32 entryQuarter = sTb->wiiHomeT * 3;
    f32 barX = Tb_WiiMotionQuarter(TBO_WII_MOTION_BARS_X, entryQuarter);
    f32 panX = Tb_WiiMotionQuarter(TBO_WII_MOTION_PAN_ENTRY_X, entryQuarter) + 11.95f;
    f32 panY = Tb_WiiIdleBob(idleQuarter, 6.0f);
    f32 giruX = Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_X, entryQuarter) - 184.05f;
    f32 giruY = Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_Y, entryQuarter) - 71.85f;
    f32 giruHover = Tb_WiiIdleBob(giruIdleQuarter, 4.5f);
    f32 radarScale = Tb_WiiFocusStretch();
    f32 iconDx = 0.0f, iconDy = 0.0f;
    f32 titleDx = 0.0f, titleDy = 0.0f;
    s32 iconAlpha = 0x80, titleAlpha = 0x80;
    f32 iconScaleX = 1.0f, iconScaleY = 1.0f;
    f32 titleScaleX = 1.0f, titleScaleY = 1.0f;

    IconWin_PortHeadingMotion(sTb->frames, &iconDx, &iconDy, &iconAlpha,
                              &iconScaleX, &iconScaleY, &titleDx, &titleDy,
                              &titleAlpha, &titleScaleX, &titleScaleY);
    /* The reversed 256x128 Wii sprite has 85 pixels of transparent canvas
       before its visible art. Set its resting bounds to x=346..512. */
    giruX -= 117.0f;

    /* The entrance plays at 3/4 speed. The idle bob has smooth turns at the
       same cadence, with independent Pan and Giru phases. */

    {
        s32 pointing = sTb->wiiPanPointing;
        s32 lipOpen = sTb->wiiDialogVoiceStarted && sTb->wiiDialogLine != 1 && sTb->wiiDialogLine != 10 &&
                      sTb->wiiDialogT <= Port_WiiDialogDuration(sTb->wiiDialogLine, sTb->wiiDialogJapanese) &&
                      Port_WiiDialogMouth(sTb->wiiDialogLine, sTb->wiiDialogT, sTb->wiiDialogJapanese);
        /* The two full-body textures contain Pan's widest mouth. The mouth
           overlays are only the half-open and closed shapes. */
        s32 mouthShape = !lipOpen ? 0 : (sTb->wiiDialogT / 10 & 1) ? 1 : 2;
        s32 blink = (idleFrame >= 80 && idleFrame < 88) || (idleFrame >= 200 && idleFrame < 208) ||
                    (idleFrame >= 320 && idleFrame < 328) || (idleFrame >= 440 && idleFrame < 448);
        f32 mouthX = (pointing ? 70.0f : 100.0f) + panX;
        f32 mouthY = (pointing ? 183.0f : 197.0f) + panY;

        Ps_SpriteScreen(pointing ? TBO_WII_PAN_POINT : TBO_WII_PAN,
                        -13.0f + panX, 9.0f + panY, 243.0f + panX, 521.0f + panY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        /* The base art already has open eyes. Only cover them for a blink. */
        if (blink) {
            f32 eyeX = (pointing ? 56.0f : 79.0f) + panX;
            f32 eyeY = (pointing ? 158.0f : 176.0f) + panY;
            Ps_SpriteScreen(pointing ? TBO_WII_PAN_POINT_EYES_CLOSED : TBO_WII_PAN_EYES_CLOSED,
                            eyeX, eyeY, eyeX + 64.0f, eyeY + 32.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        }
        if (mouthShape != 2)
            Ps_SpriteScreen(pointing ? (mouthShape ? TBO_WII_PAN_POINT_MOUTH_OPEN : TBO_WII_PAN_POINT_MOUTH_CLOSED)
                                     : (mouthShape ? TBO_WII_PAN_MOUTH_OPEN : TBO_WII_PAN_MOUTH_CLOSED),
                            mouthX, mouthY, mouthX + 32.0f, mouthY + 32.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    }
    Ps_SpriteScreen(TBO_WII_GIRU, 378.0f + giruX, 233.0f + giruY + giruHover,
                    378.0f + giruX + TB_WII_NATIVE_GIRU_W,
                    233.0f + giruY + giruHover + TB_WII_NATIVE_GIRU_H,
                    1.0f, 0.0f, 0.0f, 1.0f, PS_PLAIN, 0x80);
    {
        s32 giruTalking = sTb->wiiDialogVoiceStarted && (sTb->wiiDialogLine == 1 || sTb->wiiDialogLine == 10) &&
                          sTb->wiiDialogT <= Port_WiiDialogDuration(1, sTb->wiiDialogJapanese) &&
                          Port_WiiDialogMouth(1, sTb->wiiDialogT, sTb->wiiDialogJapanese);
        s32 eye = giruTalking ? TBO_WII_GIRU_EYE_BRIGHT : TBO_WII_GIRU_EYE_DIM;
        Ps_SpriteScreen(eye, 378.0f + giruX, 233.0f + giruY + giruHover,
                        378.0f + giruX + TB_WII_NATIVE_GIRU_W,
                        233.0f + giruY + giruHover + TB_WII_NATIVE_GIRU_H,
                        1.0f, 0.0f, 0.0f, 1.0f, PS_PLAIN, 0x80);
    }
    if (sTb->mode != TB_WII_REWARD) for (i = 0; i < 3; i++) {
        f32 y = 91.0f + (f32)i * 47.0f;
        s32 entry = sTbWiiHomeVisible[i];
        s32 selectAlpha = Tb_WiiFocusAlpha(i, sTb->wiiCur);
        u32 tint = Tb_HomeDisabled(entry) ? Ps_Rgba(0x58, 0x58, 0x58, 0x80) : PS_PLAIN;
        if (barX >= 512.0f) continue;
        f32 rowX = Tb_HomeRowX(barX);
        Tb_MouseRow(i,rowX,y,461,47,0,0);
        Ps_Foreground43Anchor(0);
        Ps_SpriteScreen(TBO_WII_BAR_BLUE, rowX, y, rowX + 461.0f, y + 58.0f,
                        0.0f, 0.0f, 1.0f, 1.0f, tint, 0x80);
        if (selectAlpha > 0)
            Ps_SpriteScreen(TBO_WII_BAR_GOLD, rowX, y, rowX + 461.0f, y + 58.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, tint, selectAlpha);
        if (entry == 3 || PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_HOME_ROW + entry])) {
            Tb_WiiLabelText(sTbWiiHomeName[entry], rowX + 49.0f, y + 2.0f,
                            selectAlpha, tint);
        } else {
            Tb_WiiTextureLabel(TBO_WII_LABEL_IDLE, TBO_WII_LABEL_SELECTED,
                               rowX + 49.0f, y + 2.0f,
                               (f32)entry * 0.25f, (f32)(entry + 1) * 0.25f, selectAlpha, tint,
                               sTboWiiLabelCapHeight[0]);
        }
        Ps_Foreground43Anchor(0);
    }
    /* The rows finish sliding in at source frame 17 (port frame 23).
       Show the radar afterward and put it directly on the selected row. */
    if (sTb->mode != TB_WII_REWARD && sTb->wiiHomeT >= 23) {
        f32 radarY = 108.0f + 47.0f * (f32)sTb->wiiCur;
        s32 radarAlpha = (sTb->wiiHomeT - 22) * 16;
        if (radarAlpha > 0x80) radarAlpha = 0x80;
        Ps_Foreground43Anchor(0);
        f32 radarX = 196.0f;
        Ps_SpriteScreen(TBO_WII_RADAR, radarX - 32.0f * radarScale, radarY - 32.0f * radarScale,
                        radarX + 32.0f * radarScale, radarY + 32.0f * radarScale,
                        0.0f, 0.0f, 1.0f, 1.0f,
                        Tb_HomeDisabled(sTbWiiHomeVisible[sTb->wiiCur]) ? Ps_Rgba(0x58, 0x58, 0x58, 0x80) : PS_PLAIN,
                        radarAlpha);
        Ps_Foreground43Anchor(0);
    }
    /* The native mc_title grows horizontally from its left edge. */
    if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_HOME_TITLE]))
        Tb_WiiTitleText(TB_WII_TX_HOME_TITLE, 62.0f + titleDx, 17.0f + titleDy,
                        titleScaleX, titleScaleY, titleAlpha);
    else
        Ps_SpriteScreen(TBO_WII_TITLE, 62.0f + titleDx,
                        17.0f + titleDy,
                        62.0f + titleDx + 512.0f * titleScaleX,
                        17.0f + titleDy + 64.0f * titleScaleY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, titleAlpha);
    Ps_SpriteScreen(TBO_WII_CREST, 45.0f + iconDx - 32.0f * iconScaleX,
                    48.0f + iconDy - 32.0f * iconScaleY,
                    45.0f + iconDx + 32.0f * iconScaleX,
                    48.0f + iconDy + 32.0f * iconScaleY,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, iconAlpha);
    Tb_DrawMessage();
}

static void Tb_DrawWiiBattle(void) {
    s32 i;
    s32 friendMenu = sTb->mode == TB_WII_FRIEND_MENU;
    s32 friendFormat = sTb->mode == TB_WII_FRIEND_FORMAT;
    s32 custom = sTb->mode == TB_WII_CUSTOM || friendFormat || friendMenu;
    s32 rows = friendMenu ? 2 : friendFormat ? 5 : custom ? (sTb->wiiRanked ? 1 : sTb->wiiNormalBattle ? 3 : 2) : 6;
    f32 giruIn = (f32)sTb->wiiBattleT / 29.0f;
    f32 giruHover = Tb_WiiIdleBob(((sTb->frames * 3) + 500) % 2000, 4.5f);
    f32 giruX, giruY;
    f32 iconDx = 0.0f, iconDy = 0.0f, titleDx = 0.0f, titleDy = 0.0f;
    f32 iconScaleX = 1.0f, iconScaleY = 1.0f, titleScaleX = 1.0f, titleScaleY = 1.0f;
    f32 radarScale = Tb_WiiFocusStretch();
    s32 iconAlpha = 0x80, titleAlpha = 0x80;
    u32 disabled = Ps_Rgba(0x58, 0x58, 0x58, 0x80);

    Ps_SetShift(0.0f);
    /* Giru's section 1 texture is 256x128. Its visible pixels begin five
       pixels into the canvas, so x=0 shows him fully at the left edge. */
    if (giruIn > 1.0f) giruIn = 1.0f;
    giruIn = giruIn * giruIn * (3.0f - 2.0f * giruIn);
    giruX = -TB_WII_NATIVE_GIRU_W + TB_WII_NATIVE_GIRU_W * giruIn;
    giruY = 82.0f + giruHover;
    Ps_SpriteScreen(TBO_WII_BATTLE_GIRU,
                    giruX, giruY, giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_BATTLE_GIRU_EYE_DIM,
                    giruX, giruY, giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);

    if (((!custom && (sTb->wiiBattleCur == 3 || sTb->wiiBattleCur == 4)) ||
         (sTb->mode == TB_WII_CUSTOM && sTb->wiiRanked)) && sTb->wiiBattleT >= 30) {
        extern int Port_AccountRankPoints(const char *);
        s32 rankPoints = Port_AccountRankPoints(Port_NetNameGet());
        Ps_SpriteScreen(TBO_WII_BATTLE_POINT_PANEL, -40.0f, 234.0f, 172.0f, 344.0f,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        Ps_SpriteScreen(TBO_WII_BATTLE_POINT_LABEL, 15.0f, 239.0f, 220.0f, 271.0f,
                        0.0f, 0.0f, 1.0f, 0.5f, PS_PLAIN, 0x80);
        for (i = 0; rankPoints >= 0 && i < 4; i++) {
            s32 place = i == 0 ? 1000 : i == 1 ? 100 : i == 2 ? 10 : 1;
            s32 digit = rankPoints / place % 10;
            f32 u = (digit % 4) * 0.25f, v = (digit / 4) * 0.25f;
            f32 x = 54.0f + (f32)i * 16.0f;
            Ps_SpriteScreen(TBO_WII_BATTLE_POINT_DIGITS, x, 270.0f, x + 32.0f, 302.0f,
                            u, v, u + 0.25f, v + 0.25f, PS_PLAIN, 0x80);
        }
    }

    for (i = 0; i < rows; i++) {
        /* In the Wii menu each lower row begins a little farther right. */
        f32 x = 170.0f + (f32)i * TB_WII_BATTLE_ROW_STEP_X
                + (1.0f - Tb_In(i * 2)) * 512.0f;
        f32 y = 67.0f + (f32)i * (custom ? (friendFormat ? 42.0f : 45.0f) : 42.0f);
        s32 focusAlpha = Tb_WiiFocusAlpha(i, friendMenu ? sTb->wiiSubCur : custom ? sTb->wiiCustomCur : sTb->wiiBattleCur);
        f32 v0 = (f32)i * 32.0f / 256.0f;
        f32 v1 = (f32)(i + 1) * 32.0f / 256.0f;
        u32 tint = (custom || (i <= 4 && (!sTbLan || i < 2))) ? PS_PLAIN : disabled;
        /* Delayed rows stay parked; compensated entrance keeps the screen boundary fixed. */
        if (Tb_In(i * 2) <= 0.0f || x >= 512.0f) continue;
        x = Tb_RowX(x, 170.0f + (f32)i * TB_WII_BATTLE_ROW_STEP_X);
        Ps_Foreground43Anchor(1);
        Tb_MouseRow(i,x,y,512,custom && !friendFormat ? 45:42,1,0);
        Ps_SpriteScreen(TBO_WII_BATTLE_BAR_BLUE, x, y, x + 512.0f, y + 64.0f,
                        0.0f, 0.0f, 1.0f, 1.0f, tint, 0x80);
        if (focusAlpha > 0)
            Ps_SpriteScreen(TBO_WII_BATTLE_BAR_GOLD, x, y, x + 512.0f, y + 64.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, tint, focusAlpha);
        if (friendMenu) {
            Tb_WiiLabelText(sTbWiiFriendAction[i], x + 46.0f, y + 4.0f, focusAlpha, tint);
        } else if (custom) {
            /* The format names have no Wii lettering sheet. Use the same
               menu-label rasterizer with the first Menu font on Original. */
            Tb_WiiLabelText(sTbWiiCustomName[i + (friendFormat ? 0 : sTb->wiiNormalBattle ? 2 : 0)],
                            x + 46.0f, y + 4.0f, focusAlpha, tint);
        } else {
            if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_BATTLE_ROW + i]))
                Tb_WiiLabelText(sTbWiiBattleName[i], x + 49.0f, y + 5.0f, focusAlpha, tint);
            else {
                Tb_WiiTextureLabel(TBO_WII_BATTLE_LABEL_IDLE, TBO_WII_BATTLE_LABEL_SELECTED,
                                   x + 49.0f, y + 5.0f, v0, v1, focusAlpha, tint,
                                   sTboWiiLabelCapHeight[1]);
            }
        }
        Ps_Foreground43Anchor(0);
    }
    if (sTb->wiiBattleT >= 20) {
        s32 cur = friendMenu ? sTb->wiiSubCur : custom ? sTb->wiiCustomCur : sTb->wiiBattleCur;
        f32 radarX = 196.0f + (f32)cur * TB_WII_BATTLE_ROW_STEP_X - Tb_RowInset() + Tb_RowOffset();
        f32 radarY = 84.0f + (f32)cur * (custom ? (friendFormat ? 42.0f : 45.0f) : 42.0f);
        Ps_Foreground43Anchor(1);
        Ps_SpriteScreen(TBO_WII_RADAR, radarX - 32.0f * radarScale,
                        radarY - 32.0f * radarScale,
                        radarX + 32.0f * radarScale, radarY + 32.0f * radarScale,
                        0.0f, 0.0f, 1.0f, 1.0f,
                        custom || sTb->wiiBattleCur <= 4 ? PS_PLAIN : disabled,
                        0x80);
        Ps_Foreground43Anchor(0);
    }
    IconWin_PortHeadingMotion(sTb->wiiBattleT, &iconDx, &iconDy, &iconAlpha,
                              &iconScaleX, &iconScaleY, &titleDx, &titleDy,
                              &titleAlpha, &titleScaleX, &titleScaleY);
    if (custom && sTb->wiiRanked) {
        Tb_WiiTitleText(TB_WII_TX_BATTLE_ROW + 3, 62.0f + titleDx, 17.0f + titleDy,
                        titleScaleX, titleScaleY, titleAlpha);
    } else if (friendMenu || friendFormat) {
        Tb_WiiTitleText(TB_WII_TX_BATTLE_ROW + 2, 62.0f + titleDx, 17.0f + titleDy,
                        titleScaleX, titleScaleY, titleAlpha);
    } else if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[custom ? (sTb->wiiNormalBattle ? TB_WII_TX_NORMAL_TITLE
                                                                               : TB_WII_TX_CUSTOM_TITLE)
                                                              : sTbWiiBattleHeader]))
        Tb_WiiTitleText(custom ? (sTb->wiiNormalBattle ? TB_WII_TX_NORMAL_TITLE : TB_WII_TX_CUSTOM_TITLE)
                               : sTbWiiBattleHeader,
                        62.0f + titleDx, 17.0f + titleDy, titleScaleX, titleScaleY, titleAlpha);
    else
        Ps_SpriteScreen(custom ? (sTb->wiiNormalBattle ? TBO_WII_NORMAL_TITLE : TBO_WII_CUSTOM_TITLE)
                               : (sTbWiiBattleHeader == TB_WII_TX_HOME_TITLE ? TBO_WII_TITLE : TBO_WII_BATTLE_TITLE),
                        62.0f + titleDx, 17.0f + titleDy,
                        62.0f + titleDx + 512.0f * titleScaleX,
                        17.0f + titleDy + 64.0f * titleScaleY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, titleAlpha);
    Ps_SpriteScreen(TBO_WII_CREST, 45.0f + iconDx - 32.0f * iconScaleX,
                    48.0f + iconDy - 32.0f * iconScaleY,
                    45.0f + iconDx + 32.0f * iconScaleX,
                    48.0f + iconDy + 32.0f * iconScaleY,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, iconAlpha);
}

/* Keep full scoring precision internally; only the presentation uses rounded hundreds. */
static s32 Tb_PointDisplay(s32 points) {
    return points / 100 + (points % 100 != 0);
}

/* Account totals for the current ranked category, refreshed outside simulation. */
static void Tb_WiiVsRecord(f32 top, s32 second) {
    static const f32 column[3] = {103.0f, 248.0f, 403.0f};
    s32 i, totals[3] = {0, 0, 0};
    extern int Port_NetStatsTotals(int, int *);
    s32 totalsReady = Port_NetStatsTotals(second, totals);

    Ps_SpriteScreen(TBO_WII_VS_PLAYERS, 18.0f, top + 32.0f, 82.0f, top + 64.0f,
                    0.0f, second ? 0.5f : 0.0f, 1.0f, second ? 1.0f : 0.5f,
                    PS_PLAIN, 0x80);
    if (sTb->resultHasPoints) {
        extern int Port_NetStatsFinalPoints(int, int *, int *);
        s32 total, change;
        if (Port_NetStatsFinalPoints(second, &total, &change)) {
            sTb->resultPoints[second] = total * 100;
            sTb->resultPointDelta[second] = change * 100;
            if (second == Port_NetMe()) Port_NetRewardRecord(change);
        }
        s32 value = sTb->resultPoints[second];
        s32 display = Tb_PointDisplay(value);
        s32 place = 1000, digits = 4;
        f32 x;
        /* Center the visible artwork, excluding transparent padding in its sheet. */
        Ps_SpriteScreen(TBO_WII_BATTLE_POINT_LABEL, 89, top + 45.5f, 183.5f, top + 66.5f,
                        1.0f / 256, 0, 119.0f / 256, 26.0f / 64, PS_PLAIN, 0x80);
        /* Draw the change arrow behind the last digit, as in the Wii result plate. */
        if (sTb->resultPointDelta[second] != 0) {
            s32 increase = sTb->resultPointDelta[second] > 0;
            f32 phase = (f32)(sTb->frames % 90) / 90.0f;
            s32 alpha = (s32)(64.0f * (1.0f - cosf(phase * 6.283185307f)));
            /* Center either arrow at the same height as the digits. */
            f32 arrowTop = top + 39.5f;
            /* Reset position while transparent, then drift in the change direction. */
            arrowTop += (increase ? -6.0f : 6.0f) * phase;
            Ps_SpriteScreen(TBO_WII_RESULT_CHANGE, 426, arrowTop, 454, arrowTop + 28,
                            0, increase ? 1 : 0, 1, increase ? 0 : 1,
                            increase ? Ps_Rgba(0x80, 0x80, 0x10, 0x80)
                                     : Ps_Rgba(0x50, 0x40, 0x80, 0x80), alpha);
        }
        x = 436.0f - (digits - 1) * 18.0f - 24.0f;
        for (; place; place /= 10, x += 18) {
            s32 digit = (display / place) % 10;
            f32 u = ((digit % 4) * 32 + 1) / 128.0f;
            f32 v = ((digit / 4) * 32 + 1) / 128.0f;
            Ps_SpriteScreen(TBO_WII_BATTLE_POINT_DIGITS, x, top + 45, x + 24, top + 62,
                            u, v, u + 19.0f / 128, v + 22.0f / 128, PS_PLAIN, 0x80);
        }
    }
    for (i = 0; i < 3; i++) {
        s32 wins = i == 1;
        f32 x = column[i];
        s32 value = totals[i];
        s32 place = 1, digits = 1;
        f32 advance, dx;
        while (place <= value / 10) { place *= 10; digits++; }
        advance = digits > 3 ? 24.0f / (digits - 1) : 12.0f;
        dx = x - (digits - 1) * advance;
        for (; totalsReady && place; place /= 10, dx += advance) {
            s32 digit = (value / place) % 10;
            f32 u = (f32)(digit % 4) * 0.25f;
            f32 v = (f32)(digit / 4) * 0.25f;
            Ps_SpriteScreen(wins ? TBO_WII_VS_WINS_DIGITS : TBO_WII_VS_DIGITS,
                            dx, top + 70.0f, dx + advance * 1.5f, top + 96.0f,
                            u, v, u + 26.0f / 128, v + 0.25f,
                            wins ? PS_PLAIN
                                 : i == 0 ? Ps_Rgba(0x50, 0x78, 0x58, 0x80) : PS_PLAIN, 0x80);
        }
        Ps_SpriteScreen(TBO_WII_VS_RECORD_LABELS,
                        x + 18.0f, top + 70.0f, x + 98.0f, top + 102.0f,
                        0.0f, (f32)i * 0.25f, 80.0f / 128.0f, (f32)(i + 1) * 0.25f,
                        PS_PLAIN, 0x80);
    }
}

/* The two halves of each Wii plate are mirrored from resource 3's left cap. */
static void Tb_DrawWiiVs(void) {
    f32 giruIn = (f32)sTb->wiiBattleT / 29.0f;
    f32 giruX, giruY;
    f32 iconDx = 0.0f, iconDy = 0.0f, titleDx = 0.0f, titleDy = 0.0f;
    f32 iconScaleX = 1.0f, iconScaleY = 1.0f, titleScaleX = 1.0f, titleScaleY = 1.0f;
    s32 iconAlpha = 0x80, titleAlpha = 0x80;

    Ps_SetShift(0.0f);
    if (giruIn > 1.0f) giruIn = 1.0f;
    giruIn = giruIn * giruIn * (3.0f - 2.0f * giruIn);
    giruX = -TB_WII_NATIVE_GIRU_W + TB_WII_NATIVE_GIRU_W * giruIn;
    giruY = 275.0f + Tb_WiiIdleBob(((sTb->frames * 3) + 500) % 2000, 4.5f);
    Ps_SpriteScreen(TBO_WII_BATTLE_GIRU, giruX, giruY,
                    giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_BATTLE_GIRU_EYE_DIM, giruX, giruY,
                    giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);

    Ps_SpriteScreen(TBO_WII_VS_PANELS, 0.0f, 63.0f, 256.0f, 191.0f,
                    0.0f, 0.0f, 1.0f, 0.5f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_VS_PANELS, 256.0f, 63.0f, 512.0f, 191.0f,
                    1.0f, 0.0f, 0.0f, 0.5f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_VS_PANELS, 0.0f, 203.0f, 256.0f, 331.0f,
                    0.0f, 0.5f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_VS_PANELS, 256.0f, 203.0f, 512.0f, 331.0f,
                    1.0f, 0.5f, 0.0f, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_VS_LINES, 0.0f, 63.0f, 512.0f, 191.0f,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_VS_LINES, 0.0f, 203.0f, 512.0f, 331.0f,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    Tb_WiiVsRecord(63.0f, 0);
    Tb_WiiVsRecord(203.0f, 1);
    Ps_SpriteScreen(TBO_WII_VS_MARK, 186.0f, 165.0f, 314.0f, 229.0f,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);

    IconWin_PortHeadingMotion(sTb->wiiBattleT, &iconDx, &iconDy, &iconAlpha,
                              &iconScaleX, &iconScaleY, &titleDx, &titleDy,
                              &titleAlpha, &titleScaleX, &titleScaleY);
    if (sTb->wiiRanked) {
        Tb_WiiTitleText(TB_WII_TX_BATTLE_ROW + 3, 62.0f + titleDx, 17.0f + titleDy,
                        titleScaleX, titleScaleY, titleAlpha);
    } else if (sTb->wiiFriend) {
        Tb_WiiTitleText(TB_WII_TX_BATTLE_ROW + 2, 62.0f + titleDx, 17.0f + titleDy,
                        titleScaleX, titleScaleY, titleAlpha);
    } else if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[sTb->wiiNormalBattle ? TB_WII_TX_NORMAL_TITLE
                                                                          : TB_WII_TX_CUSTOM_TITLE]))
        Tb_WiiTitleText(sTb->wiiNormalBattle ? TB_WII_TX_NORMAL_TITLE : TB_WII_TX_CUSTOM_TITLE,
                        62.0f + titleDx, 17.0f + titleDy, titleScaleX, titleScaleY, titleAlpha);
    else
        Ps_SpriteScreen(sTb->wiiNormalBattle ? TBO_WII_NORMAL_TITLE : TBO_WII_CUSTOM_TITLE,
                        62.0f + titleDx, 17.0f + titleDy,
                        62.0f + titleDx + 512.0f * titleScaleX,
                        17.0f + titleDy + 64.0f * titleScaleY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, titleAlpha);
    Ps_SpriteScreen(TBO_WII_CREST, 45.0f + iconDx - 32.0f * iconScaleX,
                    48.0f + iconDy - 32.0f * iconScaleY,
                    45.0f + iconDx + 32.0f * iconScaleX,
                    48.0f + iconDy + 32.0f * iconScaleY,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, iconAlpha);
}

/* Render text at screen coordinates, keeping its size with a widescreen page. */
static void Tb_RankingText(char *text, f32 x, f32 middle, f32 height, f32 width, s32 heading) {
    f32 sx = 1.0f / (Ps_X(1.0f) - Ps_X(0.0f));
    f32 sy = 1.0f / (Ps_Y(1.0f) - Ps_Y(0.0f));
    f32 scale = height * sy / (PS_CELL_UNITS * PS_CAP_OF_CELL);
    Ps_Align(1);
    if (heading) {
        /* Kind 4 is the selected Wii entry rasterizer, including its orange face. */
        s32 id = PortTbo_TextId(text, 4);
        if (id >= 0) {
            f32 scaleY = height / 19.0f;
            f32 scaleX = scaleY;
            f32 advance = height * (f32)PortTbo_TextInfo(id, 0) / 1000.0f;
            f32 left, top;
            if (width > 0 && advance > width) {
                scaleX *= width / advance;
                advance = width;
            }
            left = x - advance * 0.5f - 5.0f * scaleX;
            top = middle - 14.5f * scaleY;
            Ps_SpriteScreen(id, left, top, left + 512.0f * scaleX, top + 32.0f * scaleY,
                            0, 0, 1, 1, PS_PLAIN, 0x80);
        }
    }
    else Ps_Text((x - Ps_X(0)) * sx, Ps_TextTop((middle - Ps_Y(0)) * sy, scale), text, scale, width * sx, 0);
    Ps_Align(0);
}

/* Player names use the regular game UI glyphs, independent of menu-label artwork. */
static void Tb_RankingPlayerName(char *name, s32 x, s32 middle) {
    extern FontStyle gFontStyle;
    extern s32 Font_GetGlyphHeight(void);
    extern void Font_PrintAsciiAt(s32 x, s32 y, char *str);
    FontStyle saved = gFontStyle;
    gFontStyle.scaleX = gFontStyle.scaleY = gFontStyle.scale = 1.0f;
    gFontStyle.align = FONT_ALIGN_CENTER;
    gFontStyle.spacingX = 0;
    gFontStyle.color = 0x80FFFFFF;
    gFontStyle.shadowMode = FONT_SHADOW_DROP;
    gFontStyle.shadowColor = 0x80000000;
    gFontStyle.shadowDx = gFontStyle.shadowDy = 1;
    gFontStyle.clipX0 = gFontStyle.clipY0 = 0;
    gFontStyle.clipX1 = 511;
    gFontStyle.clipY1 = 447;
    Font_PrintAsciiAt(x, middle - Font_GetGlyphHeight() / 2, name);
    gFontStyle = saved;
}

static void Tb_RankingNumber(s32 texture, s32 value, f32 right, f32 y, f32 width, f32 height) {
    s32 digits[10], count = 0, i;
    do { digits[count++] = value % 10; value /= 10; } while (value && count < 10);
    for (i = count - 1; i >= 0; i--) {
        f32 u = (f32)(digits[i] % 4) * 0.25f;
        f32 v = (f32)(digits[i] / 4) * 0.25f;
        f32 x = right - (f32)(i + 1) * width;
        Ps_SpriteScreen(texture, x, y, x + width, y + height, u, v, u + 0.25f, v + 0.25f, PS_PLAIN, 0x80);
    }
}

/* Original Fighting Point glyphs occupy only 14–18 by 22 pixels in each 32px cell. */
static void Tb_RankingPoints(s32 value, f32 right, f32 y) {
    static const s32 inset[10] = {2, 3, 1, 2, 1, 2, 2, 2, 2, 2};
    static const s32 widths[10] = {18, 14, 18, 18, 18, 18, 18, 16, 18, 18};
    s32 digits[10], count = 0, i;
    f32 total = 0, left;
    do {
        digits[count] = value % 10;
        total += (f32)widths[digits[count++]];
        value /= 10;
    } while (value && count < 10);
    total += (f32)(count - 1); /* one pixel between visible outlines */
    left = right - total;
    for (i = count - 1; i >= 0; i--) {
        s32 digit = digits[i];
        f32 width = (f32)widths[digit];
        f32 u = (f32)((digit % 4) * 32 + inset[digit]) / 128.0f;
        f32 v = (f32)((digit / 4) * 32 + 1) / 128.0f;
        Ps_SpriteScreen(TBO_WII_BATTLE_POINT_DIGITS, left, y, left + width, y + 22,
                        u, v, u + width / 128.0f, v + 22.0f / 128.0f, PS_PLAIN, 0x80);
        left += width + 1;
    }
}

static void Tb_DrawWiiRanking(void) {
    s32 i;
    f32 iconDx = 0, iconDy = 0, titleDx = 0, titleDy = 0;
    f32 iconScaleX = 1, iconScaleY = 1, titleScaleX = 1, titleScaleY = 1;
    s32 iconAlpha = 0x80, titleAlpha = 0x80;
    char caption[128];
    Ps_SetShift(0);
    {
        f32 enter = (f32)sTb->wiiBattleT / 29.0f;
        f32 hover = Tb_WiiIdleBob(((sTb->frames * 3) + 500) % 2000, 4.5f);
        f32 x, y;
        if (enter > 1) enter = 1;
        enter = enter * enter * (3 - 2 * enter);
        x = -TB_WII_NATIVE_GIRU_W + TB_WII_NATIVE_GIRU_W * enter;
        y = 84.0f + hover;
        Ps_SpriteScreen(TBO_WII_BATTLE_GIRU, x, y, x + TB_WII_NATIVE_GIRU_W, y + TB_WII_NATIVE_GIRU_H, 0, 0, 1, 1, PS_PLAIN, 0x80);
        Ps_SpriteScreen(TBO_WII_BATTLE_GIRU_EYE_DIM, x, y, x + TB_WII_NATIVE_GIRU_W, y + TB_WII_NATIVE_GIRU_H, 0, 0, 1, 1, PS_PLAIN, 0x80);
    }
    IconWin_PortHeadingMotion(sTb->wiiBattleT, &iconDx, &iconDy, &iconAlpha,
                              &iconScaleX, &iconScaleY, &titleDx, &titleDy,
                              &titleAlpha, &titleScaleX, &titleScaleY);
    Ps_SpriteScreen(TBO_WII_CREST, 45 + iconDx - 32 * iconScaleX, 48 + iconDy - 32 * iconScaleY,
                    45 + iconDx + 32 * iconScaleX, 48 + iconDy + 32 * iconScaleY,
                    0, 0, 1, 1, PS_PLAIN, iconAlpha);
    if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_RANKING_TITLE]))
        Tb_WiiTitleText(TB_WII_TX_RANKING_TITLE, 62 + titleDx, 17 + titleDy, titleScaleX, titleScaleY, titleAlpha);
    else Ps_SpriteScreen(TBO_WII_RANK_TITLE, 62 + titleDx, 17 + titleDy,
                         62 + titleDx + 512 * titleScaleX, 17 + titleDy + 64 * titleScaleY,
                         0, 0, 1, 1, PS_PLAIN, titleAlpha);
    Ps_SpriteScreen(TBO_WII_RANK_CATEGORY, 20, 63, 532, 127, 0, 0, 1, 1, PS_PLAIN, 0x80);
    Port_AccountUiText("dragon-net/ranking/global", caption, sizeof(caption));
    Tb_RankingText(caption, 276, 80, 19, 240, 1);
    for (i = 0; i < PORT_RANKING_ROWS; i++) {
        PortRankingEntry *row = sTb->ranking.state == 1 && i < sTb->ranking.count
                                ? &sTb->ranking.entries[i] : NULL;
        f32 y = 140.0f + (f32)i * 90.0f;
        Ps_SpriteScreen(TBO_WII_RANK_ROW, 16, y, 528, y + 128, 0, 0, 1, 1, PS_PLAIN, 0x80);
        {
            s32 portrait = row && row->character >= 0 && row->character <= 160 ? row->character : 162;
            s32 rank = row ? row->rank : sTb->rankingOffset + i + 1;
            f32 rankWidth = rank < 100 ? 22 : rank < 1000 ? 16 : 10;
            if (row) Tb_RankingPlayerName(row->display_name, 288, (s32)y + 15);
            Tb_RankingNumber(TBO_WII_RANK_POSITION_DIGITS, rank, 104, y + 31, rankWidth, 36);
            Ps_SpriteScreen(TBO_WII_RANK_PORTRAIT_FRAME, 111, y + 33, 161, y + 76,
                            7.0f / 64, 10.0f / 64, 57.0f / 64, 53.0f / 64, PS_PLAIN, 0x80);
            Ps_SpriteScreen(TBO_WII_PORTRAIT_BASE + portrait, 112, y + 34, 160, y + 75,
                            8.0f / 64, 11.0f / 64, 56.0f / 64, 52.0f / 64, PS_PLAIN, 0x80);
        }
        Ps_SpriteScreen(TBO_WII_RANK_POINTS_LABEL, 212, y + 34, 340, y + 66,
                        0, 0, 0.5f, 0.5f, PS_PLAIN, 0x80);
        Tb_RankingPoints(row ? row->points : 0, 494, y + 35);
    }
    if (sTb->ranking.state == 1 && sTb->ranking.count > 0 && sTb->rankingOffset > 0)
        Ps_SpriteScreen(TBO_WII_RANK_ARROWS, 240, 116, 272, 136, 0, 0.5f, 0.5f, 1, PS_PLAIN, 0x80);
    if (sTb->ranking.state == 1 && sTb->ranking.count > 0 && sTb->rankingOffset + sTb->ranking.count < sTb->ranking.total)
        Ps_SpriteScreen(TBO_WII_RANK_ARROWS, 240, 410, 272, 430, 0.5f, 0.5f, 1, 1, PS_PLAIN, 0x80);
    Font_FlushAll();
}

static void Tb_DrawWii(void) {
    s32 i;
    if (sTb->mode == TB_WII_RANKING) { Tb_DrawWiiRanking(); return; }
    if ((sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM ||
         sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT) && Port_WiiArtAvailable()) {
        Tb_DrawWiiBattle();
        return;
    }

    if (sTb->mode == TB_WII_SETTINGS && Port_WiiArtAvailable()) {
        f32 iconDx = 0.0f, iconDy = 0.0f, titleDx = 0.0f, titleDy = 0.0f;
        f32 iconScaleX = 1.0f, iconScaleY = 1.0f, titleScaleX = 1.0f, titleScaleY = 1.0f;
        f32 panStartX, panStartY, giruStartX, giruStartY, giruHover;
        f32 radarScale = Tb_WiiFocusStretch();
        s32 iconAlpha = 0x80, titleAlpha = 0x80;

        panStartX = Tb_WiiMotionQuarter(TBO_WII_MOTION_PAN_ENTRY_X, sTb->wiiHomeT * 3) + 11.95f;
        panStartY = Tb_WiiIdleBob((sTb->frames * 3) % 2000, 6.0f);
        giruStartX = 378.0f + Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_X, sTb->wiiHomeT * 3)
                     - 184.05f - 117.0f;
        giruStartY = 233.0f + Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_Y, sTb->wiiHomeT * 3)
                     - 71.85f;
        giruHover = Tb_WiiIdleBob(((sTb->frames * 3) + 500) % 2000, 4.5f);
        /* Pan and Giru remain together on User Settings. Giru moves to the
           name field only when the keyboard opens. */
        {
            s32 idleFrame = ((sTb->frames * 3) % 2000) / 4;
            s32 pointing = sTb->wiiPanPointing;
            s32 lipOpen = sTb->wiiDialogVoiceStarted && sTb->wiiDialogLine != 1 && sTb->wiiDialogLine != 10 &&
                          sTb->wiiDialogT <= Port_WiiDialogDuration(sTb->wiiDialogLine, sTb->wiiDialogJapanese) &&
                          Port_WiiDialogMouth(sTb->wiiDialogLine, sTb->wiiDialogT, sTb->wiiDialogJapanese);
            s32 mouthShape = !lipOpen ? 0 : (sTb->wiiDialogT / 10 & 1) ? 1 : 2;
            s32 blink = (idleFrame >= 80 && idleFrame < 88) ||
                        (idleFrame >= 200 && idleFrame < 208) ||
                        (idleFrame >= 320 && idleFrame < 328) ||
                        (idleFrame >= 440 && idleFrame < 448);
            f32 eyeX = (pointing ? 56.0f : 79.0f) + panStartX;
            f32 eyeY = (pointing ? 158.0f : 176.0f) + panStartY;
            f32 mouthX = (pointing ? 70.0f : 100.0f) + panStartX;
            f32 mouthY = (pointing ? 183.0f : 197.0f) + panStartY;

            Ps_SpriteScreen(pointing ? TBO_WII_PAN_POINT : TBO_WII_PAN,
                            -13.0f + panStartX, 9.0f + panStartY,
                            243.0f + panStartX, 521.0f + panStartY,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
            if (blink)
                Ps_SpriteScreen(pointing ? TBO_WII_PAN_POINT_EYES_CLOSED : TBO_WII_PAN_EYES_CLOSED,
                                eyeX, eyeY, eyeX + 64.0f, eyeY + 32.0f,
                                0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
            if (mouthShape != 2)
                Ps_SpriteScreen(pointing ? (mouthShape ? TBO_WII_PAN_POINT_MOUTH_OPEN : TBO_WII_PAN_POINT_MOUTH_CLOSED)
                                         : (mouthShape ? TBO_WII_PAN_MOUTH_OPEN : TBO_WII_PAN_MOUTH_CLOSED),
                                mouthX, mouthY, mouthX + 32.0f, mouthY + 32.0f,
                                0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
        }
        Ps_SpriteScreen(TBO_WII_GIRU, giruStartX, giruStartY + giruHover,
                        giruStartX + TB_WII_NATIVE_GIRU_W,
                        giruStartY + giruHover + TB_WII_NATIVE_GIRU_H,
                        1.0f, 0.0f, 0.0f, 1.0f, PS_PLAIN, 0x80);
        Ps_SpriteScreen(TBO_WII_GIRU_EYE_DIM, giruStartX, giruStartY + giruHover,
                        giruStartX + TB_WII_NATIVE_GIRU_W,
                        giruStartY + giruHover + TB_WII_NATIVE_GIRU_H,
                        1.0f, 0.0f, 0.0f, 1.0f, PS_PLAIN, 0x80);
        IconWin_PortHeadingMotion(sTb->frames, &iconDx, &iconDy, &iconAlpha,
                                  &iconScaleX, &iconScaleY, &titleDx, &titleDy,
                                  &titleAlpha, &titleScaleX, &titleScaleY);
        for (i = 0; i < 2; i++) {
            f32 y = 103.0f + (f32)i * 48.0f;
            s32 entry = sTbWiiSettingsVisible[i];
            s32 disabled = Tb_SettingsDisabled(entry);
            u32 tint = disabled ? Ps_Rgba(0x58, 0x58, 0x58, 0x80) : PS_PLAIN;
            s32 focusAlpha = Tb_WiiFocusAlpha(i, sTb->wiiSubCur);

            Tb_MouseRow(i,162,y,461,48,0,0);
            Ps_SpriteScreen(TBO_WII_BAR_BLUE,
                            162.0f, y, 623.0f, y + 58.0f,
                            0.0f, 0.0f, 1.0f, 1.0f, tint, 0x80);
            if (focusAlpha > 0)
                Ps_SpriteScreen(TBO_WII_BAR_GOLD, 162.0f, y, 623.0f, y + 58.0f,
                                0.0f, 0.0f, 1.0f, 1.0f, tint, focusAlpha);
            /* Added settings entries use the same generated lettering as Custom Battle. */
            if (entry >= 2 || PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_SETTINGS_ROW + entry]))
                Tb_WiiLabelText(sTbWiiSettingsName[entry], 212.0f, y + 3.0f, focusAlpha, tint);
            else {
                Tb_WiiTextureLabel(TBO_WII_SETTINGS_LABEL_IDLE, TBO_WII_SETTINGS_LABEL_SELECTED,
                                   212.0f, y + 3.0f,
                                   (f32)i * 0.5f, (f32)(i + 1) * 0.5f, focusAlpha, tint,
                                   sTboWiiLabelCapHeight[2]);
            }
        }
        {
            f32 radarY = 120.0f + (f32)sTb->wiiSubCur * 48.0f;
            Ps_SpriteScreen(TBO_WII_RADAR, 196.0f - 32.0f * radarScale, radarY - 32.0f * radarScale,
                            196.0f + 32.0f * radarScale, radarY + 32.0f * radarScale,
                        0.0f, 0.0f, 1.0f, 1.0f,
                        Tb_SettingsDisabled(sTbWiiSettingsVisible[sTb->wiiSubCur]) ? Ps_Rgba(0x58, 0x58, 0x58, 0x80) : PS_PLAIN,
                        0x80);
        }
        if (PortTbo_WiiUseFontForArtwork(sTbWiiTextEdited[TB_WII_TX_SETTINGS_TITLE]))
            Tb_WiiTitleText(TB_WII_TX_SETTINGS_TITLE, 62.0f + titleDx, 17.0f + titleDy,
                            titleScaleX, titleScaleY, titleAlpha);
        else
            Ps_SpriteScreen(TBO_WII_SETTINGS_TITLE, 62.0f + titleDx,
                            17.0f + titleDy,
                            62.0f + titleDx + 512.0f * titleScaleX,
                            17.0f + titleDy + 64.0f * titleScaleY,
                            0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, titleAlpha);
        Ps_SpriteScreen(TBO_WII_CREST, 45.0f + iconDx - 32.0f * iconScaleX,
                        48.0f + iconDy - 32.0f * iconScaleY,
                        45.0f + iconDx + 32.0f * iconScaleX,
                        48.0f + iconDy + 32.0f * iconScaleY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, iconAlpha);
        return;
    }

}

static void Tb_DrawWiiText(void) {
    s32 i;
    s32 count = sTb->mode == TB_WII_HOME || sTb->mode == TB_WII_FRIENDS ? 3
              : sTb->mode == TB_WII_BATTLE ? 6
              : sTb->mode == TB_WII_FRIEND_FORMAT ? 5
              : sTb->mode == TB_WII_CUSTOM ? (sTb->wiiRanked ? 1 : sTb->wiiNormalBattle ? 3 : 2) : 2;
    s32 cur = sTb->mode == TB_WII_HOME ? sTb->wiiCur
            : sTb->mode == TB_WII_BATTLE ? sTb->wiiBattleCur
            : (sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_FRIEND_FORMAT) ? sTb->wiiCustomCur : sTb->wiiSubCur;
    char **names = sTb->mode == TB_WII_HOME ? sTbWiiHomeName
                  : sTb->mode == TB_WII_BATTLE ? sTbWiiBattleName
                  : sTb->mode == TB_WII_CUSTOM ? sTbWiiCustomName + (sTb->wiiNormalBattle ? 2 : 0)
                  : sTb->mode == TB_WII_FRIEND_FORMAT ? sTbWiiCustomName
                  : sTb->mode == TB_WII_FRIEND_MENU ? sTbWiiFriendAction
                  : sTb->mode == TB_WII_FRIENDS ? sTbWiiFriendsName
                  : sTb->mode == TB_WII_SETTINGS ? sTbWiiSettingsName : sTbWiiRankingName;
    f32 y0 = count == 6 ? 87.0f : 114.0f;

    if ((sTb->mode == TB_WII_SETTINGS || sTb->mode == TB_WII_BATTLE ||
         sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_FRIEND_MENU ||
         sTb->mode == TB_WII_FRIEND_FORMAT) && Port_WiiArtAvailable()) return;

    for (i = 0; i < count; i++) {
        f32 y = y0 + 36.0f * (f32)i;
        Tb_MouseRow(i,44,y,420,32,0,1);
        f32 scale = 19.0f / ((Ps_Y(1.0f) - Ps_Y(0.0f)) * PS_CELL_UNITS * PS_CAP_OF_CELL);
        Ps_Text(44.0f, Ps_TextTop(y + 15.0f, scale),
                sTb->mode == TB_WII_SETTINGS ? names[sTbWiiSettingsVisible[i]]
                : sTb->mode == TB_WII_HOME ? names[sTbWiiHomeVisible[i]] : names[i], scale, 420.0f,
                sTb->mode == TB_WII_HOME && Tb_HomeDisabled(sTbWiiHomeVisible[i]) ? 4 : i == cur ? 1 : 0);
    }
}

/* The Wii name field is 80 pixels tall inside its border. Draw each 32-pixel
   glyph on its middle line, and place the underline directly below that line.
   The name begins at the Wii field's left inset, like the original editor. */
static void Tb_DrawWiiNameText(f32 rise, s32 alpha) {
    static const char symbols[] = "#$%&@!?-+*()";
    s32 i;

    for (i = 0; i < sTb->typedLen && i < 16; i++) {
        char c = sTb->typed[i];
        s32 page = 0, row, col, sx, sy;
        f32 dx = 32.0f + (f32)i * 25.0f;
        f32 dy = 111.0f + rise;

        if (c == ' ') continue;
        if (c >= 'a' && c <= 'z') {
            c -= 'a' - 'A';
            page = 1;
        }
        if (c >= 'A' && c <= 'N') {
            row = 0;
            col = c - 'A';
            sx = 37 + col * 31 + (col >= 7 ? 10 : 0);
        } else if (c >= 'O' && c <= 'Z') {
            row = 1;
            col = c - 'O';
            sx = 32 + col * 31 + (col >= 7 ? 10 : 0);
        } else if (c >= '0' && c <= '9') {
            row = 3;
            col = c - '0';
            sx = 84 + col * 31 + (col >= 5 ? 10 : 0);
            /* The digit row's baseline is two pixels below the letter rows. */
            dy -= 2.0f;
        } else {
            for (col = 0; symbols[col] != 0 && symbols[col] != c; col++) {}
            if (symbols[col] == 0) col = 6;
            row = 2;
            sx = 27 + col * 31 + (col >= 7 ? 9 : 0);
        }
        sy = page * 128 + row * 32;
        Ps_SpriteScreen(TBO_WII_NAME_GLYPHS, dx, dy,
                        dx + 31.0f, dy + 32.0f,
                        (f32)sx / 512.0f, (f32)sy / 256.0f,
                        (f32)(sx + 31) / 512.0f, (f32)(sy + 32) / 256.0f,
                        PS_PLAIN, alpha);
    }
    Ps_SpriteScreen(TBO_WII_NAME_CURSOR,
                    32.0f + (f32)sTb->nameCursor * 25.0f, 117.0f + rise,
                    64.0f + (f32)sTb->nameCursor * 25.0f, 149.0f + rise,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, alpha);
}

static f32 Tb_WiiNamePanelOffset(void) {
    s32 frame = sTb->wiiNameExit ? sTb->wiiNameExitT : 16 - sTb->modeT;
    if (frame < 0) frame = 0;
    if (frame > 16) frame = 16;
    return (f32)frame * -18.0f;
}

static void Tb_DrawWiiName(void) {
    f32 in = sTb->wiiNameExit ? 1.0f - (f32)sTb->wiiNameExitT / 20.0f
                             : (f32)sTb->modeT / 20.0f;
    f32 rise, panX, panY, giruX, giruY, giruHover, travel, giruU0, giruU1;
    s32 giruFrame = sTb->wiiNameExit ? 29 - sTb->wiiNameExitT
                                   : sTb->modeT < 29 ? sTb->modeT : 29;
    s32 alpha;

    if (in > 1.0f) in = 1.0f;
    if (in < 0.0f) in = 0.0f;
    if (giruFrame < 0) giruFrame = 0;
    in = in * in * (3.0f - 2.0f * in);
    /* The upper panel enters from above; the keyboard runs Data Center's movie. */
    rise = Tb_WiiNamePanelOffset();
    alpha = sTb->wiiNameExit ? 0x80 : (s32)(128.0f * in);
    /* The keyboard owns its native movie motion; Wii artwork moves separately. */
    Ps_SetShift(0.0f);
    /* Data Center supplies the complete keyboard movie. Wii art supplies
       the name field, title and hosts above it. */
    /* The Wii sheet contains one side of this frame. Its straight edge is
       the centre seam, so mirror it to keep both shaped corners intact. */
    Ps_Foreground43Begin(); /* Both hosts retain proportion over the stretched keyboard. */
    panX = Tb_WiiMotionQuarter(TBO_WII_MOTION_PAN_ENTRY_X, sTb->wiiHomeT * 3) + 11.95f;
    panY = Tb_WiiIdleBob((sTb->frames * 3) % 2000, 6.0f);
    if (sTb->mode == TB_WII_NAME && in < 1.0f)
        Ps_SpriteScreen(sTb->wiiPanPointing ? TBO_WII_PAN_POINT : TBO_WII_PAN,
                        -13.0f + panX - 270.0f * in, 9.0f + panY,
                        243.0f + panX - 270.0f * in, 521.0f + panY,
                        0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    giruHover = Tb_WiiIdleBob(((sTb->frames * 3) + 500) % 2000, 4.5f);
    /* Ease along one curved path instead of stopping between horizontal and
       vertical travel. Keep the flat sprite full-size while changing facing;
       the eye overlay follows exactly the same bounds. Closing retraces it. */
    travel = (f32)giruFrame / 29.0f;
    travel = travel * travel * (3.0f - 2.0f * travel);
    f32 settingsX = 378.0f + Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_X, sTb->wiiHomeT * 3)
                    - 184.05f - 117.0f;
    f32 settingsY = 233.0f + Tb_WiiMotionQuarter(TBO_WII_MOTION_GIRU_Y, sTb->wiiHomeT * 3) - 71.85f;
    giruX = settingsX * (1.0f - travel) * (1.0f - travel);
    giruY = settingsY - (settingsY - 38.0f) * travel * travel
            - 8.0f * 4.0f * travel * (1.0f - travel) + giruHover;
    giruU0 = giruFrame < 15 ? 1.0f : 0.0f;
    if (sTb->mode == TB_WII_FRIEND_CODE) {
        /* Join Battle has no settings hosts: Giru arrives from offscreen left
           and retreats along the same horizontal path when the keyboard closes. */
        giruX = -TB_WII_NATIVE_GIRU_W * (1.0f - travel);
        giruY = 38.0f + giruHover;
        giruU0 = 0.0f;
    } else if (Tb_RowProportional()) {
        extern s32 Port_PageAspectMilli(void);
        f32 narrow = 1333.333f / (f32)Port_PageAspectMilli();
        /* Keyboard uses a left pivot; at travel=0 its screen position must match
           the right-pivot Settings sprite. Fade that pivot compensation along the path. */
        giruX += 512.0f * (1.0f / narrow - 1.0f) * (1.0f - travel);
    }
    giruU1 = 1.0f - giruU0;
    Ps_SpriteScreen(TBO_WII_GIRU, giruX, giruY,
                    giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    giruU0, 0.0f, giruU1, 1.0f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_GIRU_EYE_DIM, giruX, giruY,
                    giruX + TB_WII_NATIVE_GIRU_W, giruY + TB_WII_NATIVE_GIRU_H,
                    giruU0, 0.0f, giruU1, 1.0f, PS_PLAIN, 0x80);
    Ps_Foreground43End();
    Ps_ForegroundStretchBegin();
    /* Draw the frame at the source crop's native height. Its lower contour
       meets the keyboard's upper contour without a doubled border. */
    /* Static native backing shapes must travel with the keys, not appear at rest early. */
    f32 keyboardT = (f32)(sTb->wiiNameExit ? sTb->wiiNameExitT : sTb->modeT) / 20.0f;
    if (keyboardT < 0.0f) keyboardT = 0.0f;
    if (keyboardT > 1.0f) keyboardT = 1.0f;
    keyboardT = keyboardT * keyboardT * (3.0f - 2.0f * keyboardT);
    s32 keyboardOffset = (s32)(448.0f * (sTb->wiiNameExit ? keyboardT : 1.0f - keyboardT));
    Port_DcNameDraw(sTb->lower, sTb->keyRow, sTb->keyCol, keyboardOffset);
    Ps_SpriteScreen(TBO_WII_NAME_INPUT_FRAME, 0.0f, 42.0f + rise, 256.0f, 186.0f + rise,
                    0.0f, 100.0f / 256.0f, 1.0f, 245.0f / 256.0f, PS_PLAIN, alpha);
    Ps_SpriteScreen(TBO_WII_NAME_INPUT_FRAME, 256.0f, 42.0f + rise, 512.0f, 186.0f + rise,
                    1.0f, 100.0f / 256.0f, 0.0f, 245.0f / 256.0f, PS_PLAIN, alpha);
    Tb_DrawWiiNameText(rise, alpha);
}

/* rows of label / value, as Create Room and Profile have them */

/* The name's window: over everything else on the screen (drawn last). */

/* What the hosts say in the box at the foot of the screen (two lines at most). */
static char *Tb_Help(void) {
    if (sTb->accountWaiting) {
        static char working[128];
        Port_AccountUiText("account/working", working, sizeof(working));
        return working;
    }
    switch (sTb->mode) {
    case TB_WII_REWARD:
        return sTb->wiiDialogText;
    case TB_WII_HOME:
        if (sTb->wiiDialogText[0]) return sTb->wiiDialogText;
        return "Alright Giru! We're gonna go and find fighters we've never seen before!";
    case TB_WII_BATTLE:
        return sTbWiiBattleHelp[sTb->wiiBattleCur];
    case TB_WII_CUSTOM:
        return "Choose a battle format.";
    case TB_WII_FRIEND_MENU:
        return "Host a battle or enter a friend's room code.";
    case TB_WII_FRIEND_FORMAT:
        return "Choose the battle format for your room.";
    case TB_WII_FRIEND_CODE:
        return "Enter the six-character room code.";
    case TB_WII_VS: {
        static char matchHelp[256];
        if (sTb->wiiFriend && !sTb->found) {
            if (sTb->host) {
                const char *code = Port_LobbyCode();
                Ps_Copy(matchHelp, code[0] ? "Room code: " : "Getting a room code...", sizeof(matchHelp));
                if (code[0]) Ps_Cat(matchHelp, code, sizeof(matchHelp));
                return matchHelp;
            }
            return "Connecting to your friend...";
        }
        if (Port_WiiMenuText(sTb->found ? 54 : 53, matchHelp, sizeof(matchHelp))) return matchHelp;
        return sTb->found ? "Opponent found!" : "Searching for opponent...";
    }
    case TB_WII_FRIENDS:
        if (sTb->wiiDialogText[0]) return sTb->wiiDialogText;
        return "";
    case TB_WII_SETTINGS:
        if (sTb->wiiDialogText[0]) return sTb->wiiDialogText;
        return "";
    case TB_WII_NAME:
        if (sTb->accountCode) {
            static char help[256];
            Port_AccountUiText("account/code-help", help, sizeof(help));
            return help;
        }
        return "";
    case TB_WII_RANKING:
        if (sTb->wiiDialogText[0]) return sTb->wiiDialogText;
        return "";
    }
    return "";
}

/* Each page's title (by TB_ mode), as its heading shows it and the screen reader says it. */
static char *sTbTitle[21] = {
    [TB_WII_HOME] = "Dragon Net Battle", [TB_WII_BATTLE] = "Nintendo WFC Battle",
    [TB_WII_FRIENDS] = "Manage Friends", [TB_WII_SETTINGS] = "User Settings",
    [TB_WII_RANKING] = "View Ranking", [TB_WII_NAME] = "Create Player Name",
    [TB_WII_CUSTOM] = "Normal Battle", [TB_WII_VS] = "Normal Battle",
    [TB_WII_FRIEND_MENU] = "Friend Battle", [TB_WII_FRIEND_FORMAT] = "Friend Battle",
    [TB_WII_FRIEND_CODE] = "Friend Battle"
};
static void Tb_SetWiiTitles(void) {
    sTbTitle[TB_WII_REWARD] = sTbWiiText[TB_WII_TX_HOME_TITLE];
    sTbTitle[TB_WII_HOME] = sTbWiiText[TB_WII_TX_HOME_TITLE];
    sTbTitle[TB_WII_BATTLE] = sTbWiiText[sTbWiiBattleHeader];
    sTbTitle[TB_WII_CUSTOM] = sTbWiiText[sTb->wiiRanked ? TB_WII_TX_BATTLE_ROW + 3 : sTb->wiiNormalBattle ? TB_WII_TX_NORMAL_TITLE : TB_WII_TX_CUSTOM_TITLE];
    sTbTitle[TB_WII_FRIENDS] = sTbWiiText[TB_WII_TX_FRIENDS_TITLE];
    sTbTitle[TB_WII_SETTINGS] = sTbWiiText[TB_WII_TX_SETTINGS_TITLE];
    sTbTitle[TB_WII_RANKING] = sTbWiiText[TB_WII_TX_RANKING_TITLE];
    sTbTitle[TB_WII_NAME] = sTbWiiText[TB_WII_TX_NAME_TITLE];
    sTbTitle[TB_WII_VS] = sTb->wiiFriend ? sTbWiiText[TB_WII_TX_BATTLE_ROW + 2] : sTbTitle[TB_WII_CUSTOM];
    sTbTitle[TB_WII_FRIEND_MENU] = sTbWiiText[TB_WII_TX_BATTLE_ROW + 2];
    sTbTitle[TB_WII_FRIEND_FORMAT] = sTbTitle[TB_WII_FRIEND_MENU];
    sTbTitle[TB_WII_FRIEND_CODE] = sTbTitle[TB_WII_FRIEND_MENU];
}

/* ---- the screen reader (port/src/gs/a11y.c; silent unless the player has it on). The game's own screens are read
   from their state by port/src/a11y_game.c; these pages say what they show themselves: the page's title as it opens,
   then what the cursor is on (and its value), the code or the name typed so far, and the notes. The hosts' line is
   the box's, which the reader reads as it reads the game's. ---- */
extern void PortA11y_Say(const char *text, int flags);
#define TB_SAY_INTERRUPT 1

/* A room code letter by letter ("J D Y 2 T E"). */
static void Tb_Spell(char *out, const char *code, s32 max) {
    s32 n = 0;
    s32 i;

    for (i = 0; code[i] != 0 && n < max - 3; i++) {
        if (n > 0) {
            out[n++] = ' ';
        }
        out[n++] = code[i];
    }
    out[n] = 0;
}

/* Once a frame, after the page's update. */
static void Tb_Speak(void) {
    static s32 mode = -1;
    static s32 disconnectSaid;
    static char said[160];
    static char typed[40];
    static s32 noted;
    char focus[160];
    char part[64];
    char now[40];

    if (sTb->disconnectReason) {
        if (!disconnectSaid) {
            PortA11y_Say(sTb->disconnectLine, TB_SAY_INTERRUPT);
            disconnectSaid = 1;
        }
        return;
    }
    disconnectSaid = 0;

    if (sTb->frames == 0) { /* (the screens opened again) */
        mode = -1;
        typed[0] = 0;
    }
    focus[0] = 0;
    now[0] = 0;
    switch (sTb->mode) {
    case TB_WII_HOME:
        Ps_Copy(focus, sTbWiiHomeName[sTbWiiHomeVisible[sTb->wiiCur]], 160);
        break;
    case TB_WII_BATTLE:
        Ps_Copy(focus, sTbWiiBattleName[sTb->wiiBattleCur], 160);
        break;
    case TB_WII_CUSTOM:
        Ps_Copy(focus, sTbWiiCustomName[sTb->wiiCustomCur + (sTb->wiiNormalBattle ? 2 : 0)], 160);
        break;
    case TB_WII_FRIEND_MENU:
        Ps_Copy(focus, sTbWiiFriendAction[sTb->wiiSubCur], 160);
        break;
    case TB_WII_FRIEND_FORMAT:
        Ps_Copy(focus, sTbWiiCustomName[sTb->wiiCustomCur], 160);
        break;
    case TB_WII_VS:
        if (sTb->wiiFriend && sTb->host && !sTb->found) Ps_Copy(focus, Port_LobbyCode(), 160);
        break;
    case TB_WII_FRIENDS:
        Ps_Copy(focus, sTbWiiFriendsName[sTb->wiiSubCur], 160);
        break;
    case TB_WII_SETTINGS:
        Ps_Copy(focus, sTbWiiSettingsName[sTbWiiSettingsVisible[sTb->wiiSubCur]], 160);
        break;
    case TB_WII_RANKING:
        Port_AccountUiText("dragon-net/ranking/global", focus, 160);
        break;
    case TB_WII_NAME:
    case TB_WII_FRIEND_CODE:
        if (sTb->keyRow == WII_KEY_ROWS)
            Ps_Copy(focus, sTb->keyCol == 0 ? "Quit" : sTb->keyCol == 4 ? "Change case" : "Decide", 160);
        else if ((sTb->keyRow == 1 || sTb->keyRow == 2) && sTb->keyCol >= 12)
            Ps_Copy(focus, sTb->keyCol == 12 ? "Move text cursor left" : "Move text cursor right", 160);
        else if (sTb->keyRow == 3 && sTb->keyCol >= 12)
            Ps_Copy(focus, "Delete", 160);
        else {
            focus[0] = Port_DcNameKey(sTb->lower, sTb->keyRow, sTb->keyCol);
            focus[1] = 0;
        }
        Ps_Copy(now, sTb->typed, 40);
        break;
    default:
        Ps_Copy(focus, "Name: ", 160);
        Ps_Cat(focus, (char *)Port_NetNameGet(), 160);
        break;
    }
    if (sTb->mode != mode) {
        static char line[224];

        mode = sTb->mode;
        if (sTb->accountCode) Port_AccountUiText("account/code-title", line, 224);
        else Ps_Copy(line, sTbTitle[mode], 224);
        Ps_Cat(line, ". ", 224);
        Ps_Cat(line, focus, 224);
        PortA11y_Say(line, TB_SAY_INTERRUPT);
        Ps_Copy(said, focus, 160);
        Ps_Copy(typed, now, 40);
    } else { /* what was typed, then where the cursor went (one line: two in one blank, the second cuts the first) */
        static char line[256];
        s32 k;

        line[0] = 0;
        for (k = 0; now[k] == typed[k] && now[k] != 0; k++) {
        }
        if (now[k] != typed[k]) { /* the code or the name typed so far */
            Ps_Copy(typed, now, 40);
            Ps_Copy(line, (sTb->mode == TB_WII_FRIEND_CODE || sTb->accountCode) ? "Code: " : "Name: ", 256);
            if (sTb->mode == TB_WII_FRIEND_CODE || sTb->accountCode) {
                Tb_Spell(part, now, 64);
                Ps_Cat(line, now[0] != 0 ? part : "empty", 256);
            } else {
                Ps_Cat(line, now[0] != 0 ? now : "empty", 256);
            }
        }
        for (k = 0; focus[k] == said[k] && focus[k] != 0; k++) {
        }
        if (focus[k] != said[k]) {
            Ps_Copy(said, focus, 160);
            if (line[0] != 0) {
                Ps_Cat(line, ". ", 256);
            }
            Ps_Cat(line, focus, 256);
        }
        if (line[0] != 0) {
            PortA11y_Say(line, TB_SAY_INTERRUPT);
        }
    }
    if (sTb->note > 0 && !noted) { /* a note (copied, a code that did not work ...) */
        PortA11y_Say(sTb->noteLine, 0);
    }
    noted = sTb->note > 0;
}

/* The hosts' line (Tb_Help) for the game's box, in the game's text: 16-bit, 0xFEFF first. Preserve authored line
   breaks; otherwise wrap at spaces (three lines at most) up to the step of the box's top right corner (at about 410 of the screen's 512: the box
   is the game's movie, over the whole screen). Another buffer for another line (the screen reader tells them apart by
   that). NULL: no line. */
#define TB_BOX_LINE 224
static u16 *Tb_BoxLine(void) {
    static u16 buf[2][TB_BOX_LINE];
    static char said[TB_BOX_LINE];
    static s32 cur = -1;
    char *say = Tb_Help();
    u16 *out;
    s32 room;
    s32 n;
    s32 start;
    s32 lines;
    s32 at;
    s32 k;
    s32 explicitLines;

    if (say == NULL || say[0] == 0) {
        return NULL;
    }
    explicitLines = 0;
    for (k = 0; say[k] != 0; k++) {
        if (say[k] == '\n' || say[k] == '\r') explicitLines = 1;
    }
    for (k = 0; cur >= 0 && say[k] == said[k] && say[k] != 0; k++) {
    }
    if (cur >= 0 && say[k] == said[k]) {
        return buf[cur];
    }
    Ps_Copy(said, say, TB_BOX_LINE);
    cur = cur == 0 ? 1 : 0;
    out = buf[cur];
    room = MsgWin_PortLineRoom(400);
    out[0] = 0xFEFF;
    n = 1;
    start = 1;
    lines = 1;
    at = 0;
    while (say[at] == ' ') {
        at++;
    }
    while (say[at] != 0 && n < TB_BOX_LINE - 2) {
        s32 end = at;
        s32 mark = n;

        if (say[at] == '\n' || say[at] == '\r') {
            s32 ch = say[at++];
            if (ch == '\r' && say[at] == '\n') at++;
            out[n++] = '\n';
            start = n;
            lines++;
            while (say[at] == ' ') at++;
            continue;
        }
        while (say[end] != 0 && say[end] != ' ' && say[end] != '\n' && say[end] != '\r') {
            end++;
        }
        if (n > start) {
            out[n++] = ' ';
        }
        for (k = at; k < end && n < TB_BOX_LINE - 1;) {
            u8 ch = (u8)say[k++];
            if (ch >= 0xC2 && ch <= 0xDF && k < end && ((u8)say[k] & 0xC0) == 0x80) {
                out[n++] = (u16)(((ch & 0x1F) << 6) | ((u8)say[k++] & 0x3F));
            } else if (ch >= 0xE0 && ch <= 0xEF && k + 1 < end &&
                       ((u8)say[k] & 0xC0) == 0x80 && ((u8)say[k + 1] & 0xC0) == 0x80) {
                out[n++] = (u16)(((ch & 0x0F) << 12) | (((u8)say[k] & 0x3F) << 6) | ((u8)say[k + 1] & 0x3F));
                k += 2;
            } else {
                out[n++] = ch < 0x80 ? ch : '?';
            }
        }
        out[n] = 0;
        if (!explicitLines && mark > start && lines < 3 && Font_GetWidth(out + start) > room) { /* the word goes to the next line */
            out[mark] = '\n';
            start = mark + 1;
            lines++;
        }
        at = end;
        while (say[at] == ' ') {
            at++;
        }
    }
    out[n] = 0;
    return out;
}

/* Apply the page entrance offset. */
static void Tb_PageIn(s32 on) {
    s32 page = on;

    Ps_SetShift(page ? (1.0f - Tb_In(0)) * -160.0f : 0.0f);
}

static void Tb_Draw(void) {
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_WII_HOME:
    case TB_WII_BATTLE:
    case TB_WII_CUSTOM:
    case TB_WII_FRIEND_MENU:
    case TB_WII_FRIEND_FORMAT:
    case TB_WII_FRIENDS:
    case TB_WII_SETTINGS:
    case TB_WII_RANKING:
        Tb_DrawWii();
        break;
    case TB_WII_VS:
        if (Port_WiiArtAvailable()) Tb_DrawWiiVs();
        break;
    case TB_WII_NAME:
    case TB_WII_FRIEND_CODE:
        Tb_DrawWiiName();
        break;
    }
    Tb_PageIn(0);
    if (sTb->mode != TB_WII_NAME && sTb->mode != TB_WII_FRIEND_CODE && sTb->mode != TB_WII_RANKING)
        Tb_DrawMessage();
}

static void Tb_DrawText(void) {
    char **title = sTbTitle;

    /* its capitals centred on the crest beside it (y 32); up to the player card */
    if (!(sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE ||
          ((sTb->mode == TB_WII_SETTINGS || sTb->mode == TB_WII_BATTLE ||
            sTb->mode == TB_WII_CUSTOM || sTb->mode == TB_WII_FRIEND_MENU ||
            sTb->mode == TB_WII_FRIEND_FORMAT || sTb->mode == TB_WII_FRIEND_CODE ||
            sTb->mode == TB_WII_VS || sTb->mode == TB_WII_RANKING) && Port_WiiArtAvailable())))
        Ps_Heading(Tb_TitleLeft() + 44.0f, Ps_TextTop(32.0f, 1.3f), title[sTb->mode], 1.3f, 248.0f, 0x80);
    Tb_PageIn(1);
    switch (sTb->mode) {
    case TB_WII_HOME:
    case TB_WII_BATTLE:
    case TB_WII_CUSTOM:
    case TB_WII_FRIEND_MENU:
    case TB_WII_FRIEND_FORMAT:
    case TB_WII_FRIENDS:
    case TB_WII_SETTINGS:
        Tb_DrawWiiText();
        break;
    case TB_WII_NAME:
    case TB_WII_FRIEND_CODE:
        {
            Ps_Foreground43Begin(); /* Centre the proportional heading independently of the keyboard. */
            f32 rise = Tb_WiiNamePanelOffset();
            if (sTb->accountCode || sTb->mode == TB_WII_NAME) {
                char title[128];
                f32 width;
                f32 x;
                s32 id;
                Port_AccountUiText(sTb->accountCode ? "account/code-title" : "dragon-net/name/title",
                                   title, sizeof(title));
                id = PortTbo_TextId(title, 5);
                width = id >= 0 ? 21.0f * PortTbo_TextInfo(id, 0) / 1000.0f : 0.0f;
                /* Use the same title sheet, inset and placement as Friend Battle. */
                x = 256.0f - width * 0.5f - 9.0f;
                if (id >= 0)
                    Ps_SpriteScreen(id, x, 52.0f + rise, x + 512.0f, 116.0f + rise,
                                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
            } else {
                s32 id = PortTbo_TextId(sTbWiiText[TB_WII_TX_BATTLE_ROW + 2], 5);
                f32 width = id >= 0 ? 21.0f * PortTbo_TextInfo(id, 0) / 1000.0f : 0.0f;
                /* The heading glyphs begin nine pixels into its 512-pixel sheet. */
                Tb_WiiTitleText(TB_WII_TX_BATTLE_ROW + 2, 256.0f - width * 0.5f - 9.0f,
                                52.0f + rise, 1.0f, 1.0f, 0x80);
            }
            Ps_Foreground43End();
            Ps_ForegroundStretchBegin();
        }
        break;
    case TB_WII_VS:
        if (Port_WiiArtAvailable()) {
            if (sTb->found) {
                MsgWin_PortDrawNativeName(Port_LobbyName(0), 42, 70, 470, 111);
                MsgWin_PortDrawNativeName(Port_LobbyName(1), 42, 210, 470, 251);
            } else {
                MsgWin_PortDrawNativeName(Port_NetNameGet(), 42, 70, 470, 111);
            }
        }
        break;
    }
    Tb_PageIn(0);
    if (sTb->note > 0 && sTb->mode != TB_WII_NAME && sTb->mode != TB_WII_SETTINGS) {
        s32 a = sTb->note > 16 ? 0x80 : sTb->note * 8;

        Ps_Align(1);
        Ps_TextEx(256.0f, 330.0f, sTb->noteLine, 0.52f, 480.0f, Ps_Rgba(0xFF, 0xF8, 0xB0, a), Ps_Rgba(0xFF, 0xC8, 0x30, a), Ps_Rgba(0x5C, 0x24, 0x08, a));
        Ps_Align(0);
    }
}

/* The memory-card window is also used for the disconnect message. Keep its Dialog separate from the game's
   global instance, as the PC quit prompt does. Its text table must live in game-addressable memory. */
static Dialog *sTbDisconnectDialog;
static s32 sTbDisconnectPlain; /* memory-card confirmation style, through its closing animation */
static void *sTbDisconnectArchive;
static void *sTbDisconnectResource;
static struct {
    u32 offsets[2];
    u16 text[256];
} sTbDisconnectMessage;

static void Tb_DisconnectText(void) {
    const u8 *src = (const u8 *)sTb->disconnectLine;
    s32 used = 0;

    sTbDisconnectMessage.offsets[0] = 0;
    sTbDisconnectMessage.offsets[1] = sizeof(sTbDisconnectMessage.offsets);
    while (*src && used < 254) {
        u32 c = *src++;
        if (c >= 0xC2 && c <= 0xDF && (src[0] & 0xC0) == 0x80) {
            c = ((c & 0x1F) << 6) | (*src++ & 0x3F);
        } else if (c >= 0xE0 && c <= 0xEF && src[0] && src[1] &&
                   (src[0] & 0xC0) == 0x80 && (src[1] & 0xC0) == 0x80) {
            c = ((c & 0x0F) << 12) | ((src[0] & 0x3F) << 6) | (src[1] & 0x3F);
            src += 2;
        } else if (c >= 0x80) {
            c = '?';
        }
        if (c >= 0xD800 && c <= 0xDFFF) c = '?';
        sTbDisconnectMessage.text[used++] = (u16)c;
    }
    sTbDisconnectMessage.text[used] = 0;
}

static void Tb_DisconnectDialogInit(void) {
    Dialog *previous;

    if (sTbDisconnectDialog || sTbDisconnectArchive || !gProgress || !File_IsPartitionLoaded(1)) return;
    sTbDisconnectArchive = File_LoadSync(gProgress->baseFile + 0x19, NULL, 0);
    if (!sTbDisconnectArchive) return;
    sTbDisconnectResource = Sprite_Unpack(MPACK_AT(sTbDisconnectArchive, 2), NULL, NULL);
    if (!sTbDisconnectResource) {
        Heap_Free(sTbDisconnectArchive);
        sTbDisconnectArchive = NULL;
        return;
    }
    Tb_DisconnectText();
    previous = gDialog;
    Dialog_Init(MPACK_AT(sTbDisconnectResource, 1), sTbDisconnectMessage.offsets, sTb->accountPrompt ? 1 : 0);
    sTbDisconnectDialog = gDialog;
    sTbDisconnectPlain = sTb->accountPrompt != 0;
    if (sTbDisconnectDialog) {
        Dialog_SetLayout(0);
        Dialog_SetMsg(0);
        Dialog_SetChoices(sTb->accountPrompt);
        if (sTb->accountPrompt) Dialog_SetCursor(1);
        Dialog_Start(DIALOG_CMD_OPEN);
    }
    gDialog = previous;
}

static s32 Tb_AccountConfirmInput(void) {
    Dialog *previous = gDialog;
    s32 result;
    gDialog = sTbDisconnectDialog;
    /* Account questions own a separate Dialog instance. Route mouse input
       while that instance is selected, before native result handling. */
    {
        extern void Port_DialogMemoryMouse(void);
        Port_DialogMemoryMouse();
    }
    result = Dialog_Input(0);
    if (result == DIALOG_RESULT_SECOND ||
        (result == DIALOG_RESULT_FIRST && Port_AccountAccept())) {
        if (result == DIALOG_RESULT_SECOND) Port_AccountDiscard();
        sTb->accountPrompt = result == DIALOG_RESULT_FIRST ? 2 : 0;
        Dialog_Start(DIALOG_CMD_CLOSE);
        sTb->disconnectClosing = 1;
        sTb->disconnectCloseFrames = 0;
    } else if (result != DIALOG_RESULT_NONE) {
        sTbDisconnectDialog->flags &= ~DIALOG_FLAG_DECIDED;
        Dialog_SetCursor(1);
        if (result == DIALOG_RESULT_FIRST) {
            char code[16], format[256], message[256], warning[128];
            Port_AccountPoll(code, sizeof(code));
            Port_AccountUiText("account/save-error", warning, sizeof(warning));
            Port_AccountUiText("account/save-code", format, sizeof(format));
            Tb_AccountFormat(message, sizeof(message), format, code);
            Ps_Copy(sTb->disconnectLine, warning, sizeof(sTb->disconnectLine));
            Ps_Cat(sTb->disconnectLine, "\n", sizeof(sTb->disconnectLine));
            Ps_Cat(sTb->disconnectLine, message, sizeof(sTb->disconnectLine));
            gDialog = previous;
            Tb_DisconnectDialogTerm();
            return result;
        }
    }
    gDialog = previous;
    return result;
}

static s32 Tb_DisconnectDialogStartClose(void) {
    Dialog *previous;

    if (!sTbDisconnectDialog) return 0;
    previous = gDialog;
    gDialog = sTbDisconnectDialog;
    Dialog_Start(DIALOG_CMD_CLOSE);
    gDialog = previous;
    return 1;
}

static s32 Tb_DisconnectDialogIsOpen(void) {
    return sTbDisconnectDialog && (sTbDisconnectDialog->flash[0].flags & 2);
}

static s32 Tb_DisconnectDialogIsClosed(void) {
    return sTbDisconnectDialog && (sTbDisconnectDialog->flags & DIALOG_FLAG_CLOSED);
}

static void Tb_DisconnectDialogTerm(void) {
    Dialog *previous = gDialog;
    if (sTbDisconnectDialog) {
        gDialog = sTbDisconnectDialog;
        Dialog_Term();
        gDialog = previous;
        sTbDisconnectDialog = NULL;
    }
    sTbDisconnectPlain = 0;
    if (sTbDisconnectResource) {
        Heap_Free(sTbDisconnectResource);
        sTbDisconnectResource = NULL;
    }
    if (sTbDisconnectArchive) {
        Heap_Free(sTbDisconnectArchive);
        sTbDisconnectArchive = NULL;
    }
}

/* Shown after the saved menu state is restored from an interrupted match. */
static void Tb_DrawDisconnect(void) {
    Dialog *previous;
    f32 bob;
    f32 drop;
    s32 eyeAlpha;
    Tb_DisconnectDialogInit();
    if (!sTbDisconnectDialog) return;
    /* Screen coordinates bypass the centred 4:3 page used by menu artwork. */
    u32 dim = Ps_Rgba(0, 0, 0, 0x50);
    Ps_ScreenQuad(0.0f, 0.0f, 512.0f, 0.0f, 0.0f, 448.0f, 512.0f, 448.0f,
                  dim, dim, dim, dim);
    previous = gDialog;
    gDialog = sTbDisconnectDialog;
    extern void Port_DialogDraw43(s32 visible);
    Port_DialogDraw43(1);
    sTbDisconnectDialog = gDialog;
    gDialog = previous;
    if (sTbDisconnectPlain) return;
    bob = Tb_WiiIdleBob((sTb->frames * 3) % 2000, 4.0f);
    if (sTb->disconnectClosing) {
        drop = (f32)(sTb->disconnectCloseFrames++ * 6);
    } else {
        f32 enter = (f32)sTb->disconnectOpenFrames / 24.0f;
        if (enter > 1.0f) enter = 1.0f;
        /* Ease up from below the viewport; the eye shares the body's offset. */
        drop = 180.0f * (1.0f - enter) * (1.0f - enter) * (1.0f - enter);
        if (sTb->disconnectOpenFrames < 24) sTb->disconnectOpenFrames++;
    }
    Ps_Sprite(TBO_WII_DISCONNECT_GIRU, 192.0f, 274.0f + bob + drop, 320.0f, 424.0f + bob + drop,
                    0.0f, 0.0f, 1.0f, 1.0f, PS_PLAIN, 0x80);
    eyeAlpha = (s32)(56.0f * (1.0f - cosf((f32)(sTb->frames % 120) * (6.283185307f / 120.0f))));
    /* The glow occupies texels (1,1)..(20,21) in its 32x32 sheet; keep its round alpha edge. */
    Ps_Sprite(TBO_WII_DISCONNECT_GIRU_EYE, 249.0f, 328.0f + bob + drop, 268.0f, 348.0f + bob + drop,
                    1.0f / 32.0f, 1.0f / 32.0f, 20.0f / 32.0f, 21.0f / 32.0f, PS_PLAIN, eyeAlpha);
}

/* Wii reward plate: original panel, title, green labels and white digit sheet. */
static void Tb_RewardDigits(s32 value, f32 x, f32 y) {
    s32 place = 100000000, i;
    for (i = 0; i < 9; i++, place /= 10) {
        s32 digit = (value / place) % 10;
        f32 u = ((digit % 4) * 32 + 1) / 128.0f;
        f32 v = ((digit / 4) * 32 + 1) / 128.0f;
        Ps_SpriteScreen(TBO_WII_REWARD_DIGITS, x, y, x + 16, y + 25,
                        u, v, u + 20.0f / 128, v + 23.0f / 128, PS_PLAIN, 0x80);
        x += 14;
    }
}

static void Tb_DrawWiiReward(void) {
    Ps_SpriteScreen(TBO_WII_REWARD_PANEL, 157, 70, 567, 234,
                    3.0f / 512, 2.0f / 256, 413.0f / 512, 166.0f / 256, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_REWARD_TITLE, 170, 136, 354, 164,
                    0, 12.0f / 64, 184.0f / 256, 47.0f / 64, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_REWARD_LABELS, 207, 85, 273, 113,
                    0, 0, 74.0f / 128, 0.5f, PS_PLAIN, 0x80);
    Ps_SpriteScreen(TBO_WII_REWARD_LABELS, 207, 191, 273, 219,
                    0, 0.5f, 74.0f / 128, 1, PS_PLAIN, 0x80);
    Tb_RewardDigits(sTb->rewardZPoints, 360, 87);
    Tb_RewardDigits(gSaveData != NULL ? gSaveData->money : 0, 360, 193);
}

/* ----------------------------------------------------------------------------------------------- the frame loop */
/* The Duel menu's screen unpacked (the windows' data), while the screens run. */
static void *sTbWinRes;

static void Tb_WinInit(void) {
    void *icons;

    sTbWinRes = Sprite_Unpack(MPACK_AT(gMenuArc5, 1), NULL, NULL); /* (Duel_Main: DuelMenu_Run(1)) */
    icons = MPACK_AT(sTbWinRes, 24);
    Res_RelocateOffsets(&icons, icons, icons);
    IconWin_Init(MPACK_AT(sTbWinRes, 23), icons);
    IconWin_PortHideWords(); /* "Versus Menu" and its emblem: the screens' own title is written there */
    IconWin_Open();
    MsgWin_Init(MPACK_AT(sTbWinRes, 22), MPACK_AT(sTbWinRes, 21), 0, 0);
    MsgWin_SetText(NULL); /* the box only: the hosts' lines are the screens' own (Tb_BoxLine) */
    MsgWin_PortSameTop();
    MsgWin_Open();
}

static void Tb_WinTerm(void) {
    if (sTbWinRes == NULL) {
        return;
    }
    MsgWin_Term();
    IconWin_Term();
    Heap_Free(sTbWinRes);
    sTbWinRes = NULL;
}

s32 PortTbo_Run(void) {
    s32 dataPreviousState = 0, dataFailureShown = 0;
    sTbLanChoice = Port_LanChoice(sTbLanAddress, sizeof(sTbLanAddress));
    sTbLan = sTbLanChoice != 3;
    if (!sTbLan) Match_DataAsk();
    sTb = Heap_Alloc(sizeof(TboScreen), 0x20, 0, 2);
    memset(sTb, 0, sizeof(TboScreen));
    Tb_LoadWiiText();
    Tb_SetWiiTitles();
    sTb->wiiPrevCur = -1;
    sTb->wiiSelectT = 0;
    sTb->mode = TB_WII_HOME;
    Port_WiiDialogText(0, sTb->wiiDialogText, sizeof(sTb->wiiDialogText));
    Port_NetNameGet();
    if (!sTbLan) Port_AccountRestore(Port_NetNameGet());
    Tb_WinInit();
    PortMenuMouse_Enable(1);
    sTbMouseCount=0;
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        s32 live;

        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        if (sTb->wiiNextMode && ColorFade_IsOutDone()) {
            s32 next = sTb->wiiNextMode;
            sTb->wiiNextMode = 0;
            Tb_SetMode(next);
            if (next == TB_WII_REWARD) {
                sTb->wiiHomeT = 40;
                sTb->wiiPanPointing = 0;
                Tb_WiiDialogSet(18);
            }
            if (next == TB_WII_VS && (!sTb->wiiFriend || sTb->host != 0)) Tb_WiiCustomSearchStart();
            ColorFade_StartIn(0, 0, 0, 0x14);
        }
        s32 dataState = Tb_DataState();
        if (dataState != 0 && dataPreviousState == 0) sTb->note = 0;
        dataPreviousState = dataState;
        live = !sTb->leaving && ColorFade_IsInDone();
        if (!sTb->leaving && !dataFailureShown && dataState < 0) {
            dataFailureShown = 1;
            Tb_ShowDisconnect(2);
        }
        Tb_MouseInput(live);
        if (sTb->disconnectReason) Tb_WiiUpdate(live);
        else switch (sTb->mode) {
        case TB_WII_REWARD:
        case TB_WII_HOME:
        case TB_WII_BATTLE:
        case TB_WII_CUSTOM:
        case TB_WII_FRIEND_MENU:
        case TB_WII_FRIEND_FORMAT:
        case TB_WII_FRIENDS:
        case TB_WII_SETTINGS:
        case TB_WII_RANKING:
        case TB_WII_VS:
            Tb_WiiUpdate(live);
            break;
        case TB_WII_NAME:
            Tb_WiiNameUpdate(live);
            break;
        case TB_WII_FRIEND_CODE:
            Tb_WiiNameUpdate(live);
            break;
        }
        Tb_Speak();
        sTbMouseCount=0;
        Port_FlashMenuMouseBegin();
        Ps_PageSet();
        Tb_Backdrop();
        if ((sTb->mode == TB_WII_HOME || sTb->mode == TB_WII_REWARD) && Port_WiiArtAvailable()) {
            IconWin_Draw(); /* the original corner scrollwork and title underline */
            Ps_Foreground43Begin();
            Tb_DrawWiiHomeArt();
            if (sTb->mode == TB_WII_REWARD) Tb_DrawWiiReward();
            Ps_Foreground43End();
        } else {
            if (sTb->mode != TB_WII_NAME && sTb->mode != TB_WII_FRIEND_CODE && sTb->mode != TB_WII_RANKING) IconWin_Draw();
            if (sTb->mode == TB_WII_NAME || sTb->mode == TB_WII_FRIEND_CODE)
                Ps_ForegroundStretchBegin();
            else Ps_Foreground43Begin();
            Tb_Draw();
            Tb_DrawText();
            Ps_Foreground43End();
        }
        /* The home page uses its own artwork branch, but its modal must still draw/advance. */
        if (sTb->disconnectReason) Tb_DrawDisconnect();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sTb->frames++;
        sTb->modeT++;
        if (sTb->mode == TB_WII_HOME && live && sTb->wiiHomeT < 40) sTb->wiiHomeT++;
        if ((sTb->mode == TB_WII_BATTLE || sTb->mode == TB_WII_CUSTOM ||
             sTb->mode == TB_WII_FRIEND_MENU || sTb->mode == TB_WII_FRIEND_FORMAT ||
             sTb->mode == TB_WII_FRIEND_CODE || sTb->mode == TB_WII_VS ||
             sTb->mode == TB_WII_RANKING) && live) sTb->wiiBattleT++;
        if (sTb->wiiDialogStarted && live) sTb->wiiDialogT++;
        if (sTb->wiiSelectT < 4 &&
            (!Port_WiiArtAvailable() ||
             (sTb->mode != TB_WII_HOME || sTb->wiiHomeT >= 23) &&
             ((sTb->mode != TB_WII_BATTLE && sTb->mode != TB_WII_CUSTOM &&
               sTb->mode != TB_WII_FRIEND_MENU && sTb->mode != TB_WII_FRIEND_FORMAT) || sTb->wiiBattleT >= 30)))
            sTb->wiiSelectT++;
        if (sTb->wiiSelectT >= 4 && live && sTb->mode >= TB_WII_HOME)
            sTb->wiiFocusFrame++;
        if (sTb->note > 0) {
            sTb->note--;
        }
        if (sTb->leaving && ColorFade_IsOutDone()) {
            break;
        }
    }
    sceGsSyncPath(0, 0);
    Tb_DisconnectDialogTerm();
    if (sTb->wiiDialogStarted) Voice_Stop(0);
    Port_DcNameClose();
    Tb_WinTerm();
    PortMenuMouse_Enable(0);
    if (sTb->mode == TB_WII_VS) Port_LobbyCancel();
    {
        s32 saveRewards = Port_ZenkaiEnabled() && sTb->rewardSavePending;
        Heap_Free(sTb);
        sTb = NULL;
        /* Match Ultimate Battle: prompt, or silent Autosave, after menu teardown.
           Save or decline completes this visit; a later reward marks it again. */
        if (saveRewards) Port_MenuSaveOnExit();
    }
    Dma_ResetBuffers();
    return 0;
}

#endif

#include "battle/battle.h"
#include "sys/ramp.h"
extern int Port_NetMe(void);
extern int Port_NetBotSide(void);
extern const char *Port_NetName(int side);
extern s32 BattleResult_HasWinner(void);
extern s32 BattleResult_GetWinnerSide(void);
extern void BattleResult_Set(s32 flags, s32 reason);
typedef struct OnlineResult {
    s32 active, frames, winner, botDecided, rematchWait;
    s32 cursor[2], requested[2], ended[2];
} OnlineResult;
static OnlineResult *sOnlineResult;
extern void Port_NetResultScreen(s32 on);
extern void Port_NetResultLeave(void);
extern s32 Port_NetResultSolo(void);

static void OnlineResult_Update(void) {
    s32 side;
    if (Port_NetResultSolo() && !sOnlineResult->ended[Port_NetMe()])
        sOnlineResult->ended[!Port_NetMe()] = 1;
    if (!sOnlineResult->active) {
        sOnlineResult->active = 1;
        sOnlineResult->winner = BattleResult_HasWinner() ? BattleResult_GetWinnerSide() : -1;
    }
    if (sOnlineResult->rematchWait > 0) {
        if (--sOnlineResult->rematchWait == 0)
            BattleResult_Set(BTL_RESULT_ABORT, 0x10); /* stock return to character/team selection */
        return;
    }
    if (++sOnlineResult->frames < 20) return; /* ignore buttons carried from the victory sequence */
    for (side = 0; side < 2; side++) {
        u32 keys = gPad[side].pressed;
        s32 previousCursor = sOnlineResult->cursor[side];
        if (sOnlineResult->ended[side]) continue;
        if (side == Port_NetBotSide()) {
            keys = 0;
            /* This menu runs at 60 Hz (Gfx_EndFrame(1)), including during rollback. */
            if (sOnlineResult->frames >= 5 * 60) {
                if (sOnlineResult->ended[!side]) {
                    keys = PAD_RIGHT | PAD_CROSS;
                } else if (!sOnlineResult->botDecided) {
                    sOnlineResult->botDecided = 1;
                    keys = (Rand_Range(10) < 8 ? PAD_LEFT : PAD_RIGHT) | PAD_CROSS;
                }
            }
        }
        if (!(sOnlineResult->ended[0] || sOnlineResult->ended[1]) &&
            (keys & (PAD_LEFT | PAD_LSTICK_LEFT))) sOnlineResult->cursor[side] = 0;
        if (keys & (PAD_RIGHT | PAD_LSTICK_RIGHT)) sOnlineResult->cursor[side] = 1;
        if (side == Port_NetMe() && previousCursor != sOnlineResult->cursor[side])
            Snd_PlaySe(1, 0);
        if (keys & PAD_CROSS) {
            if (sOnlineResult->cursor[side]) {
                sOnlineResult->ended[side] = 1;
                if (side == Port_NetMe()) Snd_PlaySe(1, 1);
            } else if (!(sOnlineResult->ended[0] || sOnlineResult->ended[1]) &&
                       !sOnlineResult->requested[side]) {
                sOnlineResult->requested[side] = 1;
                if (side == Port_NetMe()) Snd_PlaySe(1, 1);
            }
        }
    }
    /* Rematching is over, but each player acknowledges only their own results. */
    if (sOnlineResult->ended[0] || sOnlineResult->ended[1]) {
        sOnlineResult->requested[0] = sOnlineResult->requested[1] = 0;
        sOnlineResult->cursor[0] = sOnlineResult->cursor[1] = 1;
        if (sOnlineResult->ended[Port_NetMe()]) {
            Port_NetResultLeave();
            BattleResult_Set(BTL_RESULT_ABORT, 0x20);
        }
    } else if (sOnlineResult->requested[0] && sOnlineResult->requested[1])
        sOnlineResult->rematchWait = 3 * 60;
}

static void OnlineResult_MouseInput(s32 closing) {
    PortMenuMouse mouse;
    s32 i,me=Port_NetMe(),hit=-1;
    if(me<0 || me>1) return;
    PortMenuMouse_Ready(!closing && ColorFade_IsInDone() && sOnlineResult->frames>=20 && !sOnlineResult->rematchWait);
    PortMenuMouse_Read(&mouse);
    for(i=0;i<sTbMouseCount;i++) {
        TbMouseRow *r=&sTbMouseRows[i];
        if(mouse.x>=r->x0 && mouse.x<r->x1 && mouse.y>=r->y0 && mouse.y<r->y1 &&
           (r->index || !(sOnlineResult->ended[0] || sOnlineResult->ended[1]))) {
            hit=r->index;PortMenuMouse_SetHover(1);break;
        }
    }
    if(hit>=0 && (mouse.moved || mouse.confirm) && sOnlineResult->cursor[me]!=hit) {
        sOnlineResult->cursor[me]=hit;Snd_PlaySe(1,0);
    }
    if(mouse.confirm && hit>=0) gPad[me].pressed|=PAD_CROSS;
    if(mouse.cancel) {sOnlineResult->cursor[me]=1;gPad[me].pressed|=PAD_CROSS;}
}

/* Scale each badge around its own center so the lettering and backing move together. */
static void Tb_ResultPulseSprite(s32 id, f32 x0, f32 y0, f32 x1, f32 y1,
                                 f32 u0, f32 v0, f32 u1, f32 v1, f32 scale) {
    f32 cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    f32 hw = (x1 - x0) * 0.5f * scale, hh = (y1 - y0) * 0.5f * scale;
    Ps_SpriteScreen(id, cx - hw, cy - hh, cx + hw, cy + hh,
                    u0, v0, u1, v1, PS_PLAIN, 128);
}

/* This is a menu phase: no battle objects, HUD, scene updates, or battle drawing remain alive. */
s32 PortTbo_ResultRun(s32 battles, s32 wins0, s32 wins1, s32 *points) {
    s32 ownedArc = gMenuArc5 == NULL;
    s32 decision = 0, closing = 0;
    char labels[2][128];
    s32 edited[2];
    s32 style = Port_NetBattleMenuStyle(-1);
    extern s32 gPortMenuMode;
    Port_NetResultScreen(1);
    gPortMenuMode = 1;
    if (ownedArc) gMenuArc5 = File_LoadSync(gProgress->baseFile, NULL, 0);
    sTb = Heap_Alloc(sizeof(*sTb), 0x20, 0, 2);
    memset(sTb, 0, sizeof(*sTb));
    sTb->mode = TB_WII_VS;
    sTb->wiiNormalBattle = style & 1;
    sTb->wiiFriend = (style >> 1) & 1;
    sTb->wiiRanked = (style >> 2) & 1;
    /* Only ranked battles award points. */
    {
        extern u64 gPortNetBattleEvents[2];
        extern s32 UbScore_OnlinePoints(void *, f32, s32, s32, u64);
        BattleResult *result = Battle_GetResult();
        extern int Port_NetStatsRanked(void);
        s32 displayDelta[2] = {0, 0}, side;
        s32 winner = BattleResult_HasWinner() ? BattleResult_GetWinnerSide() : -1;
        if (Port_NetStatsRanked()) {
            extern int Port_NetStatsStartPoints(int);
            s32 earned[2];
            void *arc = File_LoadSync(gProgress->baseFile + 3, NULL, 0);
            void *res = Sprite_Unpack(MPACK_AT(arc, 3), NULL, NULL);
            for (side = 0; side < 2; side++)
                earned[side] = UbScore_OnlinePoints(MPACK_AT(res, 13), result->health[side],
                                                   result->unk24[side], result->unk1C[side],
                                                   gPortNetBattleEvents[side]);
            Heap_Free(res);
            Heap_Free(arc);
            if (winner >= 0 && winner < 2) {
                s32 loser = !winner;
                s32 loss = earned[loser] - earned[winner];
                displayDelta[winner] = Tb_PointDisplay(earned[winner]);
                displayDelta[loser] = loss >= 0 ? Tb_PointDisplay(loss) : -((-loss) / 100);
            }
            /* Keep balances unchanged until both participant reports are settled.
               Tb_WiiVsRecord refreshes them from the server-confirmed result. */
            for (side = 0; side < 2; side++) {
                s32 previous = Port_NetStatsStartPoints(side) * 100;
                points[side] = previous;
                sTb->resultPointDelta[side] = 0;
                sTb->resultPoints[side] = points[side];
            }
            sTb->resultHasPoints = 1;
        }
        {
            extern void Port_NetStatsEnd(s32 winner, s32 finish, s32 durationMs, s32 p1Points, s32 p2Points);
            s32 finish = (result->reason & BTL_REASON_FLAG7) ? 3
                       : (result->reason & BTL_REASON_TIME_UP) ? 2
                       : winner >= 0 && result->health[winner] >= 100.0f ? 1 : 0;
            s32 durationMs = (result->clock.hours * 3600 + result->clock.minutes * 60 +
                              result->clock.seconds) * 1000 + result->clock.ms;
            Port_NetStatsEnd(winner, finish, durationMs, displayDelta[0], displayDelta[1]);
        }
    }
    Tb_LoadWiiText();
    Tb_SetWiiTitles();
    /* Online boot bypasses the main menu, so its heading samples may never have
       been captured. Sample the original main-menu movie before creating our window. */
    {
        void *arc = File_LoadSync(gProgress->baseFile + 1, NULL, 0);
        void *res = Sprite_Unpack(MPACK_AT(arc, 2), NULL, NULL);
        void *icons = MPACK_AT(res, 23);
        extern void IconWin_PortCaptureMainHeading(void);
        Res_RelocateOffsets(&icons, icons, icons);
        IconWin_Init(MPACK_AT(res, 22), icons);
        IconWin_PortCaptureMainHeading();
        IconWin_Term();
        Heap_Free(res);
        Heap_Free(arc);
    }
    Tb_WinInit();
    edited[0] = PortTbo_ResultTextCopy(0, labels[0], sizeof(labels[0]));
    edited[1] = PortTbo_ResultTextCopy(1, labels[1], sizeof(labels[1]));
    sOnlineResult = Heap_Alloc(sizeof(*sOnlineResult), 0x20, 0, 2);
    memset(sOnlineResult, 0, sizeof(*sOnlineResult));
    sOnlineResult->winner = BattleResult_HasWinner() ? BattleResult_GetWinnerSide() : -1;
    sOnlineResult->active = 1;
    PortMenuMouse_Enable(1);
    sTbMouseCount=0;
    Bgm_Play(0x10B1B);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        s32 side, me, disabled;
        f32 pulse = 1.0f + 0.06f * sinf((f32)(sTb->frames % 90) * (6.283185307f / 90.0f));
        Job_Run();
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        OnlineResult_MouseInput(closing);
        if (!closing && ColorFade_IsInDone()) {
            OnlineResult_Update();
            if (Battle_GetResult()->winner & BTL_RESULT_ABORT) {
                decision = (Battle_GetResult()->reason & 0x10) != 0;
                closing = 1;
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
        sTbMouseCount=0;
        Ps_PageSet();
        Ps_SetShift(0);
        Tb_Backdrop();
        IconWin_Draw();
        Ps_Foreground43Begin();
        Tb_DrawWiiVs();
        MsgWin_PortPrepareNativeNames();
        MsgWin_PortDrawNativeName(Port_NetName(0), 42, 70, 470, 111);
        MsgWin_PortDrawNativeName(Port_NetName(1), 42, 210, 470, 251);
        if (sOnlineResult->winner >= 0) {
            f32 top = sOnlineResult->winner ? 203.0f : 63.0f;
            Tb_ResultPulseSprite(TBO_WII_RESULT_BADGE, 12, top - 22, 110, top + 70,
                                 0, 0, 1, 1, pulse * 0.75f);
            Tb_ResultPulseSprite(TBO_WII_RESULT_WIN, 8, top - 22, 118, top + 69,
                                 18.0f / 128, 18.0f / 128, 117.0f / 128, 109.0f / 128, pulse * 0.75f);
        }
        me = Port_NetMe();
        if (me < 0 || me > 1) me = 0;
        disabled = sOnlineResult->ended[0] || sOnlineResult->ended[1];
        for (side = 0; side < 2; side++) {
            f32 x = side ? 270.0f : 10.0f;
            s32 selected = sOnlineResult->cursor[me] == side;
            u32 tint = side == 0 && disabled ? Ps_Rgba(0x58, 0x58, 0x58, 0x80) : PS_PLAIN;
            Tb_MouseRow(side,x,382,232,32,0,0);
            Ps_SpriteScreen(selected ? TBO_WII_RESULT_BAR_GOLD : TBO_WII_RESULT_BAR_BLUE,
                            x, 382, x + 232, 414, 0, 0, 225.0f / 256, 37.0f / 64, tint, 0x80);
            if (PortTbo_WiiUseFont() || edited[side]) {
                s32 id = PortTbo_TextId(labels[side], selected ? 4 : 3);
                if (id >= 0) {
                    f32 width = 19.0f * (f32)PortTbo_TextInfo(id, 0) / 1000.0f;
                    f32 fit = width > 208.0f ? 208.0f / width : 1.0f;
                    f32 left = x + (232.0f - width * fit) * 0.5f;
                    Ps_SpriteScreen(id, left, 398.0f - 16.0f * fit,
                                    left + 512.0f * fit, 398.0f + 16.0f * fit,
                                    0, 0, 1, 1, tint, 0x80);
                }
            } else {
                Ps_SpriteScreen(selected ? TBO_WII_RESULT_LABEL_SELECTED : TBO_WII_RESULT_LABEL_IDLE,
                                x + 12, 381, x + 220, 413,
                                0, side * 0.5f, 1, (side + 1) * 0.5f, tint, 0x80);
            }
            if (sOnlineResult->requested[side] || sOnlineResult->ended[side]) {
                f32 bx = (side ? 170.0f : 26.0f) + (sOnlineResult->ended[side] ? 260.0f : 0.0f);
                /* Share a center for the badge and player lettering. */
                f32 cx = bx + 32, cy = 364;
                Ps_SpriteScreen(TBO_WII_RESULT_BADGE,
                                cx - 36 * pulse, cy - 32 * pulse, cx + 32 * pulse, cy + 24 * pulse,
                                0, 0, 1, 1, PS_PLAIN, 128);
                Tb_ResultPulseSprite(TBO_WII_VS_PLAYERS, bx, 348, bx + 64, 380,
                                     0, side * 0.5f, 1, (side + 1) * 0.5f, pulse);
            }
        }
        Ps_Foreground43End();
        ColorFade_Draw();
        File_Stub264D90();
        Gfx_EndFrame(1);
        Dma_Flush();
        sTb->frames++;
        /* Match the other Wii pages: play the heading entrance after fade-in. */
        if (!closing && ColorFade_IsInDone()) sTb->wiiBattleT++;
        if (closing && ColorFade_IsOutDone()) break;
    }
    sceGsSyncPath(0, 0);
    Tb_WinTerm();
    PortMenuMouse_Enable(0);
    Heap_Free(sOnlineResult);
    sOnlineResult = NULL;
    Heap_Free(sTb);
    sTb = NULL;
    if (ownedArc) { Heap_Free(gMenuArc5); gMenuArc5 = NULL; }
    Adx_StopAll();
    gPortMenuMode = 0;
    Port_NetResultScreen(0);
    return decision;
}
