# Zenkai gameplay build flags

The independent switches below are defined in
[`include/sys/port_zenkai_features.h`](../../include/sys/port_zenkai_features.h).
The Ginyu Body Change flag defaults to **1** (enabled). Set a default to **0**, or pass `-DNAME=0` to
the compiler, then rebuild the game. These are build choices, not menu options.
Original mode retains its original behavior regardless of these switches.

| Flag | Enabled (`1`) | Disabled (`0`) |
|---|---|---|
| `PORT_ZENKAI_ORIGINAL_FIXES` | Enables the documented original-game corrections in Zenkai mode, including Classic Mode. Defaults to `1`; independent of the optional pack. See [Original-game fixes](original-game-fixes.md). | Retains the original behavior for these defects and the original English text for correction keys. |
| `PORT_ZENKAI_GREAT_APE_MOON` | On full-moon stages, Vegeta (Scouter), King Vegeta and Turles use Nappa?s complete Great Ape transformation motions and matching moon-framing camera tracks instead of creating a Power Ball. Limited to the offline Zenkai pack, outside Classic Mode. | Keeps the original Power Ball presentation on all stages. |
| `PORT_ZENKAI_GIANTS` | Enables 2.5× giant models, collision/effect/prop scaling, camera and aiming adjustments, and temporary original model/effect sizing during technique cinematics. | Uses original giant sizing and behavior, including original cinematic sizing, cameras, aiming, effects, and props. |
| `PORT_ZENKAI_STORY_UNLOCKS` | New Zenkai games start with ten characters, lock the twelve story-reward stage entries, and use additional Dragon History character and stage rewards (including variations). | New games use the original 132 characters and stage availability; story rewards use original character and stage lists, including Nail's original reward. |
| `PORT_ZENKAI_TOURNAMENT` | Enables the documented Dragon World Tour progression pools, tournament locks, boss eligibility, identity/incarnation/form priorities, individual participant slots, DP-weighted CPU results, and fixed Yamcha stages. | Restores legacy tournament behavior: full-roster Zenkai CPU generation, original boss selection and exact-ID duplicate rules, no new Cell-completion lock, 50/50 CPU results, and random Yamcha maps. |
| `PORT_ZENKAI_RAISED_DIFFICULTY` | Applies Zenkai's +10 CPU rebase (capped at 29) and its -5/0/+5 story difficulty offsets. | Uses original CPU levels and original 0/+6/+9 story offsets. |
| `PORT_ZENKAI_SSJ2_MOVESET` | Enables Goku (End) SSJ2 replacement blasts. | Uses his original moveset. |
| `PORT_ZENKAI_GINYU_BODY_CHANGE` | Body Change selects Goku (Mid) outfit 1, Kid Gohan outfit 1, Krillin outfit 2, or Vegeta (Scouter) outfit 3. Nail, Jeice and the current body are excluded. Keeps Ginyu?s blasts, support skills, camera tables, move names and pause-menu View Skill list, and removes all four changed-body stat penalties. Requires the Zenkai pack (Classic Mode off). | Restores the original six-body selection and outfits. |
| `PORT_ZENKAI_FUTURE_GOHAN_GRAB` | Base Future Gohan uses Super Saiyan Future Gohan's normal grab attacker, victim and recovery animations in the offline Zenkai pack, with Classic Mode off. Applies to every outfit. | Retains base Future Gohan's original grab. |
| `PORT_ZENKAI_EXTRA_OUTFITS` | Adds Future Gohan outfits 3 and 4 for both forms, including clean and damaged states, only with the Zenkai pack active and Classic Mode off. | Retains only the original outfits; built-in outfit payloads are excluded from compilation. |
| `PORT_ZENKAI_KAIOKEN` | Enables tiered Kaioken bonuses, upgrade costs, and HP drain. | Uses original Kaioken behavior. |
| `PORT_ZENKAI_STORY_RESTRICTIONS` | Restricts Goku (Early)'s Kaioken Attack, Spirit Bomb/Genki Dama, and Kaioken support ability in the first story episode, and his special Kienzan blast in Saiyan Saga episodes. | Removes these story-specific restrictions; moves use their normal eligibility rules. |

Changing the starting-roster flag does not rewrite existing saves. Start a new
game to obtain the selected starting roster. Saved unlocks are retained when an
existing game is loaded.

The flags can be combined freely. In particular:

- Story progression can remain enabled while the tournament changes are disabled:
  the legacy full CPU roster still fills the bracket despite the smaller starting roster.
- If story progression is disabled and tournament changes remain enabled, the
  original 132-entry starting roster activates the documented tournament exception:
  all tournaments are available and CPU eligibility uses actual unlocks only.
- Disabling raised difficulty does not disable Training's retention of its selected
  CPU behavior across transformations and teammate switches.
- Disabling story move restrictions does not enable the separate SSJ2 moveset or
  tiered Kaioken features when their build flags are disabled.

## Compiler overrides

For the desktop port build driven by `port/tools/undefined.py`, extra compiler
definitions can be supplied through `ZENKAIPOWER_PORT_CFLAGS`. For example:

```powershell
$env:ZENKAIPOWER_PORT_CFLAGS = '-DPORT_ZENKAI_GIANTS=0 -DPORT_ZENKAI_STORY_UNLOCKS=0 -DPORT_ZENKAI_TOURNAMENT=0 -DPORT_ZENKAI_RAISED_DIFFICULTY=0 -DPORT_ZENKAI_STORY_RESTRICTIONS=0'
```

Run the normal build in that environment. Other build pipelines must likewise
pass the definitions to all game-source compilation steps; editing the shared
header defaults also applies across platforms. A rebuild is required after either
kind of change.

## Runtime pack

Extra outfits default to **0** in the shared header; giant scaling defaults to **0**. They set which features
are compiled in; they do not override a saved decision to disable the pack.
[Classic Mode](zenkai-pack.md) controls the complete `zenkai_pack` at runtime.
`PORT_ZENKAI_ORIGINAL_FIXES` is separate from that pack and remains active in
Classic Mode. Its corrections still require Zenkai mode.
The two combat headers retain guarded fallbacks with the same enabled defaults.

## Verification

`python3 port/tests/ui/zenkai_feature_flags.py` compiles and checks all 16
combinations against the production save defaults, character reward function,
story restriction queries, AI level conversion, tournament generation, and
Yamcha stage setup. Tests use synthetic save/resource fixtures, with address and
undefined-behavior sanitizers; no disc assets are required.

Captain Ginyu?s complete rules, including zero Ki and remembered body HP, are
documented in [Ginyu Body Change](ginyu-body-change.md).
