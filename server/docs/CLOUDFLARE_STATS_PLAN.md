# Cloudflare battle statistics: historical implementation requirements

The full service is now implemented in `worker.js` and `full_service.js`. See
[CLOUDFLARE_DEPLOYMENT.md](CLOUDFLARE_DEPLOYMENT.md) for the current implementation,
limits and validation. The text below preserves the original design proposal;
its future-work statements and earlier account/ranked decisions are historical.

Status: future implementation plan, reviewed 2026-10-25. This document does not
enable statistics on the deployed Worker or change the production database.

The target is the existing JavaScript matchmaking Worker plus D1. Battles remain
P2P, with the existing TURN fallback when required. Cloudflare handles discovery
and battle records, not battle simulation or per-frame inputs.

## Existing implementation to preserve

| Source | Role |
| --- | --- |
| `worker.js` | Existing Cloudflare matchmaking, release checks, build signatures, private-address filtering, and TURN controls |
| `schema.sql` | Existing Worker `rooms` and `used` tables; currently no statistics tables |
| `lan_server.py` | Reference statistics API, numeric SQLite schema, migration, validation, and result semantics |
| `LAN_SERVER.md` | Local setup, full API examples, numeric mappings, and reporting limitations |
| `../../port/src/gs/net_match.c` | Game HTTP client, UUID/key capture, asynchronous reporting, retries, rematch IDs |
| `../../port/src/gs/net_match.h` | Reporting entry points |
| `../../src/battle/battle.c` | Reports selected character rosters when the battle scene starts |
| `../../src/menu/port_tbo.c` | Reports final winner, finish type, game-clock duration, and displayed point changes |
| `../../port/src/gs/net.c` | Reports interrupted sessions when leaving |
| `test_lan_server.py`, `test_report_client.py`, `report_client_test.c` | Reference server and actual C reporter tests |

The Python process and its `sqlite3` calls are reference behavior, not a Worker
deployment artifact. Add the endpoints to `worker.js` and translate database and
time handling for its runtime. No game rebuild should be required if the wire
contract below stays unchanged. Changing the built-in default service URL would
require a release; `ZENKAIPOWER_MATCH_URL` can override it without recompilation.

## Wire compatibility requirements

Keep all current matchmaking endpoints and response fields. Extend successful
room joining, quick-match joining, and authenticated polling with `uid`, a
canonical UUID string identifying the initial battle/session. Publish it to both
peers before publishing the connected peer state. An empty waiting room must not
create a battle entry.

The numeric database ID is **not** the protocol UUID. The C client enables
statistics only after receiving a 36-character `uid` string, uses it as the
session identity, and derives rematch UUIDs from it. Returning an integer under
this field silently disables current clients' reporting.

Return compact JSON using the Worker's existing `reply()` / `JSON.stringify`
helper. The current C parser recognizes some exact substrings such as
`"role":"host"`; inserting whitespace after colons previously broke matching.
Preserve the existing `peer`, `code`, `key`, `you`, relay, and version fields.
Keep responses within the client buffers: matchmaking uses 1024 bytes and
statistics responses use 4096 bytes. Do not return unbounded history or rosters
in a single response.

The game POSTs JSON; accept its current content-type behavior, including the curl
fallback. Public requests use HTTPS. Preserve the Worker's networking behavior:
the local server's policy of sharing every private address must not replace
`sameNetwork()` / `shown()` filtering on the public service.

### Statistics endpoints

| POST endpoint | Required behavior |
| --- | --- |
| `/v1/battle/start` | Authenticate the host, validate format and rosters, enrich the initial matched row or create a rematch, and return `uid` plus ISO 8601 London `start_datetime` |
| `/v1/battle/end` | Authenticate the host, validate and save the result once; return `{"battle": ...}` |
| `/v1/battle/interrupt` | Authenticate either participant, mark an unfinished record interrupted without inventing results; return the record |
| `/v1/battle/get` | Authenticate either participant and return that record only |

Start accepts `uid`, `key`, `battle_type`, and `p1c1`…`p2c5`. It must accept
`session_uid` instead of a live room `code`: the real game deletes its discovery
room before battle starts. A retained session authenticates the initial battle
and subsequent rematches after room expiry or deletion.

