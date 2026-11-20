# CPU opponent (AI)

## ZENKAI CPU levels (PC port)

With Sparking! Changes active, ordinary CPU bases use a tapered increase:
**+7** for 0–9, **+6** for 10–14, **+5** for 15–19, **+4** for 20–24,
and **+3** for 25–28. Effective levels are capped at **29**; level 29 stays 29.
Classic Mode, Original mode, disabled raised-difficulty builds, and network
sessions retain original levels. Negative dummy settings remain unchanged.

Duel, Training, and Ultimate Training retain the original five effective levels
**0, 6, 13, 21, 29**. Training retains its selected level across controller
reloads without rebasing it. Duel uses tagged exact levels to bypass tapering.

Dragon History applies `clamp(taper(scripted base) + difficulty offset, 0, 29)`,
with offsets **-5/0/+5** for Easy/Normal/Hard. Applying difficulty after the taper
preserves five-level spacing before clamping. Original offsets remain
**0/+6/+9**. Setup and mid-battle commands tag the completed result to prevent
applying tapering twice; negative dummy levels are preserved.

Mode tables and scripts retain base levels. Only live controller initialization
converts ordinary bases. The pause menu uses a canonical inverse for labels;
tapering can give neighboring bases the same effective level. Existing AI
rates, reaction checks, charge choices, delays, and five tuning bands remain
unchanged. No levels above 29 or new tactical planners are introduced.

See [the mode report](../port/difficulty-base-changes.md) and
[distribution audit](../port/cpu-level-distribution.md) for the effects by mode.
Run `python3 port/tests/battle/zenkai_ai.py` for controller, story, Duel,
Training, isolation, boundary, and existing behavior checks under ASan/UBSan.

### Measuring combat strength without the UI

The correctness checks above do not establish stronger play. Use the actual
headless battle benchmark for that:

```bash
python3 port/tests/battle/ai_strength.py \
  --binary port/build_release/zenkaipower_64 \
  --data release/linux-x64/gamedata
```

The binary must be rebuilt with the benchmark support in `port/src/headless.c`.
The driver launches actual CPU-versus-CPU battles with no window, audio device,
menu navigation or replay input. It requires the user's extracted game data.
Temporary save directories isolate the runs from personal saves. Effective CPU
levels are assigned directly with ZENKAI save features inactive: this compares
the AI tuning alone, with rebasing separately covered by `zenkai_ai.py`.

By default it compares **20 vs 0, 26 vs 6, 33 vs 13, 41 vs 21, and 49 vs 29**.
Each comparison has 36 trials (three mirror-match characters, 12 distinct seeds
per character, stage 0), and each trial runs twice with the levels exchanged
between sides: **360 scored fights**, at up to 90 seconds of battle time each.
The native winner must agree with normalized live HP captured as combat ends.
Health, maximum HP, battle ticks, KO/time-up reason and the native winner are
recorded. Integer HP crosses the simulation/host boundary; Python computes its
percentage. The result block's `frames` field counts a combat event and is not
used as elapsed time.

The driver first repeats an identical seeded fixture and rejects non-repeatable
results. Equal-level 29-vs-29 controls expose positional bias; swapping sides
balances that bias in each scored trial. A side-swapped pair is one statistical
unit. An exact sign test compares favorable and unfavorable pairs, excluding
tied pairs. Bonferroni correction covers all comparisons and both directions
at a family error threshold of 5%. The verdict is **improved**, **regressed**,
or **inconclusive**, with the score also required to favor that direction.
An inconclusive result does not establish equality or improvement.

The JSON report defaults to `port/build/ai_strength.json` and includes every
fight, the binary SHA-256, controls, repeatability check and per-level verdicts.
Exit codes: 0 = all comparisons improved, 1 = at least one inconclusive,
2 = at least one regression, 3 = invalid/incomplete benchmark. Smaller smoke
runs can use `--comparisons 49:29 --characters 0 --seeds 2`; these have little
statistical power. `--stages`, `--characters`, `--seeds` and `--seconds` broaden
the scenarios. `--self-test` validates verdict handling without game data.

Results apply to the tested matchups and stages. They do not demonstrate stronger
play against humans, team strategy, all characters, or every intermediate level.

Reference run (360 scored fights, default scenarios, Linux x64):

