#!/usr/bin/env python3
"""Run a headless CPU opponent over direct LAN or a Friend Battle room code."""

import argparse
import getpass
import os
from pathlib import Path
import re
import queue
import signal
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[2]
MODES = {
    "1v1": "S",
    "team": "T",
    "dp10": "D10",
    "dp15": "D15",
    "dp20": "D20",
    "ranked": "RS",
}
ROOM_CODE = re.compile(r"[A-Z0-9]{6}\Z")
LOBBY_CODE = re.compile(r"zenkaipower: lobby: state -?\d+, code '([A-Z0-9]{6})'")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", nargs="?", choices=MODES, help="optional mode to start immediately (ranked automatically requeues after closing)")
    startup = parser.add_mutually_exclusive_group()
    startup.add_argument("--join-code", metavar="CODE", help="join a Friend Battle room code immediately")
    startup.add_argument("--generate-code", action="store_true", help="host a Friend Battle room and print its code")
    parser.add_argument("--online", action="store_true", help="use online matchmaking for mode commands")
    parser.add_argument("--rejoin-delay", type=int, default=5, metavar="SECONDS", help="ranked rejoin delay, 0 for immediate (default: 5)")
    parser.add_argument("--host", default="127.0.0.1", help="address of the human player's game")
    parser.add_argument("--name", default="SparkingBot", help="name shown to the other player")
    credentials = parser.add_mutually_exclusive_group()
    credentials.add_argument("--login", action="store_true", help="prompt for this account's access code")
    credentials.add_argument("--access-code", metavar="CODE", help="log in with this account access code")
    parser.add_argument("--account-file", type=Path, default=ROOT / "savegame/net_account_bot.dat",
                        help="separate saved bot login (default: savegame/net_account_bot.dat)")
    parser.add_argument("--level", type=int, choices=range(1, 6), default=5,
                        metavar="1-5", help="CPU difficulty, 1 easiest and 5 hardest (default: 3)")
    parser.add_argument("--game", type=Path, default=ROOT / "port/build/zenkaipower_64", help="built game executable")
    args = parser.parse_args()
    if not 0 <= args.rejoin_delay <= 3600:
        parser.error("--rejoin-delay must be between 0 and 3600 seconds")
    game = args.game.resolve()
    if not game.is_file():
        parser.error(f"game executable not found: {game}")
    if ":" in args.host or not args.host:
        parser.error("--host must be an IPv4 address or hostname without a port")
    if args.join_code and args.mode:
        parser.error("--join-code does not take a battle mode; the host supplies it")
    if args.join_code and not ROOM_CODE.fullmatch(args.join_code.upper()):
        parser.error("--join-code must contain six letters or numbers")
    if args.generate_code and args.mode == "ranked":
        parser.error("ranked mode uses online matchmaking, not room codes")
    if args.generate_code and not args.mode:
        parser.error("--generate-code requires a mode such as 1v1 or team")

    access_code = args.access_code.strip().upper() if args.access_code is not None else None
    if args.login:
        try:
            access_code = getpass.getpass(f"Access code for {args.name}: ").strip().upper()
        except (EOFError, KeyboardInterrupt):
            parser.exit(1, "\nLogin cancelled.\n")
    if access_code is not None and not ROOM_CODE.fullmatch(access_code):
        parser.error("access code must contain six letters or numbers")
    account_file = args.account_file.resolve()
    if account_file in {(ROOT / "net_account.dat").resolve(), (ROOT / "savegame/net_account.dat").resolve(), (ROOT / "save_game/net_account.dat").resolve()}:
        parser.error("choose a separate bot account file, not net_account.dat")

    account_file.parent.mkdir(parents=True, exist_ok=True)
    legacy = ROOT / "net_account_bot.dat"
    if account_file == (ROOT / "savegame/net_account_bot.dat").resolve() and legacy.exists() and not account_file.exists():
        # Publish the existing login without overwriting another process's file.
        try:
            os.link(legacy, account_file)
        except FileExistsError:
            pass
        else:
            account_file.chmod(0o600)
            legacy.unlink()

    process: subprocess.Popen[str] | None = None
    selected_level = args.level
    current_request: tuple[str, str] | None = None
    session_ended = threading.Event()

    def read_game_log(child: subprocess.Popen[str], kind: str, ended: threading.Event) -> None:
        assert child.stderr is not None
        for line in child.stderr:
            if line.strip() == "zenkaipower: net: the session has ended":
                ended.set()
            match = LOBBY_CODE.search(line)
            if match and kind == "quick":
                print("Waiting in online matchmaking for another player in the same mode.", flush=True)
                continue
            if match and kind == "host":
                print(f"\nRoom code: {match.group(1)} — enter it under Friend Battle > Join Battle.", flush=True)
            print(line, file=sys.stderr, end="", flush=True)

    def stop_game() -> None:
        nonlocal process, current_request
        current_request = None
        if process is None:
            return
        if process.poll() is None:
            try:
                if os.name == "posix":
                    os.killpg(process.pid, signal.SIGTERM)
                else:
                    process.terminate()
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    if os.name == "posix":
                        os.killpg(process.pid, signal.SIGKILL)
                    else:
                        process.kill()
                except ProcessLookupError:
                    pass
                process.wait()
        process = None

    def start_game(request: tuple[str, str]) -> None:
        nonlocal process, current_request, session_ended
        stop_game()
        session_ended = threading.Event()
        env = os.environ.copy()
        for name in ("ZENKAIPOWER_REPLAY", "ZENKAIPOWER_DEMO", "ZENKAIPOWER_NET_HOST", "ZENKAIPOWER_NET_JOIN", "ZENKAIPOWER_LOBBY"):
            env.pop(name, None)
        env.pop("ZENKAIPOWER_BOT_ACCESS_CODE", None)
        env["ZENKAIPOWER_ACCOUNT_FILE"] = str(account_file)
        if access_code:
            env["ZENKAIPOWER_BOT_ACCESS_CODE"] = access_code
        env["ZENKAIPOWER_NET_BOT"] = "1"
        env["ZENKAIPOWER_NET_BOT_LEVEL"] = str(selected_level - 1)
        env["ZENKAIPOWER_NET_NAME"] = args.name
        kind, value = request
        if value == "ranked": kind = "quick"
        if kind == "lan":
            env["ZENKAIPOWER_LOBBY"] = f"lan:join:{args.host}:{MODES[value]}"
        elif kind == "quick":
            env["ZENKAIPOWER_LOBBY"] = f"quick:{MODES[value]}"
        elif kind == "host":
            env["ZENKAIPOWER_LOBBY"] = f"host:{MODES[value]}"
        else:
            env["ZENKAIPOWER_LOBBY"] = f"join:{value}"
        env["ZENKAIPOWER_GS"] = "none"
        env["ZENKAIPOWER_NOSOUND"] = "1"
        process = subprocess.Popen([str(game)], cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                   stderr=subprocess.PIPE, text=True, bufsize=1,
                                   start_new_session=os.name == "posix")
        threading.Thread(target=read_game_log, args=(process, kind, session_ended), daemon=True).start()
        current_request = request
        if kind == "host":
            print(f"Hosting {value} at CPU level {selected_level}; waiting for a room code...", flush=True)
        elif kind == "quick":
            print(f"Searching online for {value} at CPU level {selected_level}.", flush=True)
        elif kind == "join":
            print(f"Joining room {value} at CPU level {selected_level}.", flush=True)
        else:
            print(f"Headless bot running in {value} mode at CPU level {selected_level}, joining {args.host}.", flush=True)

    default_kind = "quick" if args.online or access_code is not None else "lan"
    help_text = f"Commands: 1v1, team, dp10, dp15, dp20 ({default_kind}); quick MODE (online, including ranked); lan MODE; host MODE (generate code); join CODE; level 1-5; exit (disconnect); quit"
    print(help_text, flush=True)
    # Keep process supervision on this thread so restarting never races with
    # exit, quit, a level change, or another mode selected at the prompt.
    commands: queue.Queue[str | None] = queue.Queue()

    def read_commands() -> None:
        try:
            while True:
                commands.put(input("bot> ").strip().lower())
        except EOFError:
            commands.put(None)

    retry_at: float | None = None
    try:
        if args.join_code:
            start_game(("join", args.join_code.upper()))
        elif args.generate_code:
            start_game(("host", args.mode))
        elif args.mode is not None:
            start_game((default_kind, args.mode))
        threading.Thread(target=read_commands, daemon=True).start()
        while True:
            try:
                command = commands.get(timeout=0.25)
            except queue.Empty:
                if (args.mode == "ranked" and current_request is not None
                        and current_request[1] == "ranked" and process is not None
                        and (process.poll() is not None or session_ended.is_set())):
                    if retry_at is None:
                        retry_at = time.monotonic() + args.rejoin_delay
                        print(f"Ranked session closed. Rejoining ranked 1v1 in {args.rejoin_delay} seconds.", flush=True)
                    elif time.monotonic() >= retry_at:
                        start_game(current_request)
                        retry_at = None
                continue
            retry_at = None
            if command is None:
                break
            if command == "exit":
                stop_game()
                print("Disconnected. Enter a mode, host MODE, or join CODE to connect again.", flush=True)
                continue
            if command == "quit":
                break
            words = command.split()
            if not words:
                continue
            if words[0] == "level" and len(words) == 2 and words[1] in ("1", "2", "3", "4", "5"):
                selected_level = int(words[1])
                if current_request is not None:
                    start_game(current_request)
                else:
                    print(f"CPU level set to {selected_level}")
            elif words[0] in MODES and len(words) in (1, 2) and (len(words) == 1 or words[1] in ("1", "2", "3", "4", "5")):
                if len(words) == 2:
                    selected_level = int(words[1])
                start_game((default_kind, words[0]))
            elif words[0] in ("quick", "lan") and len(words) == 2 and words[1] in MODES:
                start_game((words[0], words[1]))
            elif words[0] == "host" and len(words) == 2 and words[1] in MODES:
                start_game(("host", words[1]))
            elif words[0] == "join" and len(words) == 2 and ROOM_CODE.fullmatch(words[1].upper()):
                start_game(("join", words[1].upper()))
            elif command == "help":
                print(help_text)
            elif command:
                print(f"Unknown command: {command}. Type help for available commands.")
    except KeyboardInterrupt:
        print()
    finally:
        stop_game()


if __name__ == "__main__":
    main()
