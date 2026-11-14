# Tenkaichi3Decomp

[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/RsPFr4pkaN)

A native PC port of **Dragon Ball Z: Budokai Tenkaichi 3** (PlayStation 2, USA release, SLUS-21678), built on
the matching decompilation in **ZENKAIPOWER-Decompiled**. It is the game's own code compiled for PC, with a new
renderer, sound and input layer underneath; it is not an emulator.

This repository contains **no game data**. You need your own disc image of that release: the setup reads the
game's data from it, and it stays on your computer.

> **Official source.** This project lives only at
> [github.com/z3xox/Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp), and it never ships the game:
> you need your own disc. We do not make, run or endorse any website that lets you play Budokai Tenkaichi 3 in a
> browser, with or without ads; such sites are not affiliated with this project. A site that runs the game
> without your own disc is handing out the game's copyrighted code and data. Our releases are free and have no
> ads, accounts or payments.

## What works

- The whole game loop: boot, opening movie, menus, saving, fights, split screen.
- A GPU renderer (Vulkan) with the PS2's effects (outline, glow, depth tint, distance blur), each switchable.
  An OpenGL 3.3 renderer draws the same picture for machines without a working Vulkan (Esc, Video, Renderer; it
  is also tried by itself when Vulkan cannot start). It is slower than Vulkan.
- Internal resolution from 1x to 8x; 4:3, 16:9, 21:9 and wider without stretching the fight.
- Smooth motion (experimental, off by default; Esc, Video, or F9): the game computes a fight 30 times a second, as
  on the console; with this the renderer draws pictures in between, for 60, 120 or 240 a second. The fight itself,
  replays and online play are unchanged, and the two players of an online match need not agree on it. The newest
  picture reaches the screen 17 to 29 ms later than without. Known limits: pictures are left out at camera cuts
  and where the camera would pass through scenery; 240 is uneven; little tested with OpenGL. Works in both halves
  of a split screen.
- Music, voices and sound effects; keyboard and controllers, fully rebindable; two players.
- Clashes: Square counts as a direction (off by default; Esc, Controls): in a clash of two beams or two rushes the
  game counts newly pressed directions, which is why the stick is turned; with this each press of Square counts as
  one, as a tap of the d-pad does. It changes only your own pad, so it can be used online. A press can be made to
  count for 1, 1.5, 1.75 or 2 (1.75 unless set: the d-pad has four buttons to tap in turn, Square is one).