| Rebased vs original level | Wins / losses | Win rate | Verdict |
|---|---:|---:|---|
| 20 vs 0 | 70 / 2 | 97.2% | Improved |
| 26 vs 6 | 64 / 8 | 88.9% | Improved |
| 33 vs 13 | 57 / 15 | 79.2% | Improved |
| 41 vs 21 | 63 / 9 | 87.5% | Improved |
| 49 vs 29 | 58 / 14 | 80.6% | Improved |

All comparisons passed the corrected per-direction threshold of 0.005. There
were no drawn fights. The repeatability check and all fixture/winner validations
passed. The local `port/build/ai_strength.json` retains the complete run and
binary identity; these figures are a measured snapshot, not expected assertions
for future AI revisions.

### Plots and summary tables

The benchmark prints a summary table with wins, losses, draws, score, mean HP
lead, improvement p-value and verdict. Add `--plot` to a new benchmark run to
also export PNG and SVG charts with that table embedded, plus a summary CSV.
Plotting requires the optional `matplotlib` package; combat tests and terminal
tables use only the Python standard library.

To plot the saved results without running fights, using an isolated dependency
environment managed by `uv`:

```bash
uv run --no-project --with matplotlib python port/tests/battle/ai_strength.py \
  --plot-from port/build/ai_strength.json
```

With matplotlib already installed in the selected Python environment, invoke
the script directly. The default outputs are `port/build/ai_strength.png`,
`port/build/ai_strength.svg` and `port/build/ai_strength_summary.csv`.
Use `--plot-output path/prefix` to change their directory and filename prefix.
The plots show fight outcomes and mean remaining-health advantage. The table
includes the paired statistical verdict; no error bars treating the two sides
of a pair as independent are added. Incomplete or invalid reports are rejected,
and summaries are recalculated from the completed match records for export.

## Original implementation notes

Sources: `src/battle/btl_ai_mgr.c` (manager and the move action, linked),
`btl_ai_seq.c` (first AI object from 0x1B4140: step handlers 14..23, sequence runner; the
functions before 0x1B6008 are still assembly) and `btl_ai_cond.c` (start of the second object,
0x1B6D00: rule conditions and rate getters), both linked. Layout:
`include/battle/btl_ai.h` is the single definition of the AI block (`BtlAi`, per side `BtlAiWork`
with `seq`, `move`, `out`, `status`, `plan`); `btl_ai_mgr.h` adds the manager file's helper types.

Not decompiled: the rule evaluator (0x1BA760), sense and think (0x1BFF70, 0x1BAC30), most step
handlers (0x1B4C00..0x1B6008) and the virtual pad (0x1BC8A8..). Statements about those are read
from disassembly.

## Where it runs (verified)

`BtlAiMgr_Update` is called once per battle frame, after `Battle_UpdateWork` and before fighter
input is sampled. It returns at once when: paused (flag 0x100), an object load job is running
(flag 0x1000), the stage is not ready, or the sequence state is not Ready or Fight.

Per frame: measure the distance between the fighters and run a stage line-of-sight test; then
for each CPU-controlled fighter: sense, think, `BtlAi_RunSeq`, `BtlAi_SendInput`.

The AI never writes to a fighter. Its only output is `BtlCharApi_SetInjectedInput(objId,
buttons, stickX, stickY)`: the CPU plays through the same input path as a human.

## State (verified layout)

`gBtlAi`: one 0xA60 heap block. +0 AI data pointer, +4 fighter distance, +0xC sight flags,
+0x10 two per-side blocks of 0x520, +0xA50 frame counter.

