# Cloudflare matchmaking, accounts and rankings

A Cloudflare Worker for player accounts, matchmaking and battle records. Players
find each other through room codes or ranked/unranked queues; battle simulation
and per-frame inputs remain peer-to-peer.

The Worker implements persistent accounts, ranked/unranked queues, global rankings,
battle results and metadata, and connection reports. Battles remain peer-to-peer.
For deployment (including imported LAN accounts), start with the
[Cloudflare beginner tutorial](docs/CLOUDFLARE_TUTORIAL.md). Deploy using Wrangler:
the full service uses npm dependencies and a Durable Object coordinator, so pasting
`src/worker.js` alone into the dashboard no longer deploys the complete application.

[Full service architecture and operations](docs/CLOUDFLARE_DEPLOYMENT.md) explains
transactions, schema upgrades, limits, and optional TURN configuration.
[LAN_SERVER.md](docs/LAN_SERVER.md) documents the compatible wire API and local server.
[DB_MIGRATION.md](docs/DB_MIGRATION.md) covers lossless database transfers.

Sources: `src/worker.js` handles networking, `src/full_service.js` handles accounts and
statistics, and `migrations/` holds the full D1 schema. `data/schema.sql` retains only
the original room-service bootstrap for legacy tests. The LAN schema and its
upgrade logic live in `src/lan_server.py`.

## Directory layout

- `src/`: Cloudflare Worker and Python LAN server implementations.
- `tools/`: deployment, configuration and database utilities.
- `tests/`: Python server tests.
- `data/`: build signature policy, legacy SQL bootstrap and ignored local databases.
- `migrations/`: versioned D1 schema migrations.
- `docs/`: deployment, API and migration guides.

Private credentials stay in the ignored `cloudflare.local.json` at this folder's root.
Wrangler and npm configuration also stay at the root.

Run Python tests from the repository root:

```sh
python3 -m unittest discover -s server/tests -p 'test_*.py'
```

## Security and ranked settlement

Build hashes and embedded build keys are compatibility filters, not client
attestation or authorization. `/v1/hello` and `/v1/data-check` explicitly return
`build_attestation: false`. A known hash or extracted build key never substitutes
for account tokens and participant ownership. Keep Cloudflare/admin/TURN
credentials server-side, and retain rate limits and relay budgets independently
of these filters.

Ranked results require immutable reports from both participant accounts agreeing
on winner, finish type, and both performance-based base point amounts. The server
applies rank scaling and balance caps once to the agreed amounts. Missing or conflicting reports award no
points. Two colluding accounts remain an abuse risk; this is not trusted game
simulation. See [ranked scoring](../docs/port/ranked-scoring.md) for the protocol,
deployment order, and checks. Apply migration `0003_ranked_result_reports.sql`
before deploying and update both game clients; old host-only reports stay pending.

Each row in `battles` also records `stage_music_picker`: `1` means `player1`
(the host) chose the stage and music, and `2` means `player2` chose them.
The first match uses player 1; rematches use the previous winner, with draws
keeping the same chooser. Older rows have `NULL` because the chooser was not
recorded at the time. LAN SQLite adds this column on startup; Cloudflare D1
adds it through migration `0002_stage_music_picker.sql`.

The table `rooms`, one row per room (rows older than ten minutes are removed whenever a room is made):

| Column | |
|---|---|
| `code` | what the host tells the other player: 6 characters |
| `made` | seconds since 1970 |
| `version` | the game's netcode version: both players need the same |
| `ip` | the host's address as Cloudflare saw it (for the limit of rooms per address) |
| `host_key`, `join_key` | secrets handed to each player, so nobody else can read or end the room |
| `host_name`, `join_name` | the players' names |
| `host_addrs`, `join_addrs` | JSON: the addresses each game can be reached at ("ip:port"), best first |

