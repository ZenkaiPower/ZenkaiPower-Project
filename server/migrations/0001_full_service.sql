-- Additive: supports a fresh D1 database, the original Worker schema,
-- or a database imported from the current LAN server.

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

CREATE TABLE IF NOT EXISTS used (what TEXT NOT NULL, slot INTEGER NOT NULL, n INTEGER NOT NULL, PRIMARY KEY(what,slot));
CREATE INDEX IF NOT EXISTS battles_player1 ON battles(player1_uid,ranked,end_datetime);
CREATE INDEX IF NOT EXISTS battles_player2 ON battles(player2_uid,ranked,end_datetime);
CREATE INDEX IF NOT EXISTS battle_sessions_host ON battle_sessions(host_hash,started);
