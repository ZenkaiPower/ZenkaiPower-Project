#ifndef PORT_BOT_BATTLE_LOG_H
#define PORT_BOT_BATTLE_LOG_H
#include <stdio.h>
#include <string.h>
typedef struct BotBattleLog { int active, side; char names[2][64]; } BotBattleLog;
static void BotBattle_Start(BotBattleLog *log, FILE *out, int side, const char *p1, const char *p2) {
    if (log->active || side < 0 || side > 1) return;
    snprintf(log->names[0], sizeof(log->names[0]), "%s", p1 && *p1 ? p1 : "Player 1");
    snprintf(log->names[1], sizeof(log->names[1]), "%s", p2 && *p2 ? p2 : "Player 2");
    log->side = side; log->active = 1;
    fprintf(out, "zenkaipower: battle: START | %s vs %s\n", log->names[side], log->names[1-side]);
}
static void BotBattle_End(BotBattleLog *log, FILE *out, int winner, int finish, int ms) {
    if (!log->active) return;
    fprintf(out, "zenkaipower: battle: %s | %s vs %s | %s | %d:%02d\n",
            winner < 0 || winner > 1 ? "DRAW" : winner == log->side ? "WIN" : "LOSS",
            log->names[log->side], log->names[1-log->side],
            finish == 3 ? "Ring out" : finish == 2 ? "Time limit" : finish == 1 ? "Perfect" : "Battle finished",
            ms > 0 ? ms/60000 : 0, ms > 0 ? (ms/1000)%60 : 0);
    log->active = 0;
}
static void BotBattle_Interrupt(BotBattleLog *log, FILE *out) {
    if (!log->active) return;
    fprintf(out, "zenkaipower: battle: INTERRUPTED | %s vs %s | No result\n", log->names[log->side], log->names[1-log->side]);
    log->active = 0;
}
#endif