| `queue` | a quick-match room: which queue it waits in (`S`, `T`, `D10`, `D15`, `D20`); empty for a room with a code to hand on |
| `who` | a quick-match room: a random text the host's game made up for this run, so that a room it left behind is not given back to it |

## Quick match (from 0.1.27)

A player who searches a queue is put with whoever waits longest in it, or, if nobody does, gets a room of their own
in that queue and waits there exactly as a host with a room code does (the same `poll`, `addrs`, `leave`; the same
relay rules and ceilings). Five queues: single battle, team battle, DP battle of 10, 15 and 20 DP; the game gives
all of them no time limit. A room lasts ten minutes; the game searches again by itself when it runs out. Limits:
60 searches an hour from one address, and the four open rooms per address that hosting has. One relay login is
asked for per search, not per room: a search that starts again says it holds a relay address already.

**Updating a Worker that is already set up** (in this order, so that nothing is refused in between):

1. D1 -> `zenkaipower-rooms` -> Console. Run these two lines, one at a time:
   `ALTER TABLE rooms ADD COLUMN queue TEXT;`
   `ALTER TABLE rooms ADD COLUMN who TEXT;`
2. The Worker -> Edit code: replace everything with the new `src/worker.js`, Deploy.

Older releases of the game are not affected: they do not send the new request, and rooms with codes work as before.
Until the Worker is updated, a game that has the Quick match tab says "The matchmaking service does not know quick
match yet." when a search is started.

## Only the newest release (from 0.1.29)

The game tells the service which release it is (`rel`: 129 for 0.1.29). With the variable `MIN_RELEASE` set on the
Worker (Settings -> Variables and Secrets, a plain variable, e.g. `129`), a new room, a join or a quick-match search
from an older game is refused, and so is one from a game that does not say (0.1.28 and earlier). Rooms under way
are not touched. Without the variable, or at 0, everyone is let in.

- When its online screen opens, a game of 0.1.29 or later asks (`/v1/hello`) and, if it is below the minimum,
  shows at the top of that screen: "Update available. Online servers are unreachable until you update.", with its
  own number and the one that is needed. The same words answer a refused room, join or search.
- A second variable, `LATEST_RELEASE`, is optional: a game older than it, but not below the minimum, shows
  "Update available: release 0.1.NN." there and goes on working.
- Raise it when a release must be the minimum; it takes effect at the next request, with no new code.
- A game of 0.1.29 or later that is refused says so: "This release is too old ... Get the newest release."
  An older one shows its own words for an answer it does not know ("gave no room", "gave no answer").
- It closes the service, not the game: two copies of an old release can still play each other by typing an
  address (Host on a port / Join by address), and the source is open, so a changed copy can claim any number.

## The build key: optional release filter (from the release after 0.1.33)

Anyone can build the game from this repository and point it at this Worker, which then spends its keeper's
allowance (the relay above all) and puts a changed game into quick match. So an official release can be built with
a **build key**, a secret of that one release, and signs every request with it; the Worker, once it has been told
the keys, can refuse what is not signed. All of it is off until the keys are set: a game built without a key sends
what it always sent, and a Worker without `BUILD_KEYS` answers as it always did.

