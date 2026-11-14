#ifndef PORT_ACCOUNT_H
#define PORT_ACCOUNT_H
/* Calls never wait for HTTP on the game thread. Poll text is a recovery code or error.
   States: 0 idle, 1 working, 2 enter code, 3 save generated code, 4 logged in, -1 error. */
int Port_AccountBegin(const char *username, const char *code);
void Port_AccountRestore(const char *username);
int Port_AccountPoll(char *text, int capacity);
int Port_AccountLogged(const char *username);
int Port_AccountAccept(void); /* starts asynchronous creation after Yes */
void Port_AccountDiscard(void); /* No: forget the unconfirmed code/ticket */
void Port_AccountUiText(const char *id, char *out, int capacity);
#endif
