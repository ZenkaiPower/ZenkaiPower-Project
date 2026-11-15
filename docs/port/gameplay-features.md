# Gameplay and progression features

## ZENKAI Power! mode

The optional [Zenkai gameplay build flags](zenkai-build-flags.md) independently
control character progression, tournament changes, raised difficulty, and story
move restrictions.

**ZENKAI Power!** is enabled by default for new configurations. Start a
**New Game** to use the features below. You can change the mode in the Escape
settings **Misc** tab; an explicitly saved choice is preserved.

- Enabled: new games use these rules and save to `savegame/zenkaipower.sav`.
- Disabled: new games use the original gameplay rules and `savegame/game.sav`.

The two progressions are independent. Loading (including startup loading) selects
the save matching the option. Changing it during a game does not change that
game's rules or save destination; start or load a game to apply it. Cancelling
a new-game or load operation keeps the current game. Existing saves are not
converted. Online sessions continue to use their isolated, original-rule saves.

ZENKAI saves and their Options companions use a **binary v3 envelope**:
an 88-byte header followed by the binary game payload. The header keeps a stable
save ID, a content hash, and a checksum covering the ID/header
and payload. Checksums detect accidental damage; they do not prove ownership or
prevent deliberate editing. Writes use atomic replacement. Existing v2 files
remain readable and migrate on the next successful write without
changing their ID or content hash. Read-only loads leave the file untouched.
Older game releases cannot load v3. The common save keeps its original format.

## Giant characters

Controlled by `PORT_ZENKAI_GIANTS` (default **1**). Compile with
`-DPORT_ZENKAI_GIANTS=0` to restore original giant behavior, including cinematics.

Offline ZENKAI battles enlarge giant characters to **2.5× their original size**.
Collision volumes, projectile hurt bounds, melee coverage, effects, and technique
hit widths follow the enlarged models. Held and thrown prop models, including
rocks, also inherit the giant multiplier. Pose updates preserve animation root motion.

Camera and targeting use an animated midpoint focus. Normal fighters' cameras
gain 3× distance against giants; giant-versus-giant fights do not stack that
compensation. Giant-versus-small fights also adjust approach height and blast
aiming from the actual launch position.

During super-attack cinematics, giants temporarily use their original **1? model
size** and their props/effects use original scaling. The 2.5? multiplier returns
when the cinematic ends. Camera offsets retain their original behavior. See the
[giant-compatible attack list](giant-compatible-attacks.md).

Restored from `archive/zenkai-giants-2026-11-02`, with regression expectations
updated for its 2.5× tuning. Giant-versus-small Ultimate Blasts remain experimental
and need in-game verification.

## Dragon History difficulty records

For episode prerequisites and Special Saga branches grouped by their originating
saga, see the [Dragon History progression map](dragon-history-progression.md).

Each completed story episode records the highest difficulty on which it has been
beaten: Easy, Normal or Hard. Replaying an episode on a lower difficulty does not
replace a higher record.

The completion badge shows that record using three star sizes: small for Easy,
medium for Normal, and the original large star for Hard. The record is stored
when the game is saved. Completed episodes from saves without difficulty records
are treated as Easy until a higher difficulty is completed.

## Dragon History victory endings

ZENKAI saves accumulate the distinct **victory endings** reached in each episode.
Replaying an episode and reaching another victory adds its exact script ending
action ID; it does not replace earlier endings. Ordinary and ring-out victories
are recorded. Defeat endings, aborted fights and restarts are ignored.

The records use the usual story save / Autosave flow. Existing saves remain
compatible, but old clear flags cannot identify previously reached endings, so
ending history starts empty. Each ending is collected once across all difficulty
levels; the highest cleared difficulty continues to be recorded separately.
This records exact endings without assigning a canon label.

Headless persistence and capture checks:
`python3 port/tests/battle/story_endings.py`.

## ZENKAI CPU difficulty

ZENKAI Duel keeps the original five CPU levels **0, 6, 13, 21, 29** in
single/team battles and the pause-menu CPU selection. Other modes add **10** to
active CPU base levels, capped at effective level **29**. Story, Ultimate Battle
and tournament opponents receive that offset after their own level selection.
Negative training-dummy settings remain unchanged.

Dragon History's Easy / Normal / Hard settings add **-5 / 0 / 5** to the
episode's scripted base before the +10 conversion, capped at effective level 29. Easy can reach level 5 for a scripted base of 0.
Original mode retains its 0 / 6 / 9 story offsets.

