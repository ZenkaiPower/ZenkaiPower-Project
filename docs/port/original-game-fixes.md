# Original-game fixes

`PORT_ZENKAI_ORIGINAL_FIXES` defaults to `1` in
[`port_zenkai_features.h`](../../include/sys/port_zenkai_features.h).
These corrections apply to **Zenkai mode with Classic Mode either on or off**.
They do not depend on the optional Zenkai pack. Original mode keeps the original
behavior. Set the flag to `0` and rebuild to disable these corrections together.

## Implemented corrections

| Report | Original defect | Correction |
|---|---|---|
| Mission 100 fifth fighter | Moving backward from Done with four selected fighters lands on the unavailable fifth slot because the old comparison misses equality. | Clamp backward navigation to the mission's actual team limit and reject confirmation of unavailable slots. Applies to all Mission 100 team limits. |
| Electrical aura after a Duel rematch | Destroyed lightning tasks are not recreated when the aura manager remains started and the fighter's model has not changed. | Refresh each fighter's lightning eligibility during the aura-manager update. Existing lightning tasks are reused. |
| CPU switching with transformations off | The transformation-permission field also blocks teammate-switch eligibility. | In Versus, separate switching from that permission check, both for the AI query and for the actual switch input. Gauge, living teammates and combat-state checks remain required; story and training locks remain in place. |
| Incorrect default AI | Items resolve the default AI while the newly cleared fighter still has character ID 0. | Store the selected character before applying items and resolving its default AI. |
| Incomplete random-generator refill | The MT19937 refill omits the second loop, leaving 396 state words unchanged. | Restore the missing loop. Verified against an independent MT19937 implementation over 4,000 outputs, including multiple refills. This intentionally changes seeded Zenkai battle sequences. |
| Sound command overflow | Commands beyond the 32-entry per-frame queue are dropped. | Submit full batches before appending more play/stop commands. Keep the existing driver ABI and one driver tick per frame. |
| Battle arena overflow | Bump allocations can extend into the following arena. | Check remaining capacity and use tracked, aligned heap blocks for overflow allocations. Free them individually or on arena reset/battle teardown. Heap exhaustion can still fail an allocation. |
| File-request limit crash | Request 33 dereferences an empty free list. | Complete the current I/O batch before reserving another request, retaining the existing request layout and buffers. This can briefly block the caller while the batch completes. |
| Stereo music collapsed to mono | BGM starts by centering both decoded input channels. | Use the port decoder's automatic channel routing for centered BGM: stereo retains left/right separation and mono remains centered. Explicit pan and non-BGM streams retain their existing behavior. |
| Opening theme in special battles | Mission 80, Mission 100 and Survival course 3 request soundtrack slots 20/21, which are absent from the Western selectable catalog. | In Budokai Tenkaichi 3 edition mode, play the required US disc's `ZS3USOP.ADX` for those slots and loop it during battle. It is not added to music selection; the opening movie retains one-shot playback. No Japanese disc is required. |

Implementation is in the corresponding original source modules; the shared mode
gate is [`port_original_fixes.h`](../../include/sys/port_original_fixes.h).
These gameplay corrections change simulation behavior: builds using different
flag values are not equivalent for deterministic replay or multiplayer testing.

## English corrections through translation keys

The corrections are entries in
[`port/translations/ui/en.json`](../../port/translations/ui/en.json), using the
existing catalog IDs, `original` and `text` fields, plus `"zenkai_fix": true`.
Only those listed keys receive corrections. Installed game data is not rewritten.

The loader retains a correction separately from the ordinary translation. It
selects the correction only while `PORT_ZENKAI_ORIGINAL_FIXES` and Zenkai mode are
both active. The expected `original` must match the source at that key, preventing
a correction from replacing a modified or unrelated row. Other locales retain
their own translations.

| Location | Correction |
|---|---|
| Raditz, Common Enemy | “disgrace to all race” → “disgrace to our race” |
| Frieza's final transformation dialogue | “more fearsome and death itself” → “more fearsome than death itself” |
| Cell Games opening | “one of those Senzu bean” → “one of those Senzu beans” |
| Goku/Vegeta conversation and Frieza dialogue | “Freiza” → “Frieza” |
| Goku versus Nuova Shenron | “Nova Shenron” → “Nuova Shenron” |
| Super Saiyan 4 Vegeta dialogue | “Super Siayan 4” → “Super Saiyan 4” |
| Goku (Mid), View Skills | “x20 Kaiken Kamehameha” → “x20 Kaioken Kamehameha” |
| Great Ape rock attack, View Skills | “Gigant Rock Throw” → “Gigantic Rock Throw” |
| Super Saiyan 4 Gogeta biography | Remove the duplicated “this by performing” phrase. |

The six skill-script keys preserve the complete original script, replacing only
the affected move name. Ordinary menu translations retain their existing rules;
they are not gated by this feature. These entries cover rendered UTF-16 text and
translated skill-script parsing, not text baked into image textures.

## Reports awaiting reproduction or source confirmation

These reports are tracked here but **are not fixed by this change**:

| Report | What remains to verify |
|---|---|
| Japanese World Tournament/Cell Games announcers swapped | The tournament setup already chooses different announcer IDs. Verify the Japanese opening/closing voice-bank mapping before changing those assignments. |
| Special-attack explosions always on the right | Trace a reproducible attack through positional effects and streamed audio. The existing positional path calculates pan from the camera and source; the BGM correction above does not establish a fix for explosion positioning. |
| Dragon History events skipped, including Goku → Arale | Identify the exact episode, trigger and input timing, then verify event interruption and scripted switching. A general ban on event skipping could prevent intended branching. |
| Emperor's Aura with Draconic Aura | Verify regional item metadata and whether these effects are intended to be mutually exclusive. Current equipment validation already compares item type and group. |
| Vegeta scenario spelling and fusion-name inconsistencies | Confirm exact source keys and intended English naming before replacement. |
| Goku (GT) Spirit Bomb / Frieza Imprisonment Ball labels | Confirm the character, attack slot and source key. Do not globally replace “Spirit Ball,” which is also a valid attack name. |
| Truncated Majin Buu biography | Establish the missing source text before supplying a replacement. |

## Verification

Run `python3 port/tests/battle/original_fixes.py` with GCC on a native Linux host
or in the project's development container. No disc assets or Android build are
required. Tests compile both flag values and exercise Original/Zenkai gates,
Mission 100 cursor limits, AI initialization, switch restrictions, rematch aura
restoration, music routing, random sequences, sound bursts, file-request
exhaustion, arena overflow cleanup, and JSON correction selection.

The gameplay fixtures execute production functions with synthetic platform
dependencies. Visual presentation and the pending regional reports still require
in-game verification.
