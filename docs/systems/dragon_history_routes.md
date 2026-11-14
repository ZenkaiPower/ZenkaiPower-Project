# Dragon History ending routes

The original episodes distinguish **ending actions**, but do not expose an
explicit **canon completion classification**. A story-consistent ending can be
identified from its event chain and dialogue; that is an interpretation of the
episode, not a flag saved by the game.

## Decoded data

The local original-game data yielded 48 scripts (story IDs 0..47, game file IDs
0x1FF..0x22E), 1,008 action definitions and 372 ending action definitions. Every
CALL and registered event target resolved to an action in its script. These
counts include defined actions without an incoming edge in the static graph;
they are not a count of verified playable endings.

Command 9's nonnegative result operand maps through `BtlFacade_SetResult`:

| Operand | Game result | Ending actions found |
|---:|---|---:|
| 0 | Defeat, KO reason | 204 |
| 1 | Victory, KO reason | 166 |
| 2 | Defeat, flag-7 reason (ring-out/forced defeat) | 1 |
| 3 | Victory, flag-7 reason (ring-out/forced defeat) | 1 |

`-1` leaves the outcome unset while the episode continues. Action ranges
10030..10039 and 10040..10049 are treated as event priority groups by
`BtlScript_ScanEvents`; their numbering does not establish canonical status.
The GSHD chunk's contents remain uninterpreted here; the game's loader stores
its address but does not read a canon designation from it.

## Verified example: Goku/Piccolo versus Raditz

Story 0, file 0x1FF, has three victory ending actions:

| Action | Content and route | Result operand |
|---:|---|---:|
| 10030 | Piccolo's direct victory dialogue; opponent KO during several earlier Piccolo phases | 1 |
| 10031 | Goku's direct victory dialogue; opponent KO during several earlier Goku phases | 1 |
| 10032 | Longer ending in which Raditz warns of the approaching Saiyans and Piccolo responds; opponent KO after the later scripted phase 10006 | 1 |

The latter is the **story-consistent/canon-like ending**, inferred from the
scene chain and dialogue. All three explicitly set the same victory operand.

The intended longer progression is registered as:

1. `10000 -> 10001`: the sub-clock reaches 20 seconds; command 8 switches to
   member 1.
2. `10001 -> 10002`: the sub-clock reaches 40 seconds; the script switches back
   to member 0.
3. `10002 -> 10003`: the sub-clock reaches 30 seconds; dialogue introduces
   Piccolo's technique and the need to buy time.
4. `10003 -> 10005`: the sub-clock reaches 30 seconds; the script switches to
   member 1. An additional action 10004 has an opponent-event-0x22 registration
   for the same destination; that event is left unnamed here.
5. `10005 -> 10006`: the sub-clock reaches 10 seconds; another scripted setup
   occurs.
6. In phase `10006`, side 0's opponent-KO event `0x48` selects ending `10032`.

These times are **relative to sub-clock resets**, not one cumulative timer.
Player-side scene/member transitions can also wait for R3 confirmation and
interruptible fighters in `BtlScript_StartPending`. This static registration
chain is not an independently tested gameplay walkthrough.

Defeating Raditz earlier can instead select 10030 or 10031. A player-side KO
during some stages can continue the episode by switching fighters, while other
stages lead to defeat actions 10040 or 10041. Consequently, neither a simple
KO nor a single global time limit describes all story outcomes.

## Verified example: Goku versus Nam

Story 40, file 0x227, separates ordinary victory actions 10030/10031 from
ring-out victory action 10050. Action 10050 is registered for side 1 event
0x49 (fighter flag 7), sets result operand 3 and plays dialogue explicitly
describing Nam falling out of the ring. The corresponding player ring-out
action 10051 sets result operand 2.

This is a mechanically distinct finishing method, encoded as a finish reason;
it still does not supply a generic canon marker.

## What is remembered

The original `HistResult_Init` checks the victory bit to mark the episode cleared.
`Port_StoryRecordClear` additionally stores the highest beaten difficulty in
ZENKAI. These existing fields do not identify which ending occurred.
Every SetRewards command found in these 48 scripts occurs in setup action -3;
no ending-specific SetRewards command was found. Rewards still depend on
difficulty and the existing unlock/reward rules.

ZENKAI now adds **victory-ending history**, preserving each distinct action ID
reached for a saga/episode. No canon-completion label is inferred or saved.
Defeat endings remain excluded.

### ZENKAI persistence

- Command 9 captures the current GSC task's action ID only for victory operands
  1 and 3 in story battle mode. The current task is used rather than assuming
  that `gBtlScript.cur.action` identifies every nested task correctly.
- Capture resets on battle initialization/restart and survives script teardown.
  `HistResult_Init` commits it only when the story ID, final victory flags and
  KO/flag-7 reason match. Aborted tasks, aborted fights, restarts, defeats,
  original mode and online sessions do not collect endings.
- A versioned `ZEN1` block at save offset `0x3800` occupies 1,016 bytes through
  `0x3BF7`: an 8-byte header and 7 bytes of action bits for each of 9 sagas x 16
  episodes. Bits address actions 10000..10051; only validated victory actions
  are committed. The region ends before difficulty records at `0x3E00` and
  options at `0x3F00`. The save remains 0x4000 bytes and the versioned binary
  writer persists the entire block.
- Records accumulate, duplicates have no effect, and endings are shared across
  difficulties. Legacy saves remain readable and start with unknown/empty
  ending history; clear bits and unlock-all do not invent ending records.
- Normal story save / Autosave behavior persists the changes. Defeat behavior
  and its save flow are unchanged. This adds storage and query helpers, not an
  ending checklist screen.

Implementation: `include/menu/port_story_endings.h`,
`src/battle/port_story_endings.c`, the command-9 handler, script reset hooks,
and `src/menu/menu_c_c.c`. `Port_StoryHasEnding` queries a specific action;
`Port_StoryEndingCount` counts collected victories for an episode.

Run `python3 port/tests/battle/story_endings.py` for actual command-handler
capture, victory/ring-out collection, defeat and abort rejection, mode isolation,
duplicate accumulation, save-region bounds and binary write/read round trips
under AddressSanitizer / UBSan, without a UI.

The recent ZENKAI CPU changes affect difficulty, not this route classification.
The headless AI strength benchmark exercises ordinary battles, not these
episode-specific event chains.

## Reproducing the inspection

```bash
python3 port/tools/story_routes.py --data release/linux-x64/gamedata
python3 port/tests/battle/story_routes.py
```

The decoder reads user-owned loose/mod files or `pzs3us1.afs`, decompresses BPE
members, validates GSC chunks and operands, and exports:

- `port/build/story_routes.md`: all 48 scripts, ending actions, incoming
  registrations/calls and dialogue clues.
- `port/build/story_routes.json`: complete decoded commands, route graph,
  ending dialogue and per-script SHA-256 identities.

It deliberately does not assign automatic canon labels. The graph follows
CALL words and command-7 registrations; a missing static path does not prove
that an action cannot be reached through engine behavior. No game UI runs and
no game assets, saves or gameplay rules are modified.

Source anchors: `src/sys/gsc.c`, `src/battle/btl_script.c`,
`src/battle/btl_script_cmd.c`, `src/battle/btl_facade.c:456`,
`src/battle/btl_char_member.c:485`, `src/battle/battle_load.c:1141`,
`src/menu/menu_c_c.c:418`, and `include/menu/port_story_badge.h`.