End accepts `uid`, `key`, `winner`, `finish_type`, `p1_points_won`,
`p2_points_won`, and `battle_duration`. Get and interrupt accept `uid` and `key`.
Wire battle types remain `S`, `T`, `D10`, `D15`, `D20`; winners remain `p1`, `p2`,
`tie`; finishes remain `ok`, `perfect`, `timeout`, `ringout`. End defaults an omitted finish
to `ok` for existing callers. Invalid enum values must not default silently.

Battle responses retain those text enums, protocol UUIDs, ISO 8601 London
timestamps with milliseconds and explicit UTC offset, and nullable unknown
fields. Include `db_uid` for the numeric row ID. Match the local server's
remaining fields, including `ranked` and `interrupted` as 0/1.

Use JSON errors with appropriate status: 400 invalid input, 403 unauthorized
statistics access, 404 missing battle/room or route, 409 conflicting state/version
or full room, 429 throttling, and 503 transient service/database failure. Preserve
the existing matchmaking error words and release/build-key responses that the C
client recognizes. Do not return HTML errors or raw SQL errors.

## Numeric D1 storage requirements

Create versioned migrations for `battles` and `battle_sessions`; use the current
`lan_server.py` schema as the reference. Keep the existing `rooms` and `used`
tables intact. Schema bootstrap for a new installation and upgrades to an
existing Worker must both be documented and tested.

| Field | Stored representation |
| --- | --- |
| `battles.uid` | Auto-incrementing integer primary key, stable when metadata is enriched |
| `start_datetime`, `end_datetime` | Integer Unix **milliseconds**, null end until known |
| `player1`, `player2` | Host and joiner usernames copied from matchmaking |
| `battle_type` | 0 unknown, 1 single, 2 team, 3 DP10, 4 DP15, 5 DP20 |
| `winner` | 0 tie, 1 player 1, 2 player 2; null if unknown |
| `finish_type` | 0 ok, 1 perfect, 2 timeout, 3 ringout; null if unknown |
| `ranked` | Integer boolean; all current records and requests remain 0 |
| `interrupted` | Integer boolean, initially 0 |
| `p1_points_won`, `p2_points_won` | Signed integer changes in displayed fighting points; null if unknown |
| `battle_duration` | Real seconds; simulation time for a result, elapsed wall time for an interruption |
| `p1c1`…`p1c5`, `p2c1`…`p2c5` | Character IDs in selection order; unused slots null; ID zero is valid |

Retain `battle_sessions.uid` as an integer foreign key and `wire_uid` as its
unique protocol UUID. Retain hashed host/join keys, start time, request identity,
and the uniqueness constraints needed for retries. The reference `started` value
is internal Unix seconds, distinct from public timestamp columns in milliseconds.
Do not accidentally multiply duration or room expiry times by 1000.

Add indexes for actual lookup paths: protocol UUID, session/host identity, and
queue/version/age. Future leaderboard indexes are a separate feature. Enforce
enum, boolean, uniqueness, and foreign-key constraints in the database as well
as validating requests.

