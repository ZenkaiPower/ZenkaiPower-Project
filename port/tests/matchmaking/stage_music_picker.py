#!/usr/bin/env python3
"""The player controlling stage/music is saved per match, including rematches."""
from pathlib import Path
import sqlite3
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[3] / "server/src"
sys.path.insert(0, str(ROOT))
from lan_server import ApiError, Matchmaking, SCHEMA


with tempfile.TemporaryDirectory(prefix="picker-db-") as directory:
    database = Path(directory) / "matches.sqlite"
    # An existing LAN installation gains the column without losing old rows.
    with sqlite3.connect(database) as db:
        old_schema = SCHEMA.replace("    stage_music_picker INTEGER CHECK(stage_music_picker IN (1,2)),\n", "")
        db.executescript(old_schema)
        db.execute("INSERT INTO battles(start_datetime,player1,player2,battle_type) VALUES(?,?,?,?)",
                   (1, "old host", "old guest", 1))
    service = Matchmaking(database, require_accounts=False)
    with service.connect() as db:
        old = db.execute("SELECT stage_music_picker FROM battles WHERE player1='old host'").fetchone()
        assert old[0] is None
        db.execute("INSERT INTO rooms(code,made,version,ip,host_key,join_key,host_name,join_name,host_addrs,join_addrs,queue,who) "
                   "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                   ("ABC234", 100, 1, "127.0.0.1", "host-key", "join-key", "Alice", "Bob", "[]", "[]", "S", None))

        body = {"uid": str(uuid.uuid4()), "code": "ABC234", "key": "host-key", "battle_type": "S",
                "p1c1": 1, "p2c1": 2, "stage": 3, "music": 4, "stage_music_picker": 1}
        first = service.battle(db, "start", body, 100)
        assert service.battle_row(db, first["uid"])["stage_music_picker"] == 1

        rematch = {**body, "uid": str(uuid.uuid4()), "session_uid": first["uid"],
                   "stage_music_picker": 2}
        second = service.battle(db, "start", rematch, 120)
        assert service.battle_row(db, second["uid"])["stage_music_picker"] == 2

        for invalid in (0, 3, True, "2"):
            try:
                service.battle(db, "start", {**rematch, "uid": str(uuid.uuid4()),
                                             "stage_music_picker": invalid}, 130)
            except ApiError:
                pass
            else:
                raise AssertionError(f"accepted invalid picker {invalid!r}")

    # Cloudflare starts from migration 0001, then adds the column in 0002.
    with sqlite3.connect(":memory:") as db:
        for migration in sorted((ROOT.parent / "migrations").glob("*.sql")):
            db.executescript(migration.read_text())
        db.execute("INSERT INTO battles(uid,start_datetime,player1,player2,battle_type,stage_music_picker) "
                   "VALUES(?,?,?,?,?,?)", (1, 1, "Alice", "Bob", 1, 2))
        assert db.execute("SELECT stage_music_picker FROM battles WHERE uid=1").fetchone()[0] == 2
        try:
            db.execute("UPDATE battles SET stage_music_picker=3 WHERE uid=1")
        except sqlite3.IntegrityError:
            pass
        else:
            raise AssertionError("Cloudflare migration accepted an invalid picker")

print("Stage/music picker database checks passed")
