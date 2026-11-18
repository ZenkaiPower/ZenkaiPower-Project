# Cell transformation presentation

With `PORT_ZENKAI_CELL_TRANSFORMATION=1`, Cell (Perfect Form, 107) visibly
reverts to Semi-Perfect Cell (106), plays his Ultimate cinematic, then becomes
Super Perfect Cell (108) standing in his clean outfit, with a short particle
reveal before returning to play.

This applies only to **offline Zenkai with Classic Mode off**. Original mode,
Classic Mode and online battles retain the original presentation. Set the flag
in [`port_zenkai_features.h`](../../include/sys/port_zenkai_features.h) to `0`
and rebuild to disable it. The runtime feature is `cell_transformation`, in
`zenkai_pack` and `battle_mechanic_change`.

## Cinematic

The detransformation first uses the earlier model-only swap and transformation
flash, without the normal transformation voice cue. The Ultimate's preparation
and detonation dialogue remain enabled. It uses the native front transformation camera (cut 6), which follows
model nodes and the game's negative-Y-up coordinates. The camera stays
at a constant distance of 2.2 times Cell's height during the low-health
standing motion (`0x001`), preventing the detransformation camera from pulling
away. Perfect Form Cell plays this stance for 30 battle updates before the
flash and first model swap begin, so it is visible from the start as well as
after detransforming. Perfect Form borrows Semi-Perfect Cell's actual low-health
pose tracks for this presentation, rather than relying on its own motion 1.
The entrance blends over 0.25 seconds and the transition into the Ultimate
preparation blends over 0.2 seconds. The weakened stance continues animating
while the first swap loads. Cancelling before the swap restores the original
idle; subsequent full model loads keep their own animation tables.
Both temporary model reloads rebind the weakened pose before sampling the new
body and restore its breathing-cycle frame. This prevents a brief normal stance
or a restarted low-health animation between the two forms.
This pose does not change his actual health. The load requests are
given Perfect Form Cell's current outfit-damage variant for both temporary
Semi-Perfect bodies, so detransforming preserves existing battle damage.
The final Super Perfect form still uses its clean outfit. The load requests are
retired before a 30-update visible hold; loading callbacks do not share the hold
timer. The temporary Semi-Perfect form also loads the complete Ultimate
resources: animations, effect pack and sound bank. It plays the dialogue
and explosion motions (`0x161` and `0x162`) with their events intact. The opening punch
(`0x160`) and the recovery before the preparation voice cue in `0x161` are skipped. The
remaining animation keeps its original time coordinates, including the opening
motion duration in the camera clock, so the dialogue
and explosion keep their original framing and timing. These events drive the
original voice lines, sounds, visibility changes and effects instead of manually timed
transformation flashes.

The original Ultimate effect task starts its authored camera. The camera clock
sums the completed motion lengths and current animation frame, using the same
rush-cinematic timing as the Ultimate. Its stage placement tables are also used.
The paired animation event tracks (`0x165` through `0x167`) supply the original flashes,
blur and sub-effect triggers without putting the opponent into a combat action.
During this cinematic, the effect renderer allows the Ultimate's effects through
the transformation action's usual effect-hiding filter. This keeps the explosion
particles and meshes visible, including while the swelling body replaces Cell.
The normal visibility rules still apply outside the cinematic.

The swelling body uses partner 281, or 282 for costume 2, and its original
279-frame model animation. Its normal event handler remains enabled. The two
Ultimate voice lines use Semi-Perfect Cell's identity and the selected
Japanese/English lip tracks.

After the explosion, the handler stops the Ultimate camera and releases the
swelling body. The spent Semi-Perfect body stays hidden during the final load,
covered by the standard transformation particles. Clean Super Perfect Cell is
loaded into idle, with both the held-hide flag and flash mask explicitly cleared.
The native after-particles start without the flash's model hiding or stage
blanking. Super Perfect Cell appears with the native Instant Transmission
arrival effect (request `0xD`) and teleport sound (`0x20`). His body stays hidden
for the first three updates of the shrinking arrival column, then appears with
the body streaks. This does not move him or spend an additional skill cost.
The native front view holds for 20 battle updates before
control returns. Neither the closing transformation animation (`0x179`) nor
the Ultimate's damaged, prone Semi-Perfect recovery is played. Loading Super
Perfect Cell is delayed until the cinematic ends so it cannot free the Ultimate's active effects prematurely.

## Gameplay and resources

This remains a transformation: it does not run the Ultimate's damage setup,
self-damage, Ki charge or prone recovery. Effect hit records belonging to the
cinematic are rejected. The input, eligibility, two-stock cost and final
transformation recovery bonuses remain unchanged; there is no health threshold.
The temporary form does not update the active member's form, HP, Ki, stocks or
stat modifiers. Only the final form change applies transformation recovery.

Runtime asset copies are released with the battle's other moveset resources.
Installed game data is unchanged. Missing donor resources select the original
transformation instead.

## Verification

- `python3 port/tests/battle/cell_transformation.py` checks the production handler,
  model-loading waits, skipped punch, preserved camera offset, delayed final load,
  clean outfit, front-camera holds, trimmed recovery, particle reveal,
  partner cleanup, single stock charge, temporary stat preservation, camera
  timing and cinematic damage suppression. It covers both compilation flag values
  and Original/Classic/online/failure fallbacks.
- `python3 port/tests/battle/zenkai_moves.py <gamedata>` checks complete motion
  preservation, paired event timing, swelling models, both voice languages,
  source preservation, cleanup and the positional voice-event queue. It also
  covers the other borrowed Zenkai movesets.
