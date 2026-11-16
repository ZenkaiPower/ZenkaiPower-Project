# Zenkai Captain Ginyu: Body Change

Feature ID: `ginyu_body_change`. Build flag: `PORT_ZENKAI_GINYU_BODY_CHANGE`
in `include/sys/port_zenkai_features.h` (enabled by default).
These changes require the Zenkai pack, with Classic Mode off. Classic Mode,
original mode, online sessions, or setting the flag to `0` retain original rules.

## Available bodies

| Character | Outfit shown in game | Internal costume index |
|---|---|---|
| Goku (Mid) | 1 | 0 |
| Kid Gohan | 1 | 0 |
| Krillin | 2 | 1 |
| Vegeta (Scouter) | 3 | 2 |

While the self-injury power-up is active, use this alternate list instead:

| Character | Outfit shown in game | Internal costume index | Model |
|---|---|---|---|
| Goku (Mid) Super Saiyan | 3 | 2 | Normal |
| Piccolo (Early) | 1 | 0 | Damaged |
| Frieza Final Form | 1 | 0 | Damaged |

Goku uses model variant `0`; Piccolo and Frieza use damaged-model variant `1`. Nail and Jeice are excluded.
The current character is always excluded, regardless of outfit. Selection uses
the frame counter modulo the number of remaining candidates. Neither list
exchanges bodies with the opponent; the opponent's team remains unchanged.

## Moves, presentation and stats

The changed body retains Ginyu's blasts, support skills, camera tables, move names
and the pause menu's **View Skill** list. The original four changed-body stat
penalties are removed. Transformations remain blocked by the changed-body state.
After Body Change, both the Ultimate (Body Change) and the second support skill
(self-injury, ID `0x17`) are blocked and dimmed in **View Skill**. Other retained
blasts and the first support skill remain usable. Before changing bodies,
self-injury deals **10,000 HP** of damage instead of 3,000 HP. Its power-up neither
selects an opponent's body nor re-enables either blocked move.

## Opponent exchanges: temporarily disabled

The opponent-selection hook and self-injury cinematic shortcut are disabled.
All Body Changes use the normal or self-injury preset roster and normal animation sequence. No active
or reserve opponent is replaced, and no HP or stat bonuses transfer between sides.
The exchange implementation remains in the source for possible future use.

The following describes that dormant implementation, not current gameplay:

### Retained exchange implementation

- **Self-injury power-up active:** a successful Body Change takes the active
  opponent's current body. That opponent receives Ginyu's model only, keeping
  its own fighting style, animations, blasts, abilities, cameras, skill names,
  View Skill list and original voice, including idle taunts. Its combat stats instead come from Ginyu. This persists after tagging out
  and back in. With the self-injury boost active, play the opening
  kick and matching opponent reaction, then jump straight to the final firing
  clip. Skip the middle preparation shot, posing and dialogue. The final clip
  plays fully, including beam firing, impact and aftermath, before the normal
  model-load handshake and return to combat. The boost is recorded at sequence
  entry, and both fighters use the same opening-to-firing transition.

- **No power-up:** Body Change uses the preset roster, including in team battles.
  Opponent reserve characters are never selected or changed.
- **No eligible active target:** the existing preset-body fallback remains
  available. Absent or defeated opponents, Ginyu, and the same character as the
  body being left are excluded from actual exchanges.

HP follows the bodies: the caster receives the chosen target's remaining HP;
the target receives the caster's remaining HP, including self-injury damage.
Health is capped to the receiving body's maximum, with at least 1 HP. Both
changed slots have zero Ki. Reserve characters remain unchanged. Team control
stays with each side; the opponent keeps its original moves with Ginyu's stats.

Selection is planned during the successful technique's model-change step and
committed when the caster's replacement model loads. The active opponent's
replacement is then queued and completed separately. The opponent stays frozen
while its animation files reload, and starts in a neutral animation rather than
replaying a request from its previous body. Borrowed Ginyu bodies retain
his copied moves, animation and voice resources after tagging out and back in. These runtime roster
changes are reset with the fighters for a fresh battle/rematch and do not
rewrite the save or the original battle setup roster.

