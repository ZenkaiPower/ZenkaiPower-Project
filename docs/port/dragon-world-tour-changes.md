# Dragon World Tour: Zenkai changes

Status: implemented in the tournament menu, player selection, and bracket generator. Scope: Zenkai mode, including World Tour and Free. Original mode retains its existing rules.

## 1. Bosses use the eligible CPU roster

A tournament boss must belong to the eligible CPU roster defined in rule 3. A candidate qualifies through either an actual save unlock or an available saga's preview roster, including its assigned Special branches.

The existing boss candidates are (like in original):

| Tournament | Existing candidates |
|---|---|
| World Tournament | Master Roshi, Videl |
| World Martial Arts Big Tournament | Mr. Satan / Hercule |
| Cell Games | Perfect Cell, Super Perfect Cell |
| Otherworld Tournament | Pikkon, Goku (End) Super Saiyan 3 |
| Yamcha Game | Yamcha |

Filter candidates by the eligible CPU roster before selecting. If one candidate remains, use it; if both remain, retain the original equal-probability choice. When the original 132-entry exception disables preview additions, bosses likewise qualify only through actual unlocks.

The rules guarantee an eligible boss for every available tournament during normal progression:

- World Tournament and Yamcha Game have starting-roster candidates: Master Roshi and Yamcha.
- The other three tournaments open after Cell completion. Their candidates are already in the eligible Android / Cell group: Hercule, both Cell forms, and Pikkon from its assigned Special branch. Clearing Pikkon's individual episode is unnecessary for preview eligibility.
- When the original 132-entry exception disables preview additions, all existing boss candidates are already actually unlocked: every candidate belongs to that original roster.

No replacement-boss fallback or additional tournament lock is needed under these rules.

## 2. Prefer different characters over incarnations and forms

CPU entrant selection follows this priority:

1. A character identity not yet represented, including any incarnation or form.
2. An unused incarnation/family of an identity already represented.
3. An unused transformation of an incarnation/family already represented.

Consider player-selected entrants and the reserved boss when tracking represented identities and families. The [complete character-family mapping](tournament-character-families.md) assigns all 161 roster IDs to 83 identities and 104 families. Gohan/Future Gohan, adult Trunks/Kid Trunks, and Fat Buu/Buu (Evil/Super/Kid) are independent identities for this preference.

These are preferences, not bans: when too few different identities are eligible, allow another incarnation; when too few families remain, allow another eligible form to fill the bracket. Do not expand the eligible pool to locked characters outside the saga pools just to satisfy these preferences. Exact form IDs retain the no-duplicate rule except for these individual-participant exceptions:

- **Saibamen:** up to five participants.
- **Cell Juniors:** up to five participants.
- **Frieza Soldier:** one participant per valid outfit/color.
- **Appule:** one participant per valid outfit/color.

