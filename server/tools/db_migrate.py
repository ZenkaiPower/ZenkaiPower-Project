#!/usr/bin/env python3
"""Lossless application-database transfer between LAN SQLite and Cloudflare D1.

Uses only Python's standard library. See DB_MIGRATION.md for cutover instructions.
No server migrations or score recalculation are run while copying a database.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import tempfile
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
import uuid

FORMAT = "zenkaipower-database-transfer-v1"
PAGE_SIZE = 100
SQL_LIMIT = 100_000


class MigrationError(Exception):
    pass


def ident(name):
    return '"' + name.replace('"', '""') + '"'


def literal(value):
    return "'" + value.replace("'", "''") + "'"


def canonical(value):
    return json.dumps(value, sort_keys=True, ensure_ascii=True, separators=(",", ":"))


def digest(value):
    return hashlib.sha256(canonical(value).encode()).hexdigest()


def internal(name):
    return name.startswith(("sqlite_", "_cf_")) or name == "d1_migrations"


def typed_sql(column):
    c = ident(column)
    # No application value crosses the D1 JSON API as a JSON number. Hex also
    # preserves embedded NULs and text bytes that Python cannot decode as UTF-8.
    return (f"CASE typeof({c}) WHEN 'null' THEN 'n' "
            f"WHEN 'integer' THEN 'i'||CAST({c} AS TEXT) "
            f"WHEN 'real' THEN 'r'||printf('%!.26g',{c}) "
            f"WHEN 'text' THEN 't'||hex(CAST({c} AS BLOB)) "
            f"WHEN 'blob' THEN 'b'||hex({c}) END")


def normalize_cell(cell):
    if not isinstance(cell, str) or not cell:
        raise MigrationError("Invalid typed cell in database response")
    kind, value = cell[0], cell[1:]
    if kind == "n" and not value:
        return "n"
    if kind == "i" and re.fullmatch(r"-?[0-9]+", value):
        number = int(value)
        if -(2**63) <= number < 2**63:
            return "i" + str(number)
    if kind == "r":
        try:
            number = float.fromhex(value) if "0x" in value else float(value)
            if math.isfinite(number):
                return "r" + number.hex()
        except (ValueError, OverflowError):
            pass
    if kind in ("t", "b") and re.fullmatch(r"(?:[0-9a-fA-F]{2})*", value):
        return kind + value.upper()
    raise MigrationError("Unsupported/non-finite or invalid typed database cell")


def cell_sql(cell):
    cell = normalize_cell(cell)
    if cell == "n":
        return "NULL"
    kind, value = cell[0], cell[1:]
    if kind == "i":
        return value
    if kind == "r":
        number = float.fromhex(value)
        mantissa, exponent = math.frexp(number)
        significand = int(math.ldexp(mantissa, 53))
        exponent -= 53
        if not significand:
            return "-CAST(0 AS REAL)" if math.copysign(1, number) < 0 else "CAST(0 AS REAL)"
        while significand % 2 == 0:
            significand //= 2
            exponent += 1
        # SQLite's decimal literal parser can differ by one ULP for rare doubles.
        # An exact <=53-bit integer scaled by powers of two avoids that parser.
        # Every intermediate value is representable, including subnormal results.
        expression = f"CAST({significand} AS REAL)"
        operator = " * " if exponent > 0 else " / "
        remaining = abs(exponent)
        while remaining:
            step = min(remaining, 52)
            expression += operator + str(1 << step)
            remaining -= step
        return "(" + expression + ")"
    blob = "X'" + value + "'"
    return f"CAST({blob} AS TEXT)" if kind == "t" else blob


class SQLite:
    def __init__(self, connection):
        self.connection = connection

    def query(self, sql):
        cursor = self.connection.execute(sql)
        names = [c[0] for c in cursor.description] if cursor.description else []
        return [dict(zip(names, row)) for row in cursor]

    def batch(self, statements):
        with self.connection:
            self.connection.execute("BEGIN")
            for statement in statements:
                self.connection.execute(statement)


class D1:
    def __init__(self, account_id, database_id, token):
        if not re.fullmatch(r"[0-9a-fA-F]{32}", account_id or ""):
            raise MigrationError("Set CLOUDFLARE_ACCOUNT_ID to your 32-character account ID")
        try:
            database_id = str(uuid.UUID(database_id))
        except ValueError as error:
            raise MigrationError("D1 --database must be the database UUID") from error
        if not token:
            raise MigrationError("Set CLOUDFLARE_API_TOKEN (D1 Read for export; D1 Write for import)")
        self.url = f"https://api.cloudflare.com/client/v4/accounts/{account_id}/d1/database/{database_id}/query"
        self.token = token

    def request(self, body):
        request = Request(self.url, data=json.dumps(body).encode(), method="POST",
                          headers={"Authorization": "Bearer " + self.token,
                                   "Content-Type": "application/json"})
        try:
            with urlopen(request, timeout=60) as response:
                answer = json.load(response)
        except (HTTPError, URLError, TimeoutError, ValueError) as error:
            # Never echo a request/SQL body: it may contain account credentials.
            raise MigrationError("D1 request failed; check connectivity, IDs and token permissions") from error
        results = answer.get("result", [])
        if not answer.get("success") or not isinstance(results, list) or any(not r.get("success") for r in results):
            raise MigrationError("D1 rejected the operation; destination may be incomplete if an import was underway")
        return results

    def query(self, sql):
        results = self.request({"sql": sql})
        if len(results) != 1:
            raise MigrationError("Expected one D1 query result")
        return results[0].get("results", [])

    def batch(self, statements):
        results = self.request({"batch": [{"sql": sql} for sql in statements]})
        if len(results) != len(statements):
            raise MigrationError("Incomplete D1 batch response; verify the destination before using it")


@contextmanager
def sqlite_snapshot(path):
    path = Path(path).resolve()
    source = sqlite3.connect(path.as_uri() + "?mode=ro", uri=True)
    snapshot = sqlite3.connect(":memory:")
    try:
        # sqlite3_backup includes committed WAL pages and provides one consistent
        # snapshot. Copying just lan.sqlite3 can silently omit those pages.
        source.backup(snapshot)
        if snapshot.execute("PRAGMA encoding").fetchone()[0] != "UTF-8":
            raise MigrationError("Only UTF-8 SQLite databases are supported; source encoding was not changed")
        yield SQLite(snapshot)
    finally:
        snapshot.close()
        source.close()


def schema(db):
    rows = db.query("SELECT type,name,tbl_name,sql FROM sqlite_schema WHERE sql IS NOT NULL ORDER BY type,name")
    result = [row for row in rows if not internal(row["name"]) and not internal(row["tbl_name"])]
    for row in result:
        if row["type"] not in ("table", "index", "trigger", "view"):
            raise MigrationError("Unsupported schema object: " + row["name"])
        if re.match(r"\s*CREATE\s+VIRTUAL\s+TABLE", row["sql"], re.I):
            raise MigrationError("Virtual tables need a dedicated migration: " + row["name"])
    return result


def check_database(db):
    if any(list(row.values()) != ["ok"] for row in db.query("PRAGMA quick_check")):
        raise MigrationError("Database integrity check failed")
    if db.query("PRAGMA foreign_key_check"):
        raise MigrationError("Database contains foreign-key violations; no records were dropped or repaired")


def read_rows(db, name, columns, order):
    expressions = ",".join(typed_sql(c) + " AS " + ident("c" + str(i)) for i, c in enumerate(columns))
    rows = []
    offset = 0
    while True:
        page = db.query(f"SELECT {expressions} FROM {ident(name)} ORDER BY {order} LIMIT {PAGE_SIZE} OFFSET {offset}")
        rows.extend([[normalize_cell(row["c" + str(i)]) for i in range(len(columns))] for row in page])
        if len(page) < PAGE_SIZE:
            break
        offset += len(page)
    return sorted(rows, key=canonical)


def snapshot(db):
    check_database(db)
    objects = schema(db)
    tables = []
    sequences = []
    for obj in objects:
        if obj["type"] != "table":
            continue
        name = obj["name"]
        info = db.query(f"PRAGMA table_xinfo({ident(name)})")
        columns = [r["name"] for r in info if r["hidden"] == 0]
        without_rowid = bool(re.search(r"\bWITHOUT\s+ROWID\b", obj["sql"], re.I))
        if without_rowid:
            order = ",".join(ident(r["name"]) for r in sorted(info, key=lambda r: r["pk"]) if r["pk"])
        else:
            # Even rooms with a TEXT primary key use rowid to order matchmaking.
            rowid = next((c for c in ("_rowid_", "rowid", "oid") if c.lower() not in {n.lower() for n in columns}), None)
            if rowid is None:
                raise MigrationError("All rowid aliases are shadowed: " + name)
            columns.insert(0, rowid)
            order = ident(rowid)
        rows = read_rows(db, name, columns, order)
        parents = sorted({r["table"] for r in db.query(f"PRAGMA foreign_key_list({ident(name)})")})
        tables.append({"name": name, "columns": columns, "parents": parents, "rows": rows,
                       "count": len(rows), "sha256": digest(rows)})
    auto = [obj["name"] for obj in objects if obj["type"] == "table" and re.search(r"\bAUTOINCREMENT\b", obj["sql"], re.I)]
    if auto:
        where = ",".join(literal(name) for name in auto)
        # Preserve deleted-ID high-water marks, including an empty table whose
        # last inserted account/item/battle has already been deleted.
        for row in db.query("SELECT " + typed_sql("name") + " AS n," + typed_sql("seq") +
                            f" AS s FROM sqlite_sequence WHERE name IN ({where}) ORDER BY name"):
            sequences.append([normalize_cell(row["n"]), normalize_cell(row["s"])])
    return {"schema": objects, "tables": tables, "sequences": sequences}


def archive(payload):
    return {"format": FORMAT, "sha256": digest(payload), "payload": payload}


def read_archive(path):
    with open(path, encoding="utf-8") as file:
        value = json.load(file)
    if value.get("format") != FORMAT or value.get("sha256") != digest(value.get("payload")):
        raise MigrationError("Archive format/checksum mismatch")
    payload = value["payload"]
    for table in payload["tables"]:
        if table["count"] != len(table["rows"]) or table["sha256"] != digest(table["rows"]):
            raise MigrationError("Table count/checksum mismatch: " + table["name"])
    return payload


def table_order(payload):
    pending = {table["name"]: table for table in payload["tables"]}
    result = []
    done = set()
    while pending:
        ready = sorted(name for name, table in pending.items() if set(table["parents"]) <= done)
        if not ready:
            raise MigrationError("Cyclic or missing foreign-key dependencies are not supported by this transfer")
        for name in ready:
            done.add(name)
            result.append(pending.pop(name))
    return result


def statements(payload):
    # Create every table first. Insert parents before children so each D1 batch
    # can commit with foreign keys enforced; no foreign_keys=OFF workaround.
    for obj in payload["schema"]:
        if obj["type"] == "table":
            yield obj["sql"]
    for table in table_order(payload):
        columns = ",".join(ident(c) for c in table["columns"])
        for row in table["rows"]:
            if len(row) != len(table["columns"]):
                raise MigrationError("Wrong row width: " + table["name"])
            yield f"INSERT INTO {ident(table['name'])} ({columns}) VALUES ({','.join(cell_sql(c) for c in row)})"
    for obj in payload["schema"]:
        if obj["type"] == "table" and re.search(r"\bAUTOINCREMENT\b", obj["sql"], re.I):
            yield "DELETE FROM sqlite_sequence WHERE name=" + literal(obj["name"])
    for name, seq in payload["sequences"]:
        yield f"INSERT INTO sqlite_sequence(name,seq) VALUES ({cell_sql(name)},{cell_sql(seq)})"
    # Triggers are installed only after copying data: import must not fire them.
    for obj in payload["schema"]:
        if obj["type"] != "table":
            yield obj["sql"]


def verify(payload, db):
    actual = snapshot(db)
    if digest(actual) != digest(payload):
        details = []
        expected_schema = {obj["name"]: obj for obj in payload["schema"]}
        actual_schema = {obj["name"]: obj for obj in actual["schema"]}
        for name in sorted(expected_schema.keys() | actual_schema.keys()):
            if name not in actual_schema:
                details.append("missing schema object: " + name)
            elif name not in expected_schema:
                details.append("unexpected schema object: " + name)
            elif expected_schema[name] != actual_schema[name]:
                details.append("schema definition differs: " + name)
        expected_tables = {t["name"]: t for t in payload["tables"]}
        for table in actual["tables"]:
            expected = expected_tables.get(table["name"])
            if expected is not None and expected != table:
                details.append("table data/metadata differs: " + table["name"] +
                               f" (expected {expected['count']} rows, found {table['count']})")
        if payload["sequences"] != actual["sequences"]:
            details.append("AUTOINCREMENT sequences differ")
        raise MigrationError("Verification failed: " + "; ".join(details))


def preflight(payload):
    sql = list(statements(payload))
    if any(len(statement.encode()) > SQL_LIMIT for statement in sql):
        raise MigrationError("A statement exceeds D1's 100 KB limit; data was not truncated")
    db = sqlite3.connect(":memory:")
    try:
        db.execute("PRAGMA foreign_keys=ON")
        SQLite(db).batch(sql)
        verify(payload, SQLite(db))
    finally:
        db.close()
    return sql


def publish(path, writer):
    """Publish a complete private file without overwriting an existing path."""
    path = Path(path).resolve()
    fd, temporary = tempfile.mkstemp(prefix="." + path.name + ".", dir=path.parent)
    os.close(fd)
    try:
        writer(Path(temporary))
        # An exclusive link prevents a concurrent file creation from being overwritten.
        os.link(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def write_archive(path, payload):
    publish(path, lambda temporary: temporary.write_text(json.dumps(archive(payload), indent=2) + "\n", encoding="utf-8"))


def import_sqlite(path, payload):
    sql = preflight(payload)
    def restore(temporary):
        db = sqlite3.connect(temporary)
        try:
            db.execute("PRAGMA foreign_keys=ON")
            SQLite(db).batch(sql)
            verify(payload, SQLite(db))
        finally:
            db.close()
    publish(path, restore)


def import_d1(db, payload):
    sql = preflight(payload)
    if schema(db):
        raise MigrationError("D1 destination must be a new, empty database; existing tables are never replaced/merged")
    batch, size = [], 0
    for statement in sql:
        length = len(statement.encode())
        if batch and (len(batch) >= 100 or size + length > 750_000):
            db.batch(batch)
            batch, size = [], 0
        batch.append(statement)
        size += length
    if batch:
        db.batch(batch)
    verify(payload, db)


def summary(payload):
    return {"sha256": digest(payload), "tables": {t["name"]: t["count"] for t in payload["tables"]}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for command in ("export", "import", "verify"):
        p = sub.add_parser(command)
        p.add_argument("backend", choices=("sqlite", "d1"))
        p.add_argument("--database", help="SQLite path (required) or Cloudflare D1 UUID (defaults to local config)")
        p.add_argument("--archive", required=True, type=Path)
        p.add_argument("--quiesced", action="store_true", help="confirm D1 application writes are stopped during transfer")
    for command in ("inspect", "sql"):
        p = sub.add_parser(command)
        p.add_argument("--archive", required=True, type=Path)
        if command == "sql":
            p.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command in ("inspect", "sql"):
            payload = read_archive(args.archive)
            sql = preflight(payload)
            if args.command == "sql":
                publish(args.output, lambda path: path.write_text(";\n".join(sql) + ";\n", encoding="utf-8"))
        else:
            if args.backend == "d1":
                if not args.quiesced:
                    raise MigrationError("D1 transfer/verification requires --quiesced: stop application writes first")
                config_path = Path(__file__).resolve().parents[1] / "cloudflare.local.json"
                config = {}
                if config_path.exists():
                    try:
                        config = json.loads(config_path.read_text(encoding="utf-8"))
                    except (ValueError, UnicodeError):
                        raise MigrationError("Invalid cloudflare.local.json: expected a JSON object") from None
                    if not isinstance(config, dict) or any(not isinstance(v, str) for v in config.values()):
                        raise MigrationError("cloudflare.local.json must contain string values in a JSON object")
                db = D1(os.environ.get("CLOUDFLARE_ACCOUNT_ID") or config.get("account_id"),
                        args.database or os.environ.get("ZENKAIPOWER_D1_DATABASE_ID") or config.get("database_id"),
                        os.environ.get("CLOUDFLARE_API_TOKEN") or config.get("api_token"))
            elif not args.database:
                raise MigrationError("SQLite operations require --database")
            if args.command == "export":
                if args.backend == "sqlite":
                    with sqlite_snapshot(args.database) as db:
                        payload = snapshot(db)
                else:
                    payload = snapshot(db)
                    # Catch accidental writes during a paginated remote export.
                    verify(payload, db)
                preflight(payload)
                write_archive(args.archive, payload)
            else:
                payload = read_archive(args.archive)
                if args.command == "import":
                    if args.backend == "sqlite":
                        import_sqlite(args.database, payload)
                    else:
                        import_d1(db, payload)
                elif args.backend == "sqlite":
                    with sqlite_snapshot(args.database) as db:
                        verify(payload, db)
                else:
                    verify(payload, db)
        print(json.dumps(summary(payload), indent=2))
    except (MigrationError, OSError, sqlite3.Error, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f"Migration failed: {error}\n")


if __name__ == "__main__":
    main()
