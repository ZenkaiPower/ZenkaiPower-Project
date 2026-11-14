#!/usr/bin/env python3
"""LAN matchmaking compatible with worker.js, plus persistent battle records.

Python 3.10+, standard library only. Run --help for address/database options.
"""

import argparse
from contextlib import closing
from datetime import datetime
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ipaddress
import json
import math
from pathlib import Path
import re
import secrets
import sqlite3
import threading
import time
import uuid
from rank_scoring import population_tolerance, adjusted_points
from zoneinfo import ZoneInfo


QUEUES = ("S", "T", "D10", "D15", "D20", "RS")
ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"
CHARACTERS = tuple(f"p{p}c{c}" for p in (1, 2) for c in range(1, 6))
LONDON = ZoneInfo("Europe/London")
BATTLE_TYPES = {"unknown": 0, "S": 1, "T": 2, "D10": 3, "D15": 4, "D20": 5}
WINNERS = {"tie": 0, "p1": 1, "p2": 2}
FINISH_TYPES = {"ok": 0, "perfect": 1, "timeout": 2, "ringout": 3}
SCHEMA = """
CREATE TABLE IF NOT EXISTS accounts (
    uid INTEGER PRIMARY KEY AUTOINCREMENT,
    display_name TEXT NOT NULL, username TEXT NOT NULL UNIQUE,
    code_salt TEXT NOT NULL, code_hash TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    created_datetime INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS account_registrations (
    ticket_hash TEXT PRIMARY KEY,
    display_name TEXT NOT NULL, username TEXT NOT NULL,
    code_salt TEXT NOT NULL, code_hash TEXT NOT NULL,
    expires_datetime INTEGER NOT NULL,
    user_uid INTEGER REFERENCES accounts(uid)
);
CREATE TABLE IF NOT EXISTS account_tokens (
    token_hash TEXT PRIMARY KEY, user_uid INTEGER NOT NULL REFERENCES accounts(uid),
    expires_datetime INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS account_tokens_user ON account_tokens(user_uid);
CREATE INDEX IF NOT EXISTS account_tokens_expiry ON account_tokens(expires_datetime);
CREATE TABLE IF NOT EXISTS account_rooms (
    code TEXT PRIMARY KEY REFERENCES rooms(code) ON DELETE CASCADE,
    host_uid INTEGER NOT NULL REFERENCES accounts(uid),
    join_uid INTEGER REFERENCES accounts(uid)
);
CREATE TABLE IF NOT EXISTS rooms (
    code TEXT PRIMARY KEY, made INTEGER NOT NULL, version INTEGER NOT NULL,
    ip TEXT NOT NULL, host_key TEXT NOT NULL, join_key TEXT,
    host_name TEXT NOT NULL, join_name TEXT,
    host_addrs TEXT NOT NULL, join_addrs TEXT, queue TEXT, who TEXT
);
CREATE INDEX IF NOT EXISTS rooms_queue ON rooms(queue, version, made);
CREATE TABLE IF NOT EXISTS character_items (
    uid INTEGER PRIMARY KEY AUTOINCREMENT,
    item1 INTEGER CHECK(item1 BETWEEN 1 AND 65535),
    item2 INTEGER CHECK(item2 BETWEEN 1 AND 65535),
    item3 INTEGER CHECK(item3 BETWEEN 1 AND 65535),
    item4 INTEGER CHECK(item4 BETWEEN 1 AND 65535),
    item5 INTEGER CHECK(item5 BETWEEN 1 AND 65535),
    item6 INTEGER CHECK(item6 BETWEEN 1 AND 65535),
    item7 INTEGER CHECK(item7 BETWEEN 1 AND 65535)
);
CREATE TABLE IF NOT EXISTS battles (
    uid INTEGER PRIMARY KEY AUTOINCREMENT,
    start_datetime INTEGER NOT NULL, end_datetime INTEGER,
    player1 TEXT NOT NULL, player2 TEXT NOT NULL,
    player1_uid INTEGER REFERENCES accounts(uid), player2_uid INTEGER REFERENCES accounts(uid), battle_type INTEGER NOT NULL CHECK(battle_type IN (0,1,2,3,4,5)),
    ranked INTEGER NOT NULL DEFAULT 0 CHECK(ranked IN (0, 1)),
    interrupted INTEGER NOT NULL DEFAULT 0 CHECK(interrupted IN (0, 1)),
    winner INTEGER CHECK(winner IN (0,1,2)),
    finish_type INTEGER CHECK(finish_type IN (0,1,2,3)),
    p1_points_won INTEGER, p2_points_won INTEGER,
    battle_duration REAL CHECK(battle_duration >= 0),
    p1c1_costume INTEGER CHECK(p1c1_costume BETWEEN 0 AND 65535),
    p1c2_costume INTEGER CHECK(p1c2_costume BETWEEN 0 AND 65535),
    p1c3_costume INTEGER CHECK(p1c3_costume BETWEEN 0 AND 65535),
    p1c4_costume INTEGER CHECK(p1c4_costume BETWEEN 0 AND 65535),
    p1c5_costume INTEGER CHECK(p1c5_costume BETWEEN 0 AND 65535),
    p2c1_costume INTEGER CHECK(p2c1_costume BETWEEN 0 AND 65535),
    p2c2_costume INTEGER CHECK(p2c2_costume BETWEEN 0 AND 65535),
    p2c3_costume INTEGER CHECK(p2c3_costume BETWEEN 0 AND 65535),
    p2c4_costume INTEGER CHECK(p2c4_costume BETWEEN 0 AND 65535),
    p2c5_costume INTEGER CHECK(p2c5_costume BETWEEN 0 AND 65535),
    stage INTEGER CHECK(stage BETWEEN 0 AND 65535),
    music INTEGER CHECK(music BETWEEN -1 AND 65535),
    stage_music_picker INTEGER CHECK(stage_music_picker IN (1,2)),
    p1c1 INTEGER, p1c2 INTEGER, p1c3 INTEGER, p1c4 INTEGER, p1c5 INTEGER,
    p2c1 INTEGER, p2c2 INTEGER, p2c3 INTEGER, p2c4 INTEGER, p2c5 INTEGER,
    p1c1_items_uid INTEGER REFERENCES character_items(uid),
    p1c2_items_uid INTEGER REFERENCES character_items(uid),
    p1c3_items_uid INTEGER REFERENCES character_items(uid),
    p1c4_items_uid INTEGER REFERENCES character_items(uid),
    p1c5_items_uid INTEGER REFERENCES character_items(uid),
    p2c1_items_uid INTEGER REFERENCES character_items(uid),
    p2c2_items_uid INTEGER REFERENCES character_items(uid),
    p2c3_items_uid INTEGER REFERENCES character_items(uid),
    p2c4_items_uid INTEGER REFERENCES character_items(uid),
    p2c5_items_uid INTEGER REFERENCES character_items(uid)
);
CREATE TABLE IF NOT EXISTS battle_sessions (
    uid INTEGER PRIMARY KEY REFERENCES battles(uid),
    wire_uid TEXT NOT NULL UNIQUE,
    host_hash TEXT NOT NULL, join_hash TEXT NOT NULL,
    started REAL NOT NULL, request_id TEXT NOT NULL,
    p1_connection TEXT, p2_connection TEXT,
    UNIQUE(host_hash, request_id)
);
CREATE TABLE IF NOT EXISTS rank_daily_policy (
    day INTEGER PRIMARY KEY, tolerance INTEGER NOT NULL, population INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS rank_battle_scoring (
    battle_uid INTEGER PRIMARY KEY REFERENCES battles(uid) ON DELETE CASCADE,
    p1_start INTEGER NOT NULL, p2_start INTEGER NOT NULL,
    tolerance INTEGER NOT NULL, policy_day INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS battle_result_reports (
    battle_uid INTEGER NOT NULL REFERENCES battles(uid) ON DELETE CASCADE,
    side INTEGER NOT NULL CHECK(side IN (1,2)),
    result TEXT NOT NULL,
    PRIMARY KEY(battle_uid,side)
);
"""