Saibamen, Cell Juniors, and Frieza Soldiers each have one chance at new-identity priority. Extra entrants of these types fill gaps only after the other eligible choices run out. Each eligible Appule outfit retains a separate chance at new-identity priority. Player-selected entrants consume slots too. Outfit-specific slots must keep their selected outfit in battle. See the [slot rules](tournament-character-families.md#individual-participant-exceptions) for identity keys, limits, and costume counts. Eligibility still follows rule 3.

Example: with Goku (Mid) selected, prefer Krillin over Goku (End), and prefer Goku (End) over Goku (Mid) Super Saiyan when no unused identity remains. The mapping is selection policy for Zenkai; the original duplicate check only compares exact IDs.

## 3. Cumulative saga-based CPU roster

The eligible CPU entrant pool is the union of:

1. All characters actually unlocked in the current save, including unlocks earned outside Dragon History.
2. All character rewards in each available main saga, plus that saga's assigned Special Saga branches.

Use saga **availability**, not completion, to add its reward group. When a main saga becomes available, its entire group becomes eligible immediately, including Special follow-ups the player has not individually unlocked or cleared yet. Earlier available saga groups remain included.

Exception: once all original 132 roster entries are actually unlocked, disable
the saga-preview additions entirely. The CPU pool then contains **only actually
unlocked characters**, including any additional entries earned beyond those 132.

Special Saga is not an independent progression tier. Assign its episodes to the main saga that ultimately unlocks their branch, as mapped in [Dragon History progression](dragon-history-progression.md#character-unlock-groups-main-saga-plus-its-special-branches).

```text
eligible CPU entries = actual unlocked entries
                     UNION rewards of all available main sagas
                     UNION rewards of their assigned Special branches

if all original 132 roster entries are owned:
    eligible CPU entries = actual unlocked entries only
```

Eligibility does not grant permanent character unlocks or change the save's character bits. Player character selection continues to use actual unlocks; this pool replaces the current unrestricted CPU roster. Boss candidates use this same pool under rule 1.

| Latest available main saga in normal progression | Reward groups included | Preview entries including the starting roster* |
|---|---|---:|
| Saiyan | Saiyan + assigned Special | 20 |
| Frieza | Saiyan + Frieza + their Special branches | 48 |
| Android / Cell | All preceding groups + Android / Cell and assigned Special | 93 |
| Majin Buu | All preceding groups + Majin Buu and assigned Special | 122 |
| Dragon Ball GT | All preceding groups + GT | 139 |
| Dragon Ball | All preceding groups + Dragon Ball | 147 |
| What If | All preceding groups + What If | 152 |

\* Counts are unique roster entries, including transformations. Actual unlocks outside these groups can increase the pool. These are available CPU entries, not permanently unlocked player entries.

Examples:

- **New game:** ten starting entries are actually unlocked and Saiyan is available. Add the ten-entry Saiyan + Special reward group: 20 eligible CPU entries.
- **Saiyan finished, Frieza newly available:** include actual unlocks, the ten-entry Saiyan group, and the 28-entry Frieza group. With the normal starting roster, the union has 48 entries. Do not wait for Frieza episodes to be cleared before including that group.
- **An extra character is unlocked elsewhere:** retain that character in the CPU pool regardless of the current story saga.

Existing tournament difficulty/cost preferences apply within this pool and the diversity tiers in rule 2; they must not pull in otherwise ineligible characters or override the preference for unused identities/incarnations.

## 4. Cell completion gates three tournaments

The following tournaments must not appear as available choices in either World Tour or Free until **all seven Android / Cell Saga episodes are cleared**:

- World Martial Arts Big Tournament
- Cell Games
- Otherworld Tournament

This includes **Future Peace**, the seventh episode. Clearing Final Battle alone is insufficient. Merely opening Android / Cell Saga, unlocking its episodes, or making its character group eligible is insufficient. Its assigned Special episodes are not required for this gate.

World Tournament and Yamcha Game receive no additional Cell-completion restriction from this specification. Apply the restriction consistently to World Tour scheduling/selection and Free selection, rather than only hiding a menu label.

## 5. Original 132-entry roster exception

If the save has **every entry in the original game's 132-entry starting roster actually unlocked**:

- Bypass rule 4: all tournament types are available regardless of Dragon History completion.
- Disable rule 3's saga-preview additions: select CPU entrants only from actually unlocked characters.

This means ownership of a specific set, not merely any 132 character bits. Saga-preview eligibility does not count as ownership. Boss candidates use the resulting actual-unlocks-only pool, and transformation-family preferences remain in effect.

The original roster is the set granted by the non-Zenkai branch of `Save_SetDefaults`: character IDs 0-160, excluding these 29 IDs:

```text
7, 8, 9, 10, 24, 38, 54, 69, 70, 71, 72, 75, 76, 97, 110,
120, 145, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160
```

```text
all tournament types available = all seven Cell episodes cleared
                             OR all original 132 roster entries owned
```

## 6. DP-weighted CPU-only results

Zenkai resolves CPU-versus-CPU matches using the entrants' current roster-form
DP costs as random weights:

```text
P(left wins) = left DP / (left DP + right DP)
```

For example, 8 DP versus 2 DP gives 80% versus 20%; equal DP gives 50/50.
Outfit-specific individuals use their character's DP. Items and difficulty do
not add extra weights. A zero-DP entrant has zero weight against a positive-DP
entrant; if both weights are zero, use 50/50. Negative costs are treated as zero.

This computes the result immediately, without running an AI battle or loading
battle assets. Matches involving a player still use the actual battle result.
Original mode keeps its existing 50/50 CPU-only draw.

## 7. Yamcha Game stages

Zenkai Yamcha Game uses fixed Desert stages:

| Round | Stage |
|---|---|
| Opening round, quarterfinals, semifinals | Desert - Noon (ID 19) |
| Bracket final | Desert - Evening (ID 31) |
| Yamcha boss fight | Desert - Night (ID 32) |

Original mode retains its random selection from unlocked Yamcha Game stages.

## Acceptance checks for implementation

- A new save can fill a tournament from the 20-entry Saiyan pool without using characters or bosses outside that pool.
- Frieza availability expands the pool immediately to include its 28-entry group; no individual Frieza or Special clears are required for this expansion.
- Actual unlocks from other modes remain eligible at every story stage.
- Generation prefers unused identities, then unused incarnations/families, then unused forms; it terminates without exceeding the allowed pool.
- The individual-participant exceptions allow five Saibamen, five Cell Juniors, one Frieza Soldier per valid outfit/color, and one Appule per valid outfit/color. Extra Saibamen, Cell Juniors, and Frieza Soldiers fill gaps in the bracket. Total limits include player-selected entrants.
- An original boss candidate qualifies when actually unlocked or included in an available saga's preview group; candidates outside the eligible CPU pool are never selected.
- Clearing six Cell episodes leaves the three gated tournaments unavailable; clearing all seven permits them.
- Owning all original 132 entries permits all tournament types even with no Cell clears. Owning 132 entries with an original-roster entry missing does not activate the exception.
- With all original 132 entries owned, an additional locked character remains ineligible even if its saga-preview group would otherwise include it. An additional actually unlocked character remains eligible.
- World Tour and Free apply the same tournament gate. Original mode remains unaffected.
- CPU-only matches use DP-proportional odds, advance the winner once, and never resolve player matches automatically. Equal-DP and Original-mode matches retain 50/50 odds.

## Implementation references

- [Tournament rules and roster data](../../include/menu/port_tour_rules.h): saga pools, identity/family IDs, Cell gate, and original-roster exception.
- [Zenkai bracket generation](../../include/menu/port_tour_bracket.h): bounded candidate selection, boss reservation, diversity tiers, and individual participant slots.
- Regression checks: `python3 port/tests/ui/tour_rules.py`. Checks run without disc assets under AddressSanitizer and UndefinedBehaviorSanitizer; full in-game verification remains separate.
- [Bracket_FillEntrants](../../src/menu/menu_k_f.c): routes Zenkai through its progression-based selector and preserves Original generation.
- [TourMenu](../../src/menu/menu_j_b.c): Dragon World Tour menu.
- [Save_SetDefaults](../../src/sys/save.c): original 132-entry starting roster and Zenkai starting roster.
- [Dragon History progression map](dragon-history-progression.md): saga availability graph and combined main/Special character groups.
- [HistResult_BuildRewards](../../src/menu/menu_c_c.c): actual character rewards and story clear/unlock records.
