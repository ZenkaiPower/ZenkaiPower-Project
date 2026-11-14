# Japanese music unlocks: disc verification

Verified directly against the local SLPS-25815 ISO, its SLPS_258.15 executable,
BIN/DBZP.BIN overlay, and DATA/PZS3JP1.AFS. No gameplay changes are made by this report.

## Confirmed unlocks

| Music slot | Japanese title / English name | Requirement |
| --- | --- | --- |
| 7 | ロマンティックあげるよ / I'll Give You Romance | Complete the story ending |
| 20 | Super Survivor (Vocal Full ver.) | Win the Otherworld Tournament, difficulty 3 |
| 21 | Super Survivor (Instrumental Full ver.) | Win the Cell Games, difficulty 2 |
| 22 | Super Survivor (Vocal OP ver.) | Win the World Martial Arts Big Tournament, difficulty 3 |
| 23 | Super Survivor (Instrumental OP ver.) | Win the World Tournament, difficulty 2 |

OP versions are the short versions referred to in unlock guides. Tournament IDs in
`include/menu/menu_l.h` are World = 0, Big = 1, Cell = 2, Otherworld = 3, Yamcha = 4.
Difficulty indices 0, 1, 2 correspond to displayed difficulties 1, 2, 3.

## Executable evidence

The Japanese ELF's default-music loop at virtual addresses 0x2662B8–0x266310
(file offsets 0x1672B8–0x167310) skips slot 7 and slots 20–23. It writes the other
music bits to the save field at offset 0xC30. This establishes that these five
songs start locked, unlike the current USA defaults.

The ending flow in the Japanese DBZP.BIN at file offsets 0x8230–0x8238 reads that
same save field, ORs it with 0x80, and writes it back: completing this flow unlocks
slot 7. The corresponding USA code survives in `src/menu/menu_b_c.c`.

The Japanese tournament reward handler has a seventh reward type. Its jump table
at virtual address 0x3B6EC0 maps index 6 to 0x36B058 (overlay file offset 0x36458;
overlay base 0x334C00). At file offsets 0x36458–0x36474, it computes `1 << value`
and ORs that into the save's music bits at offset 0xC30. This is executable music
unlock code, rather than a table entry that might go unused.

## Reward-table evidence

AFS entry 453 of JP1 is the first tournament bracket resource. Decode it with the
same BPE decoder as the game and take resource section 41. Its tournament records
are 0x64 bytes each; each has a 0x10-byte header followed by seven reward types,
each holding three signed difficulty values. Reward type 6 contains:

| Tournament | Difficulty 1 | Difficulty 2 | Difficulty 3 |
| --- | ---: | ---: | ---: |
| World | -1 | 23 | -1 |
| Big | -1 | -1 | 22 |
| Cell | -1 | 21 | -1 |
| Otherworld | -1 | -1 | 20 |
| Yamcha | -1 | -1 | -1 |

`-1` means no reward. These values match all four tournament conditions above.

The USA counterpart, US1 entry 454, retains the same type-6 music reward values.
However, the original USA `Bracket_GivePrizes` (retained by the non-PORT branch in
`src/menu/menu_k_f.c`) ignores case 6;
`SAVE_BGM_COUNT` is 20; and its selection/sound-test menus drop the four extra
entries. The PORT branches now enable those entries and rewards for METEOR. The optional Japanese soundtrack import changes playback and names, but
originally retained those USA progression and selection rules. The port now restores Japanese offline progression
when METEOR is selected; see the music progression section of [README.md](README.md).

Restoring Japanese progression requires activating the retained music
reward type, exposing slots 20–23, and using the Japanese initial music unlock
mask. It does not require inventing tournament rewards. Save migration and online
state consistency also need to be handled when making that gameplay change.
