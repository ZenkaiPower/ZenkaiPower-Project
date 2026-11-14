# Local matchmaking and battle records

Future production migration is specified in
[Cloudflare statistics requirements](CLOUDFLARE_STATS_PLAN.md).

Run the local server with Python 3.10 or later (standard library only):

```sh
python3 server/src/lan_server.py --host 0.0.0.0 --port 8787
```

It creates `server/data/lan.sqlite3`. Override its location with `--db PATH`.
London timestamps require the system's IANA timezone database (`Europe/London`).
On systems without it, install the Python `tzdata` package.

On **both players' machines**, start the game using the server machine's LAN IP:

```sh
ZENKAIPOWER_LAN=online ZENKAIPOWER_MATCH_URL=http://192.168.1.10:8787 ./play.sh
```

For two games on the server machine itself, use `127.0.0.1` instead of the LAN IP.
`0.0.0.0` is a server listen address, not the client destination. The variable is
`ZENKAIPOWER_MATCH_URL`, not `T3_MATCH_URL`. `ZENKAIPOWER_LAN=online` makes the Wii battle search use
matchmaking; without it, that screen defaults to direct LAN. Friend Battle room
codes use matchmaking regardless of that setting.

First open User Settings, choose Player Name, and use Initialize Data to register
or log in. Record the recovery code when creating an account. Use a different
account for each player, even when testing two games on one computer.

Use Friend Battle's room codes or the online quick-match queues. Existing direct
LAN discovery is a separate path and does not use this server. Allow inbound TCP
8787 on the server machine if its firewall blocks it. The players still exchange
battle traffic directly over UDP; the server handles HTTP matchmaking and records.
There is no TURN relay, Cloudflare account, or deployment involved.

The server supports the existing game's `/v1/hello`, `/host`, `/join`, `/quick`,
`/queues`, `/poll`, `/addrs`, `/leave`, and `/relay` requests (each path after
`/v1/`). Rooms expire after ten minutes, quick matches pair the oldest compatible
waiting room, and room keys plus account tokens authorize polling, address updates,
and leaving.
The quick-match queues are `S`, `T`, `D10`, `D15`, `D20`, and `RS` (ranked single).
All local players retain their private addresses. Relay requests return `noRelay`.
Cloudflare's release restrictions, build-signature checks, billing controls, and
public-service rate limits are not enabled on this trusted LAN server.

The SQLite schema and its migrations are defined in `lan_server.py` (`SCHEMA` and
`Matchmaking` initialization). The adjacent `schema.sql` belongs to the Cloudflare
Worker and does not define the local battle-statistics tables. Do not import it
to initialize the local server; running `lan_server.py` handles initialization.

After server changes, stop and restart the Python server. After game reporting
code changes, rebuild the game and restart both game processes. Migration alone
does not fill in missing results from older matches.

## Recording a battle

**Joining a room or pairing in quick match now creates a battle entry automatically.**
An empty waiting room does not create a battle record. The new entry contains a
numeric database ID, a separate server-generated wire UUID, the matching time, both player names, and
the queue's battle type. Room-code matches use `unknown` for battle type because
the current room-code protocol does not tell the server which battle format is
selected. Character slots and result fields stay null until supplied.

Both `/join` (including a joining `/quick` response) and authenticated `/poll`
return the record's `uid`. Clients may ignore this extra field and still create
records through matchmaking, provided they authenticate (or use `--allow-guests`
for older protocol tests). Polling does not create duplicate entries.

**The game reports battle statistics automatically when matchmaking returns a UUID.**
The host reports the battle-scene start, selected character rosters, actual battle
format, and completed result. The result includes the winner, signed changes in
displayed fighting points, and duration from the game's battle clock in seconds.
`timeout` takes precedence when the game ends on its time limit; otherwise a
winner with 100% remaining team health is recorded as `perfect`, and other finishes
as `ok`. Ranked Single queue `RS` records `ranked=1`; other queues record `0`.

Reports run on a separate thread and retry up to three times without blocking the
battle. They survive normal room cleanup and in-process return to the menus.
Each rematch gets a separate UUID. Both clients submit ranked results with their
own account token and room key; only the host submits unranked results.
The console prints `zenkaipower: stats: battle/start saved` and `battle/end saved` when the
server acknowledges them, or `failed after retries` if reporting fails.