class ApiError(Exception):
    def __init__(self, message, status=400):
        super().__init__(message)
        self.status = status


def integer(value):
    return type(value) is int


def digest(key):
    return hashlib.sha256(key.encode()).hexdigest()


def london_timestamp(epoch):
    return datetime.fromtimestamp(epoch, LONDON).isoformat(timespec="milliseconds")


def clean_name(value):
    name = str(value if value is not None else "Player")
    return "".join(c for c in name if " " <= c <= "~")[:16] or "Player"


def clean_addrs(value):
    if not isinstance(value, list):
        raise ApiError("bad request")
    result = []
    for address in value[:5]:
        if not isinstance(address, str):
            continue
        raw = address.removeprefix("relay=")
        try:
            host, port = raw.rsplit(":", 1)
            ipaddress.ip_address(host.strip("[]"))
            if port.isdecimal() and 1 <= int(port) <= 65535:
                result.append(address)
        except ValueError:
            continue
    if not result:
        raise ApiError("bad request")
    return result


DATA_POLICY = json.loads((Path(__file__).resolve().parents[1] / "data/game_data_signature.json").read_text())

class Matchmaking:
    def __init__(self, database, require_accounts=True):
        self.database = str(database)
        self.require_accounts = require_accounts
        self.account_attempts = {}
        self.account_limit_lock = threading.Lock()
        self.hash_attempts = {}
        self.hash_inflight = 0
        with closing(self.connect()) as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.executescript(SCHEMA)
            db.execute("PRAGMA foreign_keys=OFF")
            with db:
                db.execute("BEGIN IMMEDIATE")
                account_columns = {row["name"] for row in db.execute("PRAGMA table_info(accounts)")}
                if "username_key" in account_columns:
                    db.execute("ALTER TABLE accounts RENAME COLUMN username TO display_name")
                    db.execute("ALTER TABLE accounts RENAME COLUMN username_key TO username")
                columns = {row["name"] for row in db.execute("PRAGMA table_info(battles)")}
                for field in ("player1_uid", "player2_uid"):
                    if field not in columns:
                        db.execute(f"ALTER TABLE battles ADD COLUMN {field} INTEGER REFERENCES accounts(uid)")
                if "finish_type" not in columns:
                    db.execute("ALTER TABLE battles ADD COLUMN finish_type TEXT "
                               "CHECK(finish_type IN ('ok', 'perfect', 'timeout'))")
                if "ranked" not in columns:
                    db.execute("ALTER TABLE battles ADD COLUMN ranked INTEGER NOT NULL DEFAULT 0 "
                               "CHECK(ranked IN (0, 1))")
                if "interrupted" not in columns:
                    db.execute("ALTER TABLE battles ADD COLUMN interrupted INTEGER NOT NULL DEFAULT 0 "
                               "CHECK(interrupted IN (0, 1))")
                battle_sql = db.execute("SELECT sql FROM sqlite_master WHERE name='battles'").fetchone()[0]
                if next(row["type"] for row in db.execute("PRAGMA table_info(battles)") if row["name"] == "uid") != "INTEGER" or "finish_type IN (0,1,2,3)" not in battle_sql:
                    self.migrate_numeric(db)
                metadata_columns = {row["name"] for row in db.execute("PRAGMA table_info(battles)")}
                for field, minimum in (("stage", 0), ("music", -1)):
                    if field not in metadata_columns:
                        db.execute(f"ALTER TABLE battles ADD COLUMN {field} INTEGER CHECK({field} BETWEEN {minimum} AND 65535)")
                if "stage_music_picker" not in metadata_columns:
                    db.execute("ALTER TABLE battles ADD COLUMN stage_music_picker INTEGER CHECK(stage_music_picker IN (1,2))")
                for slot in CHARACTERS:
                    field = slot + "_costume"
                    if field not in metadata_columns:
                        db.execute(f"ALTER TABLE battles ADD COLUMN {field} INTEGER CHECK({field} BETWEEN 0 AND 65535)")
                item_columns = {row["name"] for row in db.execute("PRAGMA table_info(battles)")}
                for slot in CHARACTERS:
                    field = slot + "_items_uid"
                    if field not in item_columns:
                        db.execute(f"ALTER TABLE battles ADD COLUMN {field} INTEGER REFERENCES character_items(uid)")
                session_columns = {row["name"] for row in db.execute("PRAGMA table_info(battle_sessions)")}
                for field in ("p1_connection", "p2_connection"):
                    if field not in session_columns:
                        db.execute(f"ALTER TABLE battle_sessions ADD COLUMN {field} TEXT")
                for side in (1, 2):
                    db.execute(f"UPDATE battles SET player{side}=(SELECT username FROM accounts "
                               f"WHERE uid=battles.player{side}_uid) WHERE player{side}_uid IS NOT NULL")
                db.execute("UPDATE battles SET p1_points_won=0,p2_points_won=0 "
                           "WHERE ranked=0 AND end_datetime IS NOT NULL AND interrupted=0 AND winner IS NOT NULL")
            db.execute("PRAGMA foreign_keys=ON")

    @staticmethod
    def migrate_numeric(db):
        """Rebuild both related tables in the caller's transaction, preserving records."""
        battles = [dict(row) for row in db.execute("SELECT * FROM battles ORDER BY rowid")]
        sessions = [dict(row) for row in db.execute("SELECT * FROM battle_sessions")]
        db.execute("DROP TABLE battle_sessions")
        db.execute("DROP TABLE battles")
        for statement in SCHEMA.split(";"):
            if statement.strip():
                db.execute(statement)
        identifiers = {}
        for row in battles:
            old_uid = row.pop("uid")
            for field in ("start_datetime", "end_datetime"):
                if isinstance(row[field], str):
                    row[field] = round(datetime.fromisoformat(row[field]).timestamp() * 1000)
            for field, codes in (("battle_type", BATTLE_TYPES), ("winner", WINNERS), ("finish_type", FINISH_TYPES)):
                if isinstance(row[field], str):
                    row[field] = codes[row[field]]
            if isinstance(old_uid, int):
                row["uid"] = old_uid
            columns = list(row)
            inserted = db.execute(f"INSERT INTO battles ({','.join(columns)}) VALUES ({','.join('?' for _ in columns)})",
                                  tuple(row[c] for c in columns))
            identifiers[old_uid] = inserted.lastrowid
        for row in sessions:
            db.execute("INSERT INTO battle_sessions (uid,wire_uid,host_hash,join_hash,started,request_id,p1_connection,p2_connection) "
                       "VALUES (?,?,?,?,?,?,?,?)", (identifiers[row["uid"]], row.get("wire_uid", row["uid"]), row["host_hash"],
                                               row["join_hash"], row["started"], row["request_id"],
                                               row.get("p1_connection"), row.get("p2_connection")))

    @staticmethod
    def battle_row(db, wire_uid):
        """Keep the existing wire protocol while the database uses numeric values."""
        row = db.execute("SELECT b.*, s.wire_uid, s.p1_connection, s.p2_connection FROM battles b JOIN battle_sessions s USING(uid) "
                         "WHERE s.wire_uid=?", (wire_uid,)).fetchone()
        if row is None:
            return None
        result = dict(row)
        result["db_uid"] = result["uid"]
        result["uid"] = result.pop("wire_uid")
        result["items"] = {}
        for slot in CHARACTERS:
            item_uid = result.get(slot + "_items_uid")
            if item_uid is not None:
                loadout = db.execute("SELECT item1,item2,item3,item4,item5,item6,item7 FROM character_items WHERE uid=?",
                                     (item_uid,)).fetchone()
                result["items"][slot] = list(loadout)
        for field in ("p1_connection", "p2_connection"):
            if result[field] is not None: result[field] = json.loads(result[field])
        for field in ("start_datetime", "end_datetime"):
            if result[field] is not None:
                result[field] = london_timestamp(result[field] / 1000)
        for field, codes in (("battle_type", BATTLE_TYPES), ("winner", WINNERS), ("finish_type", FINISH_TYPES)):
            if result[field] is not None:
                result[field] = next(name for name, code in codes.items() if code == result[field])
        if result["ranked"]:
            reports = [r[0] for r in db.execute("SELECT result FROM battle_result_reports WHERE battle_uid=? ORDER BY side", (result["db_uid"],))]
            result["result_status"] = ("confirmed" if result["winner"] is not None and not result["interrupted"] else
                                       "disputed" if len(reports) == 2 and reports[0] != reports[1] else "pending")
        return result

    @staticmethod
    def account_totals(db, wire_uid):
        battle = db.execute("SELECT battles.uid,player1_uid,player2_uid,ranked FROM battles "
                            "JOIN battle_sessions USING(uid) WHERE wire_uid=?", (wire_uid,)).fetchone()
        if battle is None:
            return {}
        totals = {"stats_ranked": battle["ranked"]}
        scoring = db.execute("SELECT * FROM rank_battle_scoring WHERE battle_uid=?", (battle["uid"],)).fetchone()
        if scoring:
            totals.update(rank_p1_start=scoring["p1_start"], rank_p2_start=scoring["p2_start"],
                          rank_tolerance=scoring["tolerance"])
        for side in (1, 2):
            uid = battle[f"player{side}_uid"]
            totals[f"p{side}_rank_points"] = Matchmaking.rank_points(db, uid)
            for ranked, label in ((0, "unranked"), (1, "ranked")):
                row = db.execute("SELECT COUNT(*) AS battles, "
                    "COALESCE(SUM((player1_uid=? AND winner=1) OR (player2_uid=? AND winner=2)),0) AS wins, "
                    "COALESCE(SUM((player1_uid=? AND winner=2) OR (player2_uid=? AND winner=1)),0) AS losses "
                    "FROM battles WHERE (player1_uid=? OR player2_uid=?) AND ranked=? "
                    "AND end_datetime IS NOT NULL AND interrupted=0 AND winner IS NOT NULL",
                    (uid, uid, uid, uid, uid, uid, ranked)).fetchone()
                for field in ("battles", "wins", "losses"):
                    totals[f"p{side}_{label}_{field}"] = row[field]
        return totals

    def connect(self):
        db = sqlite3.connect(self.database, timeout=10)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA foreign_keys=ON")
        return db

    def request(self, path, body, ip):
        # One write transaction keeps competing quick-match/join/result requests atomic.
        if path.startswith("/v1/account/"):
            self.limit_account_request(path, body, ip)
        prepared = None
        slow_account = path in ("/v1/account/login", "/v1/account/prepare", "/v1/account/register")
        if slow_account:
            self.reserve_account_hash(ip)
        try:
            if slow_account:
                # Hash outside the SQLite write transaction, allowing matchmaking
                # and other accounts to proceed while scrypt runs.
                action = path.rsplit('/', 1)[1]
                name = self.username(body.get('username'))
                with closing(self.connect()) as lookup:
                    user = lookup.execute('SELECT * FROM accounts WHERE username=?', (name.casefold(),)).fetchone()
                if action != 'login' and user:
                    raise ApiError('username already exists', 409)
                if action == 'login' and (not isinstance(body.get('code'), str) or not re.fullmatch(r'[A-Za-z0-9]{6}', body['code'])):
                    raise ApiError('code must contain six letters or numbers')
                code = body['code'].upper() if action == 'login' else ''.join(secrets.choice(ALPHABET) for _ in range(6))
                salt = (user['code_salt'] if user else '00' * 16) if action == 'login' else secrets.token_hex(16)
                prepared = {'code': code, 'salt': salt, 'computed': self.code_hash(code, salt),
                            'verifier': user['code_hash'] if user else None}
            with closing(self.connect()) as db, db:
                db.execute("BEGIN IMMEDIATE")
                now = time.time()
                db.execute("DELETE FROM rooms WHERE made < ?", (now - 600,))
                return self.dispatch(db, path, body, ip, now, prepared)
        finally:
            if slow_account:
                with self.account_limit_lock:
                    self.hash_inflight -= 1

    def reserve_account_hash(self, ip):
        now = time.monotonic()
        with self.account_limit_lock:
            self.hash_attempts = {k: v for k, v in self.hash_attempts.items() if v[0] > now - 60}
            if self.hash_inflight >= 2 or any(self.hash_attempts.get(k, (now, 0))[1] >= maximum
                                             for k, maximum in (('global', 30), ('ip:' + ip, 10))):
                raise ApiError('account service busy; try again later', 429)
            for key in ('global', 'ip:' + ip):
                began, count = self.hash_attempts.get(key, (now, 0))
                self.hash_attempts[key] = (began, count + 1)
            self.hash_inflight += 1

    @staticmethod
    def username(value):
        if (not isinstance(value, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{1,14}[A-Za-z0-9]", value)
                or sum("A" <= c <= "Z" or "a" <= c <= "z" for c in value) < 3
                or value.casefold() == "player"):
            raise ApiError("Invalid name, please chose another one.")
        return value

    def limit_account_request(self, path, body, ip):
        # Bounded per-process counters: requests are counted before their transaction,
        # including failures. Recovery codes have six characters, so throttle guessing.
        now = time.monotonic()
        login = path == "/v1/account/login"
        name = str(body.get("username", ""))[:16].casefold()
        keys = [("ip:" + ip, 30 if login else 120)]
        if login:
            keys.append(("name:" + name, 5))
        with self.account_limit_lock:
            self.account_attempts = {k: v for k, v in self.account_attempts.items() if v[0] > now - 900}
            if len(self.account_attempts) >= 10000 and any(k not in self.account_attempts for k, _ in keys):
                raise ApiError("too many account requests; try again later", 429)
            if any(self.account_attempts.get(k, (now, 0))[1] >= limit for k, limit in keys):
                raise ApiError("too many account requests; try again later", 429)
            for key, _ in keys:
                began, count = self.account_attempts.get(key, (now, 0))
                self.account_attempts[key] = (began, count + 1)

    @staticmethod
    def code_hash(code, salt):
        return hashlib.scrypt(code.encode(), salt=bytes.fromhex(salt), n=16384, r=8, p=1,
                              maxmem=64 * 1024 * 1024).hex()

    @staticmethod
    def account_token(db, user, now):
        token = secrets.token_hex(32)
        expires = round((now + 90 * 86400) * 1000)
        db.execute("DELETE FROM account_tokens WHERE expires_datetime <= ?", (round(now * 1000),))
        db.execute("INSERT INTO account_tokens VALUES (?,?,?)", (digest(token), user["uid"], expires))
        return {"user_uid": user["uid"], "username": user["username"], "display_name": user["display_name"], "token": token, "expires": expires}

    @staticmethod
    def account_auth(db, body, now):
        token = body.get("account_token")
        if not isinstance(token, str) or not re.fullmatch(r"[a-f0-9]{64}", token):
            raise ApiError("initialize your player data before matchmaking", 403)
        user = db.execute("SELECT a.* FROM accounts a JOIN account_tokens t ON t.user_uid=a.uid "
                          "WHERE t.token_hash=? AND t.expires_datetime>? AND a.active=1",
                          (digest(token), round(now * 1000))).fetchone()
        if user is None:
            raise ApiError("account login expired or inactive; initialize your data again", 403)
        return user

    @staticmethod
    def rank_policy(db, now):
        day = int(now // 86400)
        policy = db.execute("SELECT * FROM rank_daily_policy WHERE day=?", (day,)).fetchone()
        if policy is None:
            cutoff = round((now - 30 * 86400) * 1000)
            end = round(now * 1000)
            active = db.execute("SELECT uid FROM accounts WHERE active=1 AND uid IN ("
                "SELECT player1_uid FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL "
                "AND end_datetime BETWEEN ? AND ? UNION "
                "SELECT player2_uid FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL "
                "AND end_datetime BETWEEN ? AND ?)", (cutoff, end, cutoff, end)).fetchall()
            points = [Matchmaking.rank_points(db, row["uid"]) for row in active]
            db.execute("INSERT INTO rank_daily_policy VALUES (?,?,?)",
                       (day, population_tolerance(points), len(points)))
            policy = db.execute("SELECT * FROM rank_daily_policy WHERE day=?", (day,)).fetchone()
        return policy

    @staticmethod
    def freeze_ranked(db, battle_uid, now):
        row = db.execute("SELECT ranked,player1_uid,player2_uid FROM battles WHERE uid=?", (battle_uid,)).fetchone()
        if not row or not row["ranked"] or row["player1_uid"] is None or row["player2_uid"] is None:
            return
        if db.execute("SELECT 1 FROM rank_battle_scoring WHERE battle_uid=?", (battle_uid,)).fetchone():
            return
        policy = Matchmaking.rank_policy(db, now)
        db.execute("INSERT INTO rank_battle_scoring VALUES (?,?,?,?,?)", (battle_uid,
                   Matchmaking.rank_points(db, row["player1_uid"]), Matchmaking.rank_points(db, row["player2_uid"]),
                   policy["tolerance"], policy["day"]))

    @staticmethod
    def rank_points(db, uid):
        row = db.execute("SELECT COALESCE(SUM(CASE WHEN player1_uid=? THEN p1_points_won "
                         "ELSE p2_points_won END),0) FROM battles WHERE ranked=1 "
                         "AND (player1_uid=? OR player2_uid=?) AND interrupted=0 "
                         "AND end_datetime IS NOT NULL AND winner IS NOT NULL", (uid, uid, uid)).fetchone()
        return min(9999, max(0, row[0]))

    @staticmethod
    def ranking(db, body):
        offset, limit = body.get("offset", 0), body.get("limit", 3)
        if not integer(offset) or not 0 <= offset <= 1000000 or not integer(limit) or not 1 <= limit <= 50:
            raise ApiError("offset must be 0–1000000 and limit 1–50")
        # Points use the same completed ranked results as account/stats.
        ranking_sql = """
            WITH scores AS (
                SELECT player1_uid AS uid, COALESCE(p1_points_won,0) AS points,
                       (winner=1) AS wins FROM battles
                WHERE ranked=1 AND interrupted=0 AND end_datetime IS NOT NULL AND winner IS NOT NULL
                UNION ALL
                SELECT player2_uid, COALESCE(p2_points_won,0), (winner=2) FROM battles
                WHERE ranked=1 AND interrupted=0 AND end_datetime IS NOT NULL AND winner IS NOT NULL
            ), totals AS (SELECT uid, SUM(points) AS points, SUM(wins) AS wins FROM scores GROUP BY uid)
            , eligible AS (SELECT a.uid, a.username, a.display_name,
                   MIN(9999, MAX(0, COALESCE(t.points,0))) AS points,
                   COALESCE(t.wins,0) AS wins
            FROM accounts a LEFT JOIN totals t ON t.uid=a.uid WHERE a.active=1)
        """
        rows = db.execute(ranking_sql + "SELECT * FROM eligible WHERE points > 0 OR wins > 0 "
                          "ORDER BY points DESC, username ASC, uid ASC LIMIT ? OFFSET ?",
                          (limit, offset)).fetchall()
        entries = []
        for position, row in enumerate(rows, offset + 1):
            # Every occupied slot on the victorious side counts once, in either mode.
            selects = [f"SELECT p{side}c{slot} AS character FROM battles WHERE player{side}_uid=? "
                       f"AND interrupted=0 AND end_datetime IS NOT NULL AND winner={side}"
                       for side in (1, 2) for slot in range(1, 6)]
            character = db.execute("SELECT character,COUNT(*) AS uses FROM (" + " UNION ALL ".join(selects) +
                                   ") WHERE character IS NOT NULL GROUP BY character ORDER BY uses DESC,character ASC LIMIT 1",
                                   [row["uid"]] * 10).fetchone()
            entries.append({**dict(row), "rank": position, "character": character[0] if character else -1})
        total = db.execute(ranking_sql + "SELECT COUNT(*) FROM eligible WHERE points > 0 OR wins > 0").fetchone()[0]
        return {"offset": offset, "total": total, "entries": entries}

    def account(self, db, action, body, now, prepared=None):
        if action == "stats":
            user = self.account_auth(db, body, now)
            return {"rank_points": self.rank_points(db, user["uid"])}
        if action in ("session", "logout"):
            user = self.account_auth(db, body, now)
            if action == "logout":
                db.execute("DELETE FROM account_tokens WHERE token_hash=?", (digest(body["account_token"]),))
            return {"user_uid": user["uid"], "username": user["username"], "display_name": user["display_name"], "active": True}
        if action == "confirm":
            ticket = body.get("ticket")
            if not isinstance(ticket, str) or not re.fullmatch(r"[a-f0-9]{64}", ticket):
                raise ApiError("invalid registration ticket", 403)
            pending = db.execute("SELECT * FROM account_registrations WHERE ticket_hash=? AND expires_datetime>?",
                                 (digest(ticket), round(now * 1000))).fetchone()
            if not pending:
                raise ApiError("registration expired; generate a new access code", 403)
            self.username(pending["display_name"])
            if pending["user_uid"] is None:
                if db.execute("SELECT 1 FROM accounts WHERE username=?", (pending["username"],)).fetchone():
                    raise ApiError("username already exists", 409)
                uid = db.execute("INSERT INTO accounts(display_name,username,code_salt,code_hash,created_datetime) VALUES(?,?,?,?,?)",
                                 (pending["display_name"], pending["username"], pending["code_salt"], pending["code_hash"], round(now * 1000))).lastrowid
                db.execute("UPDATE account_registrations SET user_uid=? WHERE ticket_hash=?", (uid, digest(ticket)))
            else:
                uid = pending["user_uid"]
            user = db.execute("SELECT * FROM accounts WHERE uid=?", (uid,)).fetchone()
            if not user or not user["active"]:
                raise ApiError("inactive account", 403)
            return self.account_token(db, user, now)
        name = self.username(body.get("username"))
        user = db.execute("SELECT * FROM accounts WHERE username=?", (name.casefold(),)).fetchone()
        if action == "lookup":
            return {"exists": user is not None, "active": bool(user and user["active"])}
        if action in ('prepare', 'register', 'login') and prepared is None:
            raise ApiError('account hash preparation required', 503)
        if action == "prepare":
            if user:
                raise ApiError("username already exists", 409)
            code, salt, ticket = prepared['code'], prepared['salt'], secrets.token_hex(32)
            db.execute("DELETE FROM account_registrations WHERE expires_datetime<=?", (round(now * 1000),))
            db.execute("INSERT INTO account_registrations VALUES(?,?,?,?,?,?,NULL)",
                       (digest(ticket), name, name.casefold(), salt, prepared['computed'], round((now + 900) * 1000)))
            return {"code": code, "ticket": ticket}
        if action == "register":
            if user:
                raise ApiError("username already exists", 409)
            code, salt = prepared['code'], prepared['salt']
            uid = db.execute("INSERT INTO accounts(display_name,username,code_salt,code_hash,created_datetime) "
                             "VALUES (?,?,?,?,?)", (name, name.casefold(), salt, prepared['computed'],
                                                  round(now * 1000))).lastrowid
            user = {"uid": uid, "username": name.casefold(), "display_name": name}
            return {**self.account_token(db, user, now), "code": code}
        if action == "login":
            code = body.get("code")
            if not isinstance(code, str) or not re.fullmatch(r"[A-Za-z0-9]{6}", code):
                raise ApiError("code must contain six letters or numbers")
            if (not user or not user["active"] or user['code_salt'] != prepared['salt'] or
                    user['code_hash'] != prepared['verifier'] or not secrets.compare_digest(prepared['computed'], user["code_hash"])):
                raise ApiError("invalid username or code, or inactive account", 403)
            return self.account_token(db, user, now)
        raise ApiError("unknown account endpoint", 404)

    def battle_account(self, db, action, body):
        uid = body.get("session_uid", body.get("uid")) if action == "start" else body.get("uid")
        row = db.execute("SELECT b.player1_uid,b.player2_uid,s.host_hash,s.join_hash FROM battles b "
                         "JOIN battle_sessions s USING(uid) WHERE s.wire_uid=?", (uid,)).fetchone()
        if row:
            host = secrets.compare_digest(row["host_hash"], digest(str(body.get("key", ""))))
            expected = row["player1_uid" if host else "player2_uid"]
            if expected != body["_user_uid"] or (action == "start" and not host):
                raise ApiError("unauthorized account", 403)
            return (row["player1_uid"], row["player2_uid"])
        if action == "start" and "code" in body:
            room = self.room(db, body)
            self.authenticate_room(room, body, host_only=True)
            owners = db.execute("SELECT * FROM account_rooms WHERE code=?", (room["code"],)).fetchone()
            if owners and owners["host_uid"] == body["_user_uid"]:
                return (owners["host_uid"], owners["join_uid"])
        raise ApiError("unauthorized account", 403)

    def make_room(self, db, body, ip, now, queue=None, who=None):
        addresses = clean_addrs(body.get("addrs"))
        if not integer(body.get("v")):
            raise ApiError("bad request")
        key = secrets.token_hex(16)
        for _ in range(10):
            code = "".join(secrets.choice(ALPHABET) for _ in range(6))
            if db.execute("SELECT 1 FROM rooms WHERE code=?", (code,)).fetchone():
                continue
            db.execute("INSERT INTO rooms VALUES (?,?,?,?,?,NULL,?,NULL,?,NULL,?,?)",
                       (code, int(now), body["v"], ip, key, clean_name(body.get("name")),
                        json.dumps(addresses), queue, who))
            if body.get("_user_uid") is not None:
                db.execute("INSERT INTO account_rooms(code,host_uid) VALUES (?,?)", (code, body["_user_uid"]))
            return {"code": code, "key": key, "you": ip}
        raise ApiError("busy", 503)

    def join_room(self, db, room, body, ip, now):
        addresses = clean_addrs(body.get("addrs"))
        owners = db.execute("SELECT r.*, a.active FROM account_rooms r JOIN accounts a ON a.uid=r.host_uid WHERE code=?", (room["code"],)).fetchone()
        if self.require_accounts and (not owners or not owners["active"]):
            raise ApiError("host account is inactive", 403)
        if owners and owners["host_uid"] == body.get("_user_uid"):
            raise ApiError("cannot match your own account", 409)
        if not integer(body.get("v")):
            raise ApiError("bad request")
        if body["v"] != room["version"]:
            raise ApiError("version", 409)
        if room["join_key"] is not None:
            raise ApiError("full", 409)
        key = secrets.token_hex(16)
        addresses.append("seen=" + ip)
        # This server introduces LAN peers, so keep their private addresses.
        db.execute("UPDATE rooms SET join_key=?, join_name=?, join_addrs=? WHERE code=?",
                   (key, clean_name(body.get("name")), json.dumps(addresses), room["code"]))
        if owners:
            db.execute("UPDATE account_rooms SET join_uid=? WHERE code=?", (body.get("_user_uid"), room["code"]))
        uid = str(uuid.uuid4())
        inserted = db.execute("INSERT INTO battles (start_datetime, player1, player2, battle_type) "
                              "VALUES (?,?,?,?)", (round(now * 1000), room["host_name"],
                                                   clean_name(body.get("name")), BATTLE_TYPES["S" if room["queue"] == "RS" else room["queue"] or "unknown"]))
        db.execute("UPDATE battles SET ranked=? WHERE uid=?", (int(room["queue"] == "RS"), inserted.lastrowid))
        if owners:
            db.execute("UPDATE battles SET player1_uid=?,player2_uid=?, "
                       "player1=(SELECT username FROM accounts WHERE uid=?), "
                       "player2=(SELECT username FROM accounts WHERE uid=?) WHERE uid=?",
                       (owners["host_uid"], body.get("_user_uid"), owners["host_uid"],
                        body.get("_user_uid"), inserted.lastrowid))
        db.execute("INSERT INTO battle_sessions (uid,wire_uid,host_hash,join_hash,started,request_id) VALUES (?,?,?,?,?,?)",
                   (inserted.lastrowid, uid, digest(room["host_key"]), digest(key), now, "matched"))
        return {"key": key, "you": ip, "uid": uid, "stats_ranked": int(room["queue"] == "RS"),
                "peer": {"name": room["host_name"], "addrs": json.loads(room["host_addrs"])}}

    def room(self, db, body):
        code = body.get("code")
        if not isinstance(code, str):
            raise ApiError("no room", 404)
        room = db.execute("SELECT * FROM rooms WHERE code=?", (code.upper(),)).fetchone()
        if room is None:
            raise ApiError("no room", 404)
        return room

    def authenticate_room(self, room, body, host_only=False):
        key = body.get("key")
        if not isinstance(key, str) or not key:
            raise ApiError("no room", 404)
        host = secrets.compare_digest(key, room["host_key"])
        if not host and (host_only or not room["join_key"] or
                         not secrets.compare_digest(key, room["join_key"])):
            raise ApiError("no room", 404)
        return host

    def dispatch(self, db, path, body, ip, now, prepared=None):
        action = path.removeprefix("/v1/")
        body = dict(body)
        body.pop("_user_uid", None)
        if action.startswith("account/"):
            return self.account(db, action[8:], body, now, prepared)
        if action == "ranking":
            return self.ranking(db, body)
        if action == "hello":
            return {"min": 0, "latest": 0, "now": int(now), "accounts": True,
                    "build_attestation": False, "ranked_result_reports": 2}
        if action == "data-check":
            return {"expected": body.get("data") if body.get("data") in DATA_POLICY["signatures"] else "",
                    "scheme": DATA_POLICY["scheme"], "files": DATA_POLICY["files"],
                    "check": "compatibility", "build_attestation": False}
        if action in ("host", "join", "quick") and body.get("data") not in DATA_POLICY["signatures"]:
            raise ApiError("game data", 403)
        if (self.require_accounts and action != "queues") or (action == "quick" and body.get("queue") == "RS") or action.startswith("battle/"):
            account = self.account_auth(db, body, now)
            body["_user_uid"] = account["uid"]
            if action in ("host", "join", "quick"):
                supplied = body.get("name", account["username"])
                if not isinstance(supplied, str) or supplied.casefold() != account["username"]:
                    raise ApiError("player name does not match logged-in account", 403)
                body["name"] = account["display_name"]
        if action == "host":
            return self.make_room(db, body, ip, now)
        if action == "queues":
            if not integer(body.get("v")):
                raise ApiError("bad request")
            counts = dict.fromkeys(QUEUES, 0)
            for row in db.execute("SELECT queue, COUNT(*) AS n FROM rooms WHERE version=? "
                                  "AND join_key IS NULL GROUP BY queue", (body.get("v", 0),)):
                if row["queue"] in counts:
                    counts[row["queue"]] = row["n"]
            return {"waiting": counts}
        if action == "quick":
            queue, who = body.get("queue"), body.get("who")
            if queue not in QUEUES or not isinstance(who, str) or not re.fullmatch(r"[A-Za-z0-9]{8,24}", who):
                raise ApiError("bad request")
            if queue == "RS" and body.get("_user_uid") is None:
                raise ApiError("ranked battles require an account", 403)
            clean_addrs(body.get("addrs"))
            if not integer(body.get("v")):
                raise ApiError("bad request")
            # A restarted client replaces its previous waiting search. Otherwise
            # peers can be paired with a dead process until the room expires.
            if body.get("_user_uid") is not None:
                db.execute("DELETE FROM rooms WHERE queue IS NOT NULL AND join_key IS NULL "
                           "AND code IN (SELECT code FROM account_rooms WHERE host_uid=?)",
                           (body["_user_uid"],))
            room = db.execute("SELECT r.* FROM rooms r LEFT JOIN account_rooms ar USING(code) "
                              "LEFT JOIN accounts a ON a.uid=ar.host_uid WHERE queue=? AND version=? "
                              "AND join_key IS NULL AND who != ? "
                              "AND (?=0 OR (a.active=1 AND ar.host_uid != ?)) ORDER BY made, r.rowid LIMIT 1",
                              (queue, body["v"], who, int(self.require_accounts), body.get("_user_uid", -1))).fetchone()
            if room:
                return {"role": "join", "code": room["code"], **self.join_room(db, room, body, ip, now)}
            return {"role": "host", **self.make_room(db, body, ip, now, queue, who)}
        if action.startswith("battle/"):
            # Guest discovery is optional; battle records always require account ownership.
            owners = self.battle_account(db, action[7:], body)
            result = self.battle(db, action[7:], body, now)
            if owners and action == "battle/start":
                db.execute("UPDATE battles SET player1_uid=?,player2_uid=?, "
                           "player1=(SELECT username FROM accounts WHERE uid=?), "
                           "player2=(SELECT username FROM accounts WHERE uid=?) WHERE uid="
                           "(SELECT uid FROM battle_sessions WHERE wire_uid=?)", (*owners, *owners, result["uid"]))
            if action == "battle/start":
                battle_uid = db.execute("SELECT uid FROM battle_sessions WHERE wire_uid=?", (result["uid"],)).fetchone()[0]
                self.freeze_ranked(db, battle_uid, now)
            if action != "battle/connection":
                result.update(self.account_totals(db, result.get("uid", body.get("uid"))))
            return result
        if action not in ("join", "poll", "addrs", "leave", "relay"):
            raise ApiError("bad request", 404)
        room = self.room(db, body)
        if action == "join":
            return self.join_room(db, room, body, ip, now)
        host = self.authenticate_room(room, body)
        if self.require_accounts:
            owners = db.execute("SELECT * FROM account_rooms WHERE code=?", (room["code"],)).fetchone()
            if not owners or owners["host_uid" if host else "join_uid"] != body["_user_uid"]:
                raise ApiError("unauthorized account", 403)
            if action == "poll" and owners["join_uid"] is not None:
                if db.execute("SELECT count(*) FROM accounts WHERE uid IN (?,?) AND active=1",
                              (owners["host_uid"], owners["join_uid"])).fetchone()[0] != 2:
                    raise ApiError("participant account is inactive", 403)
        if action == "poll":
            side = "join" if host else "host"
            answer = {"peer": None if room[side + "_key"] is None else {
                "name": room[side + "_name"], "addrs": json.loads(room[side + "_addrs"])}}
            battle = db.execute("SELECT wire_uid FROM battle_sessions WHERE host_hash=? "
                                "ORDER BY started DESC, rowid DESC LIMIT 1",
                                (digest(room["host_key"]),)).fetchone()
            if battle:
                answer["uid"] = battle["wire_uid"]
                answer["stats_ranked"] = int(room["queue"] == "RS")
            return answer
        if action == "addrs":
            if not host:
                raise ApiError("bad request")
            db.execute("UPDATE rooms SET host_addrs=? WHERE code=?",
                       (json.dumps(clean_addrs(body.get("addrs"))), room["code"]))
        elif action == "leave":
            db.execute("DELETE FROM rooms WHERE code=?", (room["code"],))
        elif action == "relay":
            return {"noRelay": "Local LAN server: connect directly; no TURN relay configured."}
        return {}

    @staticmethod
    def connection_summary(value):
        integer_fields = {"rtt_samples", "duration_ms", "input_packets_sent", "input_packets_received",
                          "input_bytes_sent", "input_bytes_received", "wait_count", "rollbacks", "rollback_frames",
                          "input_delay_frames", "rollback_window_frames", "protocol_version"}
        ping_fields = {"avg_ping_ms", "min_ping_ms", "max_ping_ms", "jitter_ms"}
        required = integer_fields | ping_fields | {"wait_ms", "relayed"}
        if not isinstance(value, dict) or not required <= set(value) or set(value) - required - {"save_mode", "save_id", "save_hash"}:
            raise ApiError("invalid connection summary fields")
        if "save_mode" in value and value["save_mode"] not in ("regular", "zenkai"):
            raise ApiError("invalid save mode")
        # New clients send the pair together; old clients can omit both.
        # These values are authenticated locally but *client-reported* to us.
        if ("save_id" in value) != ("save_hash" in value):
            raise ApiError("save metadata must be a pair")
        if "save_id" in value:
            if (value.get("save_mode") != "zenkai" or not isinstance(value["save_id"], str) or
                not re.fullmatch(r"[0-9a-f]{32}", value["save_id"]) or
                not isinstance(value["save_hash"], str) or not re.fullmatch(r"[0-9a-f]{64}", value["save_hash"])):
                raise ApiError("invalid save metadata")
        for field in integer_fields:
            if not integer(value[field]) or not 0 <= value[field] <= 2**53 - 1:
                raise ApiError("invalid connection counter")
        if value["input_delay_frames"] > 30 or value["rollback_window_frames"] > 30:
            raise ApiError("invalid network frame configuration")
        for field in ping_fields | {"wait_ms"}:
            number = value[field]
            if field in ping_fields and number is None: continue
            if type(number) not in (int, float) or not math.isfinite(number) or not 0 <= number <= 2**53 - 1:
                raise ApiError("invalid connection measurement")
        if type(value["relayed"]) is not bool:
            raise ApiError("invalid relay flag")
        if value["rtt_samples"] == 0:
            if any(value[f] is not None for f in ping_fields): raise ApiError("ping requires samples")
        elif any(value[f] is None for f in ping_fields - {"jitter_ms"}):
            raise ApiError("missing ping measurement")
        else:
            if not value["min_ping_ms"] <= value["avg_ping_ms"] <= value["max_ping_ms"] < 5000:
                raise ApiError("invalid ping range")
        if (value["rtt_samples"] < 2) != (value["jitter_ms"] is None):
            raise ApiError("invalid jitter sample count")
        return value

    @staticmethod
    def character_loadouts(body):
        loadouts = body.get("items", {})
        if not isinstance(loadouts, dict) or any(slot not in CHARACTERS for slot in loadouts):
            raise ApiError("items must map character slots to seven item IDs")
        result = {}
        for slot, items in loadouts.items():
            if body.get(slot) is None:
                raise ApiError("items require an occupied character slot")
            if not isinstance(items, list) or len(items) != 7 or any(
                    value is not None and (not integer(value) or not 0 <= value <= 65535) for value in items):
                raise ApiError("each character requires seven item IDs between 0 and 65535, or null")
            result[slot] = [value or None for value in items]
        return result

    def battle(self, db, action, body, now):
        key = body.get("key")
        if not isinstance(key, str) or not key:
            raise ApiError("unauthorized", 403)
        hashed = digest(key)
        if action == "start":
            # Caller chooses a UUID per battle so retries cannot create duplicate records.
            uid = body.get("uid")
            try:
                if str(uuid.UUID(uid)) != uid:
                    raise ValueError()
            except (ValueError, TypeError, AttributeError):
                raise ApiError("uid must be a canonical UUID") from None
            for field, minimum in (("stage", 0), ("music", -1)):
                value = body.get(field)
                if value is not None and (not integer(value) or not minimum <= value <= 65535):
                    raise ApiError(f"{field} must be an integer between {minimum} and 65535 or null")
            picker = body.get("stage_music_picker")
            if picker is not None and (not integer(picker) or picker not in (1, 2)):
                raise ApiError("stage_music_picker must be 1, 2 or null")
            for slot in CHARACTERS:
                value = body.get(slot + "_costume")
                if value is not None and (not integer(value) or not 0 <= value <= 65535 or body.get(slot) is None):
                    raise ApiError("costumes must be integer IDs for occupied character slots")
            loadouts = self.character_loadouts(body)
            previous = self.battle_row(db, uid)
            if previous:
                session = db.execute("SELECT * FROM battle_sessions WHERE wire_uid=?", (uid,)).fetchone()
                if not secrets.compare_digest(session["host_hash"], hashed):
                    raise ApiError("unauthorized", 403)
                if session["request_id"] != "matched" or previous["end_datetime"]:
                    # Retrying a start is allowed; changing its metadata is not.
                    if previous["battle_type"] != body.get("battle_type") or any(
                            previous[c] != body.get(c) for c in CHARACTERS) or previous["items"] != loadouts or any(
                                previous[field] != body.get(field) for field in ("stage", "music", "stage_music_picker") + tuple(c + "_costume" for c in CHARACTERS)):
                        raise ApiError("battle already started with different data", 409)
                    return {"uid": uid, "start_datetime": previous["start_datetime"]}
            if "session_uid" in body:
                if not isinstance(body["session_uid"], str):
                    raise ApiError("bad request")
                origin = db.execute("SELECT b.player1,b.player2,b.ranked,s.host_hash,s.join_hash FROM battles b "
                                    "JOIN battle_sessions s USING(uid) WHERE s.wire_uid=?",
                                    (body["session_uid"],)).fetchone()
                if not origin or not secrets.compare_digest(origin["host_hash"], hashed):
                    raise ApiError("unauthorized", 403)
                room = {"host_name": origin["player1"], "join_name": origin["player2"]}
                ranked = origin["ranked"]
                join_hash = origin["join_hash"]
            else:
                room = self.room(db, body)
                self.authenticate_room(room, body, host_only=True)
                if not room["join_key"]:
                    raise ApiError("battle needs two players", 409)
                ranked = int(room["queue"] == "RS")
                join_hash = digest(room["join_key"])
            battle_type = body.get("battle_type")
            if battle_type not in BATTLE_TYPES or battle_type == "unknown":
                raise ApiError("battle_type must be S, T, D10, D15 or D20")
            if ranked and battle_type != "S":
                raise ApiError("ranked battles require single battle")
            roster = [body.get(c) for c in CHARACTERS]
            if any(v is not None and (not integer(v) or not 0 <= v <= 65535) for v in roster):
                raise ApiError("characters must be integer IDs or null")
            for side in (0, 5):
                if roster[side] is None:
                    raise ApiError("each player needs at least one character")
                if battle_type == "S" and any(v is not None for v in roster[side + 1:side + 5]):
                    raise ApiError("single battle allows one character per player")
                found_empty = False
                for value in roster[side:side + 5]:
                    if value is None:
                        found_empty = True
                    elif found_empty:
                        raise ApiError("character slots must be consecutive")
            start = london_timestamp(round(now * 1000) / 1000)
            columns = ("start_datetime", "player1", "player2", "battle_type", "stage", "music", "stage_music_picker") + CHARACTERS + tuple(c + "_costume" for c in CHARACTERS)
            pending = db.execute("SELECT b.uid FROM battles b JOIN battle_sessions s USING(uid) "
                                 "WHERE s.host_hash=? AND s.request_id='matched' AND b.end_datetime IS NULL",
                                 (hashed,)).fetchone()
            values = (round(now * 1000), room["host_name"], room["join_name"], BATTLE_TYPES[battle_type], body.get("stage"), body.get("music"), picker, *roster, *(body.get(c + "_costume") for c in CHARACTERS))
            if pending:
                # Enrich the automatically created entry instead of duplicating it.
                # A caller may supply its own UUID on the first explicit start.
                numeric_uid = pending["uid"]
                db.execute("DELETE FROM battle_sessions WHERE uid=?", (numeric_uid,))
                db.execute(f"UPDATE battles SET {','.join(c + '=?' for c in columns)} WHERE uid=?",
                           (*values, pending["uid"]))
            else:
                numeric_uid = db.execute(f"INSERT INTO battles ({','.join(columns)}) VALUES ({','.join('?' for _ in columns)})",
                                         values).lastrowid
            for slot, items in loadouts.items():
                item_uid = db.execute("INSERT INTO character_items(item1,item2,item3,item4,item5,item6,item7) "
                                      "VALUES (?,?,?,?,?,?,?)", items).lastrowid
                db.execute(f"UPDATE battles SET {slot}_items_uid=? WHERE uid=?", (item_uid, numeric_uid))
            db.execute("UPDATE battles SET ranked=? WHERE uid=?", (ranked, numeric_uid))
            db.execute("INSERT INTO battle_sessions (uid,wire_uid,host_hash,join_hash,started,request_id) VALUES (?,?,?,?,?,?)",
                       (numeric_uid, uid, hashed, join_hash, now, uid))
            return {"uid": uid, "start_datetime": start}
        if action not in ("end", "get", "interrupt", "connection"):
            raise ApiError("bad request", 404)
        uid = body.get("uid")
        if not isinstance(uid, str):
            raise ApiError("bad request")
        session = db.execute("SELECT * FROM battle_sessions WHERE wire_uid=?", (uid,)).fetchone()
        if not session:
            raise ApiError("no battle", 404)
        host = secrets.compare_digest(session["host_hash"], hashed)
        if not host and not secrets.compare_digest(session["join_hash"], hashed):
            raise ApiError("unauthorized", 403)
        row = self.battle_row(db, uid)
        if action == "get":
            return {"battle": dict(row)}
        if action == "connection":
            summary = self.connection_summary(body.get("connection"))
            side = 1 if host else 2
            db.execute(f"UPDATE battle_sessions SET p{side}_connection=? WHERE uid=?",
                       (json.dumps(summary, separators=(",", ":"), allow_nan=False), session["uid"]))
            return {"uid": uid, "player": side, "connection": summary}
        if action == "interrupt":
            if row["end_datetime"] is None:
                db.execute("UPDATE battles SET end_datetime=?, interrupted=1, battle_duration=? WHERE uid=?",
                           (round(now * 1000), round(max(0, now - session["started"]), 3), session["uid"]))
            return {"battle": self.battle_row(db, uid)}
        winner = body.get("winner")
        finish_type = body.get("finish_type", "ok")
        if finish_type not in FINISH_TYPES:
            raise ApiError("finish_type must be ok, perfect, timeout or ringout")
        if winner not in ("p1", "p2", "tie"):
            raise ApiError("winner is required")
        elapsed = max(0.0, now - session["started"])
        duration = body.get("battle_duration", round(elapsed, 3))
        if type(duration) not in (int, float) or not math.isfinite(duration) or not 0 <= duration <= 86400:
            raise ApiError("battle_duration must be seconds between zero and 86400")
        if not row["ranked"]:
            if not host:
                raise ApiError("only the host reports unranked results", 403)
            points = (0, 0)
        else:
            participants = (row["player1_uid"], row["player2_uid"])
            if (None in participants or participants[0] == participants[1] or
                    db.execute("SELECT count(*) FROM accounts WHERE uid IN (?,?) AND active=1", participants).fetchone()[0] != 2):
                raise ApiError("ranked results require two active participant accounts", 403)
            side = 1 if host else 2
            points = (body.get("p1_points_won"), body.get("p2_points_won"))
            if any(not integer(p) or not -(2**31) <= p < 2**31 for p in points):
                raise ApiError("signed integer base point changes are required")
            result = json.dumps({"winner": winner, "finish": finish_type, "points": points}, separators=(",", ":"))
            reports = dict(db.execute("SELECT side,result FROM battle_result_reports WHERE battle_uid=?", (session["uid"],)))
            if side in reports and reports[side] != result:
                raise ApiError("result report cannot be changed", 409)
            db.execute("INSERT OR IGNORE INTO battle_result_reports VALUES(?,?,?)", (session["uid"], side, result))
            other = reports.get(3 - side)
            if other != result:
                return {"battle": self.battle_row(db, uid), "result_status": "disputed" if other is not None else "pending"}
            # Matching peers supply the original performance amounts; the server
            # applies rank scaling and balance caps exactly once.
            # New sessions freeze at start. Older in-flight sessions freeze on
            # their first report; completed legacy rows are left unchanged.
            if row["end_datetime"] is None or row["interrupted"]:
                self.freeze_ranked(db, session["uid"], now)
            scoring = db.execute("SELECT * FROM rank_battle_scoring WHERE battle_uid=?", (session["uid"],)).fetchone()
            if not scoring:
                raise ApiError("ranked scoring snapshot missing", 503)
            points = adjusted_points((scoring["p1_start"], scoring["p2_start"]), winner, points, scoring["tolerance"])
        if row["end_datetime"] and not row["interrupted"]:
            if ((row["winner"], row["p1_points_won"], row["p2_points_won"]) != (winner, *points)
                    or row["finish_type"] != finish_type):
                raise ApiError("battle already ended with different data", 409)
        else:
            db.execute("UPDATE battles SET end_datetime=?, winner=?, p1_points_won=?, "
                       "p2_points_won=?, battle_duration=?, finish_type=?, interrupted=0 WHERE uid=?",
                       (round(now * 1000), WINNERS[winner], *points, duration, FINISH_TYPES[finish_type], session["uid"]))
        return {"battle": self.battle_row(db, uid)}


class Handler(BaseHTTPRequestHandler):
    def setup(self):
        super().setup()
        self.connection.settimeout(15)

    def reply(self, body, status=200):
        # The existing game recognizes some fields using compact JSON substrings.
        # Match the Worker's JSON.stringify output, including separators.
        payload = json.dumps(body, allow_nan=False, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        self.reply({"service": "ZENKAI Power! LAN server", "timezone": "Europe/London"},
                   200 if self.path == "/" else 404)

    def do_POST(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 16384 or self.headers.get("Transfer-Encoding"):
                raise ApiError("bad request")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict) or not self.path.startswith("/v1/"):
                raise ApiError("bad request")
            answer = self.server.matchmaking.request(self.path, body, self.client_address[0])
        except ApiError as error:
            self.reply({"error": str(error)}, error.status)
        except (ValueError, UnicodeError):
            self.reply({"error": "bad request"}, 400)
        except sqlite3.Error:
            self.log_error("SQLite operation failed")
            self.reply({"error": "database unavailable"}, 503)
        else:
            self.reply(answer)

    def log_message(self, format, *args):
        # Do not put room keys, names, or result payloads in access logs.
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0", help="listen address (default: all IPv4 interfaces)")
    parser.add_argument("--port", default=8787, type=int)
    parser.add_argument("--db", type=Path, default=(Path(__file__).resolve().parents[1] / "data/lan.sqlite3"))
    parser.add_argument("--allow-guests", action="store_true", help="legacy test mode: allow unauthenticated matchmaking")
    args = parser.parse_args()
    args.db.parent.mkdir(parents=True, exist_ok=True)
    service = Matchmaking(args.db, require_accounts=not args.allow_guests)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.matchmaking = service
    print(f"LAN matchmaking listening on {args.host}:{server.server_port}; SQLite: {args.db.resolve()}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