## Body strength

Ginyu uses the preset body's combat stats while retaining his own moves
and support abilities (with Body Change and self-injury disabled afterward). Normal and guarded melee/ki-blast damage come from the
body's corresponding attack records; Blast 2 and Ultimate damage use the body's
corresponding slots. Move identities, hit counts, reactions and animations stay
with the original move set. Movement speeds, ki/Blast Stock recovery, max-power
duration, charge rates and damage resistance follow the new body.

The dormant opponent-exchange implementation also swaps seven member stat bonuses. The opponent's
maximum HP and Blast Stock capacity are recalculated for Ginyu; Ginyu's are
recalculated for the acquired body. Equipped abilities remain with their original
fighter and still apply their modifiers. HP transfer and zero Ki remain unchanged.
These rules also survive team switches; returning to a model does not compound
its damage values. Body height and radius also follow the rendered model.
Both fighters' animations are retargeted from a saved copy of their original
skeleton's rest translations and height to the received body's skeleton. A
small character in Ginyu's body must not sink, and Ginyu in a small body must
not float. Vertical animation movement scales by the target/source height ratio;
the source rest position is subtracted before adding the target rest position.
These copies survive model reloads and are cleared on fighter reset.

## Team switch portrait

Body Change reloads the member's portrait resource for the borrowed character.
The reserve/switch gauge shows that body after tagging out, and full team reloads
keep the body's portrait while retaining Ginyu's animation and voice resources.
The original battle setup roster and save data remain unchanged.

## Frieza tail idle

When Ginyu borrows Frieza Final Form, his body retains Ginyu's animation
while tail nodes 4 through 7 use Frieza's matching rotation tracks for ground idle,
airborne idle, and Ki-charge start, loop and finish. Ground idle uses motions
`0` and `1`; airborne idle uses `0x26` and `0x27`; charging uses `0x34`,
`0x35` and `0x36`.
The tail cycle is synchronized to the retained idle loop and still uses normal
pose blending. Other body parts, attacks and other characters are unaffected.
The original Frieza animations are loaded once from the player's game data during
model binding; no extracted animation asset is embedded in the source.

## Ki and health

Every successful Body Change empties the Ki gauge, including repeat changes.
When leaving a body, its remaining HP is remembered. The first visit to another
body starts at that body's maximum HP, as in the original game. Returning to a
previously used body restores the HP it had when left, capped at its current
maximum; returning does not refill health.

The HP history remains implemented for repeat changes triggered by other game
flows, but normal player input cannot use Body Change again after changing bodies.
Self-injury is also disabled after the change.

HP history is kept separately for each side and team member. It lasts for the
current battle and resets when the fighter is reset for a new battle/rematch;
it is not written to the save file. It does not grant money, unlock characters,
or change the opponent's HP during preset-body changes.

## Implementation and validation

- `src/battle/btl_char_action.c`: filtered body/outfit selection.
- `src/battle/bobj_a.c` and `battle_load.c`: retain move and presentation data across model reloads.
- `src/battle/btl_char_mgr.c`: health history, zero Ki, and removal of stat penalties.
- `include/battle/port_ginyu_health.h`: per-side/per-member HP history.
- `src/battle/btl_act_j.c`: allow Ginyu's blasts and support skills after changing.
- `src/battle/btl_seq.c` and `btl_demo_cam.c`: View Skill eligibility and camera behavior.

Run `python port/tests/battle/ginyu_body_change.py` in the compiler environment.
Automated tests cover selection, compile/Classic gates, reload retention,
View Skill eligibility and health history. Attack/camera presentation still
requires in-game verification.

Exchange regression checks: `python port/tests/battle/ginyu_exchange.py`.
