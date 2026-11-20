# CPU base difficulty changes by game mode

Sparking! Changes applies a tapered increase to ordinary CPU base levels,
starting at **+7** and decreasing for stronger opponents. Effective levels remain
capped at **29**. Duel, Training, and Ultimate Training keep their original
levels. Dragon History applies the same taper before its story difficulty offsets.

The change requires **Zenkai Mode with Classic Mode off**, the
`PORT_ZENKAI_RAISED_DIFFICULTY` build flag, and an offline battle. Classic Mode,
Original mode, and LAN/online sessions retain their original difficulty rules.

## Ordinary CPU level conversion

A base level comes from the mode's opponent data; the effective level is used by
the live CPU controller. Ordinary modes now use this mapping:

| Original base | Increase | Effective range |
|---|---:|---|
| 0–9 | +7 | 7–16 |
| 10–14 | +6 | 16–20 |
| 15–19 | +5 | 20–24 |
| 20–24 | +4 | 24–28 |
| 25–28 | +3, capped at 29 | 28–29 |
| 29 | 0 | 29 |

The mapping never decreases across a band boundary. Some neighboring bases
share an output, but only bases **26–29** reach the maximum, compared with
**19–29** under the previous flat +10 rule. Bases **21 and above** reach 25–29;
the previous threshold was 15. Negative dummy and scripted behavior values
remain unchanged.

## Effects by mode

| Mode | Effect |
|---|---|
| Duel, single and team battles | Original effective levels 0, 6, 13, 21, 29, including pause-menu selection. |
| Dragon History | Tapers the scripted base first, then applies Easy/Normal/Hard offsets of -5/0/+5, clamped to 0-29. Both setup and scripted level changes use this rule. |
| Dragon World Tour | Tapered conversion after the tournament chooses its opponent and round base. |
| Ultimate Battle and Mission 100 | Tapered conversion of the opponent levels supplied by mission, course, or ranking data. |
| Survival | Tapered conversion for each sequential opponent. |
| Sim Dragon | Tapered conversion for each selected enemy. |
| Disc Fusion | Tapered conversion for courses, ranking opponents, and intruders. |
| Training and Ultimate Training | Original active CPU levels and negative dummy settings; the selection persists across switches and transformations. |
| Scripted training lessons | Negative behavior settings remain unchanged. Other levels follow the battle's applicable conversion. |
| CPU versus CPU | Follows the containing mode: Duel preserves levels; affected modes apply their conversion. |
| Dragon Net Battle, LAN and online | Original levels; network checks disable the increase. |

This feature does not itself change opponent rosters, health, equipment,
tournament pools, or unlock conditions.

## Dragon History difficulty offsets

For a nonnegative scripted base `B`, let `T(B)` be the ordinary tapered
conversion, including its cap at 29. Story applies the selected difficulty after
tapering:

| Difficulty | Original offset | Sparking! Changes effective level |
|---|---:|---|
| Easy | 0 | `clamp(T(B) - 5, 0, 29)` |
| Normal | +6 | `T(B)` |
| Hard | +9 | `clamp(T(B) + 5, 0, 29)` |

Applying the offset after tapering preserves five-level steps around Normal
until the cap intervenes. A scripted base of 13 becomes **14, 19, 24**;
a base of zero becomes **2, 7, 12**. Negative dummy settings remain unchanged.
Story values are tagged after the calculation to prevent applying the taper
a second time during controller setup or resets.

In the audited story content, all scripted bases are below 15. Consequently,
Hard retains five levels above Normal and Easy five below Normal throughout
these scripts. Higher hypothetical bases can still compress at the cap.

## Distribution in the installed game data

The following static audit uses `build/ZenkaiPower-Windows/gamedata`.