Per side (`BtlAiWork`): `objId`, `type` (+4, the member's aiType), `level` (+8, its cpuLevel), the character's AI parameter pointer
(+0x18, from fighter object +0x934), then:
- `seq` (+0x28): flags, an 8-entry stack of `{id, arg}` actions, phase 0..3, step, timers;
- move work (+0xC0): keep-distance per move type, a 16-point path, rebuild timer;
- output (+0x268): buttons, stick X, stick Y, per-button toggles;
- status (+0x2C0): situation flags, opponent class and action, three 90-frame timers;
- plan (+0x2E8): rule evaluation state, eight pre-drawn rolls, a 300-frame cooldown.

## How it decides

- (read from disassembly) Decisions are data: the common AI data (in common file 2) holds an
  action table, eight rule lists and 32 per-`aiType` profiles. A rule is a 0x1C-byte record:
  a situation group, up to eight condition ids with one argument each, and an action.
- (verified) Conditions 0..34 are the `BtlAiCond_*` functions.
- (read from disassembly) Rules are re-evaluated every frame; there is no think interval.
  Slowness comes from level-scaled percentage rolls and timers.
- (verified) An action is four phase functions (init, start, run, end). The move action picks
  a target at a keep-distance from the opponent, follows a path rebuilt at most every 60
  frames, and gives up after 61 frames without progress.

## Difficulty (verified)

`BtlAi_ScaleByLevel(level, lo, hi) = lo + (int)((hi - lo) / 29.0f * max(level, 0) + 0.01f)`:
every tuning value is a pair of bytes for level 0 and level 29, linearly interpolated. Five-step
tables use `level / 6`. `aiType` selects a profile of rates, not different code. A negative
level (the training dummy) evaluates only one rule list and never reacts.

The menu's five difficulty levels map to 0, 6, 13, 21, 29 (see battle.md).

## AI button bits (read from disassembly)

1 GUARD, 2 DASH, 4 BLAST, 8 RUSH, 0x10..0x80 UP/DOWN/LEFT/RIGHT, 0x100 LOCKON, 0x200 CHARGE,
0x800 ASCEND, 0x1000 DESCEND, 0x2000 R3. The gameplay names are guesses from the default
button layout.

## Randomness and determinism

- (verified by a call scan of 0x1B4140..0x1C0400) The AI's only generator is `Rand_Range`, the
  broken-refill Mersenne Twister: 49 call sites in 40 functions.
- (verified) Conditions draw even when the result is then discarded.
- (read from disassembly) The rule evaluator draws 8 values each time it enters a new rule
  group, so the number of draws per frame depends on the situation.
- (verified by call scan) The stream is shared with about 198 call sites in the menu overlay
  and a few in the main executable.
- (verified) The AI skips any frame on which an object load job is running, so its timing
  depends on how long a load takes.
- (verified) No pad, clock or camera reads in the decompiled AI functions.
- (verified, `btl_input.c`) Replays record CPU fighters as inputs; the AI is not re-simulated.
  (inferred) During playback the AI still runs and consumes random numbers; its output is
  overwritten.

## World orientation (verified in `rigid.c`, inferred here)

+Y is down: the AI holds ASCEND when the target's Y is more than 30 below the fighter's.

## Additions from the fighter-core batch

Sources: `src/battle/btl_ai_act.c` (virtual pad, attack actions, sense; 55 of 56 match) and
`src/battle/btl_ai_think.c` (rules and thinking; 36 of 43 match, so the rule evaluator below
is read from disassembly plus near-matching C).

### Virtual pad (verified)

Actions write the pad through `BtlAiPad_Set(side, hold, press, once, special, x, y)`:
- hold: sent every call; press: sent on alternate frames (a mark bit flips each frame), so the
  game sees repeated button-downs; once: sent one time until the action clears the marks.
- The stick is `(accumulator + (x, y)) / 2` with the accumulator cleared every frame: with one
  call per frame the CPU sends half the deflection it asks for.
- AI bit 0x100 maps to the lock-on battle bit; the others map to themselves; holding 0x200 and
  0x400 with press 0x2000 sends L3+R3.
- A stuck detector ends the move action when the fighter is dashing or flying but has moved
  less than its radius for more than 30 checks.

### Attack actions (verified mechanics; names guessed)

Action 0x19 ("combo"): each step is one roll over 11 level-scaled weights (blast, blast with a
direction, held rush with a random direction, guard with a side step, dash, idle...), with the
weight row chosen by the opponent's and own state class. Action 0x1A ("follow-up") rolls among
five options. Charge time is rolled: 10..50% chance of a full charge by `level / 6`.

### Sense (verified bit logic)

`BtlAiSense_Update` builds a 64-bit situation word per CPU side per frame: sequence state,
distance band (close / middle / far, also as rule groups), opponent behind, line of sight
blocked, incoming projectile, opponent downed or stunned, own gauge in four bands, health
thresholds scaled by level, and more. All 64 bits are listed in `btl_ai_act.h`. Sense draws no
random numbers and reads no pad, clock or camera.

### Thinking

- (verified) Per CPU fighter per frame: evaluate rule list 7; if nothing started and the level
  is not negative, list 4, then list 0, then list `{1,2,3,5,6,7}[plan.next]`.
- (read from disassembly) Rules are taken in file order; a rule's group must be a set bit of
  the situation word; the first rule whose conditions all pass ends that list.
- (verified) Weighted choice: each condition position has a pre-drawn roll and a running sum; a
  weighted condition adds its level-scaled rate and passes when the roll is below the sum, so
  consecutive rules of one group form a weighted pick.
- (verified) There are 118 condition functions; rule ids go to 127 through a mapping table.

### AI data file (verified in `AiThink_BindData`)

Member 4 of common file 2. A 0x148-byte header (41 sizes, then 41 pointer slots the game fills
in memory), then the action table, eight rule lists and 32 profiles back to back. A profile is
two 0x2C0-byte rate sets (level 0 and level 29) of nine tables. The column-code tables that say
which condition each weight belongs to are in the executable (0x2EDC70..0x2EDF08), not in the
file. A port must redo the load-time pointer fix-ups or convert them to offsets.

### Random draws

Everything is `Rand_Range` (the shared Mersenne Twister). The evaluator draws eight values each
time it enters a new rule group, before testing anything; some conditions draw one more whether
or not they pass; the attack actions draw one to four per decision. Draws per frame therefore
depend on the situation, on rule order in the data, and on where each rule fails.

## Generic action scripts (`btl_ai_seq_a.c`, 0x1B4140..0x1B6008; verified unless marked)

37 of 38 functions match per function (`BtlAiStep_GuardUntilSafe` is two instructions off); not
linked yet (to be merged into the head of btl_ai_seq.c). Names implying game meaning are
guesses.

- **A generic action is a script in the AI data**: per action id a first-step index, a step
  count, a pick function (0..15), a timeout in seconds and a flag word; per step (0x14 bytes) a
  handler (0..23), a group, the hold / press / once button bits, a "special", and the fighter
  state class reached when the input works. One step per group is played, group after group.
- **Three function tables**: phases (0x2C46B8: init, start, run, end), 16 pick functions
  (0x2C46C8), 24 step handlers (0x2C4708; 0..13 here, 14..23 in btl_ai_seq.c).
- **Run phase, per frame**: note whether the input took effect; run the step handler (a
  finished step costs one frame with no input); interrupt test; while the input has not taken
  effect count frames and give up after timeout x 30 (not counted while any fighter has flag
  0x128); a per-action stage test; then send the step's buttons through `BtlAiPad_Set`.
  Generic actions never move the stick. Before and after an action the CPU presses DASH to
  stop a dash.
- **Pick functions** choose among a group's alternatives and set the step's parameter: random,
  by the rule's argument, seconds or frames from the argument, ki target, skill charge, a
  level-scaled reaction delay (22..44 frames at levels 0..5 down to 0 at levels 24..29), a
  level-weighted combo branch, fusion / transformation slot, member switch (score = health% x
  0.7 + ki% x 0.3; best first with 60/40, 50/35/15 or 40/30/30), the prompted button, guard
  mode, a movement direction weighted away from the stage edge.
- **The CPU answers button prompts without error or delay** (it reads the prompt's button).
- **Step handlers 0..13**: reach a state class, charge ki to a percentage, until blocked,
  countdown, charged, not being hit, guard until safe, face the opponent, switch queued, etc.
- **Random draws**: 10 direct `Rand_Range` sites in 7 pick functions (several draw even when the
  value is unused), none in the phases or step handlers 0..13.
- No pad, clock, camera or pause read. Timers stand still while any fighter has flag 0x128.
- Corrections: `PickFusionSlot` / `PickTransformSlot` and several other by-id accessors listed
  as "no caller" in btl_capi_b are called from here; in btl_ai_seq.c the local named `busy`
  (from step handler 0) is inverted: it is 1 when the class is reached.
- Not available yet: the per-action button scripts themselves are data (common file 2, member
  4); dumping them with this layout gives the full "action id -> buttons" table.
