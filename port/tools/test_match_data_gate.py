"""Game-level matchmaking verification test against a local HTTP server only.

Run: python3 port/tools/test_match_data_gate.py --game release/linux-x64/ZenkaiPower
Requires installed game data. Does not join real rooms or use saved accounts.
"""
import argparse
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time


def check(game, accepted):
    requests = []
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass
        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            requests.append((self.path, body))
            response = {'expected': body.get('data', '') if accepted else ''} if self.path == '/v1/data-check' else {'error': 'not found'}
            data = json.dumps(response, separators=(',', ':')).encode()
            self.send_response(200)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    with tempfile.TemporaryDirectory(prefix='zenkaipower-match-data-') as temporary:
        env = os.environ.copy()
        for key in list(env):
            if key.startswith(('ZENKAIPOWER_LOBBY', 'ZENKAIPOWER_NET_', 'ZENKAIPOWER_BOT_', 'ZENKAIPOWER_REPLAY', 'ZENKAIPOWER_DEMO', 'ZENKAIPOWER_MATCH_', 'ZENKAIPOWER_DATA', 'ZENKAIPOWER_FILE_ALIAS')):
                env.pop(key)
        env.update(ZENKAIPOWER_LOBBY='join:ABCDEF', ZENKAIPOWER_GS='none', ZENKAIPOWER_NOSOUND='1',
                   ZENKAIPOWER_MATCH_URL=f'http://127.0.0.1:{server.server_port}',
                   ZENKAIPOWER_ACCOUNT_FILE=temporary+'/account', ZENKAIPOWER_SETTINGS=temporary+'/settings',
                   ZENKAIPOWER_SAVES=temporary+'/saves')
        child = subprocess.Popen([str(game)], cwd=game.parent, env=env,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        lines = []
        def read():
            for line in child.stderr:
                lines.append(line)
        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        try:
            deadline = time.monotonic() + 45
            while time.monotonic() < deadline and child.poll() is None:
                if any(path == '/v1/join' for path, _ in requests) or any('Build compatibility check failed' in line for line in lines):
                    break
                time.sleep(.05)
        finally:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill(); child.wait()
            reader.join(timeout=2)
            server.shutdown(); server.server_close()
    paths = [path for path, _ in requests]
    assert paths and paths[0] == '/v1/data-check', (paths, ''.join(lines))
    assert len(requests[0][1].get('data', '')) == 64
    if accepted:
        assert '/v1/join' in paths, (paths, ''.join(lines))
        join = next(body for path, body in requests if path == '/v1/join')
        assert join['data'] == requests[0][1]['data']
    else:
        assert '/v1/join' not in paths, paths
        assert any('Build compatibility check failed' in line for line in lines), ''.join(lines)
    print('PASS:', 'approved signature checked before join' if accepted else 'rejected signature blocks join with accurate error')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    args = parser.parse_args()
    check(args.game.resolve(), False)
    check(args.game.resolve(), True)