When a session ends before a result is available, either peer can report an
interruption: `interrupted=1`, an end timestamp, and elapsed wall-clock duration,
with no invented winner, finish type, or point changes. Matching ranked reports
(or the host's unranked result) can supersede an interruption if reports cross in flight. Late interruption reports
cannot overwrite completed results. Normal application exit gives pending reports
up to two seconds to finish; a crash, forced termination, or server outage can
leave an incomplete record. Pending reports are not persisted to disk.

This integration is optional: Cloudflare Workers that do not return a battle UUID
continue to provide matchmaking without statistics calls. Direct LAN connections
that bypass matchmaking have no server session and do not create records here.

The host is player 1 and the joining player is player 2. Player names are copied
from the authenticated room, rather than trusted from result submissions. They
are account usernames; `player1_uid` and `player2_uid` retain permanent account IDs.
`uid` on the API is an opaque UUID for one battle, not a player. In SQLite,
`battles.uid` is an auto-incrementing integer; `battle_sessions.wire_uid` maps
the game’s UUID to that numeric ID. API battle records include `db_uid` so tools
can identify the numeric database row without changing existing game clients.
Each reported rematch needs a new UUID.

Send JSON with POST, using `Content-Type: application/json`. The game currently
also sends JSON without that content-type, which the server accepts.

1. To supply the actual battle start and roster, call `/v1/battle/start` using the **host's room key** and the automatically created `uid`:

   ```json
   {
     "code": "ABC234",
     "key": "HOST_KEY_FROM_MATCHMAKING",
     "account_token": "HOST_ACCOUNT_LOGIN_TOKEN",
     "uid": "e02aca31-3113-4273-a4ae-b526b10ebac5",
     "battle_type": "T",
     "p1c1": 0,
     "p1c2": 8,
     "p2c1": 12
   }
   ```

   Reuse the `uid` returned by matching to enrich the existing row. For the first
   explicit start, a caller-generated UUID is also accepted and replaces the
   pending row's UUID atomically. Once explicitly started, retry using the same
   UUID and metadata if the response is lost. For later rematches, generate a new
   UUID (e.g. `str(uuid.uuid4())` in Python). The response is
   `{"uid":"...","start_datetime":"2026-07-01T13:00:00.000+01:00"}`.
   Supported battle types are `S` (single), `T` (team), `D10`, `D15`, `D20`.
   Characters are game character IDs (integer 0–65535); zero is a valid ID.
   Unused slots are omitted or null. Each roster starts at `c1` and has no gaps;
   single battles have only `c1`. Team and DP battles allow up to five slots.

2. After the battle, call `/v1/battle/end` with the same UUID and the reporting
   participant's own key and account token (host only for unranked):

   ```json
   {
     "uid": "e02aca31-3113-4273-a4ae-b526b10ebac5",
     "key": "HOST_KEY_FROM_MATCHMAKING",
     "account_token": "HOST_ACCOUNT_LOGIN_TOKEN",
     "winner": "p1",
     "finish_type": "ok",
     "p1_points_won": 20,
     "p2_points_won": -20,
     "battle_duration": 125.5
   }
   ```

   `winner` is `p1`, `p2`, or `tie`. `finish_type` is `ok`, `perfect`, `timeout`, or `ringout`;
   omitted finish types default to `ok`. Point fields are signed 32-bit integer
   base amounts from the original battle-performance calculation, before rank
   scaling and balance caps. The server applies that adjustment exactly once.
   Ranked settlement needs matching winner, finish type, and base amounts from both distinct
   active participant accounts. Each side's first report is immutable. Missing
   reports are `pending`, conflicting reports are `disputed`, and neither changes
   balances. `battle.result_status` exposes this state without disclosing the
   other participant's pending report. Unranked records award zero points.
   Duration is seconds of battle time. If omitted, it defaults to server elapsed
   time since `/start`. Explicit duration must be between 0 and 86400 seconds.
   It can exceed wall time because the game's battle speed may be accelerated.
   The response contains `{"battle": {...}}`, using the compatible API formats
   described below rather than the raw numeric database formats.
   Repeating an identical result returns the saved record; conflicting results
   return HTTP 409. Either player's key can read the record through
   `/v1/battle/get` with `uid`, `key`, and that participant’s `account_token`.

For reporting after the lobby room has been removed, send `session_uid` (the
original UUID returned by matchmaking) with the host's key instead of `code`.
This authorizes both the first battle and later rematches without keeping the
room open. The game uses this form automatically.

POST `/v1/battle/interrupt` with `uid`, either player's `key`, and their
`account_token` to mark an
unfinished battle as interrupted. It is safe to retry; completed results remain
unchanged. `/leave` only removes the lobby room and is not treated as an
interruption, since the game also leaves that room on successful connection.

The automatic entry marks the pairing time until `/start` updates it to the
battle-scene start. Battle records
and their authorization survive room expiry, leaving, and server restarts.
Only hashed copies of room keys are retained in `battle_sessions` for this purpose.

Ranked results require agreement from both authenticated peers. This prevents a
single peer forging a ranked result, but colluding clients can still invent
matching reports; it is not trusted simulation. Build hashes/HMAC keys are only
compatibility filters. Battle records require accounts even in legacy guest
discovery mode. See [ranked scoring](../../docs/port/ranked-scoring.md).

## SQLite columns

The `battles` table contains:

| Columns | Meaning |
| --- | --- |
| `uid` | Auto-incrementing integer primary key (`1`, `2`, …) |
| `start_datetime`, `end_datetime` | Integer Unix timestamps in **milliseconds**. These represent absolute instants; the API formats them in Europe/London (GMT/BST). Unfinished end times are null. |
| `player1`, `player2` | Host and joiner account usernames |
| `player1_uid`, `player2_uid` | Permanent numeric account IDs; nullable for historical or guest records |
| `battle_type` | `0` unknown, `1` single (`S`), `2` team (`T`), `3` DP10, `4` DP15, `5` DP20 |
| `interrupted` | `0` normally; `1` if a peer reports an interrupted session, with result fields left null |
| `ranked` | Boolean stored as SQLite integer (`0` = false, `1` = true). Queue `RS` creates ranked single battles (`1`); other queues create non-ranked battles (`0`). |
| `winner` | `0` tie, `1` player 1, `2` player 2; null until completed |
| `finish_type` | `0` ok, `1` perfect, `2` timeout, `3` ringout; null until completed or if unknown in old records |
| `p1_points_won`, `p2_points_won` | Signed point changes; null until completed |
| `battle_duration` | Seconds as a real number: game-clock time for completed battles, elapsed wall time for reported interruptions, otherwise null |
| `p1c1` … `p1c5`, `p2c1` … `p2c5` | Character IDs in roster order; unused slots are null |

Existing databases migrate automatically on server startup. Legacy UUID primary
keys become sequential numeric IDs, ISO 8601 timestamps become Unix milliseconds,
and text battle/result codes become the numbers above. Session links and wire UUIDs
are preserved. The migration is transactional and safe to rerun. Unknown result
fields remain null; `ranked` and `interrupted` default to zero when absent.

The HTTP API keeps its original UUID, ISO 8601 London timestamp, and text-code
format so existing games continue to work. Only the database storage format changes.

Inspect the saved data with a SQLite client, for example:

```sh
sqlite3 server/data/lan.sqlite3 'SELECT * FROM battles;'
```

## Tests

```sh
python3 -m unittest discover -s matchmaking -p test_lan_server.py -v
```

Tests use an HTTP server on an ephemeral loopback port and a temporary database.
They cover room codes, version isolation, concurrent quick matching, authorization,
room expiry, persistent battle records, retries, invalid inputs, signed points,
ties, and London's winter/summer UTC offsets.

To also compile and run the actual C reporting code against the test server
(Linux, GCC, pkg-config, curl, and SDL3 development files required):

```sh
python3 -m unittest discover -s matchmaking -p 'test_*.py' -v
```

The C integration test verifies HTTP reporting, full rosters, displayed point
changes, rematches after room cleanup, finish types, and interruptions. It does
not simulate a complete two-player game.

## Player accounts and Initialize Data

Accounts are required by default on the local server. Restart the server after
upgrading; existing battle history remains intact and receives nullable
`player1_uid` / `player2_uid` columns. Historical names are not automatically
claimed or linked to a newly registered account.

In User Settings, **Player Name** selects the account username. **Initialize
Data** is enabled unless that name is the currently validated login (comparison
is case-insensitive). For a new username, `account/prepare` returns a temporary
six-character recovery code and a secret registration ticket. No account or login
token exists yet. The game displays the code in a native memory-card confirmation
window. **No** discards the local ticket and closes the window, without creating
an account or reserving the username. **Yes** starts `account/confirm` in the
background; only this request creates the account and returns a login token to
save locally. Success is shown after the request completes. For an existing username,
the game opens the Data Center keyboard used by the name editor, allowing exactly
six letters/digits, with no spaces or punctuation. A correct code restores access.
The keyboard keeps its normal opening/closing animations. Account HTTP requests
run on a background thread, outside battle simulation.

The username is the account name throughout the game; there is no separate
display name. Names contain 3–16 ASCII letters, digits, underscores (`_`) or hyphens (`-`).
The name `Player` is reserved (case-insensitive). Names must start with a letter, contain at least three letters, and end with a
letter or digit. Every invalid name shows “Invalid name, please chose another one.”
Spaces and other punctuation are rejected by the editor before saving, by the
account client before any HTTP request, and by the server before preparation or
confirmation. `Pan` and `PAN` identify the same account. The server preserves
the spelling chosen at registration. Editing Player Name selects another account;
it does not rename or delete an existing account.

Recovery codes use six random characters from `ABCDEFGHJKMNPQRSTUVWXYZ23456789`.
Only salted scrypt hashes are stored (`N=16384`, `r=8`, `p=1`); login accepts
lowercase input too. Anyone holding the code can recover the account. Losing both
the code and saved login means losing access; there is no email recovery.
Registration returns the code once. The account exists on the server immediately;
the game saves the issued token only after Yes. Keep the code before closing the
program, including if it cannot save the local token.

Login tokens contain 32 random bytes (64 hexadecimal characters), are hashed in
SQLite, and expire after 90 days. The game validates a saved login when entering
Dragon Net Battle and restores it automatically. The local `net_account.dat`
contains the username, token, expiry, and server URL, never the recovery code.
It is written atomically, uses mode 0600 on Unix, and is ignored by Git. A token
belongs to its server URL; changing `ZENKAIPOWER_MATCH_URL` requires login on that service.
The file holds the latest confirmed account; switching accounts does not delete
server data. Keep each account's recovery code.

New tables:

- `accounts`: numeric `uid`, unique case-folded `username`, `display_name`,
  `code_salt`, `code_hash`, `active` (0/1), `created_datetime` (Unix milliseconds).
- `account_tokens`: `token_hash`, account `user_uid`, `expires_datetime` (Unix ms).
- `account_registrations`: temporary ticket hash, username/display name, code salt
  and hash, expiry, and the account UID only after confirmation. Tickets expire
  after 15 minutes and survive server restarts. Expired rows are cleaned on the
  next preparation. Confirmation retries reuse the same account; competing tickets
  for one name cannot create duplicate accounts. Neither codes nor tickets are
  stored as plaintext. The older explicit `account/register` endpoint still
  registers immediately for API clients; the game UI uses prepare/confirm.
- `account_rooms`: discovery room code, `host_uid`, and nullable `join_uid`.
  The row expires with its room; permanent battle participant IDs remain.

To deactivate an account administratively, set `accounts.active=0` using SQLite.
The server rejects its logins and matching requests. Setting it back to 1 restores
access. `/account/logout` revokes the supplied token. There is no in-game account
administration or username-change operation.

### Account API

All endpoints are JSON POSTs under `/v1/`. Public deployment must use HTTPS;
HTTP is supported for local LAN testing. Account codes and tokens are never sent
to a P2P opponent, included in access logs, or put in curl command arguments.
The confirmation and error messages use the editable port translation catalog;
native Yes/No labels and the keyboard keep their existing localization. Preserve
the `%s` placeholder in `account/save-code`; it is replaced with the generated code.

| Endpoint | Request | Response |
| --- | --- | --- |
| `account/lookup` | `username` | `exists`, `active` |
| `account/register` | `username` | `user_uid`, canonical `username`, `code`, `token`, `expires` |
| `account/login` | `username`, six-character `code` | `user_uid`, canonical `username`, `token`, `expires` |
| `account/session` | `account_token` | `user_uid`, canonical `username`, `active` |
| `account/logout` | `account_token` | Same identity; supplied token is revoked |

The `hello` response advertises `accounts:true`. Duplicate registration returns
409. Invalid, expired, inactive, or mismatched account credentials return 403.
A recovery-code format error returns 400. Failed requests count toward throttling:
5 login attempts per username and 30 per source IP per 15-minute window; other
account requests are limited to 120 per IP in that window. Limits are bounded,
process-local, and reset on server restart. Public hosting needs durable limits
and monitoring before rollout.

Matchmaking requests and battle reporting include `account_token` in addition to
the existing room/session key. The server derives the username from that token,
rejects attempts to use another name, checks active participants when pairing,
and prevents matching an account with itself. Room keys remain private per-room
credentials; they do not replace account authentication. Reporting is authorized
against the participant's numeric account ID as well as the battle key. Rematches
retain both account IDs even after discovery-room cleanup. Queued reports retain
the participating account token if the player later changes accounts.

For existing protocol tests or older clients only, `--allow-guests` disables
account enforcement on matchmaking/reporting. The game account endpoints still
work. Normal play should use the default authenticated server. Direct LAN
connections that bypass the matchmaking service also bypass its account checks.
Ranked results are still client-reported; account authentication does not verify
the reported gameplay outcome.

Run the account tests alongside the existing protocol/reporting tests:

```sh
python3 -m unittest discover -s matchmaking -p 'test_*.py'
```

Account schema migration renames the old `username` to `display_name` and
`username_key` to `username`. `username` is the unique case-folded login identifier;
`display_name` preserves its displayed spelling. Account API responses include both.
Authenticated battle `player1` / `player2` values use `username`; rooms show
`display_name`. Existing battles linked by account UID are updated on startup;
unlinked historical names remain unchanged. Ringout is finish type `3`.

## Account battle totals and non-ranked points

Battle API responses include `stats_ranked` and lifetime counters for each account:
`p1_unranked_battles`, `p1_unranked_wins`, `p1_unranked_losses`, and the corresponding
`p2_` and `_ranked_` fields. Totals follow account UIDs across sessions and either
player side. Only completed, uninterrupted battles with a recorded winner count;
ties increase battles without increasing wins or losses. Unlinked legacy battles
are not assigned to accounts by matching names.

The VS/result record plates fetch these totals asynchronously and show the category
of the current battle. Queue `RS` is ranked; other queues are non-ranked. The
non-ranked result screen skips point calculation and hides the points label,
numbers and change animation. Both client and server record point changes as `0`.
On server startup, completed non-ranked historical records are normalized to zero
points too. Ranked records keep their point changes.

## Ranked single battles

Rank Battle is enabled on the battle menu. Focusing it loads the logged-in
account's current rank points through authenticated `POST /v1/account/stats`.
Its format page reuses Custom Battle's layout and animations with only **1 VS 1**.
The search uses the separate online `RS` queue, even when ordinary battle searches
are configured for direct LAN. It cannot match a non-ranked `S` search.

The server sets `ranked=1` when matching `RS`, enforces single battle rosters,
and preserves the category for rematches. Points use the existing game scoring
calculation and result display. The current point total is the sum of completed,
uninterrupted ranked point changes for that account, clamped to 0–9999. Account
UIDs determine the owner of points; switching player sides does not reset them.
Battle API totals include `p1_rank_points` and `p2_rank_points`.

A bot can queue with `python3 port/tools/lan_bot.py ranked --name SparkingBot --online`.
When launched with the positional `ranked` mode, it automatically queues for ranked 1v1
again five seconds after the session ends or the game process closes. `exit` disconnects without requeuing;
`quit` stops the bot. Selecting a different mode also suspends automatic ranked requeuing.
Each launch first restores the saved bot session from `net_account_bot.dat`
(or `--account-file`). An access-code login is only attempted if session restoration
fails, so requeuing with `--access-code` does not repeatedly log in.
Use the same `ZENKAIPOWER_MATCH_URL` on both clients. This implementation is in the Python
matchmaking server; the hosted Worker needs the account/statistics backend before
it can provide ranked play. Restart the Python server and both games after updating.

Ranked rewards and deductions are scaled by the frozen pre-battle rank gap and
a daily population-based tolerance. See [Ranked points](../../docs/port/ranked-scoring.md)
for the formula, examples, snapshot tables, rounding, and migration behavior.

## Per-player connection diagnostics

`battle_sessions.p1_connection` and `p2_connection` contain JSON summaries for
Player 1 and Player 2. Existing sessions get nullable columns on startup; no
historical measurements are fabricated. `battle/get` exposes each JSON value as
an object alongside the battle record.

Both clients independently post `battle/connection` with `uid`, their session
`key`, account token, and a `connection` object. The key and account determine
which column is writable: a client cannot choose or overwrite the other player's
column. Repeated reports replace only that client's summary and do not change
results, points, or interruption status. Reports run on the existing asynchronous
queue, including retries. Host results and each peer's diagnostics are separate
requests, so diagnostics failure does not invalidate a completed result.

Measurement starts at battle-scene initialization, resets on every rematch, and
stops at the result or disconnection report. It excludes character selection and
result-menu waiting. It is outside rollback state and counts actual network work,
including work performed while resimulating. Abrupt process termination can leave
a summary missing; a null column means no report was received.

| JSON field | Meaning |
| --- | --- |
| `rtt_samples` | Count of accepted input-echo RTT observations |
| `avg_ping_ms` | Arithmetic mean of those round-trip times |
| `min_ping_ms`, `max_ping_ms` | Lowest/highest accepted RTT |
| `jitter_ms` | Mean absolute difference between consecutive accepted RTT samples |
| `duration_ms` | Wall-clock measurement period in milliseconds |
| `input_packets_sent`, `input_bytes_sent` | Input send attempts and application payload bytes |
| `input_packets_received`, `input_bytes_received` | Received input messages and application payload bytes |
| `wait_count`, `wait_ms` | Number and cumulative duration of waits for peer inputs |
| `rollbacks`, `rollback_frames` | Number of rollbacks and total frames scheduled for resimulation |
| `input_delay_frames`, `rollback_window_frames` | Negotiated frame settings |
| `relayed` | Whether the session used the relay rather than a direct peer connection |
| `protocol_version` | Game network protocol version |
| `save_mode` | Optional source save mode (`regular` or `zenkai`), captured before entering the isolated online save folder |
| `save_id` | Optional 32-character lowercase hex random ID for the original v2 ZENKAI save; stable across normal writes, new for each new game |
| `save_hash` | Optional 64-character lowercase hex BLAKE2b-256 of that save's *exact plaintext payload*; recalculated after changes |

`save_id` and `save_hash` must either both be present or both be absent. They
are only allowed with `save_mode: "zenkai"`. Missing metadata is supported for
older clients or unavailable save metadata. The
v2 save envelope authenticates both fields alongside the ciphertext, and the
content hash excludes the encrypted envelope and random nonce. The services
receive these values from the client, so they cannot verify the original save
independently. No schema migration is needed: both fields are stored inside the
existing per-player connection JSON. Upgrade both services before the clients.

RTT uses the existing echoed input timestamps minus the peer's reported holding
time, accepting values from 0 through 4,999 ms. Duplicate and older echoed
timestamps are ignored. This arithmetic mean differs from the in-game meter,
which shows the minimum of its recent RTT window. With no samples, ping and
jitter fields are null. A single sample gives a valid ping but null jitter.
A real 0 ms RTT remains zero. No packet-loss percentage is inferred: redundant
inputs, retransmission and reordering prevent that calculation from these counts.
Payload byte counts exclude UDP/IP headers and relay encapsulation. These
summaries contain no peer IP addresses, session keys, or account tokens.

Example query:

```sql
SELECT uid,
       json_extract(p1_connection, '$.avg_ping_ms') AS p1_avg_ping_ms,
       json_extract(p2_connection, '$.avg_ping_ms') AS p2_avg_ping_ms,
       json_extract(p1_connection, '$.relayed') AS p1_relayed,
       json_extract(p1_connection, '$.save_id') AS p1_save_id,
       json_extract(p1_connection, '$.save_hash') AS p1_save_hash,
       json_extract(p2_connection, '$.save_id') AS p2_save_id,
       json_extract(p2_connection, '$.save_hash') AS p2_save_hash
FROM battle_sessions;
```

Restart the Python server to add the columns, and restart both updated clients.
Tests: `python3 -m unittest discover -s matchmaking -p test_connection_stats.py`
and `python3 port/tools/test_net_save_mode.py`. For save ID/hash integrity and
version 1 compatibility, run the C regression in `port/tests/save/zenkai.c`.

## Equipped character items

`character_items` stores one immutable loadout snapshot per reported character
slot, with numeric `uid` and seven nullable item-ID columns: `item1` through
`item7`. `battles` links all ten roster positions through `p1c1_items_uid` …
`p1c5_items_uid` and `p2c1_items_uid` … `p2c5_items_uid`. Each link is a foreign key
to `character_items.uid`, beside the existing character-ID field (`p1c1`, etc.).

The game reports the first seven `BattleMember.items.id` entries for every occupied
slot at battle-scene start. IDs remain one-based, matching the equipped item data;
0 (empty) becomes SQL NULL. Slot order and gaps are preserved. The eighth internal
entry is not a visible equipped item and is not reported. Item values are stored
as 16-bit IDs (1–65535), allowing the game data to define their meaning.

The optional `battle/start` payload is:

```json
{
  "items": {
    "p1c1": [1, 0, 3, 0, 5, 6, 7],
    "p2c1": [0, 0, 0, 0, 0, 0, 0]
  }
}
```

These fields accompany the usual UUID, credentials, battle type, and character
IDs. Arrays must contain exactly seven IDs (0–65535) or nulls, and may refer only
to occupied roster slots. A known empty loadout gets a linked row with seven NULL
values. A missing loadout, unoccupied character slot, or historical record keeps
a NULL link: it is unknown/unreported, not a fabricated empty loadout.

`battle/get` returns the ten link fields and an `items` map containing reported
loadouts, with empty slots represented as null. Identical start retries reuse the
original links and rows; changing items on an existing explicit start is rejected.
Each rematch captures new loadout rows, preserving the previous battle's items.
The updated client reports items for every occupied character on both teams; old
clients may omit them.

Example query for Player 1's first character:

```sql
SELECT b.uid, b.p1c1 AS character_id,
       i.item1, i.item2, i.item3, i.item4, i.item5, i.item6, i.item7
FROM battles AS b
LEFT JOIN character_items AS i ON i.uid = b.p1c1_items_uid;
```

Server startup creates the new table and adds nullable links to existing battles.
Restart the Python server, game, and bot after updating.
Tests: `python3 -m unittest discover -s matchmaking -p test_character_items.py`.

## Battle stage and music

`battles.stage` and `battles.music` store the numeric stage and music IDs used at
battle start. The game reports the resolved selections, including a randomly
selected song. The stage is the opening stage; later stage transitions do not
replace it. IDs refer to the game data installed on the clients.

`battle/start` accepts optional `"stage": 6` and `"music": 12` fields, and
`battle/get` returns them. Stage IDs accept 0–65535; music accepts -1–65535,
where -1 means explicitly no music. NULL means unknown or unreported, including
historical records and reports from older clients. Start retries must preserve
these values; each rematch records its own stage and music.

Server startup adds the nullable columns to existing databases. Restart the
server, game, and bot to capture these fields in new battles.

```sql
SELECT uid, stage, music FROM battles;
```

## Character costumes

Each roster position has a nullable costume-ID column in `battles`:
`p1c1_costume` through `p1c5_costume`, and `p2c1_costume` through
`p2c5_costume`. The game reports the selected starting costume for every occupied
slot, using the game's zero-based costume ID (0 is the first costume).
Transformations or damage during battle do not replace this starting selection.

These optional fields accompany the character IDs in `battle/start` and are
returned by `battle/get`. IDs accept integers 0–65535 for occupied slots only.
NULL means unreported, historical, or unoccupied; it does not mean costume 0.
Retries preserve the original selections, and rematches capture fresh selections.
Server startup adds these columns to existing databases. Restart the server,
game, and bot after updating.

## Online selection deadline

Online 1v1, team, and DP selection share one 60-second deadline, counted in
synchronized simulation frames. The counter starts with character selection and
continues into the stage/music screen without resetting. It slides from the
center to the right during that transition. Digits and the diamond frame use the
extracted Wii `section-02/resource-047` textures. Loading stalls and frozen frames do
not consume synchronized selection time. Each new selection screen starts at 60.

At expiry, confirmed fighters remain selected. Unfinished 1v1 choices receive a
random unlocked character and valid costume. Teams preserve confirmed members
and fill remaining slots with unused fighters within the DP budget, then finish.
Auto-selected fighters use empty item sets. If stage selection is unfinished,
a random unlocked stage and song are previewed and confirmed automatically.
The bot also chooses a random valid costume for each fighter it selects.

Both players must use the updated client (network protocol 9).
Test: `python3 -m unittest discover -s matchmaking -p test_selection_timeout.py`.

## Global rankings

View Ranking opens the original Wii ranking layout with **Global** as its sole
category. Up/Down scroll three entries at a time and Triangle returns to the
battle menu. Requests run asynchronously, refreshing when the screen is opened.
The list always draws three rows; missing entries show their position number,
the original Z portrait placeholder, no player name, and zero points. Loading and unavailable results use the same empty rows
without status messages or a Confirm-to-reload action. Ranking rows use the Wii title, category
bar, purple panels, Fighting Point label, number atlases and scroll arrows.

`POST /v1/ranking` accepts optional integer `offset` (default 0, maximum 1000000)
and `limit` (default 3, 1–50). This read-only endpoint can be viewed without login.
It returns `offset`, `total` and `entries`; each entry contains `uid`, `username`,
`display_name`, `rank`, `points`, and `character`.

Active accounts are ordered by lifetime ranked points, descending, with username
and account UID as deterministic tie breakers. Positions are consecutive, even
when points tie. Totals use the same 0–9999 clamp and completed, noninterrupted
ranked results as account/stats. Active accounts appear when they have positive
ranked points or at least one ranked win; this keeps zero-point winners on the
list. Accounts without ranked results, zero/negative totals and no ranked wins,
and inactive accounts are excluded. The returned total and pagination count
only eligible accounts.

The profile portrait is the character most frequently selected by that account
across completed, noninterrupted **victories** in ranked **and** casual battles,
on either side. Losses and draws do not count. Every occupied slot on the winning
team counts once per victory. Equal usage counts choose the
lowest character ID. Unfinished searches and missing historical rosters add no
usage. Accounts with no recorded character in a victory use the original Z placeholder.
Portrait textures come from Wii section 1/resource 15, indexed by character ID.

No database migration is required. Restart the Python server for the new endpoint.
Tests: `python3 -m unittest discover -s matchmaking -p test_ranking.py`.

Online selection preserves each player's local customization data: the three item
presets per character, customization levels, saved custom-character records, item
ownership and capacity. Protocol 11 exchanges these profiles in acknowledged
768-byte chunks before shared startup; each side's grid and item panel read that
side's profile on both clients. Original personal saves are restored after the
session, and existing battle-format restrictions still apply. Both games (including
the bot executable) need the updated protocol; the matchmaking server needs no
schema migration for this exchange.

For bidirectional database moves with exact values and verification, see
[LAN SQLite ↔ Cloudflare D1 transfer](DB_MIGRATION.md).

### Required binary signature

Keep `game_data_signature.json` beside `lan_server.py`. The server exposes `POST /v1/data-check` and
refuses host/join/quick requests unless `data` is in the policy's `signatures` allowlist. The game hashes
its executable, companions and bundled SDL, then checks presence of 68,605 required logical resources;
it no longer hashes resource contents on online menu entry. Empty approval lists reject every client.
Register final builds with `python3 port/tools/make_online_signature.py --build /path/to/final/executable`,
then deploy the policy. Update client/server together. See `docs/port/README.md` for the binary roles,
signature definition, excluded files, timings and tests. This is not anti-cheat attestation.
