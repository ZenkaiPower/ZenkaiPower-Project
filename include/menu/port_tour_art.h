#ifndef MENU_PORT_TOUR_ART_H
#define MENU_PORT_TOUR_ART_H

/* Dragon World Tour movie scopes; preserve native PS2 drawing. */
#ifdef PORT
extern void Port_FlashDrawMenuArt(void *movie, s32 screen, s32 visible);
extern void Port_MsgWinDrawInset43(s32 line);
extern void Port_GetWinDrawCentered43(void);
extern void Port_ItemPanelDrawMenuArt(s32 side);
extern void Port_ItemHelpDrawMenuArt(s32 item);
#define PORT_TOUR_DRAW(movie, scope) Port_FlashDrawMenuArt(movie, scope, 1)
#define PORT_TOUR_MESSAGE(line) Port_MsgWinDrawInset43(line)
#define PORT_TOUR_REWARDS() Port_GetWinDrawCentered43()
#define PORT_TOUR_ITEMS(side) Port_ItemPanelDrawMenuArt(side)
#define PORT_TOUR_HELP(item) Port_ItemHelpDrawMenuArt(item)
#else
#define PORT_TOUR_DRAW(movie, scope) Flash_Draw(movie)
#define PORT_TOUR_MESSAGE(line) MsgWin_Draw(0, 0, line)
#define PORT_TOUR_REWARDS() GetWin_Draw()
#define PORT_TOUR_ITEMS(side) ItemPanel_Draw(side)
#define PORT_TOUR_HELP(item) ItemHelp_Draw(item)
#endif

#endif
