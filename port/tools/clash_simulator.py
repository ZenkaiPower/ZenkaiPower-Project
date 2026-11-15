"""Headless Tap Strike calibration. Python 3, no dependencies."""
import argparse
import math
import re
from pathlib import Path

FPS = 60
DIRECTIONS = (1, 2, 4, 8)


def current_steps():
    source = Path(__file__).resolve().parents[1] / "src/gs/gs_input.c"
    match = re.search(r"remaining\[socket\] \+= (\d+);", source.read_text())
    if not match:
        raise ValueError("Cannot find production tap conversion in gs_input.c")
    return int(match[1])


def stick_mask(frame, rotations, radius, deadzone, phase):
    angle = phase + frame * rotations * math.tau / FPS
    x, y = radius * math.sin(angle), -radius * math.cos(angle)
    return axis_mask(x, y, deadzone)


def axis_mask(x, y, deadzone):
    if deadzone >= 0:
        magnitude = math.hypot(x, y)
        scale = max(0, (magnitude - deadzone) / (1 - deadzone))
        x, y = (x / magnitude * scale, y / magnitude * scale) if magnitude else (0, 0)
    else:
        def axis(v):
            byte = min(255, max(0, int((v + 1) * 128)))
            delta = byte - 127.5
            return math.copysign(max(0, abs(delta) - 50) / 77.5, delta)
        x, y = axis(x), axis(y)
    length = math.hypot(x, y)
    if length > 1:
        x, y = x / length, y / length
    return ((1 if y < -.5 else 0) | (2 if x > .5 else 0) |
            (4 if y > .5 else 0) | (8 if x < -.5 else 0))


def simulate(seconds, rotations, taps, steps, radius=1, deadzone=-1, phase=0):
    samples = ((stick_mask(frame, rotations, radius, deadzone, phase),
                taps > 0 and (frame * taps / FPS) % 1 < .5)
               for frame in range(round(seconds * FPS)))
    return score_samples(samples, steps)


def score_samples(samples, steps):
    """Score sampled (direction mask, Strike held) pairs using the same tap queue."""
    previous_stick = previous_tap = pending = direction = 0
    scores = {name: [0, 0] for name in ("beam", "blow")}
    edges = [0, 0]
    presses = 0
    previous_down = False
    for frame, (analog_mask, down) in enumerate(samples):
        pulse = pending > 0
        if down and not previous_down:
            pending += steps
            presses += 1
        if pending:
            direction = (direction + 1) & 3
            pending -= 1
            pulse = True
        tap_mask = DIRECTIONS[direction] if down or pulse else 0
        new = [bool(analog_mask & ~previous_stick), bool(tap_mask & ~previous_tap)]
        previous_stick, previous_tap, previous_down = analog_mask, tap_mask, down
        for side, pressed in enumerate(new):
            edges[side] += pressed
            # BtlAct_ClashStruggleHandler: actionFrame >= 0x10.
            scores["beam"][side] += int(frame >= 16 and pressed)
            # BtlAct_ClashBlowsHandler: no abilities or technique bonus.
            scores["blow"][side] += int(pressed or frame % 4 == 0)
    return scores, edges, presses, pending


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=5)
    parser.add_argument("--rotations", type=float, default=2, help="full stick turns per second")
    parser.add_argument("--taps", type=float, default=5, help="Strike presses per second")
    parser.add_argument("--steps", type=int, default=current_steps(), help="directions emitted per tap")
    parser.add_argument("--radius", type=float, default=1, help="stick tilt, 0..1")
    parser.add_argument("--deadzone", type=float, default=-1, help="-1 original; otherwise radial fraction 0..<1")
    parser.add_argument("--phase", type=float, default=0, help="initial stick angle in degrees")
    parser.add_argument("--sweep", action="store_true", help="compare directions per tap from 1 to 8")
    args = parser.parse_args()
    if not all(math.isfinite(v) for v in (args.seconds, args.rotations, args.taps, args.radius, args.deadzone, args.phase)):
        parser.error("Inputs must be finite")
    if not (0 < args.seconds <= 3600 and 0 <= args.rotations <= 30 and 0 <= args.taps <= 30
            and 1 <= args.steps <= 60 and 0 <= args.radius <= 1
            and (args.deadzone == -1 or 0 <= args.deadzone < 1)):
        parser.error("Invalid range: seconds 0..<3600, rates 0..30, steps 1..60; see --help")
    print(f"60 Hz, {args.seconds:g}s; rotation {args.rotations:g}/s; taps {args.taps:g}/s")
    print("Equal characters; input scoring only, not full battle/winner simulation.")
    print("steps  presses  pending | beam: stick / tap | blow: stick / tap | input edges: stick / tap")
    rows = []
    for steps in range(1, 9) if args.sweep else [args.steps]:
        scores, edges, presses, pending = simulate(args.seconds, args.rotations, args.taps, steps,
                                                   args.radius, args.deadzone, math.radians(args.phase))
        print(f"{steps:5} {presses:8} {pending:8} | {scores['beam'][0]:8} / {scores['beam'][1]:3} | "
              f"{scores['blow'][0]:8} / {scores['blow'][1]:3} | {edges[0]:10} / {edges[1]}")
        rows.append((sum(abs(a-b) for a, b in scores.values()), steps))
    if args.sweep:
        print(f"Closest combined score in this sample: {min(rows)[1]} directions/tap.")
    print("Pending directions are discarded at the end of the chosen window, as on clash exit.")


if __name__ == "__main__":
    main()