- Texture packs made for PCSX2 (`.dds` or `.png`): copied into the `textures` folder next to the game, they are used as they are.
- Stages and music from outside the disc: a stage model (`.unk`) dropped into the `stages` folder next to the
  game, or a song (`.mp3`, converted in the background when the game starts, or `.adx`) into `songs`, is added to the stage select, its name written in the game's own
  lettering (cut from the disc's name pictures while the game runs). They are not written to the save and are
  not offered in an online match. See [docs/port/adding_stages.md](docs/port/adding_stages.md) and
  [adding_songs.md](docs/port/adding_songs.md).
- In the game, **F1** opens and closes settings, or **START + SELECT** on a controller
  (d-pad and Cross to choose, Circle to leave, L1 / R1 for tabs). Controls has a row to choose
  another key or button. Escape cancels dialogs and never exits the game. The settings panel
  uses the game's dialogue font, native frame and button textures, and opening/closing sounds.

- **Online play, experimental** (from release 0.1.8): the main menu's hidden "Dragon Net Battle"
  entry is back and opens a small window. One player hosts and gets a six-character room code, the other joins
  with the code: no port forwarding (from 0.1.16; hosting on a port and joining by address is still there);
  both get the same roster with everything unlocked, each sees their own fighter's view full screen, and leaving
  the match puts each player back where they were with their own save. Linux and Windows play against each
  other. With rollback (the host chooses how many frames, and the input delay) your own moves come out at once
  and the game corrects itself when the other player's buttons arrive.
  The host also chooses the battle (single, team, or DP battle with a limit of 10, 15 or 20) and the time limit.
  Each player sees their own gauges on the left with the players' names under them, and hears the voice language
  chosen in their own save.
  Online chat is off by default (`chat=1` in the settings file enables it). When enabled, Tab opens a line, Enter sends it, Esc or Tab puts it away (the keyboard
  does not play while a line is typed; a controller does; the key can be changed under Controls). Lines show in the
  lower left corner for a few seconds; the line also has "Mute rival". It is not
  part of the game's state and a copy without it simply sees nothing.

Online play is new. It has been played over the internet between a Linux and a Windows machine. The input delay
is chosen from the connection when a match starts (or set by the host), both players need the same release (the
game says so when they differ; 0.1.10 does not play against 0.1.8 / 0.1.9, and 0.1.15 does not play against
0.1.14 and earlier). The optional meter (`meter=1` in the settings file) shows the frame rate and, in a match, ping, rollbacks and waits. The 32-bit
Linux build has no rollback. [docs/netplay_notes.md](docs/netplay_notes.md) is the working log of how it is built
and what was checked.

Room codes go through a small matchmaking service (a Cloudflare Worker; its whole source is in
[matchmaking](matchmaking), and `ZENKAIPOWER_MATCH_URL` points the game at another copy of it). The game tells it
the name typed in the window and the addresses it can be reached at; the service keeps them with the room for at
most ten minutes and hands them to the player who enters the code. The two games then send to each other
directly. Where the routers do not allow that, the match goes through a relay (Cloudflare's TURN service), which
passes the match's packets (the buttons pressed) on and keeps nothing. Without a room code none of this is used.

See [docs/port/README.md](docs/port/README.md) for the running log of what is done, what is verified and what is not.

## Requirements

- A 64-bit x86 processor and a graphics card with Vulkan support. Most of the work is on the processor (the
  game's arithmetic is reproduced exactly as on the PS2); a recent mid-range CPU is recommended.
- Linux (glibc 2.35 or newer) or Windows 10 / 11.
- macOS on Apple Silicon: **experimental**, built from source only (see below).
- About 4 GB of disc space, and your disc image as an `.iso` file.

## Installing a release

Unpack the release, start `ZenkaiPower-setup` (`ZenkaiPower-setup.exe` on Windows), choose your disc image, press Play. The
setup checks that the image is the unmodified USA release and unpacks the game's data next to the program.

On a machine without a working Vulkan the setup window and the game use OpenGL by themselves. To ask for OpenGL
from the start (a Vulkan driver that starts but is incomplete, as on Intel Ivy Bridge), set `ZENKAIPOWER_GPU_API=gl`:
`ZENKAIPOWER_GPU_API=gl ./ZenkaiPower-setup`, and the same for `./ZenkaiPower`. The setup also runs without a
window: `./ZenkaiPower-setup --install <disc image>`.

## Building from source

```
python3 install.py --iso <your disc image>
```

does everything on Linux: checks the tools, reads the disc, builds the 64-bit program and runs the game's demo
fight as a test. `python3 install.py --check` lists what is missing. `port/setup/build.sh && ./ZenkaiPower-setup` is
the same with a window.

After editing code, run `python3 install.py --rebuild`. Compiler dependency files
track the headers each source uses, so only affected objects are rebuilt. Renderer
and C++ compilation also run in parallel; set `ZENKAIPOWER_JOBS=8` to choose the number of
workers. An unchanged executable skips linking. The first build after upgrading
these scripts rebuilds existing objects once to generate dependency files.

For a compile-and-link cycle without setup or the demo test:

```sh
ZENKAIPOWER_CC=clang64 python3 port/tools/undefined.py && ZENKAIPOWER_CC=clang64 python3 port/tools/link.py
```

Use `ZENKAIPOWER_FULL_REBUILD=1` to force a complete rebuild with the direct tools.

### macOS (experimental)

Apple Silicon Macs, native arm64, built from source on the Mac itself; there is no macOS release archive. The
attract-demo fight gives the same state as the Linux program on every frame, the rewind test (rollback) passes,
and the game draws through Metal with sound; online play against Linux / Windows has not been tried on two machines
yet. With [Homebrew](https://brew.sh):

```
xcode-select --install                       # (once: Apple's command line tools)
brew install llvm sdl3 shaderc spirv-cross
python3 install.py --iso <your disc image>   # then ./play.sh
```

`python3 install.py --check` lists what is missing. How the build works and what is left:
[docs/port/macos_plan.md](docs/port/macos_plan.md).

The release archives for Linux and Windows are built from the repository alone, with no disc involved:
`port/release/build.sh` (needs only docker) or the GitHub workflow in `.github/workflows/release.yml`. The
repository carries the shape of the game's built-in data tables without their values (`port/data`); the
finished program fetches the values from your disc when it starts.

## Layout

| Path | Contents |
|---|---|
| `src/`, `include/` | The game's code, from ZENKAIPOWER-Decompiled; changes for PC are inside `#ifdef PORT` |
| `port/src/` | The PC side: memory, files, memory card, movies; `gs/` renderer, sound, input, settings window |
| `port/tools/` | The build: pointer rewriting for 64-bit, packaging, extraction |
| `port/data/` | The game's built-in data tables as shape only (labels, sizes, pointer slots), no values |
| `port/release/` | The container and scripts that build the release archives |
| `port/setup/` | The setup window |
| `port/third_party/` | Dear ImGui (MIT), the maths library of newlib |
| `docs/` | Notes on the game (from the decompilation) and `docs/port/` on the port |

This project was made with the help of AI.

## Contributors

- [RexxColder](https://github.com/RexxColder): the OpenGL 3.3 renderer beside Vulkan (the split of the renderer
  into a shared recorder and two back ends), and stages and songs from outside the disc (the manifest, the
  install scripts, the menu changes and their documentation), in
  [pull request #1](https://github.com/z3xox/Tenkaichi3Decomp/pull/1). Both were brought onto the main line
  with changes; the documents under `docs/port/` say which.

## Licence

The port's own code (everything under `port/` except `port/third_party/`, the setup, the build tools, and the
PC changes inside `#ifdef PORT`) is under the [GNU Affero General Public License, version 3](LICENSE)
(AGPL-3.0-only) in every commit after `06e4671` (2026-10-14). Up to and including that commit it was under the
[MIT licence](LICENSE-MIT), and that code, as it was then, stays available under MIT; contributions made under
MIT keep their MIT notice inside the AGPL whole. Whoever passes the program on, changed or not, or runs a
changed version for others over a network (a lobby or relay server, a web page), has to offer its source under
the same licence. The libraries in `port/third_party/` keep their own licences (Dear ImGui: MIT; newlib's maths
library: its BSD-style terms, in that folder).
The game's code in `src/` and `include/` comes from the decompilation and is not ours to license: it belongs
to the game's rights holders.

## Legal

This project is not affiliated with or endorsed by the game's developers, publishers or rights holders. It
exists for preservation, study and interoperability, and distributes none of the game's assets. All rights to
the game belong to their owners. You need your own legally obtained copy of the game to use it.

### Native bot GUI

`bot/ZenkaiPower-bot` (`bot/ZenkaiPower-bot.exe` on Windows) runs the same headless CPU
opponent as `port/tools/lan_bot.py`, with a native SDL3/ImGui interface and no
Python runtime dependency. Release archives include it in the `bot/` folder.

From the repository root, build only the bot:

```sh
./port/bot/build.sh       # Linux: ./bot/ZenkaiPower-bot
./port/bot/build.sh win   # Windows: bot/ZenkaiPower-bot.exe
```

The dependencies match the setup build: C++17, make, SDL3 development files, and
MinGW-w64 plus SDL3's MinGW package when cross-compiling Windows. Both builds also
run inside `port/release/build.sh` and the release workflow.

Launch the bot from the release's `bot/` folder, or browse to the game executable. Choose
Join room, Host room, Online matchmaking, or LAN; select a battle mode and CPU
level, then Connect. Ranked matchmaking can automatically rejoin after a session ends. Enable
"Wait before rejoining" to set a delay from 0 to 3600 seconds, or disable it for
immediate requeueing. The default delay is five seconds and the choice is saved.
Activity shows both players, win/loss/draw, finish reason and battle duration;
interrupted battles are marked without assigning a win or loss. Session totals
count wins, losses and draws while the GUI stays open. Disconnect stops the owned game process and cancels retries.
Controls can be changed while disconnected.

Use a separate bot account name and access code for online play. The code stays masked in the GUI. Options and the access code persist in the
bot executable's folder as `settings.cfg` and `access.dat`. The access code is
stored as plain text so the folder can be moved between computers; Linux restricts
the file to its owner. Clear the access-code field to remove its saved value.
The game stores its bot session in `net_account_bot.dat` in that same folder,
separate from the player's account. Saved bot sessions let you leave the access
code blank on later launches. Existing sessions from the Python bot can be used
by copying that bot account file into the bot folder. LAN needs no login.
The GUI displays process output in memory and writes no activity logs.