Levels are capped at 29 with no added 30..49 tuning bands. Original mode and
online play retain their existing difficulty. The full mapping and implementation
details are in [the AI documentation](../systems/ai.md).

## Saving Ultimate Battle rewards

When Z Points are gained in Ultimate Battle, exiting the mode offers the
**Save game data?** prompt used by Dragon History. With Autosave enabled, the
save happens automatically instead.

The save flow appears after Ultimate Battle has closed, over a black background,
before returning to the main menu. Saving preserves the earned points; declining
the prompt does not write them to disk.

Ultimate Training uses the same exit save flow when at least one lesson gains a
new completion mark during the visit. Completed lessons already marked before
entering Training do not trigger another save. The visit remains active across
tutorial and practice-battle hand-offs, so those completions are included when
the player eventually returns to the main menu.

## Starting character roster

A new ZENKAI Power! game starts with these nine base characters unlocked
(ten character entries including a transformation):

| Available at the start | Form |
| --- | --- |
| Goku (Early) | Base |
| Kid Gohan | Base |
| Piccolo (Early) | Base |
| Krillin | Base |
| Yamcha | Base |
| Tien | Base |
| Chiaotzu | Base |
| Master Roshi | Base and Full Power |
| Yajirobe | Base |

All other character entries start locked. The additional Dragon History
character rewards are listed below:

| Saga | Episode | Characters unlocked, in popup order |
| --- | --- | --- |
| Saiyan | First | Raditz → Great Ape Raditz |
| Saiyan | Second | Nappa → Great Ape Nappa → Saibamen |
| Saiyan | Third | Vegeta (Scouter) → Great Ape Vegeta |
| Frieza | Super Saiyan?! | Dodoria → Zarbon → Zarbon Transformed → Guldo → Jeice → Burter → Recoome → Captain Ginyu |
| Frieza | Frieza's Ultimate Transformation! | Frieza (1st Form) → Frieza (2nd Form) → Frieza (3rd Form) → Nail |
| Frieza | Defeat Frieza! | Goku (Mid), base form only → Frieza (Final Form) |
| Frieza | Legendary Super Saiyan | Goku (Mid) Super Saiyan → Frieza (100% Power) |
| Android | Enraged Android | Mecha Frieza → Trunks (Sword) → Trunks (Sword) Super Saiyan → Vegeta → Vegeta Super Saiyan → Android 19 → Android 20 |
| Android | Vegeta's Curiosity | Piccolo (End) → Android 17 → Android 18 → Cell (1st Form) → Cell (2nd Form) → Trunks → Trunks Super Saiyan |
| Android | Perfect Body | Super Vegeta → Super Trunks → Perfect Cell |
| Android | Cell Games Begin | Mr. Satan → Goku (End) → Goku (End) Super Saiyan → Teen Gohan → Teen Gohan Super Saiyan |
| Android | Gohan Explodes! | Android 16 → Cell Jr. |
| Android | Final Battle | Teen Gohan Super Saiyan 2 → Super Perfect Cell |
| Android | Future Peace | Adult Gohan → Adult Gohan Super Saiyan → Goten → Goten Super Saiyan → Kid Trunks → Kid Trunks Super Saiyan → Vegeta (End) → Vegeta (End) Super Saiyan → Videl → Great Saiyaman |
| Majin Buu | Destined Battle | Spopovich → Dabura → Adult Gohan Super Saiyan 2 → Supreme Kai (Kaioshin) → Goku (End) Super Saiyan 2 → Vegeta (End) Super Saiyan 2 |
| Majin Buu | Farewell to the Proud Warrior | Majin Vegeta → Majin Buu |
| Majin Buu | Final Fusion | Kibito Kai (Kibitoshin) → Gotenks → Gotenks Super Saiyan → Gotenks Super Saiyan 3 → Ultimate Gohan → Evil Buu → Super Buu → Super Buu (Gotenks Absorbed) → Super Buu (Gohan Absorbed) |
| Majin Buu | Battle for the Universe | Goku (End) Super Saiyan 3 |
| Majin Buu | Conclusion | Kid Buu → Vegito → Super Vegito |
| Special | Alone for the Final Battle | Frieza Soldier → Appule → Cui → Bardock → Great Ape Bardock |
| Special | Final Battle for Earth | Turles → Great Ape Turles |
| Special | Super Saiyan Goku | Slug → Giant Slug |
| Special | Destroy the Makyo Star | Garlic Jr. → Super Garlic Jr. |
| Special | Strongest VS Strongest | Salza → Cooler → Cooler (Final Form) |
| Special | Clash of the Ultimate Warriors | Metal Cooler |
| Special | Ultimate Battle! Three Super Saiyans | Android 13 → Fusion Android 13 |
| Special | Legendary Super Saiyan | Broly, base form only |
| Special | Blaze! Ultimate Burning Battle | Broly Super Saiyan |
| Special | Galactic Crisis | Zangya → Bojack |
| Special | I Will Protect the Earth! | Full Power Bojack → Pikkon |
| Special | Dangerous Duo! The Ultimate Warrior Doesn't Sleep | Broly Legendary Super Saiyan |
| Special | Innocent Monster | Janemba |
| Special | Fusion Reborn! Goku and Vegeta | Super Janemba → Super Gogeta |
| Special | Dragon Fist Explosion! If Goku Doesn't, Who Will? | Great Saiyaman 2 (Videl) → Tapion → Hirudegarn |
| Dragon Ball GT | Immortal Monster | Pan → Uub → Majuub → Baby Vegeta → Super Baby 1 → Super Baby 2 → Great Ape Baby |
| Dragon Ball GT | Ultimate Android | Super 17 |
| Dragon Ball GT | Ultimate Super Gogeta | Syn Shenron |
| Dragon Ball GT | A Miracle Saves the Day! | Omega Shenron |
| Dragon Ball | Ceiling Vs. Ground | Kid Goku → Great Ape → Grandpa Gohan |
| Dragon Ball | Battle in Korin Tower | Tao Pai Pai |

