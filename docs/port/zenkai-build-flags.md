# Zenkai gameplay build flags

The five independent switches below are defined in
[`include/sys/port_zenkai_features.h`](../../include/sys/port_zenkai_features.h).
They default to **1** (enabled). Set a default to **0**, or pass `-DNAME=0` to
the compiler, then rebuild the game. These are build choices, not menu options.
Original mode retains its original behavior regardless of these switches.

| Flag | Enabled (`1`) | Disabled (`0`) |
|---|---|---|
| `PORT_ZENKAI_GIANTS` | Enables 2.5× giant models, collision/effect/prop scaling, camera and aiming adjustments, and temporary original model/effect sizing during technique cinematics. | Uses original giant sizing and behavior, including original cinematic sizing, cameras, aiming, effects, and props. |
| `PORT_ZENKAI_STORY_UNLOCKS` | New Zenkai games start with ten entries and use additional Dragon History character rewards. | New games start with the original 132 entries; story rewards use the original character list, including Nail's original reward. |
| `PORT_ZENKAI_TOURNAMENT` | Enables the documented Dragon World Tour progression pools, tournament locks, boss eligibility, identity/incarnation/form priorities, individual participant slots, DP-weighted CPU results, and fixed Yamcha stages. | Restores legacy tournament behavior: full-roster Zenkai CPU generation, original boss selection and exact-ID duplicate rules, no new Cell-completion lock, 50/50 CPU results, and random Yamcha maps. |
| `PORT_ZENKAI_RAISED_DIFFICULTY` | Applies Zenkai's +10 CPU rebase (capped at 29) and its -5/0/+5 story difficulty offsets. | Uses original CPU levels and original 0/+6/+9 story offsets. |
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
  tiered Kaioken features.

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

## Existing independent combat flags

- `PORT_ZENKAI_SSJ2_MOVESET`: defaults to **0** in
  [`port_zenkai_moves.c`](../../src/battle/port_zenkai_moves.c).
- `PORT_ZENKAI_KAIOKEN`: defaults to **0** in
  [`port_hp_drain.h`](../../include/battle/port_hp_drain.h).

These keep their existing values and can still be overridden independently.

## Verification

`python3 port/tests/ui/zenkai_feature_flags.py` compiles and checks all 16
combinations against the production save defaults, character reward function,
story restriction queries, AI level conversion, tournament generation, and
Yamcha stage setup. Tests use synthetic save/resource fixtures, with address and
undefined-behavior sanitizers; no disc assets are required.
