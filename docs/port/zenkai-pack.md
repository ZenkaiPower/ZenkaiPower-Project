# Classic Mode: Zenkai pack

`zenkai_pack` is the seven-feature gameplay pack controlled by **Classic Mode**
under **Options → More Options → General Settings**. New offline Zenkai saves
start with the pack on and Classic Mode off. All seven build flags default to enabled.
Enabling Classic Mode permanently disables the pack; its displayed value is the inverse
of the saved pack state.

| Feature ID | Feature | Build flag | `battle_mechanic_change` |
|---|---|---|---|
| `giant_scaling` | Giant model, collision, camera/aim, effect and prop adjustments; original visual sizing during technique cinematics | `PORT_ZENKAI_GIANTS` | Yes |
| `dragon_history_unlocking` | Ten-character starting roster and Dragon History character rewards | `PORT_ZENKAI_STORY_UNLOCKS` | No |
| `world_tour_change` | Tournament progression pools, locks, bosses, participant diversity, DP results and Yamcha stages | `PORT_ZENKAI_TOURNAMENT` | No |
| `raise_base_level` | Raised CPU base level and revised story difficulty offsets | `PORT_ZENKAI_RAISED_DIFFICULTY` | No |
| `story_restrictions` | Story-accurate restrictions on Goku (Early)'s moves | `PORT_ZENKAI_STORY_RESTRICTIONS` | No |
| `goku_ssj2` | Goku (End) SSJ2 replacement blasts | `PORT_ZENKAI_SSJ2_MOVESET` | Yes |
| `tiered_kaioken` | Kaioken tiers, bonuses, upgrade costs and HP drain | `PORT_ZENKAI_KAIOKEN` | Yes |

`battle_mechanic_change` is a named subset containing exactly `giant_scaling`,
`goku_ssj2`, and `tiered_kaioken`. It has no separate menu toggle. The registry
exposes membership through `Port_ZenkaiFeatureInGroup`; feature availability uses
`Port_ZenkaiFeatureEnabled`. Unknown IDs/groups return false.

## Permanent choice for each save

| Saved state | Classic Mode | Entry state |
|---|---|---|
| New save; no Zeni earned | Off | Yes; can select On and confirm |
| Any positive Zeni earned, even if spent later | Off | Visible, disabled |
| Classic Mode enabled (pack disabled) | On | Visible, disabled |
| Older save without an earnings record | Off | Visible, disabled |

Selecting the option opens the native On/Off selector. Selecting On opens a
second Yes/No confirmation: "Enable Classic Mode? You cannot turn it off again."
No is the default; cancelling either selector changes nothing. The policy is
checked again when Yes commits. After earning Zeni or confirming Classic Mode,
the entry remains visible and focusable in General Settings, but is dimmed and
cannot open its picker. The row count and Default footer position stay unchanged.

Enabling Classic Mode retains the Zenkai save, Options menu, and existing progress.
It does not switch to `game.sav`. The original 132 starting entries are granted
without removing extra unlocks, since Dragon History unlocking is now disabled.
General Settings Default does not reset this choice. The normal Options save
flow persists it with the main game save; saving is required for restart persistence.
A new game is a new choice, independent of other saves.

Old saves cannot establish whether Zeni was earned and subsequently spent. They
keep the pack enabled and locked instead of using their current balance as proof.
Original-mode and online saves do not receive this pack policy. Compile-time
flags can still disable individual features; an On pack cannot override them.

## Persistence and reward coverage

The main 0x4000-byte payload reserves eight bytes at `0x3EF0`: `SPK`, version 1,
then a flags byte and three reserved zero bytes. Flag bit 0 records **ever earned
positive Zeni**; bit 1 records **permanently disabled**. The record is outside the
legacy Options block at `0x3F00` and is covered by the existing save checksums.
No main-save size or Options-companion format change is required.

New-game defaults initialize the record before applying the starting roster.
`Save_AddMoney` records positive rewards before balance clamping. The shared
`UbScore_Transfer` records positive integer transfers into save money, covering
Ultimate Battle, Mission 100, Survival and Sim Dragon. Spending, zero rewards,
and fractional transfers that round down to zero do not create earnings. Debug
Unlock All also records its positive money grant. Loading or spending cannot
clear either saved lock.

## Checks

- `port/tests/save/zenkai_pack.py`: saved locks, rewards, direct transfers, original
  roster on disable, IDs, groups and nine build configurations.
- `port/tests/ui/options_video.py`: native picker, cancel/No, delayed confirmation,
  Yes, disabled rows/navigation and category Default preserving the pack state.
- Existing giant, SSJ2 moveset, Kaioken, difficulty, story and tournament checks
  exercise the individual features. In-game layout/animation review is still needed.