In ZENKAI Power! mode, **Super Saiyan?!** normally grants the Frieza force
instead of its original Nail reward, and **Frieza's Ultimate Transformation!**
grants Frieza's first three forms followed by Nail. If all eight Frieza-force
characters (Dodoria through Captain Ginyu) and Frieza's first three forms are
already unlocked when **Super Saiyan?!** is cleared, Nail is granted in that
original episode. **Frieza (Final Form)** unlocks alongside base Goku (Mid) in
**Defeat Frieza!**; **Frieza (100% Power)** unlocks later, in **Legendary Super
Saiyan**. Already unlocked characters are skipped.

In ZENKAI Power! mode, **all Dragon History character rewards**, including the
original game's rewards, can be earned on **Easy, Normal, or Hard**. Easy clears
also record episode completion. Previously unlocked characters remain unlocked.

Character unlocks are stored when saving. Further unlocks use the existing
progression system.
Tournament CPU entrants use actual unlocks plus the reward groups of available
main sagas and their assigned Special branches. Owning the original 132-entry
starting roster disables the preview additions. Bosses use the same pool;
selection prefers different identities, then incarnations, then forms, with
individual slots for Saibamen, Cell Juniors, Frieza Soldier, and Appule.
Player selection still requires actual unlocks. World Martial Arts Big Tournament,
Cell Games, and Otherworld Tournament require all seven Cell episodes cleared,
unless the original 132-entry roster is owned. See the
[Dragon World Tour rules](dragon-world-tour-changes.md) for details.

This starting roster applies to new games. Loading an existing save restores its
own unlocked characters. Changing the starting defaults does not modify existing
saves.

## Goku (End) Super Saiyan 2 blasts

An active offline **ZENKAI Power!** game applies these replacements
(`PORT_ZENKAI_SSJ2_MOVESET`, enabled by default). ORIGINAL mode and online
sessions retain the original moves, voice lines and View Skills entries.

| Slot | Original | Replacement | Donor |
| --- | --- | --- | --- |
| First Blast 2 | Kamehameha | Super Kamehameha | Goku (End) Super Saiyan 3 |
| Ultimate Blast | Super Kamehameha | Instant Kamehameha | Goku (End) Super Saiyan |

The replacement includes the donor's attack parameters (including cost and
damage), motion block, effect sequence, and complete View Skills entry in the
selected language. The regular Super Kamehameha uses SSJ2's original Ultimate
Blast charge/fire dialogue and lip tracks. Instant Kamehameha uses SSJ1's
charge/fire dialogue and matching lip tracks. Both follow the selected English
or Japanese voice language independently of the text language. Meteor Blast and both Blast 1 moves remain unchanged.
It applies when selecting SSJ2 directly and when transforming into it, for either
fighter. Other forms retain their original moves.

The rule follows the active save's mode, rather than the settings checkbox.
Ordinary games and online sessions retain the original moves. Disc resources and
saved customization are not edited; compact donor resources are loaded for the
battle and freed when its objects are released.