| Sample | Entries counted | Original average level | With changes average level | Approximate Duel label before → after | Original 20–24 | With changes 20–24 | Original 25–29 | With changes 25–29 | Original at 29 | With changes at 29 |
|---|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|
| Mission 100 opponents | 321 | 17.22 | 22.03 | Hard → Hard | 24.9% | 53.0% | 6.5% | 23.4% | 0.3% | 6.5% |
| Survival opponent appearances | 150 | 13.44 | 19.16 | Normal → Hard | 6.0% | 38.0% | 2.0% | 6.7% | 0.0% | 0.7% |
| Disc Fusion course opponent appearances | 34 | 13.21 | 18.91 | Normal → Hard | 5.9% | 38.2% | 0.0% | 2.9% | 0.0% | 0.0% |
| Disc Fusion ranking opponents | 100 | 15.37 | 20.63 | Normal → Hard | 18.0% | 31.0% | 6.0% | 20.0% | 2.0% | 5.0% |
| Disc Fusion intruders | 37 | 23.00 | 27.00 | Hard → Very Hard | 100.0% | 0.0% | 0.0% | 100.0% | 0.0% | 0.0% |
| Sim Dragon rounds | 7 | 17.86 | 22.71 | Hard → Hard | 42.9% | 57.1% | 0.0% | 28.6% | 0.0% | 0.0% |
| Story Easy enemy setup commands | 82 | 6.65 | 8.54 | Easy → Easy | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% |
| Story Normal enemy setup commands | 82 | 12.65 | 13.54 | Normal → Normal | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% | 0.0% |
| Story Hard enemy setup commands | 82 | 15.65 | 18.54 | Normal → Hard | 2.4% | 48.8% | 0.0% | 0.0% | 0.0% | 0.0% |

### Change from the previous flat increase

| Sample | At 25–29 with flat +10 | At 25–29 with taper | At 29 with flat +10 | At 29 with taper |
|---|---:|---:|---:|---:|
| Mission 100 | 67.6% | 23.4% | 39.6% | 6.5% |
| Survival | 36.7% | 6.7% | 12.0% | 0.7% |
| Disc Fusion courses | 32.4% | 2.9% | 5.9% | 0.0% |
| Disc Fusion ranking ladder | 44.0% | 20.0% | 28.0% | 5.0% |
| Disc Fusion intruders | 100.0% | 100.0% | 100.0% | 0.0% |
| Sim Dragon rounds | 71.4% | 28.6% | 42.9% | 0.0% |

Approximate Duel labels use the nearest original preset: Very Easy (0), Easy
(6), Normal (13), Hard (21), and Very Hard (29). Exact midpoint ties choose the
higher preset. This comparison labels numeric averages; it does not measure
combat strength or change the in-game difficulty labels.

Original averages are arithmetic means over the entries in each row, including
original story offsets of 0, +6, and +9. These samples use different units:
mission opponent slots, course appearances, ranking opponents, tournament
presets, and static story commands. They are not weighted by player activity.
There is no combined percentage of all played battles.

The [full audit](cpu-level-distribution.md) includes tournament tiers and story
level-change commands, with all counting rules. Reproduce it with:

```sh
python port/tools/audit_cpu_levels.py --data build/ZenkaiPower-Windows/gamedata
```

Generated JSON in `port/build/cpu-level-audit.json` includes histograms,
group counts, and hashes of the decoded source files.

## Behavior and level persistence

Higher levels feed the existing rates, reactions, charge choices, and delays.
The original five tuning bands remain in use; there are no new levels above
29 or additional tactical planners. Distribution counts establish level
compression, not perceived difficulty or measured win-rate improvement.

Opponent records retain their base levels, so resetting a fighter does not
compound the increase. Training restores its selected original level directly.
Duel values bypass tapering; completed story calculations are tagged to avoid
applying the taper twice.
The pause-menu label uses a canonical inverse where several bases share an
output; it does not modify stored opponent levels.

Disabling `PORT_ZENKAI_RAISED_DIFFICULTY` restores original CPU levels and story
offsets of 0/+6/+9. Training selection retention is independent of this flag.

## Implementation and verification

- [Level conversion and story offsets](../../include/battle/btl_ai_level.h).
- [Live controller setter and Training restoration](../../src/battle/btl_ai_mgr.c).
- [Story setup and scripted enemy level changes](../../src/battle/btl_script_cmd.c).
- [Pause-menu CPU selection](../../src/battle/hud_0.c).
- [Single battle setup](../../src/menu/menu_c_e.c) and [team battle setup](../../src/menu/menu_e_b.c).
- [AI behavior reference](../systems/ai.md) and [build flags](zenkai-build-flags.md).

Run the controller regression checks with:

```sh
python3 port/tests/battle/zenkai_ai.py
```

These checks cover conversion, story offsets, Duel preservation, network
isolation, negative settings, repeated initialization, and existing AI tuning.
They verify the implementation rules rather than the perceived difficulty of
every episode or opponent.
