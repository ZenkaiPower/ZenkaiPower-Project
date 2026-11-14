"""Clear application rows from one or more configured Cloudflare D1 databases."""
import argparse
import json
import os
import sys
import uuid
from pathlib import Path

from db_migrate import D1, MigrationError, ident, internal

HERE = Path(__file__).resolve().parents[1]
CONFIG_PATH = HERE / "cloudflare.local.json"


def load_settings(database_ids):
    try:
        config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError, UnicodeError):
        raise MigrationError("Cannot read server/cloudflare.local.json") from None
    if not isinstance(config, dict):
        raise MigrationError("cloudflare.local.json must contain a JSON object")

    account_id = os.environ.get("CLOUDFLARE_ACCOUNT_ID") or config.get("account_id")
    token = (os.environ.get("CLOUDFLARE_API_TOKEN") or config.get("api_token")
             or config.get("deploy_api_token"))
    if database_ids:
        selected_ids = database_ids
    else:
        selected_ids = [os.environ.get("ZENKAIPOWER_D1_DATABASE_ID") or config.get("database_id")]
    if any(not isinstance(value, str) or not value for value in selected_ids):
        raise MigrationError("Set database_id in cloudflare.local.json or pass --database")

    try:
        selected_ids = list(dict.fromkeys(str(uuid.UUID(value)) for value in selected_ids))
    except ValueError:
        raise MigrationError("Each database ID must be a D1 UUID") from None
    return account_id, token, selected_ids


def deletion_order(db):
    rows = db.query(
        "SELECT name FROM sqlite_schema WHERE type='table' ORDER BY name"
    )
    tables = [row["name"] for row in rows if not internal(row["name"])]
    dependents = {table: set() for table in tables}
    for table in tables:
        for foreign_key in db.query(f"PRAGMA foreign_key_list({ident(table)})"):
            parent = foreign_key["table"]
            if parent in dependents and parent != table:
                dependents[parent].add(table)

    ordered = []
    visiting = set()
    visited = set()

    def visit(table):
        if table in visiting:
            raise MigrationError(
                "Circular foreign-key references found; no rows were deleted"
            )
        if table in visited:
            return
        visiting.add(table)
        for child in sorted(dependents[table]):
            visit(child)
        visiting.remove(table)
        visited.add(table)
        ordered.append(table)

    for table in tables:
        visit(table)
    return ordered


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--database", action="append", default=[], metavar="UUID",
        help="D1 UUID to clear; repeat for additional project databases",
    )
    parser.add_argument(
        "--dry-run", action="store_true", help="show affected tables and row counts only",
    )
    args = parser.parse_args(argv)

    try:
        account_id, token, database_ids = load_settings(args.database)
        plans = []
        for database_id in database_ids:
            db = D1(account_id, database_id, token)
            tables = deletion_order(db)
            counts = {
                table: db.query(f"SELECT COUNT(*) AS count FROM {ident(table)}")[0]["count"]
                for table in tables
            }
            plans.append((database_id, db, tables, counts))

        for database_id, _, tables, counts in plans:
            print(f"D1 database {database_id}")
            if not tables:
                print("  No application tables found.")
            for table in tables:
                print(f"  {table}: {counts[table]} rows")

        if args.dry_run:
            print("Dry run: no rows were deleted.")
            return 0
        if input("Stop the Worker first. Type CLEAR to delete these rows: ") != "CLEAR":
            print("Cancelled; no rows were deleted.")
            return 1

        for database_id, db, tables, _ in plans:
            if tables:
                db.batch([f"DELETE FROM {ident(table)}" for table in tables])
            print(f"Cleared application rows from D1 database {database_id}.")
        return 0
    except (MigrationError, OSError, ValueError, KeyError, TypeError) as error:
        print(f"D1 clear failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())