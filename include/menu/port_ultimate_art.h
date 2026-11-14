#ifndef MENU_PORT_ULTIMATE_ART_H
#define MENU_PORT_ULTIMATE_ART_H

/* Ultimate Battle opts into named foreground rules; native PS2 draws stay unchanged. */
#ifdef PORT
extern void Port_FlashDrawMenuArt(void *movie, s32 screen, s32 visible);
extern void Port_MsgWinDrawInset43(s32 line);
extern void Port_DialogDraw43(s32 visible);
#define PORT_UB_DRAW(movie, scope) Port_FlashDrawMenuArt(movie, scope, 1)
#define PORT_UB_MESSAGE(line) Port_MsgWinDrawInset43(line)
#define PORT_UB_DIALOG(visible) Port_DialogDraw43(visible)
#else
#define PORT_UB_DRAW(movie, scope) Flash_Draw(movie)
#define PORT_UB_MESSAGE(line) MsgWin_Draw(0, 0, line)
#define PORT_UB_DIALOG(visible) Dialog_Draw(visible)
#endif

#endif
