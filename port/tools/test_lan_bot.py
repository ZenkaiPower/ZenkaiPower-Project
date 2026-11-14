"""Process-level checks for the ranked bot supervisor, without a game/server."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


@unittest.skipUnless(os.name == "posix", "fake executable uses a POSIX shebang")
class RankedReconnectTests(unittest.TestCase):
    def test_ranked_requeues_and_exit_cancels(self):
        self.check_requeue(keep_running=False)

    def test_ended_session_requeues_while_process_stays_alive(self):
        self.check_requeue(keep_running=True)

    def check_requeue(self, keep_running):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "launches"
            game = Path(directory) / "game"
            game.write_text(
                f"#!{sys.executable}\n"
                "import os, sys, time\n"
                f"with open({str(log)!r}, 'a') as f:\n"
                "    f.write(os.environ['ZENKAIPOWER_LOBBY'] + '\\n')\n"
                + ("print('zenkaipower: net: the session has ended', file=sys.stderr, flush=True)\n"
                   "print('zenkaipower: stats: battle/interrupt saved', file=sys.stderr, flush=True)\n"
                   "time.sleep(60)\n" if keep_running else "")
            )
            game.chmod(0o755)
            child = subprocess.Popen(
                [sys.executable, str(Path(__file__).with_name("lan_bot.py")),
                 "ranked", "--game", str(game)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True,
            )
            try:
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    if log.exists() and len(log.read_text().splitlines()) >= 2:
                        break
                    if child.poll() is not None:
                        self.fail(f"supervisor exited early: {child.communicate()}")
                    time.sleep(0.05)
                self.assertEqual(log.read_text().splitlines(), ["quick:RS", "quick:RS"])
                child.stdin.write("exit\n")
                child.stdin.flush()
                time.sleep(6)
                self.assertEqual(len(log.read_text().splitlines()), 2)
                output, errors = child.communicate("quit\n", timeout=5)
                self.assertEqual(child.returncode, 0, errors)
                self.assertIn("Rejoining ranked 1v1", output)
                self.assertIn("Disconnected.", output)
            finally:
                if child.poll() is None:
                    child.kill()
                    child.communicate()


if __name__ == "__main__":
    unittest.main()
