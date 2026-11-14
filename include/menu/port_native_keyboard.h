#ifndef MENU_PORT_NATIVE_KEYBOARD_H
#define MENU_PORT_NATIVE_KEYBOARD_H
#define PORT_KEYBOARD_CONFIRM (-1)
#define PORT_KEYBOARD_DELETE (-2)
/* Game thread: enable only while a native game keyboard accepts input. */
void Port_NativeKeyboardSet(int active);
int Port_NativeKeyboardPop(void);
/* 0 uppercase, 1 lowercase, -1 while native input is disabled/captured. */
int Port_NativeKeyboardPage(void);
#endif
