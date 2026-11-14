#!/usr/bin/env python3
"""Regression: GNU ld's *(COMMON) directive must not hide game globals."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import make_state


def test_common_after_directive():
    game = make_state.OBJ / "battle_battle.o"
    port = make_state.OBJ / "gs_net.o"
    listing = f"""Linker script and memory map
.bss 0x1000 0x40
 *(COMMON)
 COMMON 0x1000 0x4 {game}
 COMMON 0x1010 0x4 {port}
"""
    game_ranges, port_ranges, _ = make_state.linux_map(listing)
    assert (0x1000, 0x1004) in game_ranges
    assert (0x1010, 0x1014) in port_ranges


if __name__ == "__main__":
    test_common_after_directive()