## Implementation references

- Goku SSJ2 blasts: `src/battle/port_zenkai_moves.c`, bound and released by
  `src/battle/btl_obj.c`. Resource regression check:
  `python3 port/tests/battle/zenkai_moves.py <gamedata-directory>`.
- Starting unlocks: `Save_SetDefaults()` in `src/sys/save.c` (PC build only).
- Story difficulty records: `include/menu/port_story_badge.h`.
- Story badge artwork: `port/src/story_badge.c`.
- Ultimate Battle exit save flow: `src/menu/menu_p_c.c`.

## First episode move restriction

In the first Saiyan Saga episode, Goku (Early) can use Kamehameha and Solar Flare
under their normal rules. Kaioken Attack, Spirit Bomb (Genki-Dama), and the
Kaioken support ability are disabled.
The restriction applies only to player-side Goku in this episode, on every
difficulty and on replays. Other characters, opponents and modes are unaffected.
View Skills omits the restricted moves and their descriptions for this episode. No hidden items are added, and saved customization is unchanged.

### Zenkai HP-drain abilities and Kaioken

`battle/port_hp_drain.h` reserves ability bits `0x7D`, `0x7E`, and `0x7F`
for 50, 100, and 150 HP per tick. The existing fighter tick runs every 30
simulation frames, respecting time-stop, hit-stop, and fight-state gates.
The strongest active tier wins. Damage is exact and nonlethal, without a
hit/combo reaction. These are internal ability effects, not new shop items.

Kaioken is restored to its original activation Ki refill, passive Ki drain,
charging restrictions, aura palette, and charge effects. Zenkai adds three tiers:

| Activation text | Four stat bonuses | HP drain/tick | Total stocks | With ability `0x15` |
| --- | --- | --- | --- | --- |
| Kaio-ken | +5 | 50 | 3 | 2 |
| Kaio-ken x3 | +10 | 100 | 6 | 4 |
| Kaio-ken x4 | +15 | 150 | 9 | 6 |

Activation buys the highest affordable tier. While active, another activation
buys as many remaining tiers as possible, paying only for the added tiers.
At tier three, activation is blocked until Kaioken ends. Tier state uses
reserved member ability bits `0x7A` and `0x7B`, so rollback retains it.
There is no
last-health-bar cutoff, low-health lockout, or extra empty-Ki activation rule.
Kaioken HP drain can reduce health to zero and KO its user. Standalone HP-drain
abilities still leave at least 1 HP. Native Ki exhaustion still ends Kaioken.
Original mode keeps native single-tier Kaioken and has no added HP drain.
The ZENKAI Kaioken changes can be disabled at build time with
`PORT_ZENKAI_KAIOKEN=0`; standalone HP-drain abilities remain enabled.

Checks: `python3 port/tests/battle/hp_drain.py` and
`python3 port/tests/battle/kaioken_aura.py`.

### Training regeneration

In ZENKAI mode, **Ultimate Training -> Training** uses team selection with up to
five fighters per side, without a DP limit. It keeps Training's dummy opponent,
unlimited time, and regeneration settings. Original mode retains one-on-one
Training. Tutorial lessons keep their existing battle setups.
The CPU difficulty selected in the pause menu is retained when a fighter
transforms or switches to a reserve teammate.

Zenkai's native Training pause menus (battle modes 5 and 6) include
**Training Settings** directly after COM Settings, with **Health Regeneration**,
**Ki Regeneration**, **Blast Stock Regeneration**, and **Switch Gauge Regeneration**.
Each opens native **On** / **Off** choices.
All start On for each new battle
and retain their selection across pause/resume. Off suppresses Training's
extra automatic refill (200 HP, 1,000 Ki, or 3,333 blast gauge units per frame)
for both fighters. Health Regeneration also restores 200 HP per frame to living
reserve teammates, up to their own maximum HP, without reviving defeated members.
Switch Gauge Regeneration Off disables Training's bonus switch-gauge refill;
the gauge continues to refill at its normal battle rate. On adds the Training
bonus refill. In ZENKAI mode, turning Health Regeneration off also removes
Training's 1-HP damage floor, allowing fighters to be KO'd; with it on, the floor
remains.
Healing skills, character regeneration, and equipped regeneration abilities
retain their regular rules. Original mode retains its original menus/refills.

Checks: `python3 port/tests/battle/training_regeneration.py` and
`python3 port/tests/battle/hp_drain.py`.
