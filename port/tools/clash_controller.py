"""Play two controller calibration trials in the terminal. Uses the game's SDL3.dll."""
import argparse
import ctypes as C
import ctypes.util
from pathlib import Path
import sys
import time

from clash_simulator import FPS, axis_mask, current_steps, score_samples


class Controller:
    def __init__(self, library=None, index=0):
        repo = Path(__file__).resolve().parents[2]
        candidates = ([library] if library else [
            str(repo / "build/ZenkaiPower-Windows/SDL3.dll"),
            str(repo / "release/ZenkaiPower-Windows/SDL3.dll"),
            ctypes.util.find_library("SDL3"),
        ])
        self.sdl = None
        for candidate in candidates:
            if not candidate:
                continue
            try:
                self.sdl = C.CDLL(candidate)
                break
            except OSError:
                pass
        if self.sdl is None:
            raise RuntimeError("SDL3 library not found. Build the Windows game or pass --sdl PATH.")
        self.pad = None
        declarations = {
            "SDL_Init": (C.c_bool, [C.c_uint32]),
            "SDL_Quit": (None, []),
            "SDL_GetError": (C.c_char_p, []),
            "SDL_GetGamepads": (C.POINTER(C.c_uint32), [C.POINTER(C.c_int)]),
            "SDL_free": (None, [C.c_void_p]),
            "SDL_OpenGamepad": (C.c_void_p, [C.c_uint32]),
            "SDL_CloseGamepad": (None, [C.c_void_p]),
            "SDL_GetGamepadName": (C.c_char_p, [C.c_void_p]),
            "SDL_GamepadConnected": (C.c_bool, [C.c_void_p]),
            "SDL_UpdateGamepads": (None, []),
            "SDL_PumpEvents": (None, []),
            "SDL_GetGamepadAxis": (C.c_int16, [C.c_void_p, C.c_int]),
            "SDL_GetGamepadButton": (C.c_bool, [C.c_void_p, C.c_int]),
            "SDL_SetHint": (C.c_bool, [C.c_char_p, C.c_char_p]),
        }
        for name, (restype, argtypes) in declarations.items():
            function = getattr(self.sdl, name)
            function.restype, function.argtypes = restype, argtypes
        self.sdl.SDL_SetHint(b"SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", b"1")
        if not self.sdl.SDL_Init(0x2000):  # SDL_INIT_GAMEPAD
            raise RuntimeError(self.error())
        try:
            count = C.c_int()
            ids = self.sdl.SDL_GetGamepads(C.byref(count))
            try:
                if not ids or not 0 <= index < count.value:
                    raise RuntimeError(f"No controller at index {index}; detected {count.value}. Connect it before starting.")
                self.pad = self.sdl.SDL_OpenGamepad(ids[index])
            finally:
                self.sdl.SDL_free(ids)
            if not self.pad:
                raise RuntimeError(self.error())
            self.name = self.sdl.SDL_GetGamepadName(self.pad).decode("utf-8", "replace")
        except Exception:
            self.close()
            raise

    def error(self):
        return self.sdl.SDL_GetError().decode("utf-8", "replace")

    def read(self, button):
        self.sdl.SDL_PumpEvents()
        self.sdl.SDL_UpdateGamepads()
        if not self.sdl.SDL_GamepadConnected(self.pad):
            raise RuntimeError("Controller disconnected; trial discarded.")
        axes = [max(-1, self.sdl.SDL_GetGamepadAxis(self.pad, axis) / 32767) for axis in (0, 1)]
        return axes[0], axes[1], bool(self.sdl.SDL_GetGamepadButton(self.pad, button))

    def close(self):
        if self.pad:
            self.sdl.SDL_CloseGamepad(self.pad)
            self.pad = None
        self.sdl.SDL_Quit()


def record(controller, label, frames, button, deadzone):
    input(f"\n{label}\nPress Enter when ready. Keep the stick centered and release buttons.")
    for remaining in (3, 2, 1):
        print(f"Starting in {remaining}...", flush=True)
        time.sleep(1)
    x, y, down = controller.read(button)
    if down or axis_mask(x, y, deadzone):
        raise RuntimeError("Start with the stick centered and Strike released. Please rerun.")
    print("GO!", flush=True)
    start = time.perf_counter()
    samples = []
    for frame in range(frames):
        target = start + frame / FPS
        delay = target - time.perf_counter()
        if delay > 0:
            time.sleep(delay)
        if time.perf_counter() - target > 1 / FPS:
            # Do not invent repeated samples to catch up after a scheduling pause.
            raise RuntimeError("Sampling fell more than one frame behind. Close busy applications and retry.")
        x, y, down = controller.read(button)
        samples.append((axis_mask(x, y, deadzone), down))
        if frame % FPS == 0:
            print(f"\r{(frames-frame)/FPS:.1f}s left  ", end="", flush=True)
    print("\rSTOP!                 ")
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=5, help="duration of each trial")
    parser.add_argument("--steps", type=int, default=current_steps())
    parser.add_argument("--button", choices=("west", "south", "east", "north"), default="west")
    parser.add_argument("--controller", type=int, default=0)
    parser.add_argument("--sdl", help="path to SDL3.dll or libSDL3.so")
    parser.add_argument("--deadzone", type=float, default=-1, help="-1 original, or radial fraction 0..<1")
    args = parser.parse_args()
    if not (0 < args.seconds <= 60 and 1 <= args.steps <= 60 and args.controller >= 0
            and (args.deadzone == -1 or 0 <= args.deadzone < 1)):
        parser.error("Use seconds 0..60, steps 1..60, and a valid deadzone/controller index.")
    controller = None
    try:
        controller = Controller(args.sdl, args.controller)
        button = {"south": 0, "east": 1, "west": 2, "north": 3}[args.button]
        print(f"Controller: {controller.name}")
        print(f"Strike: {args.button} face button (west = Square / X). Ctrl+C cancels.")
        print("Close the game first so controller input does not also control it.")
        frames = max(1, round(args.seconds * FPS))
        stick = record(controller, "Trial 1: rotate the LEFT stick at your comfortable clash speed.",
                       frames, button, args.deadzone)
        taps = record(controller, f"Trial 2: tap the {args.button.upper()} face button at your comfortable speed.",
                      frames, button, args.deadzone)
        # Discard Strike during the stick trial and stick movement during the tap trial.
        stick_scores, stick_edges, _, _ = score_samples(((mask, False) for mask, _ in stick), args.steps)
        tap_samples = [(0, down) for _, down in taps]
        print("\nInput scores for equal characters; both clash scoring models use the recorded trials.")
        print("steps | beam: stick / tap | blow: stick / tap | presses | queued at end")
        rows = []
        for steps in sorted(set(range(1, 9)) | {args.steps}):
            scores, edges, presses, pending = score_samples(tap_samples, steps)
            print(f"{steps:5} | {stick_scores['beam'][0]:8} / {scores['beam'][1]:3} | "
                  f"{stick_scores['blow'][0]:8} / {scores['blow'][1]:3} | {presses:7} | {pending}"
                  + ("  current selection" if steps == args.steps else ""))
            rows.append((sum(abs(stick_scores[key][0]-scores[key][1]) for key in scores), steps))
        print(f"Stick direction edges: {stick_edges[0]} (roughly {stick_edges[0]/4:.2f} turns).")
        print(f"Closest combined score for these trials: {min(rows)[1]} directions per tap.")
        print("Repeat trials before choosing a conversion. Game settings were not changed.")
    except (RuntimeError, OSError, AttributeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("\nCancelled; no result saved.")
        return 130
    finally:
        if controller:
            controller.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
