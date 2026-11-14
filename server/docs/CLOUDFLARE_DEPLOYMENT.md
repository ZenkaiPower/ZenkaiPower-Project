# Full Cloudflare matchmaking service

For the first deployment, use [the beginner tutorial](CLOUDFLARE_TUTORIAL.md#7-deploy-the-full-worker).
The Worker supports the current `lan_server.py` API: accounts, registration tickets,
login tokens, all six queues (including ranked single `RS`), global rankings,
matched battle identities, explicit starts/rematches, results, interruptions, and
per-player connection reports. Character items, costumes, stage and music persist
in the same numeric schema. Imported access codes use the same scrypt parameters.

## Runtime and consistency

`worker.js` retains the existing release/build-signature checks, private-address
filtering and TURN controls. `full_service.js` implements accounts and statistics.
Dependencies are locked in `package-lock.json`; use Node 24 LTS and `npm ci`.

All database operations reach the same named `MatchCoordinator` Durable Object. Its
`blockConcurrencyWhile` gate serializes validation reads and database writes, even
across different edge locations. Each operation's related mutations use **one D1
batch**, which commits or rolls back together. Room pairing commits the room,
account ownership, battle and retained authorization before returning the peer.
Registration confirmation commits the user, ticket ownership and login token
in one batch. Starts commit all loadouts and frozen rank scoring together.
Result/interruption races are serialized and result retries cannot award twice.

The coordinator does not store application data in its own SQLite store; D1 remains
the source of truth and the bidirectional migration tool still works. Every request
uses a D1 session pinned initially to the primary for read-after-write consistency.
Do not run another Worker with another coordinator against the same live database,
or write manually to it while serving traffic: those writes bypass serialization.
Use one Worker/coordinator per database. A copied/staging Worker needs its own D1.

This deliberately uses one coordinator for correctness at modest matchmaking
traffic, not unlimited global throughput. D1 round trips occupy the gate;
password hashing does not. Cloudflare applies a 30-second timeout to a concurrency-blocking
callback. Scale to partitioned coordinators with a redesigned transaction protocol
if this becomes a bottleneck; do not simply shard the current global gate.

### Bounded account hashing

Registration/preparation/login reserve work through the coordinator before any
scrypt runs: at most two outstanding jobs, ten hash jobs per IP per minute, and
thirty globally per minute, in addition to existing per-IP/per-name account
limits. D1 counters persist global/IP budgets across coordinator restarts and
count failed logins. Saturation returns HTTP 429 before hashing. Limits are
conservative fixed defaults in source; monitor account 429 responses when sizing
the service.

The edge Worker computes scrypt outside the global mutation gate, then submits a
single-use completion to the private coordinator binding. These internal routes
are rejected at the public HTTP boundary. Jobs expire after 60 seconds, and
pending jobs are deliberately lost on coordinator restart (clients retry).
Completion rechecks account existence, active status, salt/hash, and username
availability before committing tokens or registrations. Client-supplied hashes
are never trusted. No binding or database schema change is needed for this fix.

The Python server similarly hashes before opening its SQLite write transaction,
with two in-flight jobs and the same per-IP/global hashing budgets. Its limits
are per process and reset on restart; run one server process per database.

The default configuration omits custom CPU limits for Free-plan deployment.
Scrypt registration/login may exceed the Free CPU allowance. For reliable scrypt
operation, use Workers Paid and set `limits.cpu_ms=30000` in the local config. No game simulation or per-frame packets run on
the Worker. TURN, when configured, is a separate service with separate usage costs.

## Schema and upgrades

`migrations/0001_full_service.sql` is additive and idempotent for:

- A new D1 database.
- The current legacy Worker schema (`rooms` with `queue`/`who`, and `used`).
- An import of the current LAN database.

Wrangler records applied versions in `d1_migrations`. Earlier LAN schemas need the
current Python server's migrations before export. No numeric IDs are regenerated.
The full migration adds indexes, so verify a lossless transfer **before** applying
it; a later comparison against the original archive correctly detects those schema
additions. Keep the original database and archive until rollout is verified.

Before an upgrade, stop writes, export a typed backup with `db_migrate.py`, and test
restoring into an unused destination. Apply migrations, deploy, test a login and
both queue types, then reopen access. To roll back code, deploy the prior compatible
bundle/version with Wrangler while retaining additive tables. Do not roll back to
room-only code for authenticated players. A fresh destination can be restored from
the archive and selected in the binding; preserve any newer production data first.

## Local development

From the repository root, configure settings, then change into the service directory:

```bash
python3 server/tools/configure_cloudflare.py
cd matchmaking
npm ci
npx wrangler d1 migrations apply DB --local --config wrangler.local.json
npx wrangler dev --config wrangler.local.json
```

The local D1/DO state lives in ignored `.wrangler/`; these commands do not migrate
the remote database. In another terminal at the repository root:

```bash
ZENKAIPOWER_LAN=online ZENKAIPOWER_MATCH_URL=http://127.0.0.1:8787 ./play.sh
```

Use the port printed by Wrangler if it differs. Stop `lan_server.py` first if it
already occupies 8787.

## Separate staging

Create an empty, separate D1 database in the dashboard. From the repository root:

```bash
python3 server/tools/configure_cloudflare.py --staging \
  --name zenkaipower-server-staging --database-id STAGING_DATABASE_UUID
cd matchmaking
npx wrangler d1 migrations apply DB --remote --config wrangler.staging.local.json
npx wrangler deploy --config wrangler.staging.local.json
```

Point both staging players at that Worker's printed URL. Never attach staging to
the production D1 database. The production configuration remains unchanged.

## Authentication, limits and retention

Authentication is on by default (`ALLOW_GUESTS=false`). Names have 3–16 characters,
start with an ASCII letter, contain only ASCII letters/digits/underscore/hyphen,
end with a letter/digit, and contain at least three letters. The canonical username
is lowercase; the entered casing is retained in `display_name`.

`account/prepare` only creates a 15-minute ticket. `account/confirm` creates the
user, so closing or declining the code screen does not reserve/create the account.
The legacy `account/register` API remains supported and creates immediately.
Codes are six random characters from the existing alphabet; only salted scrypt
hashes are stored. Saved login tokens expire after 90 days and logout revokes the
submitted token. Inactive accounts cannot match or report. Both the participant
key and the account identity must authorize a report; keys alone are insufficient.

Account attempts are counted in D1, including failures: 30 logins per source IP
and 5 per canonical username per fixed 15-minute window; other account requests
share a 120-per-IP ceiling. Fixed windows intentionally differ from the LAN
process's rolling in-memory counters, and survive a Worker redeploy. Hashed IP/name
keys avoid placing those raw strings in the limiter table. Existing discovery and
relay limits remain. Reports allow 120 requests per account and 600 per IP per
minute; duplicate legitimate retries remain within those ceilings. Requests are
limited to 16 KiB while streaming, before JSON parsing.

Rooms expire after 10 minutes. Participant authorization/battle history survives
room deletion; up to 1,000 battles may use one root host key. Mutating reports are
accepted for 30 days after their authorizing battle/session starts. Authorized reads
remain possible afterwards. Historical battles/accounts are not automatically
deleted. Expired login tokens and registration tickets are pruned on issuance;
old limiter buckets are pruned opportunistically by the existing room maintenance.
Set a deliberate backup/history retention policy for your deployment; cleanup
never invents a loss or removes completed history on discovery-room expiry.

Rank scoring mirrors the LAN formula (including daily population tolerance,
start-of-battle score snapshots, whole displayed points, clamps, and zero unranked
points). Ranked and unranked all-time totals are separate. Rankings show active
players with positive points; the profile character counts occupied slots on
winning teams across both modes, with lowest character ID breaking ties.

Results remain host-reported, as on LAN. Account authentication and build
signatures do not prove that a result is genuine. Competitive anti-cheat and
moderation are separate from this API parity implementation.

## Configuration and diagnostics

The generated `wrangler.local.json` contains account/database IDs and binds `DB`
and `COORDINATOR`; it never contains the migration API token. Wrangler browser
login needs deployment rights. A D1-only token can migrate data but cannot deploy
this Worker. `cloudflare.local.json` remains ignored and private.

Keep existing release controls (`MIN_RELEASE`, `LATEST_RELEASE`, `BUILD_KEYS`,
`REQUIRE_KEY`) when upgrading an existing deployment. Account/ranking/report routes
are covered by `REQUIRE_KEY=all`; `quick` restricts only quick matchmaking as before.
A signed report need not contain `rel` in its JSON body. Set optional plain
variables in the local Wrangler config and optional secrets with
`npx wrangler secret put NAME --config wrangler.local.json`.

For TURN setup, retain the [existing relay instructions](README.md#the-relay-optional).
Set secrets with Wrangler, keep the analytics variables and admin key configured,
and add `"triggers":{"crons":["*/5 * * * *"]}` to your local config when using
relay monitoring. Scheduled checks go through the same coordinator. No reporting
endpoint allocates TURN credentials.

Logs:

```bash
npx wrangler tail --config wrangler.local.json
```

Errors return compact JSON with 400/403/404/409/413/429/503 status as appropriate.
Database failures log a bounded category, never raw SQL, payloads, codes or keys.
The public `/v1/hello` returns `accounts:true`. Admin `/v1/status` still requires
`ADMIN_KEY`; account credentials are not admin credentials. Cloudflare dashboard
observability is enabled; monitor CPU, errors, latency and D1 reads/writes.

## Validation

```bash
cd matchmaking
npx wrangler deploy --dry-run --config wrangler.local.json
```

A dry run bundles and validates configuration without deploying. Local tests do
not establish that a production account's permissions, billing, TURN access or
network connectivity are configured; verify a two-game match on staging before
opening the production service.

Runtime references:

- [D1 batch transactions and sessions](https://developers.cloudflare.com/d1/worker-api/d1-database/)
- [Durable Object concurrency gate and timeout](https://developers.cloudflare.com/durable-objects/api/state/)
- [Wrangler configuration](https://developers.cloudflare.com/workers/wrangler/configuration/)
- [Workers CPU limits](https://developers.cloudflare.com/workers/platform/limits/)
