# Clash calibration simulator

Run from the repository root with Python 3 (no extra packages):

    python port/tools/clash_simulator.py --rotations 2 --taps 5 --sweep

This compares two full analog-stick turns per second against five Strike presses
per second, with 1 through 8 emitted directions per tap. Change the rates to match
what you can comfortably perform. Without --sweep, --steps selects one conversion;
its default is read from gs_input.c (currently 3). This tool never changes the game.

    python port/tools/clash_simulator.py --seconds 1.3 --rotations 3 --taps 6 --steps 2

Read the beam and blow columns separately. A closer stick/tap score means a closer
conversion for that input rate and window. Repeat with several speeds, durations,
and starting angles (--phase in degrees); a single closest result is not a universal
balance recommendation. At ordinary speeds, four directions correspond roughly
to one full rotation, so start with directions/tap = 4 * rotations/second / taps/second.

The simulator samples at 60 Hz and models:
- Circular stick movement, original axis deadzone by default, direction press edges,
  and a neutral stick before the sample starts.
- Tap press/release edges, queued directions on consecutive frames, holding the
  final direction until release, and cancellation of unfinished pulses at clash exit.
- Beam/rush struggle input points after the first 16 action frames.
- Blow clash input points, with the default automatic point every fourth frame
  only when no direction was newly pressed.

Sources: port/src/gs/gs_input.c (clash_square), src/sys/pad.c,
src/battle/btl_input.c, src/battle/btl_act_f.c (ClashStruggleHandler), and
src/battle/btl_act_e.c (ClashBlowsHandler).

This is an input-scoring model, not an engine replay or a live controller reader.
The duration is a chosen scoring window, not the complete cinematic duration.
It excludes animation timing, character/technique power, abilities, opponent
interaction, and final winner resolution. Blow clash phases outside the active
struggle are excluded. Radial --deadzone (0 to below 1) approximates the production
byte quantization; --radius controls stick tilt (0 to 1). High rates can lose edges
at 60 Hz, and very fast taps can leave queued directions unfinished.

For actual-game confirmation, launch with ZENKAIPOWER_CLASH_LOG=1 and compare the
input point logs for beam and blow clashes. The simulator does not require game
assets, a running game, or a rebuild.

## Play with a controller

Connect your controller, close the game, then run:

    python port/tools/clash_controller.py

No Python packages are needed. On Windows it loads SDL3.dll from
build/ZenkaiPower-Windows. Pass --sdl PATH for a different SDL3 library.

1. Press Enter, center the stick, and release buttons during the countdown.
2. At GO, rotate the LEFT stick comfortably until STOP.
3. Press Enter again and release controls during the countdown.
4. At GO, repeatedly tap Square (PlayStation) or X (Xbox) until STOP.
5. Compare the beam and blow scores printed for each conversion.

The two trials last five seconds each. For longer trials:

    python port/tools/clash_controller.py --seconds 10

The default Strike input is the west face button. For a different binding use
--button south, east, or north. --controller 1 selects a second SDL gamepad;
--deadzone should match the game setting (-1 for Original, 0.15 for 15%).
--steps includes a chosen conversion in the report and marks it as selected;
it does not change the actual game configuration.

This records the physical controller at 60 Hz, using the same scoring function
as the synthetic simulator. It compares separate trials rather than playing
against an AI. Both clash scoring models are reported together; their earlier
limitations still apply. A disconnected controller or sampling interruption
discards the trial. Ctrl+C cancels. Results stay in the terminal.

## Selected game calibration: 3 directions per tap

The selected conversion was reduced from 4 to 3 after a live controller comparison
reported 3 as the closest combined score for the measured rotation and tapping
speeds. This is a calibration choice based on those trials, not a universal
equivalence for every player.

Each new Strike press queues three distinct cardinal directions on three
consecutive input frames (50 ms of input at 60 Hz). The direction sequence
continues across taps. Holding Strike keeps the final direction held without
adding repeated input points; releasing does not cancel already queued directions.
Rapid taps can queue more directions; exiting the clash discards pending input.

A normal full stick circle produces roughly four new directional presses.
Therefore one tap represents about three quarters of a circle in input count,
and four taps represent roughly three circles. It is not three times the strength
of a full rotation. Compared with the old four-direction setting, this reduces
the queued directions per tap by 25%.

Both clash types receive the same three-direction conversion. Beam points ignore
the initial 16 action frames; blow points include automatic fallback increments,
so final score differences need not be exactly 25%. Window boundaries, tap timing,
sampling, abilities, and character power also affect actual play.

The implementation is in clash_square() in port/src/gs/gs_input.c. Keyboard/mouse
Strike uses the conversion during clashes; gamepad Strike uses it when the
alternative clash option is enabled. Normal gamepad rotation is unchanged.
The existing game input test checks the three-direction sequence, no repeat on
hold, and completion after immediate release. Both calibration programs read the
production conversion as their default.

Rebuild the game for each target platform to use the new conversion. The Python
tools use the updated default immediately; running them does not modify the game.
