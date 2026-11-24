"""Regression tests against real SQLite transactions and the public LAN API."""
from contextlib import closing
from pathlib import Path
import tempfile
import sqlite3
import time
import unittest

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from lan_server import ApiError, DATA_POLICY, Matchmaking, digest


class ResultSecurity(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.path = Path(self.temporary.name) / 'test.sqlite3'
        self.service = Matchmaking(self.path)
        self.now = time.time()
        with closing(self.service.connect()) as db, db:
            for uid in (1, 2, 3):
                db.execute('INSERT INTO accounts(uid,display_name,username,code_salt,code_hash,active,created_datetime) VALUES(?,?,?,?,?,?,?)',
                           (uid, f'User{uid}', f'user{uid}', '00' * 16, 'unused', 1, round(self.now * 1000)))
                db.execute('INSERT INTO account_tokens VALUES(?,?,?)',
                           (digest(str(uid) * 64), uid, round((self.now + 86400) * 1000)))
            # Existing balances; historical results are not rewritten by the fix.
            db.execute('INSERT INTO battles(uid,start_datetime,end_datetime,player1,player2,player1_uid,player2_uid,battle_type,ranked,winner,p1_points_won,p2_points_won) VALUES(1,1,2,?,?,?,?,1,1,1,500,500)',
                       ('user1', 'user2', 1, 2))
            db.execute('INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type,ranked) VALUES(2,?,?,?,?,?,1,1)',
                       (round(self.now * 1000), 'user1', 'user2', 1, 2))
            db.execute('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(2,?,?,?,?,?)',
                       ('00000000-0000-4000-8000-000000000002', digest('host'), digest('join'), self.now, 'started'))
            self.service.freeze_ranked(db, 2, self.now)

    def request(self, side=1, action='end', **changes):
        body = {'uid': '00000000-0000-4000-8000-000000000002', 'key': 'host' if side == 1 else 'join',
                'account_token': str(side) * 64, 'winner': 'p1', 'finish_type': 'ok', 'battle_duration': 45,
                'p1_points_won': 100, 'p2_points_won': -60}
        body.update(changes)
        return self.service.request('/v1/battle/' + action, body, '127.0.0.1')

    def assert_pending(self, status='pending'):
        result = self.request(action='get')
        self.assertEqual(result['battle']['result_status'], status)
        self.assertIsNone(result['battle']['winner'])
        self.assertEqual((result['p1_rank_points'], result['p2_rank_points']), (500, 500))

    def test_battle_versions_are_snapshots(self):
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE accounts SET game_version=129 WHERE uid=1')
            db.execute('UPDATE accounts SET game_version=130 WHERE uid=2')
            db.execute("INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type) VALUES(3,1,'user1','user2',1,2,1)")
            db.execute("INSERT INTO battles(uid,start_datetime,player1,player2,battle_type) VALUES(4,1,'user1','user2',1)")
            db.execute('UPDATE battles SET player1_uid=1,player2_uid=2 WHERE uid=4')
            db.execute('UPDATE accounts SET game_version=131')
            # Retried starts reassign the same owners and must preserve snapshots.
            db.execute('UPDATE battles SET player1_uid=1,player2_uid=2 WHERE uid IN (3,4)')
            self.assertEqual([tuple(row) for row in db.execute('SELECT p1_game_version,p2_game_version FROM battles WHERE uid IN (3,4) ORDER BY uid')], [(129,130), (129,130)])
            # Historical battles remain unknown rather than being backfilled.
            self.assertEqual(tuple(db.execute('SELECT p1_game_version,p2_game_version FROM battles WHERE uid=1').fetchone()), (None,None))
        Matchmaking(self.path)
        with closing(self.service.connect()) as db:
            self.assertEqual(tuple(db.execute('SELECT p1_game_version,p2_game_version FROM battles WHERE uid=3').fetchone()), (129,130))

    def test_battle_classic_modes_are_snapshots(self):
        self.request(action='save', classic_mode=True)
        self.request(side=2, action='save', classic_mode=False)
        result = self.request(action='get')['battle']
        self.assertEqual((result['p1_classic_mode'], result['p2_classic_mode']), (1, 0))
        # Retries, or a later loaded save, cannot overwrite the recorded battle.
        self.request(action='save', classic_mode=False)
        self.request(side=2, action='save', classic_mode=True)
        result = self.request(action='get')['battle']
        self.assertEqual((result['p1_classic_mode'], result['p2_classic_mode']), (1, 0))
        for invalid in (0, 1, 'true', [], {}):
            with self.assertRaises(ApiError):
                self.request(action='save', classic_mode=invalid)
        with self.assertRaises(ApiError):
            self.request(action='save', classic_mode=False, account_token='2' * 64)
        with closing(self.service.connect()) as db, db:
            self.assertEqual(tuple(db.execute('SELECT p1_classic_mode,p2_classic_mode FROM battles WHERE uid=1').fetchone()), (None, None))
            db.execute("INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type) VALUES(3,1,'user1','user2',1,2,1)")
            db.execute("INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(3,?,?,?,?,?)",
                       ('00000000-0000-4000-8000-000000000003', digest('host'), digest('join'), self.now, 'classic-snapshot'))
        wire = '00000000-0000-4000-8000-000000000003'
        self.request(action='save', uid=wire, classic_mode=False)
        self.request(side=2, action='save', uid=wire, classic_mode=True)
        result = self.request(action='get', uid=wire)['battle']
        self.assertEqual((result['p1_classic_mode'], result['p2_classic_mode']), (0, 1))
        Matchmaking(self.path)
        result = self.request(action='get')['battle']
        self.assertEqual((result['p1_classic_mode'], result['p2_classic_mode']), (1, 0))

    def test_classic_mode_d1_migrations_match_lan(self):
        def triggers(db):
            return {row[0]: ' '.join(row[1].split()).rstrip(';').lower()
                    for row in db.execute("SELECT name,sql FROM sqlite_master WHERE type='trigger' AND name LIKE 'battle_classic_modes_%'")}

        with sqlite3.connect(':memory:') as migrated, closing(self.service.connect()) as lan:
            for path in sorted((Path(__file__).resolve().parents[1] / 'migrations').glob('*.sql')):
                migrated.executescript(path.read_text())
            self.assertEqual(triggers(migrated), triggers(lan))
            self.assertEqual(triggers(migrated), {})
            self.assertNotIn('classic_mode', {row[1] for row in migrated.execute('PRAGMA table_info(accounts)')})
            columns = {row[1] for row in migrated.execute('PRAGMA table_info(battles)')}
            self.assertTrue({'p1_classic_mode', 'p2_classic_mode'} <= columns)
            self.assertTrue({'battle_nature', 'p1_sparking_changes', 'p2_sparking_changes'} <= columns)

    def test_battle_nature_all_combinations_and_report_orders(self):
        for host in (False, True):
            for join in (False, True):
                for order in ((1, 2), (2, 1)):
                    uid = 10 + int(host) * 4 + int(join) * 2 + order[0]
                    wire = f'00000000-0000-4000-8000-{uid:012d}'
                    with self.subTest(host=host, join=join, order=order):
                        with closing(self.service.connect()) as db, db:
                            db.execute("INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type) VALUES(?,1,'user1','user2',1,2,1)", (uid,))
                            db.execute('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(?,?,?,?,?,?)',
                                       (uid, wire, digest('host'), digest('join'), self.now, wire))
                        self.assertEqual(self.request(action='get', uid=wire)['battle']['battle_nature'], 0)
                        for side in order:
                            self.request(side, action='save', uid=wire, sparking_changes=host if side == 1 else join)
                        self.assertEqual(self.request(action='get', uid=wire)['battle']['battle_nature'], int(host and join))
        self.service = Matchmaking(self.path)
        self.assertEqual(self.request(action='get', uid=wire)['battle']['battle_nature'], 1)

    def test_battle_nature_requires_both_sparking_saves(self):
        self.assertEqual(self.request(action='get')['battle']['battle_nature'], 0)
        self.request(action='save', sparking_changes=True)
        self.assertEqual(self.request(action='get')['battle']['battle_nature'], 0)
        self.request(side=2, action='save', sparking_changes=True)
        self.assertEqual(self.request(action='get')['battle']['battle_nature'], 1)
        self.request(side=2, action='save', sparking_changes=True)  # idempotent
        with self.assertRaises(ApiError):
            self.request(side=2, action='save', sparking_changes=False)

    def test_battle_nature_mixed_saves_and_validation(self):
        self.request(side=2, action='save', sparking_changes=False)
        self.request(action='save', sparking_changes=True)
        self.assertEqual(self.request(action='get')['battle']['battle_nature'], 0)
        for invalid in (0, 1, 'true', [], {}):
            with self.assertRaises(ApiError):
                self.request(action='save', sparking_changes=invalid)
        with self.assertRaises(ApiError):
            self.request(action='save', sparking_changes=True, account_token='2' * 64)
        with closing(self.service.connect()) as db:
            self.assertEqual(db.execute('SELECT battle_nature FROM battles WHERE uid=1').fetchone()[0], 0)

    def test_host_alone_cannot_award_points(self):
        self.request()
        self.request()  # A retry is not a second vote.
        self.assert_pending()

    def test_performance_rewards_are_not_replaced_with_fixed_rewards(self):
        self.request(1, p1_points_won=240, p2_points_won=-90)
        result = self.request(2, p1_points_won=240, p2_points_won=-90)
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (240, -90))

    def test_point_disagreement_cannot_settle(self):
        self.request(1, p1_points_won=240)
        self.request(2, p1_points_won=241)
        self.assert_pending('disputed')

    def test_invalid_base_points_are_not_stored(self):
        for value in (None, True, 1.5, '100', 2**31, -(2**31)-1):
            with self.assertRaises(ApiError):
                self.request(p1_points_won=value)
        with closing(self.service.connect()) as db:
            self.assertEqual(db.execute('SELECT count(*) FROM battle_result_reports').fetchone()[0], 0)

    def test_performance_scaling_applies_once(self):
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE rank_battle_scoring SET p1_start=3000,p2_start=2000,tolerance=250 WHERE battle_uid=2')
        self.request(1, p1_points_won=210, p2_points_won=-77)
        for side in (2, 1, 2):
            result = self.request(side, p1_points_won=210, p2_points_won=-77)
            self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (120, -44))

    def test_matching_reports_use_performance_rewards_once_in_either_order(self):
        self.request(2)
        result = self.request(1, battle_duration=46)
        self.assertEqual(result['battle']['result_status'], 'confirmed')
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (100, -60))
        self.assertEqual((result['p1_rank_points'], result['p2_rank_points']), (600, 440))
        for side in (1, 2):
            retry = self.request(side, battle_duration=50)
            self.assertEqual((retry['p1_rank_points'], retry['p2_rank_points']), (600, 440))

    def test_disputes_and_changed_reports_cannot_settle(self):
        self.request()
        result = self.request(2, winner='p2')
        self.assertEqual(result['result_status'], 'disputed')
        self.assert_pending('disputed')
        with self.assertRaises(ApiError) as error:
            self.request(2, winner='p1')
        self.assertEqual(error.exception.status, 409)
        self.assert_pending('disputed')

    def test_tie_awards_zero(self):
        self.request(1, winner='tie')
        result = self.request(2, winner='tie')
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (0, 0))

    def test_opponent_winner_and_finish_disagreement(self):
        self.request(1, winner='p2', finish_type='perfect')
        self.request(2, winner='p2', finish_type='ok')
        self.assert_pending('disputed')

    def test_valid_account_cannot_use_opponents_key(self):
        self.request()
        for changes in ({'key': 'host'}, {'account_token': '3' * 64}, {'key': 'bogus'}):
            with self.assertRaises(ApiError) as error:
                self.request(2, **changes)
            self.assertEqual(error.exception.status, 403)
        self.assert_pending()

    def test_inactive_opponent_blocks_settlement(self):
        self.request()
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE accounts SET active=0 WHERE uid=2')
        with self.assertRaises(ApiError):
            self.request(1)
        self.assert_pending()

    def test_reports_survive_restart(self):
        self.request()
        self.service = Matchmaking(self.path)
        result = self.request(2)
        self.assertEqual(result['battle']['result_status'], 'confirmed')

    def test_rank_gap_scaling_and_caps_remain_server_controlled(self):
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE rank_battle_scoring SET p1_start=3000,p2_start=2000,tolerance=250 WHERE battle_uid=2')
        self.request()
        result = self.request(2)
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (57, -34))

    def test_unranked_remains_host_only_and_awards_zero(self):
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE battles SET ranked=0 WHERE uid=2')
        with self.assertRaises(ApiError) as error:
            self.request(2)
        self.assertEqual(error.exception.status, 403)
        result = self.request()
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (0, 0))

    def test_balance_caps(self):
        with closing(self.service.connect()) as db, db:
            db.execute('UPDATE rank_battle_scoring SET p1_start=9999,p2_start=0 WHERE battle_uid=2')
        self.request()
        result = self.request(2)
        self.assertEqual((result['battle']['p1_points_won'], result['battle']['p2_points_won']), (0, 0))

    def test_compatibility_hash_is_not_authorization(self):
        signature = next(iter(DATA_POLICY['signatures']))
        check = self.service.request('/v1/data-check', {'data': signature}, '127.0.0.1')
        self.assertEqual(check['expected'], signature)
        self.assertFalse(check['build_attestation'])
        for action in ('host', 'quick'):
            with self.assertRaises(ApiError) as error:
                self.service.request('/v1/' + action, {'data': signature, 'queue': 'RS'}, '127.0.0.1')
            self.assertEqual(error.exception.status, 403)
        with self.assertRaises(ApiError):
            self.request(account_token='', data=signature)

    def test_bad_telemetry_is_not_stored(self):
        for value in (float('nan'), float('inf'), -1, 86401, True):
            with self.assertRaises(ApiError):
                self.request(battle_duration=value)
        with closing(self.service.connect()) as db:
            self.assertEqual(db.execute('SELECT count(*) FROM battle_result_reports').fetchone()[0], 0)


if __name__ == '__main__':
    unittest.main()
