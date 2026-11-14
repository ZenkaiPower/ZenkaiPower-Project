#!/usr/bin/env python3
"""Online single/team grids hide password characters; offline grids retain them."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
single = (root / "src/menu/menu_c_e.c").read_text()
team = (root / "src/menu/menu_e_b.c").read_text()


def helper(source, name):
    start = source.index(f"static void {name}(")
    return source[start:source.index("\n}", start) + 2]


code = r'''
#include <assert.h>
typedef int s32;
#define MCHR_CUSTOM 0xA3
#define MCHR_EMPTY 0xA4
#define TS_CUSTOM 0xA3
#define TS_EMPTY 0xA4
static int session;
int Port_NetSession(void) { return session; }
typedef struct { int id; } Cell;
static struct { int cellCount[2], customCount[2]; Cell grid[2][4]; } charWork, *gCharSel = &charWork;
static struct { int cellCount[2], customCount[2]; Cell grid[2][4]; } teamWork, *gTeamSel = &teamWork;
''' + helper(single, "CharSel_HideOnlinePasswordCharacters") + "\n" + helper(team, "TeamSel_HideOnlinePasswordCharacters") + r'''
int main(void) {
    for (int side = 0; side < 2; side++) {
        charWork.cellCount[side] = teamWork.cellCount[side] = 3;
        charWork.customCount[side] = teamWork.customCount[side] = 1;
        charWork.grid[side][0].id = teamWork.grid[side][0].id = 1;
        charWork.grid[side][1].id = teamWork.grid[side][1].id = MCHR_CUSTOM;
        charWork.grid[side][2].id = teamWork.grid[side][2].id = 2;
    }
    session = 1;
    for (int side = 0; side < 2; side++) {
        CharSel_HideOnlinePasswordCharacters(side);
        TeamSel_HideOnlinePasswordCharacters(side);
        assert(charWork.grid[side][0].id == 1 && teamWork.grid[side][0].id == 1);
        assert(charWork.grid[side][1].id == MCHR_EMPTY && teamWork.grid[side][1].id == TS_EMPTY);
        assert(charWork.grid[side][2].id == 2 && teamWork.grid[side][2].id == 2);
        assert(charWork.customCount[side] == 0 && teamWork.customCount[side] == 0);
    }
    charWork.grid[0][1].id = teamWork.grid[0][1].id = MCHR_CUSTOM;
    charWork.customCount[0] = teamWork.customCount[0] = 1;
    session = 0;
    CharSel_HideOnlinePasswordCharacters(0);
    TeamSel_HideOnlinePasswordCharacters(0);
    assert(charWork.grid[0][1].id == MCHR_CUSTOM && teamWork.grid[0][1].id == TS_CUSTOM);
    assert(charWork.customCount[0] == 1 && teamWork.customCount[0] == 1);
}
'''
assert single.index("ChrGrid_Build(&gCharSel->cellCount[i]") < single.index("CharSel_HideOnlinePasswordCharacters(i);")
assert team.index("ChrGrid_Build(&gTeamSel->cellCount[i]") < team.index("TeamSel_HideOnlinePasswordCharacters(i);")
with tempfile.TemporaryDirectory(prefix="online-password-") as directory:
    path = Path(directory) / "password.c"
    exe = Path(directory) / "password"
    path.write_text(code)
    subprocess.run(["cc", "-std=gnu99", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("Online password-character selection checks passed")