Use prepared statements and bound values. Keep numeric IDs within JavaScript's
safe integer range when exposed as JSON numbers; D1's wider integer storage does
not make arbitrary 64-bit values safe in a JavaScript `Number`.
[D1 type conversion](https://developers.cloudflare.com/d1/worker-api/)

Unix timestamps are timezone-independent instants. Format API timestamps with
`Europe/London` timezone rules, including GMT/BST transitions, rather than a
fixed offset or the Worker's default timezone. Preserve millisecond precision.

## Lifecycle and validation requirements

1. A successful pairing atomically creates one matched record and retains both
   participants' authorization. Quick match knows the format; room-code matches
   initially use numeric type 0 until the host supplies the actual format.
2. Polling only reads this identity. Repeated polling must not create records.
   Removing a discovery room must not remove its battle records or authorization.
3. The first explicit start updates the existing row and start timestamp, keeping
   its numeric ID. Retrying identical metadata returns the existing response;
   incompatible metadata after an explicit start returns 409. Preserve reference
   support for a caller replacing the pending wire UUID on its first start.
4. Each rematch creates a separate row and protocol UUID. The current peers derive
   the UUID by XORing the original UUID's final 32 bits with the rematch counter;
   changing this algorithm requires a coordinated client/protocol change.
5. A valid host result completes the row once. Repeated identical submissions
   return the saved record; conflicting submissions return 409.
6. Either peer can report an interruption. Set its end time and elapsed duration,
   mark `interrupted=1`, and leave winner, finish, and points unknown. An authorized
   host completion may supersede an interruption; an interruption must never
   overwrite a completed result.
7. A successful lobby `/leave` is not an interruption. Do not infer a loss from
   room expiry, a missing HTTP request, or server restart.

Validate character IDs as integers 0–65535, at least one per side, consecutive
slots, at most five, and only one per side for single battle. Reject booleans as
numeric character IDs or point changes. Validate points as signed 32-bit integers
and explicit duration as a finite number from 0 through 86400 seconds. Duration
can exceed elapsed wall time because game speed can be accelerated. Do not
require point changes to sum to zero: the existing game does not guarantee that.

The game determines the result: time limit means `timeout`; otherwise a winner
with 100% remaining team health means `perfect`; all other finishes mean `ok`.
This definition uses final health and is not proof that no damage ever occurred.
Points must match the displayed totals' change, including rounding and clamping.
Do not replace them with a server ranking formula during this migration.

## Atomicity and concurrent requests

The local server serializes writes with SQLite `BEGIN IMMEDIATE`. Do not assume
separate Worker invocations or JavaScript reads/writes have that same protection.

Design and test atomic operations for pairing, room reservation, battle/session
creation, first-start enrichment, rematch creation, and competing end/interrupt
requests. Use conditional SQL updates, uniqueness constraints, and supported D1
batch operations where they meet the required atomicity. Inspect affected-row
counts and re-read an already committed result on a retry. Never rely on an
unprotected "read, see empty, then insert" sequence.

If an operation needs coordination beyond a supported D1 batch, explicitly choose
and document a coordinator such as a Durable Object. Do not assume it makes
separate writes to an external D1 database automatically atomic. Preserve the
database constraints and retry behavior regardless of the coordination choice.
[D1 binding API](https://developers.cloudflare.com/d1/worker-api/d1-database/),
[Durable Objects](https://developers.cloudflare.com/durable-objects/concepts/what-are-durable-objects/)

Return success only after the database commit. A reporting request must not be
acknowledged and then stored solely in an unawaited background operation. If read
replication is enabled later, verify read-after-write behavior for retries and
result reads against the selected D1 session/consistency configuration.

## Authentication, public-service limits, and privacy

Preserve `MIN_RELEASE`, `LATEST_RELEASE`, `BUILD_KEYS`, `REQUIRE_KEY`, and clock
correction behavior. Decide explicitly which signature policy covers the new
routes. The current reporter signs requests through the existing HTTP helper,
but its JSON body does not include `rel`; do not reject valid signatures merely
because that optional body field is absent.

Route `/v1/battle/*` before the Worker's generic six-character room-code lookup,
while retaining the applicable authentication checks. These requests may have no
room code and must work after the room has expired.

Store only hashes of retained participant keys, not raw keys. Host-only start/end
and participant-only get/interrupt checks must survive room cleanup. A release's
embedded build key is not a player account or proof of a genuine battle.

Add explicit reporting limits by authenticated session and source address, body
size limits compatible with the local 16 KiB ceiling, and bounded rematches per
session. Select and document actual limits using expected traffic and the current
Cloudflare plan before public rollout. Avoid throttling legitimate duplicate
retries as if they were new matches. Preserve existing matchmaking and TURN cost
controls; statistics must not request a TURN allocation.

Define retention independently for short-lived rooms, battle history, and the
authorization needed for late reports. A root session can authorize rematches,
so do not delete its authorization while that game session is still allowed to
report. Define how excessively old sessions become read-only or expire.

Do not log raw request bodies, participant keys, signatures, TURN credentials, or
full address lists. Log bounded operation/error categories and aggregate counts.
Keep administrative export/status access separate from participant endpoints.

## Migration, configuration, and deployment

- Add checked-in Worker configuration, D1 binding `DB`, versioned migration SQL,
  and reproducible local/staging/production commands. Use a separate staging
  database and endpoint. Recheck current limits and pricing before deployment;
  this plan does not promise a particular free allowance.
- Apply additive tables/indexes first, then deploy routes, then enable UUID
  responses. Do not advertise statistics support until all reporting routes and
  storage are ready. Old clients may ignore `uid` and continue matching.
- Back up D1 before migration and test restoration. Back up local SQLite using
  SQLite's backup/export facilities so WAL contents are included. Do not copy
  only an open main `.sqlite3` file and assume it is a consistent snapshot.
- Decide whether local test history belongs in production. If imported, preserve
  protocol UUIDs and session links. An empty destination can preserve numeric IDs;
  a populated destination needs an explicit collision/remapping policy for both
  tables. Preserve timestamps and nulls. Do not fabricate older missing results.
- D1 supports SQLite data imports, but its enforced foreign keys require a
  D1-compatible migration plan. Do not copy the Python migration's
  `PRAGMA foreign_keys=OFF` literally; use supported deferred constraints and
  ordered operations, then validate all references.
  [Import/export](https://developers.cloudflare.com/d1/best-practices/import-export-data/),
  [foreign keys](https://developers.cloudflare.com/d1/sql-api/foreign-keys/),
  [versioned migrations](https://developers.cloudflare.com/d1/reference/migrations/)
- Record an explicit schema version and inspect row counts, mappings, uniqueness,
  numeric storage types, and referential integrity after migration. Restarting or
  redeploying must not renumber existing numeric IDs.
- Roll back application behavior by disabling UUID advertising and new reporting
  writes if needed. Keep the additive data tables and allow safe in-flight
  requests during the chosen rollout window; do not drop history as a rollback.

The game launch override for staging is:

```sh
ZENKAIPOWER_LAN=online ZENKAIPOWER_MATCH_URL=https://YOUR-STAGING-WORKER ./play.sh
```

`ZENKAIPOWER_MATCH_URL` takes the service base URL, without `/v1`. Both games must use the
same service. A local HTTP address is for LAN testing; use HTTPS for Cloudflare.

## Observability and reporting limitations

Track endpoint success/failure, retry/conflict/throttle counts, latency, database
errors, matched/running/completed/interrupted counts, unmatched session age, and
read/write usage. Set operational alerts and backup/restore procedures before
public launch. Configure/report storage and request ceilings for the chosen plan.

The current reporter is FIFO, asynchronous, and retries each job three times.
It logs `zenkaipower: stats: ... saved` or `failed after retries`. Its pending queue is
in-memory; normal exit allows up to two seconds to flush. A process crash, forced
termination, or long outage can leave an incomplete record. The Worker cannot
recover missing character/result information by observing matchmaking alone.

Persistent client outboxes, longer/backoff-aware retry policies, acknowledgments
across application restarts, and automatic classification of abandoned sessions
are separate features. Specify them before promising guaranteed delivery. A
cleanup policy must not label an unreported result a defeat or timeout.

## Acceptance tests and release gate

Reuse the scenarios in the local HTTP and C reporting suites; add Worker/D1 tests
against real supported transactions and a staging deployment. The existing C
reporter integration test targets the Python server, not Cloudflare.

- [ ] Existing Worker matchmaking, version/signature checks, private-address
  filtering, same-network play, and TURN tests continue to pass.
- [ ] Both host and joiner receive the same UUID before play; responses are
  compact, within client limits, and readable by the actual C parser.
- [ ] Room codes and every quick-match format save one matched record per pair.
  Concurrent clients cannot take the same slot or create duplicate records.
- [ ] First start enriches one numeric row; identical retries are harmless and
  conflicting metadata is rejected. Rosters preserve ID zero and empty slots.
- [ ] Start/end/get/interrupt work after `/leave` and ten-minute room expiry.
  Rematches get distinct UUIDs/rows while retaining valid session authorization.
- [ ] Every numeric mapping and timestamp conversion round-trips; GMT/BST dates
  are correct, and stored IDs/times/enums remain numeric. Unknowns stay null.
- [ ] Winner/finish/points/duration match a full two-player game's results screen
  and game clock, including ties, timeouts, full-health wins, rounding, and clamps.
- [ ] Duplicate result submissions, a lost response after commit, concurrent
  end/interrupt requests, and database failures cannot corrupt results or award
  points twice. Interrupted matches do not invent results.
- [ ] Unauthorized, malformed, overlarge, expired, replayed-conflict, and excessive
  requests fail safely; no secret values appear in errors or logs.
- [ ] Restart/deploy and old-client compatibility pass. Workers without UUID
  support still allow current clients to match without sending statistics.
- [ ] Migration/import, ID collision policy, backup restoration, staged rollout,
  rollback, operational limits, and monitoring are demonstrated and documented.

## Future player accounts: username and recovery code

Status: agreed future implementation plan, 2026-10-25. This section specifies
Cloudflare work. The local server and game now implement this flow; see
[local accounts](LAN_SERVER.md#player-accounts-and-initialize-data) for the current
API, six-character codes, token lifecycle, and limitations. Apply the same account
model to the future Cloudflare service.

Each account has one unique username. There is no separate display name. Use the
username throughout the game and matchmaking, with a permanent internal numeric
user ID linking the account to battle history and future rankings.

### Registration and login flow

1. The player enters the game and registers a username. The server enforces
   case-insensitive uniqueness: `Goku` and `goku` cannot be separate accounts.
2. The server creates the permanent numeric user ID and generates a strong random
   secret recovery code. The code acts as the account's password.
3. The game displays the recovery code and tells the player to save it to avoid
   losing access to their username. Explain that losing both the saved login and
   recovery code means losing account access; this plan has no email or other
   recovery method.
4. The game saves a separate login token locally for automatic login on later
   launches. Keep this token separate from the recovery code.
5. To restore access or use another computer, the player enters their username
   and recovery code. The server validates them and issues a new login token.
6. Before pairing players or allowing a match to start, matchmaking validates the
   login token and checks that the account is valid, active, and eligible to match.
   Enforce authentication when creating or joining rooms, entering queues, and
   submitting battle results; registration alone does not authorize later requests.
7. Battle records reference both participants' permanent numeric user IDs. The
   server derives account identity from authentication rather than trusting a
   username supplied in a request. Use usernames as the player-facing names.

### Credential and account requirements

- Generate recovery codes on the server and store only their hashes in the
  database. Limit failed login attempts to resist guessing.
- Anyone holding a recovery code can access that account. Keep recovery codes
  and login tokens private, separate from public room codes, and out of logs.
- Never send recovery codes or login tokens to the opponent. The matchmaking
  server authenticates accounts; the P2P connection receives only the information
  needed to connect and play.
- Use HTTPS when the service becomes public. Local HTTP remains for LAN testing.
- Define token expiration, revocation, and account activation/deactivation before
  implementation. An inactive account must not be allowed to start a new match.
- Account authentication establishes who is playing. Battle-result validation is
  separate work, especially before rankings: authenticated client reports alone
  do not prove that a result is genuine.

### Account acceptance checks

- [ ] Registration rejects case-insensitive duplicate usernames and returns a
  recovery code plus a separate login token for a newly created account.
- [ ] A saved login token supports automatic login; username plus recovery code
  restores access and issues a new login token.
- [ ] The registration screen explains saving the code and the consequences of
  losing both credentials; no separate display-name field is introduced.
- [ ] Invalid credentials and inactive accounts cannot create/join rooms, enter
  queues, or start matches; unauthorized result submissions are rejected.
- [ ] Battle history uses permanent account IDs and shows the corresponding
  usernames. Credentials are never exposed to peers, logs, or public responses.
- [ ] Recovery codes are stored as hashes, failed logins are limited, and public
  account traffic uses HTTPS.

## Separate requirements before ranked play

Keep `ranked=0` during this migration. Public rankings need a separate design for
authenticated persistent player IDs, username policy, rating/season rules,
disconnects and disputes, report agreement or stronger verification, farming and
collusion controls, moderation, privacy/retention, and leaderboard pagination.
Host-submitted results and release signatures alone are not sufficient evidence
for trusted rankings. Document and test those decisions before enabling ranked
writes or using these records to award competitive ratings.
