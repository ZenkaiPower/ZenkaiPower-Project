# LAN SQLite ↔ Cloudflare D1 database transfer

`db_migrate.py` copies the application's database in either direction. It preserves
schema SQL, every user table and column, SQLite storage types, rowids, account and
battle IDs, foreign keys, indexes, triggers, views, and AUTOINCREMENT high-water
marks. It does not recalculate scores, rename users, normalize JSON, prune expired
records, extend tokens, or run the LAN server's startup migrations.

This is a database move, not a merge of two independently updated servers. Imports
require a new destination: an unused SQLite filename or an empty D1 database. The
original database is never overwritten. After transfer, follow [Worker deployment](CLOUDFLARE_TUTORIAL.md#7-deploy-the-full-worker)
to apply additive service indexes and deploy the compatible Worker. Verification
against the original archive should be done before schema migrations, since new
indexes intentionally change the schema fingerprint.

Python 3.10+ and its standard library are sufficient. No Wrangler or extra Python
packages are required. D1 operations use the official Cloudflare REST API.

## LAN → Cloudflare

Finish active matches and stop application writes before switching servers. SQLite
exports use SQLite's backup API, so committed data still in `lan.sqlite3-wal` is
included in a consistent snapshot. Exporting does not alter the source database.

From the repository root:

```bash
python3 server/tools/db_migrate.py export sqlite \
  --database server/data/lan.sqlite3 --archive lan-transfer.json

python3 server/tools/db_migrate.py inspect --archive lan-transfer.json
```

Create a **new, empty D1 database**, then set these environment variables:

```bash
export CLOUDFLARE_ACCOUNT_ID="your-account-id"
read -rs -p "Cloudflare API token: " CLOUDFLARE_API_TOKEN
export CLOUDFLARE_API_TOKEN
```

Use its database **UUID**, not the binding name. The token needs D1 Write access
for import and D1 Read access for export/verification. Keep the destination Worker
unbound or in maintenance until the import and verification finish.

```bash
python3 server/tools/db_migrate.py import d1 \
  --database NEW_D1_DATABASE_UUID --archive lan-transfer.json --quiesced

python3 server/tools/db_migrate.py verify d1 \
  --database NEW_D1_DATABASE_UUID --archive lan-transfer.json --quiesced
```

Only after verification succeeds should the compatible Worker be bound to that D1
database and matchmaking reopened. Both commands print per-table counts and the
same SHA-256 fingerprint; they never print account hashes, tokens, or row contents.

## Cloudflare → LAN

Stop application writes on the source D1 database. `--quiesced` declares that you
have done so: paginated REST reads cannot hold a database-wide snapshot lock.
The exporter also reads the database a second time and rejects detected changes.

```bash
python3 server/tools/db_migrate.py export d1 \
  --database SOURCE_D1_DATABASE_UUID --archive cloudflare-transfer.json --quiesced

python3 server/tools/db_migrate.py import sqlite \
  --database server/data/restored.sqlite3 --archive cloudflare-transfer.json

python3 server/tools/db_migrate.py verify sqlite \
  --database server/data/restored.sqlite3 --archive cloudflare-transfer.json
```

The restored file is fully built and verified before it is published under the
requested filename. An existing file is never replaced. Start the LAN server with
the verified destination:

```bash
python3 server/src/lan_server.py --db server/data/restored.sqlite3
```

The copied schema must be compatible with the version of the LAN server you start.
Account hashes and session token hashes remain intact. The game also binds saved
logins to its matchmaking URL, so changing the server URL can require logging in
again even though the database credentials were preserved. Active game processes,
UDP connections, and the Python server's in-memory attempt counters are not SQL
records and are not transferred.

## Exact values and validation

The archive is versioned JSON with explicit type prefixes for NULL, INTEGER,
REAL, TEXT, and BLOB. Integer values cross the D1 API as decimal **strings** to
avoid JavaScript number rounding above `2^53`. Reals cross as 26-digit SQLite
text and are archived as exact binary64 hex values. Imports reconstruct reals from
an exact significand and powers of two, avoiding rare decimal-parser rounding.
Text and BLOB values use hex,
including embedded NUL bytes and original JSON strings. SQL import values are
literals, so large IDs are not sent as JSON numbers or bound floating-point values.

The tool verifies:

- Database integrity and foreign keys before export and after restoration.
- The archive checksum and each table's row count and typed-data checksum.
- Schema, explicit rowids, every typed cell, and ID sequences after importing.
- A local reconstruction of the archive before any D1 import writes.

A repeated row is preserved; an expired token or pending registration is preserved.
Account salts/hashes, room keys, pending registration tickets, battle/session IDs,
rank policy snapshots, items, outfits, stages, music and connection JSON are copied
as stored. Explicit rowids preserve the `rooms` ordering used by matchmaking.
`sqlite_sequence` high-water marks preserve the next account/item/battle IDs even
when larger IDs were previously deleted.

Cloudflare-managed `_cf_*` tables, `d1_migrations`, and SQLite's own optimizer/schema
metadata are platform state and are excluded. Application sequence values are the
exception, and are retained. Database PRAGMAs such as page size/journal mode and
physical SQLite file layout are not application data and are not copied.

The transfer supports the current LAN schema and the existing Worker schema. It
rejects virtual tables, cyclic/self-referencing foreign keys, non-finite REALs,
non-UTF-8 local databases, and statements larger than D1's 100 KB limit rather
than silently dropping or rounding data. It loads the archive/snapshot into memory.

SQLite restoration uses a transaction and exclusive file publication. D1 import
uses transactional batches of up to 100 statements, **not one transaction covering
the whole database**. An API failure can leave a partially populated new D1
destination. Do not route traffic to it; use another empty database and retry from
the unchanged archive. The tool does not delete partially imported databases.

Archives and SQL exports contain account credential material and are created with
private file permissions. Store them as database backups, not in source control.
Checksums detect accidental changes; they are not digital signatures.

## Optional SQL file

An inspected D1-compatible SQL file can also be generated:

```bash
python3 server/tools/db_migrate.py sql \
  --archive lan-transfer.json --output lan-transfer.sql
```

This file creates tables and inserts parents before children, restores sequence
values, then creates indexes/views/triggers. It does not contain unsupported
`BEGIN`/`COMMIT` commands or disable foreign keys. If importing it manually into an
empty D1 database with Wrangler, run the tool's `verify d1` command afterwards.
Use the tool's typed D1 export for the return journey rather than relying on raw
JSON number exports to retain large integer IDs.

Cloudflare references:

- [D1 query API, including batch requests](https://developers.cloudflare.com/api/resources/d1/subresources/database/methods/query/)
- [D1 transactions/batch behavior](https://developers.cloudflare.com/d1/worker-api/d1-database/#batch)
- [D1 import/export](https://developers.cloudflare.com/d1/best-practices/import-export-data/)
- [D1 foreign keys](https://developers.cloudflare.com/d1/sql-api/foreign-keys/)
- [D1 limits](https://developers.cloudflare.com/d1/platform/limits/)
