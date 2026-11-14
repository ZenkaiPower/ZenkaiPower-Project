#include "../../src/gs/bot_battle_log.h"
#include <cassert>
#include <string>
int main() {
    FILE *out = tmpfile(); assert(out);
    BotBattleLog log{};
    BotBattle_Start(&log, out, 0, "Bot", "Opponent");
    BotBattle_Start(&log, out, 0, "Bot", "Opponent"); // duplicate callback
    BotBattle_End(&log, out, 0, 1, 65000);
    BotBattle_End(&log, out, 0, 1, 65000);
    BotBattle_Start(&log, out, 1, "Opponent", "Bot"); BotBattle_End(&log, out, 0, 3, 4000);
    BotBattle_Start(&log, out, 1, "Opponent", "Bot"); BotBattle_End(&log, out, -1, 2, 0);
    BotBattle_Start(&log, out, 0, "Bot", "Opponent"); BotBattle_Interrupt(&log, out); BotBattle_Interrupt(&log, out);
    fseek(out, 0, SEEK_END); std::string text((size_t)ftell(out), '\0'); rewind(out);
    assert(fread(text.data(), 1, text.size(), out) == text.size()); fclose(out);
    assert(text.find("WIN | Bot vs Opponent | Perfect | 1:05") != std::string::npos);
    assert(text.find("LOSS | Bot vs Opponent | Ring out") != std::string::npos);
    assert(text.find("DRAW | Bot vs Opponent | Time limit") != std::string::npos);
    assert(text.find("INTERRUPTED | Bot vs Opponent | No result") != std::string::npos);
    assert(text.find("WIN |", text.find("WIN |") + 1) == std::string::npos);
    puts("PASS: bot/opponent names, both bot sides, win/loss/draw, finish type, duration, interruption and duplicate callbacks");
}