**What it is not.** The key is inside every copy of the release (scrambled, so that it is not found by listing the
program's texts, and no better hidden than that): someone who takes the program apart has it. It keeps out a
build made from the source, not a determined person, and it is no check against cheating. That is why each
release has its own key and the Worker can forget one: a key that got out is worth something only until the
release it belongs to is retired.

**What a signed request carries**, as headers:

    X-ZENKAIPOWER-Release: 134                  the release (0.1.34), as `rel` in the body
    X-ZENKAIPOWER-Time:    1791900000           seconds since 1970
    X-ZENKAIPOWER-Sign:    <64 hex digits>      HMAC-SHA256(key, text), in lower-case hex

and `text` is these six lines joined by a line feed (none after the last):

    ZENKAIPOWER-SIGN-1
    POST
    /v1/quick                           the path
    134                                 the release, exactly as in the header
    1791900000                          the time, exactly as in the header
    <64 hex digits>                     SHA-256 of the body, exactly the bytes that are sent

The key is the text itself, as bytes (not decoded from hex). `port/src/gs/net_sign.c` is the game's side.

**The Worker's settings** (Settings -> Variables and Secrets):

| Name | Kind | |
|---|---|---|
| `BUILD_KEYS` | secret | the keys, as JSON: `{"134":"<key of 0.1.34>","135":"<key of 0.1.35>"}`. Not set: nothing is checked |
| `REQUIRE_KEY` | variable | what a failed check means: `off` (or not set), `quick`, `all` |
| `MIN_RELEASE` | variable | as before; a key of a release below it is not accepted either |

| `REQUIRE_KEY` | quick match (`/v1/quick`) | room codes, relay, queue counts | `/v1/hello` |
|---|---|---|---|
| no `BUILD_KEYS` at all | open | open | open |
| not set, or `off` | open; checked, logged, counted | open; checked, logged | open |
| `quick` | **signed only** | open; checked, logged | open |
| `all` | **signed only** | **signed only** | open |

A refused request is answered with status 403 and `{"error":"build key","why":"...","now":<the Worker's time>}`.
`why` is `none` (nothing was signed), `release` (the Worker has no key for that release; or it is below
`MIN_RELEASE`; or the header's release is not the body's), `sign` (not this key's signature of this request: a wrong
key, a changed body, another path), or `time` (signed for a time more than five minutes from the Worker's clock).
The game then says "Quick match needs an official release of the game." (in the online window, where a failed
search is reported), or for a room code "Room codes need an official release of the game. Playing by address
still works."

- **Clocks.** Players' clocks are often minutes or hours off. `/v1/hello`, which the game asks when its online
  screen opens, now also says the Worker's time, and the game signs by that; a request still refused for its time
  is sent once more by the `now` in the refusal. Only if that fails too does the player read "This computer's
  clock is wrong: set its date and time, then try again." Only a holder of the key is told `time`: the signature
  is checked first.
- **What it does not stop.** A signed request can be sent again unchanged for five minutes (the Worker keeps no
  record of signatures; the limits per address still count it). It cannot be changed: body, path, release and time
  are all in what is signed.
- **A `BUILD_KEYS` that cannot be read** (not JSON) gives the Worker no keys, so with `quick` or `all` it refuses
  everyone. Hence the first stage, `off`, and the check below.
- Nothing logs a key or a signature. A failed check is logged as `build key: quick from release 133: none,
  refused` (or `let in`), and counted per day.
- `REQUIRE_KEY` without `BUILD_KEYS` does nothing. Deleting `BUILD_KEYS` therefore opens the Worker again.

**Seeing what it does.** With `ADMIN_KEY` set (see the relay's ceiling below), the status request also answers

    "buildKey":{"releases":[134],"require":"off","today":{"ok":212,"none":40,"release":0,"sign":0,"time":3}}

the releases it has a key for (never a key), the stage, and how today's new rooms, joins and searches checked
out; `"problem"` is there when `BUILD_KEYS` or `REQUIRE_KEY` is not as it should be.

### Step by step, for the keeper

Nothing below needs a tool but a browser, and Python for one line. `<key>` stands for the key you make; it is
written nowhere in this repository and should not be pasted anywhere but the two places named.

**Once: the Worker's code.** Workers & Pages -> the Worker -> Edit code: replace everything with the new
`src/worker.js`, Deploy. (No change to the database.) Nothing changes for anyone yet.

**(a) Make a key.** On your own computer:

    python3 -c "import secrets; print(secrets.token_hex(32))"

It prints 64 letters and digits. Keep it in your password manager, under the release's number. Make a new one for
every release; never reuse one.

**(b) Give it to the build.** On GitHub: the repository -> Settings -> Secrets and variables -> Actions -> New
repository secret. Name `ZENKAIPOWER_BUILD_KEY`, value `<key>`. (Later releases: the pencil beside the secret, paste the
new key.) GitHub never shows it again and hides it in logs. Only a tag's build (`v0.1.34`) is given it; the
build's log then has the line `build key: given`, else `build key: none`.

**(c) Tell the Worker.** Cloudflare: Workers & Pages -> the Worker -> Settings -> Variables and Secrets -> Add.
Type **Secret**, name `BUILD_KEYS`, value (one line, with the quotes, the number being the release: 0.1.34 is 134):

    {"134":"<key>"}

Deploy. Cloudflare does not show a secret again either: keep this whole line in your password manager, because
adding a release means pasting the whole line again with one more entry. Do this **before** the release is
published: a release whose key the Worker does not know is refused once (d) is on.

**(d) Switch it on, in stages.** The variable `REQUIRE_KEY` (type Text, not secret):

1. Leave it out (or `off`) at first. Publish the release. Nothing is refused; look at the status request's
   `buildKey` after a day: `releases` must list your release, there must be no `problem`, and `ok` should grow
   as players update. `sign` or `release` counts from the new release mean (b) and (c) do not hold the same key.
2. `quick`: quick match only for signed releases. Set `MIN_RELEASE` to the first release that has a key at the
   same time: older releases have no key, and unless they are told to update they show their own words for an
   answer they do not know ("The matchmaking service does not know quick match yet.").
3. `all`, if you want room codes and the relay closed to other builds as well.

Going back is the same variable: `off` refuses nothing, at once.

**(e) At each new release.** 1. Make a new key (a). 2. Replace the GitHub secret with it (b). 3. Add it to the
line in your password manager and paste the whole line as `BUILD_KEYS` again (c), e.g.
`{"134":"<key of 0.1.34>","135":"<key of 0.1.35>"}`. 4. Raise `PORT_RELEASE`, tag, let the workflow build, check
its log says `build key: given`. 5. Before publishing the draft, a check that costs nothing: the Linux program
started with `ZENKAIPOWER_SIGN_TEST=1` prints `sign: build key yes ...` and ends (`build key none`: it has no key).

**Retiring a key** (a release was taken apart, or is simply old): raise `MIN_RELEASE` above that release (its
players are told to update), or paste `BUILD_KEYS` again without its entry (its players are told they need an
official release). The first is the kinder one; do both if the key is known to be out.

**A fork** that runs its own Worker makes its own keys, or none: without the secrets everything is as it was.

## What it keeps

Discovery rooms keep the following for at most ten minutes: the room code, the two player names, the addresses each game reported about
itself, and the host's address as Cloudflare saw it (to limit how many rooms one address can open). Accounts and battle/session history are stored separately and survive room cleanup;
see [retention and authentication](docs/CLOUDFLARE_DEPLOYMENT.md).

## Setting up the full service

Follow [the beginner tutorial](docs/CLOUDFLARE_TUTORIAL.md#7-deploy-the-full-worker)
for installation, migration, deployment and game launch commands. `DB` and
`COORDINATOR` are both required. Authentication is enabled by default.

The dashboard-only procedure from older releases deployed only the room-code
service. Use the checked-in Wrangler configuration for this version.

## The relay (optional)

Some pairs of players cannot reach each other directly (strict routers, mobile connections). With a TURN key the
host of a room also gets an address at Cloudflare's TURN service that passes the match's packets on.

1. Realtime -> TURN Server -> Create. Note the Turn Token ID and the API token (shown once).
2. The Worker -> Settings -> Variables and Secrets: add two **secrets**, `TURN_KEY_ID` and `TURN_KEY_TOKEN`. Deploy.

Without the two secrets the Worker works as before, without a relay. The key never leaves the Worker: each host is
handed a name and password that are good for three hours. Cloudflare's TURN is free for the first 1,000 GB a month
(a match is about 50 MB an hour, and only matches that cannot connect directly use it).

## A relay login only when a match needs one (from 0.1.29)

A game of 0.1.29 or later gets no relay login with its room. The two games first try to reach each other directly;
if they have not after four seconds, the host asks (`/v1/relay`, answered only once someone has joined), takes a
relay address, adds it to the room, and the one who joined learns it by asking the room again (`/v1/poll`). Logins
are then used by matches that need the relay, not by rooms: with a login for every room, the first day of quick
match used up the day's 500 on searches that never played through the relay, and the relay was off for everyone
until midnight UTC. An older game is still given its login with the room. `TURN_DAILY_MAX` (a plain variable)
raises the day's count if it is ever reached again; what protects the bill is the measuring below, not that count.

## A ceiling on the relay's use

Cloudflare has no spending limit for its TURN service (a budget alert only sends an e-mail, a day late), and a
relay login lets its holder send as much as they like for three hours. So the Worker measures the relay's use
itself, from Cloudflare's analytics, every five minutes:

- a login that has passed more than 1 GB in three hours is ended at once (a match is about 0.05 GB an hour;
  variable `TURN_LOGIN_GB`);
- when the calendar month's use reaches 500 GB (variable `TURN_MONTHLY_GB`; Cloudflare charges after 1,000),
  every login in use is ended and no more are given until the next month. Rooms go on working, without a relay.

Both directions are counted, which is more than is charged for. The analytics run some minutes behind, so the
ceiling can be passed by what flows in that time. To set it up:

1. Variable `CF_ACCOUNT_ID`: the account's ID (Workers & Pages overview, on the right).
2. Secret `CF_ANALYTICS_TOKEN`: an API token (My Profile -> API Tokens -> Create Token -> custom) with the single
   permission Account -> Account Analytics -> Read.
3. Secret `ADMIN_KEY`: 16 or more characters of your own choosing, for the status check below.
4. Settings -> Trigger Events -> Cron Trigger: `*/5 * * * *`.

Once `CF_ACCOUNT_ID` or `CF_ANALYTICS_TOKEN` is set, no relay login is given unless a measurement has succeeded in
the last two hours: a ceiling that cannot be checked is not relied on. To measure at once and see the result:

    curl -s -X POST $W/v1/status -d '{"admin":"<your ADMIN_KEY>"}'

answers for instance `{"measured":true,"monthGB":0.42,"limitGB":500,"relay":"on","loginsSeen":3,"loginsEnded":0,
"loginsToday":7,...}`, or `"measured":false` and why.

## What a player is told about the other

The name, the public address and port, and for the one who joins the host's relay address. An address of a
private network (192.168.x.x and the like) is handed over only when the two are on one network: seen by the
Worker under the same address (the same network part, for IPv6), or with the same public IPv4 address in what the
games found out themselves.

## Limits against abuse

Anyone can send requests to a Worker, so it counts (table `used`):

- an address may make 20 rooms an hour and have 4 open at a time;
- an address may enter 30 codes an hour that name no room (codes are not to be found by trying);
- an address is given at most 10 relay logins a day, and all addresses together 500 a day (the variable
  `TURN_DAILY_MAX` sets another number). Past that, rooms still work, without a relay. This is the ceiling on what
  the relay can be made to cost: a login lets its holder send through the relay for three hours.

Without the table `used` no relay logins are given at all (rooms work). What the counts do not prevent: many
addresses using up the Worker's own daily requests (100,000 on the free plan; matchmaking then stops until the
next day, at no cost). If a relay key is abused, deleting it (Realtime -> TURN Server) ends every login made from
it at once. A billing notification in the dashboard is worth setting.

## Trying it by hand

    W=https://<worker>.<subdomain>.workers.dev
    curl -s -X POST $W/v1/host -d '{"v":3,"name":"A","addrs":["203.0.113.5:7000"]}'
    # -> {"code":"ABC234","key":"...","you":"<your address>"}
    curl -s -X POST $W/v1/join -d '{"v":3,"code":"ABC234","name":"B","addrs":["198.51.100.9:7000"]}'
    # -> {"key":"...","you":"...","peer":{"name":"A","addrs":["203.0.113.5:7000"]}}
    curl -s -X POST $W/v1/poll -d '{"code":"ABC234","key":"<the host key>"}'
    # -> {"peer":{"name":"B","addrs":["198.51.100.9:7000"]}}

## Limits

The free plan allows 100,000 requests a day. A host asks every two seconds while waiting, so ten minutes of
waiting are 300 requests. A room code is good for ten minutes. One address can have four rooms open.

### Binary compatibility gate

`game_data_signature.json` is bundled into the Worker. `/v1/data-check` echoes a client's signature only
when it is in `signatures`; host/join/quick requests must carry an approved signature in `data`, including
when guests are allowed. The `zenkaipower-binaries-v2` client hashes the executable, companions and bundled SDL,
and checks the presence of the required 68,605 logical data files without reading their contents.
Register final packaged builds with `python3 port/tools/make_online_signature.py --build /path/to/executable`
from the repository root, then deploy the policy. Previous approvals remain unless `--reset` is passed.
The `signatures` object is keyed by hash, with each entry containing the game
`version` and `platform`. Versions come from the packaged artifact's
`game-info.pak`, not the current checkout. Use `--version 1.0.0` only when an
older artifact lacks that metadata. The server still checks the unchanged
`signatures` keys; metadata labels do not grant approval. Historical entries
whose original artifacts are unavailable retain an `unknown` platform rather
than a guessed label. Re-registering the matching artifact fills in its labels.
For Docker Desktop mount permission errors, use `--stdout` and save the JSON
through PowerShell on the Windows host; the root README contains the complete
command. Check the container exit code before replacing the policy. Avoid
redirecting stdout directly onto the policy file: it is also read to preserve
existing approvals, and shell redirection can truncate it before generation.
An empty list rejects every client. See `docs/port/README.md` for the complete definition and tests.

### Source save identity and content hash in battle reports

ZENKAI saves use authenticated **v2** envelopes; v1 files are rejected and must
be replaced with a new save. Every new logical save is assigned a cryptographically random 16-byte
`save_id` (32 lowercase hexadecimal characters). Its identity persists across
normal writes, regardless of the file path. Starting a new game generates a new
ID even when the previous file is overwritten at the same path.

`save_hash` is the 32-byte **BLAKE2b-256 digest of the exact plaintext save
payload** (64 lowercase hex characters). It is recomputed on each write. It
excludes the envelope, nonce, authentication tag and ID: two saves with exactly
the same plaintext bytes always have the same hash even when encrypted bytes
differ. In v2, the ID and hash are authenticated as AEAD associated data. Loading
a tampered ID, digest, or payload is rejected; ID immutability here means the
game's normal save lifecycle preserves it, not that a modified client cannot
forge a new encrypted save.

During online play, each client captures its **original** save metadata before
entering the isolated session save folder. The `battle/connection` report then
contains `save_mode: "regular"` or `"zenkai"`, plus this optional pair for v2
ZENKAI saves:

```json
{
  "save_mode": "zenkai",
  "save_id": "0123456789abcdef0123456789abcdef",
  "save_hash": "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
}
```

The example illustrates the fields, not an actual save's identifiers. Regular
saves never include either hash field. An absent pair means an older client or
unavailable metadata.
Both services require the two fields together (32 and 64 lowercase hex chars)
when supplied and allow legacy reports without them. These are **client-reported
values**: services store them but cannot independently verify their provenance.

The existing JSON columns `battle_sessions.p1_connection` and `p2_connection`
store each player's values separately; **no SQL migration is needed**. Deploy
the updated LAN/Cloudflare service **before** distributing clients that send
the new fields, because older strict validators reject additional keys.

```sql
SELECT wire_uid,
       json_extract(p1_connection, '$.save_mode') AS player1_save_mode,
       json_extract(p1_connection, '$.save_id') AS player1_save_id,
       json_extract(p1_connection, '$.save_hash') AS player1_save_hash,
       json_extract(p2_connection, '$.save_mode') AS player2_save_mode,
       json_extract(p2_connection, '$.save_id') AS player2_save_id,
       json_extract(p2_connection, '$.save_hash') AS player2_save_hash
FROM battle_sessions;
```

Run `python3 port/tools/test_net_save_mode.py` for session capture/restore,
JSON validation/storage and player authorization in the LAN and Worker paths.
Run the save encryption regression `port/tests/save/zenkai.c` for ID persistence,
new-game regeneration, hash determinism and authenticated tamper rejection.


## Allow one online bot account

Bot permission belongs to one existing account UID, not its display name. The
protected account requires its normal access code plus a private 256-bit credential.
Existing account tokens also require that credential on every authenticated request.
Other accounts cannot request bot mode. No database migration is needed.

1. Create the dedicated bot account normally (for example `SparkingBot`) and keep
   its access code private. Setup accepts the username; deployment looks up the
   active account in D1 and authorizes its permanent UID automatically.

2. Generate the credential and a local JSON file using Docker in PowerShell.
   Replace `SparkingBot` with your bot account username. Keep this PowerShell window open:

   ```powershell
   $env:ZENKAIPOWER_BOT_CREDENTIAL = docker run --rm --entrypoint python3 zenkaipower -c "import secrets; print(secrets.token_hex(32))"
   if ($LASTEXITCODE -ne 0) { throw 'Credential generation failed' }
   docker run --rm --entrypoint python3 -e ZENKAIPOWER_BOT_CREDENTIAL `
     --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
     server/tools/bot_config.py --username SparkingBot
   if ($LASTEXITCODE -ne 0) { throw 'Bot configuration generation failed' }
   ```

   This creates ignored `server/bot.local.json` containing only `BOT_ACCOUNT_USERNAME`
   and `BOT_CREDENTIAL_SHA256`. It refuses to overwrite an existing configuration.
   Store the raw credential from `$env:ZENKAIPOWER_BOT_CREDENTIAL` in your password
   manager before closing PowerShell. Never commit or distribute the raw credential.
   Do not rerun the random-generation line unless you intend to rotate the credential.

3. Apply the JSON to Cloudflare using your existing local credentials:

   ```powershell
   docker run --rm -it --platform linux/amd64 --entrypoint python3 `
     --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
     server/tools/deploy_portable.py
   ```

   This resolves the username to an active account UID, deploys the updated Worker,
   and merges the authorization hash into its variables,
   preserving other local variables. It authenticates with `server/cloudflare.local.json`
   and applies pending database migrations through the normal deploy process.
   Keep `bot.local.json` for subsequent deployments. Only the hash is uploaded.

4. Rebuild the game containing the updated network client. Start the bot from the
   PowerShell window where the credential is set:

   ```powershell
   & .\release\bot\ZenkaiPower-bot.exe
   ```

   Choose the rebuilt game executable and enter the dedicated username and normal
   access code. The bot passes the inherited credential to the game automatically;
   it is not saved in bot preferences or the account file. Use HTTPS for online service.

5. To rotate access, generate a new credential, replace the server hash, and restart
   the bot with the new environment value. Old credentials immediately stop working
   for new authenticated requests. To revoke access, remove the hash but retain the UID.

This protects the dedicated account even when a client pretends to be a human.
It controls declared bot mode; modified clients that hide automation are not
attested, and this does not add server-authorized CPU flags to the peer handshake.
LAN service behavior is unchanged. The normal account/IP login rate limits still apply.


### Launch the bot without an environment variable

Place a private file named `bot.local.json` in the same folder as
`ZenkaiPower-bot.exe` (or the Linux bot executable):

```json
{
  "ZENKAIPOWER_BOT_CREDENTIAL": "YOUR_ORIGINAL_64_CHARACTER_PRIVATE_CREDENTIAL"
}
```

Use the original raw credential that produced the deployed hash, replacing the
placeholder. You can then double-click the bot launcher. A nonempty
`ZENKAIPOWER_BOT_CREDENTIAL` environment variable takes priority over the file.
Rebuild the bot launcher once to enable this fallback; no additional game change
is required for it. Missing files remain allowed for LAN use; malformed files
produce an error before launching the game.

This launcher file contains a secret. Keep it private, do not commit it, and do
not include it in release archives. It is different from `server/bot.local.json`,
which contains the account identity and hash for deployment. Copying that server
file beside the launcher will not provide the private credential.


The bot build automatically creates `release/bot/bot.local.json` beside the
executable, including when the executable is already up to date. Pass
`-e ZENKAIPOWER_BOT_CREDENTIAL` to the Docker build command to populate it with
your current private credential. Without that variable, the build creates an
empty template you must fill before online use. Existing files are preserved;
credential rotation requires updating the launcher file yourself. The build
cannot recover a raw credential from `server/bot.local.json`'s hash. This private
file is for your local bot installation only; exclude it from distributed releases.


## Network address retention

A scheduled cleanup runs every five minutes, even without matchmaking traffic.
Expired rooms and their host IP / player connection addresses are deleted after
the existing ten-minute room lifetime (normally within fifteen minutes of creation).
IP-based rate-limit records are removed after their enforcement window: one minute
for connection reports and account hashing, fifteen minutes for account requests,
one hour for room/search limits, and one day for relay limits, plus scheduling delay.
Active rate-limit windows and battle history are preserved. Normal deployments
ensure the five-minute Cron Trigger exists, including with older local Wrangler files.
This removes records from the live application database; Cloudflare logs and
backup retention are separate and are not changed by this cleanup.

Player accounts store `game_version`, the latest authenticated client-reported release number (`rel`, from `PORT_RELEASE`). Login, registration confirmation, session checks and authenticated matchmaking update it when supplied. Older clients leave it unchanged; existing accounts start with NULL. Apply migration `0004_player_game_version.sql` to D1; the LAN server upgrades SQLite automatically. This is the game release, separate from the netcode version `v`.

Accounts also store `created_game_version`, the release reported when the account is created by direct registration or registration confirmation. Later logins, confirmation retries and session requests never change it. Existing accounts and clients that omit the creation release remain NULL. Apply D1 migration `0007_account_creation_version.sql`; LAN SQLite adds the column automatically.

Classic Mode is stored only on battles, as `p1_classic_mode` and `p2_classic_mode`: `1` enabled, `0` disabled, NULL unreported. Each participant captures their own source save's status at battle start and submits it through authenticated `battle/save`; the final `battle/connection` report also carries that captured status as a fallback. Only that participant's field can be filled, and retries never overwrite an existing value. Each rematch reports independently. The status survives the separate online save and process relaunches. Battle get responses include both fields; historical battles and older clients remain NULL.

Apply D1 migration `0006_battle_classic_modes.sql`. LAN SQLite adds the two battle columns automatically. Classic Mode is client-reported battle metadata, not a matchmaking rule or account state.

Battles store immutable `p1_game_version` and `p2_game_version` snapshots of the accounts? reported releases when participants are attached to a new battle, including rematches. Battle get responses include both fields. Existing battles and unreported releases remain NULL. Apply D1 migration `0005_battle_game_versions.sql` after `0004`; LAN upgrades automatically. Database triggers cover both server implementations and preserve snapshots on start retries.
