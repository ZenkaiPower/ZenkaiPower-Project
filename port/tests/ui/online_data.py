#!/usr/bin/env python3
"""Online policy tests; --scan checks binary changes and required logical file presence."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT / 'server/src'), str(ROOT / 'port/tools')]
from lan_server import Matchmaking, ApiError, DATA_POLICY
from make_online_signature import policy, build_signature
import make_online_signature


class PolicyTests(unittest.TestCase):
    def test_generated_manifest(self):
        self.assertEqual(policy(), DATA_POLICY)
        self.assertEqual(DATA_POLICY['scheme'], 'zenkaipower-binaries-v2')
        self.assertIsInstance(DATA_POLICY['signatures'], dict)
        self.assertNotIn('signature_metadata', DATA_POLICY)

    def test_policy_preserves_metadata_and_upgrades_old_approvals(self):
        approved = 'a' * 64
        details = {'version': '1.0.0', 'platform': 'windows-x64'}
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / 'policy.json'
            with patch.object(make_online_signature, 'DESTINATION', destination):
                destination.write_text(json.dumps({'scheme': 'zenkaipower-binaries-v2',
                    'signatures': [approved], 'signature_metadata': {approved: details}}))
                converted = policy()
                self.assertEqual(converted['signatures'], {approved: details})
                self.assertNotIn('signature_metadata', converted)
                destination.write_text(json.dumps(converted))
                self.assertEqual(policy(), converted)
                self.assertEqual(policy(reset=True)['signatures'], {})
                with patch.object(make_online_signature, 'build_signature', return_value='b' * 64), \
                     patch.object(make_online_signature, 'build_metadata', return_value=details):
                    self.assertEqual(policy(['fixture'])['signatures'],
                                     {approved: details, 'b' * 64: details})

    def test_server_enforcement(self):
        approved = 'a' * 64
        with tempfile.TemporaryDirectory() as temporary, patch.dict(DATA_POLICY, signatures={approved: {'version': '1.0.0', 'platform': 'windows-x64'}}):
            server = Matchmaking(Path(temporary) / 'server.db', require_accounts=False)
            check = server.request('/v1/data-check', {'data': approved}, '127.0.0.1')
            self.assertEqual(check['expected'], approved)
            self.assertEqual(server.request('/v1/data-check', {}, '127.0.0.1')['expected'], '')
            for action in ('host', 'join', 'quick'):
                for signature in (None, '', '0' * 64, approved.upper()):
                    with self.assertRaises(ApiError) as refused:
                        server.request('/v1/' + action, {'data': signature}, '127.0.0.1')
                    self.assertEqual(str(refused.exception), 'game data')
                    self.assertEqual(refused.exception.status, 403)
            room = server.request('/v1/host', {'v': 1, 'name': 'Goku', 'addrs': ['127.0.0.1:3456'],
                                  'data': approved}, '127.0.0.1')
            self.assertIn('code', room)


def scan_tests():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        binary = root / 'scanner'
        subprocess.run(['cc', '-O2', '-ffunction-sections', '-fdata-sections', '-Iport/src',
                        'port/tests/ui/online_data.c', 'port/src/plat_verify.c', 'port/src/gs/net_sign.c',
                        '-Wl,--gc-sections', '-pthread', '-o', str(binary)], cwd=ROOT, check=True)
        executable = root / 'game'
        executable.write_bytes(b'fixture game executable')
        Path(str(executable) + '.mem').write_bytes(b'fixture memory layout')
        def scan(data):
            env = dict(os.environ)
            env.pop('ZENKAIPOWER_FILE_ALIAS', None)
            return subprocess.run([str(binary), str(data), str(executable)], env=env, capture_output=True, text=True)
        original = scan(ROOT / 'gamedata')
        assert original.returncode == 0, original.stderr
        assert original.stdout.strip() == build_signature(executable), original.stderr
        modified = root / 'data'
        modified.mkdir()
        for source in (ROOT / 'gamedata').iterdir():
            if source.name != 'mods':
                (modified / source.name).symlink_to(source, target_is_directory=source.is_dir())
        with (ROOT / 'gamedata/pzs3us0.afs').open('rb') as archive:
            archive.seek(8)
            offset, size = struct.unpack('<II', archive.read(8))
            archive.seek(offset)
            blob = bytearray(archive.read(size))
        blob[-1] ^= 1
        override = modified / 'mods/pzs3us0/00000.bin'
        override.parent.mkdir(parents=True)
        override.write_bytes(blob)
        changed = scan(modified)
        assert changed.returncode == 0, changed.stderr
        assert changed.stdout.strip() == original.stdout.strip()  # Content no longer checked.
        executable.write_bytes(b'changed game executable')
        changed_binary = scan(modified)
        assert changed_binary.stdout.strip() == build_signature(executable)
        assert changed_binary.stdout.strip() != original.stdout.strip()
        Path(str(executable) + '.mem').write_bytes(b'changed memory layout')
        changed_memory = scan(modified)
        assert changed_memory.stdout.strip() == build_signature(executable)
        assert changed_memory.stdout.strip() != changed_binary.stdout.strip()
        bundled = executable.parent / 'lib/libSDL3.so.0'
        bundled.parent.mkdir()
        bundled.write_bytes(b'fixture bundled SDL library')
        changed_library = scan(modified)
        assert changed_library.stdout.strip() == build_signature(executable)
        assert changed_library.stdout.strip() != changed_memory.stdout.strip()
        # Re-scan sees changes despite a copied .verified cache and equal file size.
        missing = root / 'missing'
        missing.mkdir()
        assert scan(missing).returncode == 1
        print('Binary and companion changes detected; data-content changes ignored; missing files rejected')


if __name__ == '__main__':
    scan = '--scan' in sys.argv
    if scan:
        sys.argv.remove('--scan')
        scan_tests()
    unittest.main()
