"""Account hashing admission, concurrency and revalidation against real SQLite."""
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from lan_server import ApiError, Matchmaking


class AccountWork(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.service = Matchmaking(Path(self.temporary.name) / 'accounts.db')

    def call(self, action, ip='127.0.0.1', **body):
        return self.service.request('/v1/account/' + action, body, ip)

    def test_register_login_and_confirmation(self):
        user = self.call('register', username='FreshUser')
        logged = self.call('login', username='FreshUser', code=user['code'].lower())
        self.assertEqual(logged['user_uid'], user['user_uid'])
        prepared = self.call('prepare', username='TicketUser')
        confirmed = self.call('confirm', ticket=prepared['ticket'])
        self.assertEqual(confirmed['username'], 'ticketuser')
        with self.assertRaises(ApiError) as error:
            self.call('login', username='FreshUser', code='000000', computed='0' * 128)
        self.assertEqual(error.exception.status, 403)

    def test_player_game_version_is_persisted(self):
        user = self.call('register', username='VersionUser', rel=129)
        def version():
            with closing(self.service.connect()) as db:
                return db.execute('SELECT game_version FROM accounts WHERE uid=?', (user['user_uid'],)).fetchone()[0]
        self.assertEqual(version(), 129)
        self.call('session', account_token=user['token'], rel=130)
        self.assertEqual(version(), 130)
        self.call('session', account_token=user['token'])
        self.assertEqual(version(), 130)
        with self.assertRaises(ApiError):
            self.call('session', account_token=user['token'], rel=True)
        self.assertEqual(version(), 130)
        self.call('login', username='VersionUser', code=user['code'], rel=128)
        self.assertEqual(version(), 128)
        reopened = Matchmaking(self.service.database)
        with closing(reopened.connect()) as db:
            self.assertEqual(db.execute('SELECT game_version FROM accounts WHERE uid=?', (user['user_uid'],)).fetchone()[0], 128)

    def test_hashing_does_not_hold_write_transaction(self):
        user = self.call('register', username='FreshUser')
        entered, release = threading.Event(), threading.Event()
        original = self.service.code_hash

        def blocked(code, salt):
            entered.set()
            if not release.wait(5):
                raise RuntimeError('test hash was never released')
            return original(code, salt)

        with patch.object(self.service, 'code_hash', side_effect=blocked), ThreadPoolExecutor(max_workers=2) as pool:
            login = pool.submit(self.call, 'login', username='FreshUser', code=user['code'])
            try:
                self.assertTrue(entered.wait(2))
                hello = pool.submit(self.service.request, '/v1/hello', {}, '127.0.0.1')
                self.assertTrue(hello.result(timeout=2)['accounts'])
                # Administrative account changes can commit while hashing runs.
                with closing(self.service.connect()) as db, db:
                    db.execute('UPDATE accounts SET active=0 WHERE uid=?', (user['user_uid'],))
            finally:
                release.set()
            with self.assertRaises(ApiError) as error:
                login.result(timeout=2)
            self.assertEqual(error.exception.status, 403)
        self.assertEqual(self.service.hash_inflight, 0)

    def test_global_budget_counts_failures_before_hashing(self):
        with patch.object(self.service, 'code_hash', return_value='0' * 128) as hasher, patch('lan_server.time.monotonic', return_value=1000):
            for i in range(30):
                with self.assertRaises(ApiError) as error:
                    self.call('login', ip='192.0.2.' + str(i), username='MissingUser' + str(i), code='ABCDEF')
                self.assertEqual(error.exception.status, 403)
            with self.assertRaises(ApiError) as error:
                self.call('login', ip='203.0.113.1', username='AnotherMissing', code='ABCDEF')
            self.assertEqual(error.exception.status, 429)
            self.assertEqual(hasher.call_count, 30)
        self.assertEqual(self.service.hash_inflight, 0)

    def test_per_ip_budget_and_inflight_limit(self):
        with patch('lan_server.time.monotonic', return_value=1000):
            self.service.reserve_account_hash('192.0.2.1')
            self.service.reserve_account_hash('192.0.2.2')
            with self.assertRaises(ApiError) as error:
                self.service.reserve_account_hash('192.0.2.3')
            self.assertEqual(error.exception.status, 429)
            self.service.hash_inflight = 0
            for _ in range(9):
                self.service.reserve_account_hash('192.0.2.1')
                self.service.hash_inflight -= 1
            with self.assertRaises(ApiError):
                self.service.reserve_account_hash('192.0.2.1')


if __name__ == '__main__':
    unittest.main()
