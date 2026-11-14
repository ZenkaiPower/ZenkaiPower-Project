# Cloudflare database tutorial for beginners

This guide explains how to copy your local matchmaking database to Cloudflare
and bring it back. Run terminal commands on your Ubuntu computer, from this
repository. Dashboard steps happen in your web browser.

The Worker now supports accounts, ranked/unranked matchmaking, global rankings,
battle history, character items/outfits, stage/music, and connection reports.
After copying your database, **section 7 deploys the server**. Copying a database
alone does not create the public server URL.
## 1. Understand the parts

| Name | What it means here |
| --- | --- |
| Cloudflare account | Your account for managing the hosted services. |
| D1 | Cloudflare's SQL database service. It holds a copy of your SQLite data. |
| Worker | JavaScript code that receives game requests and accesses D1. |
| Binding | A connection between a Worker and a database, named `DB` in this project. |
| Account ID | Identifies your Cloudflare account; it is not your email. |
| Database UUID | Identifies one D1 database; it is not its friendly name. |
| API token | A secret password allowing the transfer tool to access D1. |
| Archive | A local JSON backup created by our transfer tool. |

You do not need a domain name, Node.js, Wrangler, or extra Python packages for
the database transfers below. You need Python 3.10 or newer.

## 2. Create your Cloudflare account

1. Open <https://dash.cloudflare.com/>.
2. Sign up, verify your email if requested, and sign in.
3. Open your account dashboard. You do not need to add a website/domain.
4. Find and copy your **Account ID** from the account details. Dashboard layouts
   can change; Cloudflare's [account ID instructions](https://developers.cloudflare.com/fundamentals/account/find-account-and-zone-ids/)
   show the available ways to find it.

Keep the Account ID nearby. Do not confuse it with a Zone ID.

## 3. Create an empty D1 database

1. In the dashboard, open **Storage & Databases → D1**.
2. Choose **Create database** and name it `zenkaipower-transfer`.
3. Finish creation and open the database.
4. Find its **Database ID / UUID** in its details and copy it.

Cloudflare's [D1 getting started guide](https://developers.cloudflare.com/d1/get-started/)
provides the current creation steps. Review the dashboard's plan and usage limits
before enabling services; this guide does not require adding a TURN relay.

**Leave the database empty.** Do not run `schema.sql`, create tables, or attach a
running Worker to it before importing. The importer creates the complete schema
from your LAN database and refuses a destination with existing application tables.

## 4. Create an API token

1. Open your profile's **API Tokens** page:
   <https://dash.cloudflare.com/profile/api-tokens>.
2. Select **Create Token → Create Custom Token**.
3. Name it `BT3 database transfer`.
4. Under permissions, select **Account → D1 → Edit**.
5. Under account resources, include **Specific account** and choose the account
   containing your new database.
6. Review and create the token. Copy the secret shown on the success page.

D1 Edit permits the reads and writes needed for the full transfer. See
[Cloudflare's token instructions](https://github.com/cloudflare/cloudflare-docs/blob/production/src/content/partials/d1/generate-d1-api-token.mdx).

Keep this token private. Do not paste it into source files, screenshots, chat, or
a Git commit. If it is exposed, revoke it on the API Tokens page and create another.

## Optional: keep credentials in a local project file

The file `server/cloudflare.local.json` is automatically read by D1
commands. It is excluded from Git. Fill its three values in your editor:

```json
{
  "account_id": "YOUR_ACCOUNT_ID",
  "api_token": "YOUR_API_TOKEN",
  "database_id": "YOUR_DATABASE_UUID"
}
```

Use the Account ID, not a user ID. Keep the token private; this file contains it
in plain text. If the file is missing, create it and restrict permissions with
`chmod 600 server/cloudflare.local.json`.

With this file filled in, you can skip the environment-variable setup below and
omit `--database` from D1 commands. For example:

```bash
python3 server/tools/db_migrate.py import d1 \
  --archive lan-transfer.json --quiesced

python3 server/tools/db_migrate.py verify d1 \
  --archive lan-transfer.json --quiesced
```

Environment variables override the file's values. An explicit `--database`
overrides both. SQLite commands still require `--database`.

## 5. Prepare your Ubuntu terminal

Open a terminal and run:

```bash
cd ~/Desktop/Tenkaichi3Decomp
python3 --version
python3 server/tools/db_migrate.py --help
```

If your repository lives somewhere else, change the `cd` path.

Set the Account ID and database UUID, replacing the example text inside quotes:

```bash
export CLOUDFLARE_ACCOUNT_ID="PASTE_YOUR_ACCOUNT_ID"
export ZENKAIPOWER_D1_DATABASE_ID="PASTE_YOUR_DATABASE_UUID"
```

Enter the API token without displaying it or putting the token itself into your
shell history:

```bash
read -rs -p "Cloudflare API token: " CLOUDFLARE_API_TOKEN
export CLOUDFLARE_API_TOKEN
```

Paste the token and press Enter. Nothing appears while entering it; that is normal.
These variables apply to this terminal. If you close it, set them again.

## 6. Copy LAN → Cloudflare

Finish active games. In the terminal running `lan_server.py`, press **Ctrl+C** to
stop it. Keep the original `lan.sqlite3` and any `-wal`/`-shm` files together; do
not delete them. The exporter includes committed WAL data automatically.

Export the database:

```bash
python3 server/tools/db_migrate.py export sqlite \
  --database server/data/lan.sqlite3 \
  --archive lan-transfer.json
```

If you normally start the server with a different `--db` path, export that file
instead. Inspect the backup:

```bash
python3 server/tools/db_migrate.py inspect --archive lan-transfer.json
```

You should see table counts and a SHA-256 fingerprint. Import into your empty D1
database:

```bash
python3 server/tools/db_migrate.py import d1 \
  --database "$ZENKAIPOWER_D1_DATABASE_ID" \
  --archive lan-transfer.json --quiesced
```

`--quiesced` means **you have already stopped application writes**. It does not
stop a server for you. Keep this destination unused while importing and checking it.

Verify the copy:

```bash
python3 server/tools/db_migrate.py verify d1 \
  --database "$ZENKAIPOWER_D1_DATABASE_ID" \
  --archive lan-transfer.json --quiesced
```

Continue only if the command succeeds. Verification checks the complete schema,
values, IDs, and sequences against the archive.

At this point you have a verified Cloudflare copy. Your original local database
has not been replaced or deleted. You may restart the original LAN server:

```bash
python3 server/src/lan_server.py --db server/data/lan.sqlite3
```

If you do, new local battles and account changes will not automatically appear in
Cloudflare. This is a snapshot transfer, not continuous synchronization.

## 7. Deploy the full Worker

You can now deploy the full account/ranked service. The steps below upload code
and run additive SQL migrations in the database you select. Keep application
writes stopped until setup is complete.

### Automatic deployment (recommended, including Docker)

You can run the entire process with `deploy_cloudflare.py`. It reads
`cloudflare.local.json`, installs locked dependencies, checks the
bundle, applies additive remote migrations, and deploys. It stops on any failure.
It does not transfer/replace your LAN database; finish section 6 first if needed.

For deployment, edit your Cloudflare token to include **Account → Workers Scripts
→ Edit**, **Account → D1 → Edit**, and **Account → Account Settings → Read**, scoped
to your account. Workers permissions also govern Durable Object deployment. See
[Cloudflare Workers permissions](https://developers.cloudflare.com/workers/authorization/workers/).
If using the dashboard's **Edit Cloudflare Workers** token template, also add D1
Edit. This workers.dev setup does not need a custom-domain route permission.

You can use that deployment-capable token as `api_token`, or keep the D1-only token
and add a separate `deploy_api_token` in the same private file:

```json
{
  "account_id": "YOUR_ACCOUNT_ID",
  "database_id": "YOUR_DATABASE_UUID",
  "api_token": "YOUR_D1_TOKEN",
  "deploy_api_token": "YOUR_DEPLOYMENT_TOKEN"
}
```

No browser login is needed for the automatic script. Tokens are passed through
process environment variables, never command arguments or the generated config.

From the repository root, **rebuild your Docker image** to include Node.js 24/npm:

```bash
docker build --platform linux/amd64 -t zenkaipower .
```

Check everything without changing Cloudflare:

```bash
docker run --rm --platform linux/amd64 \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,source=$(pwd),target=/work" \
  zenkaipower python3 server/tools/deploy_cloudflare.py --dry-run
```

Deploy with the same command **without `--dry-run`**:

```bash
docker run --rm --platform linux/amd64 \
  --user "$(id -u):$(id -g)" \
  --mount "type=bind,source=$(pwd),target=/work" \
  zenkaipower python3 server/tools/deploy_cloudflare.py
```

The credentials file is available through the runtime mount; it is not copied
into the image. Existing local Worker variables, triggers and bindings are
preserved; the database UUID is updated from the credentials file. Use
`--name your-worker-name` to choose the public Worker name. Do not run against a
live database while doing a database transfer. The script performs remote
migrations and deployment only when you omit `--dry-run`.

If Node.js 24/npm are installed on your host, the equivalent command is:

```bash
python3 server/tools/deploy_cloudflare.py
```

After success, use the URL Wrangler prints with the launch command below.
The remaining subsections describe the manual equivalent; you can skip them after
a successful automatic deployment. The default configuration supports Free-plan deployment; see the scrypt CPU
limitation below before choosing a hosting plan.

### Install Node.js

Install **Node.js 24 LTS** using the Linux instructions on the
[official Node.js download page](https://nodejs.org/en/download). It includes npm.
Open a fresh terminal and check:

```bash
node --version
npm --version
```

The Node version should start with `v24`. Python alone is sufficient for database
transfer, but bundling the full Worker requires Node/npm.

### Generate your local deployment configuration

From the repository root, with `cloudflare.local.json` filled in:

```bash
python3 server/tools/configure_cloudflare.py
cd matchmaking
npm ci
```

This creates `wrangler.local.json` using your Account ID and database UUID. It
contains no API token and is excluded from Git. The default public Worker name
is `zenkaipower-server`. To choose another name, use `--name your-worker-name` when
running the Python helper. You do not need a domain name.

### Give Wrangler deployment permission

The D1-only token used by the migration tool cannot deploy Workers. Use Wrangler's
browser login for deployment:

```bash
unset CLOUDFLARE_API_TOKEN
npx wrangler login
```

A browser opens. Sign in to the same Cloudflare account and approve Wrangler's
requested permissions. Your migration tool can still read its token from
`cloudflare.local.json`; you have only removed the terminal environment override.

The configuration omits custom CPU limits so it can deploy on the Workers Free
plan. Scrypt password hashing for registration and login can still exceed the
Free CPU allowance; deployment success does not guarantee login will work. Test
registration and imported-account login before opening the service. For reliable
scrypt operation, use Workers Paid and add `"limits": {"cpu_ms": 30000}` to your
local Wrangler configuration. Review [Workers limits](https://developers.cloudflare.com/workers/platform/limits/).

### Apply the service schema

For an imported LAN database, verify your transfer first using section 6. Then,
from `server`, run:

```bash
npx wrangler d1 migrations apply DB --remote --config wrangler.local.json
```

This also works for a new, empty D1 database or the current original Worker
`rooms`/`used` database. It adds the full schema and indexes without dropping
existing tables. Old LAN schemas should be upgraded with the current LAN server
before export. Do not run `schema.sql` instead; that is the legacy room-only schema.

### Test locally, then upload

Preview the deployment bundle without uploading anything:

```bash
npx wrangler deploy --dry-run --config wrangler.local.json
```

Deploy:

```bash
npx wrangler deploy --config wrangler.local.json
```

If Wrangler asks you to choose a `workers.dev` subdomain, choose a public name
you are comfortable sharing; it becomes part of the server address.

Wrangler prints a URL such as:

```text
https://zenkaipower-server.YOUR-SUBDOMAIN.workers.dev
```

The configuration creates the `COORDINATOR` Durable Object binding and the `DB`
D1 binding. Keep both. Do not paste only `worker.js` into the dashboard editor;
Wrangler bundles the other source file and password-hashing dependency.

### Check the server and launch your game

Replace the example hostname with the exact URL Wrangler printed:

```bash
curl -sS -X POST \
  https://zenkaipower-server.YOUR-SUBDOMAIN.workers.dev/v1/hello \
  -H 'Content-Type: application/json' -d '{}'
```

Expect JSON containing `"accounts":true` and release/time fields. From the
repository root, launch:

```bash
ZENKAIPOWER_LAN=online \
ZENKAIPOWER_MATCH_URL=https://zenkaipower-server.YOUR-SUBDOMAIN.workers.dev \
./play.sh
```

Use the base URL without `/v1`. Both players and bots must use the same URL.
Initialize Data lets players register or log in; imported player access codes
remain valid. A new server URL can require logging in again.

For a ranked bot, from the repository root:

```bash
ZENKAIPOWER_MATCH_URL=https://zenkaipower-server.YOUR-SUBDOMAIN.workers.dev \
python3 port/tools/lan_bot.py --name "SparkingBot" --access-code YOUR_CODE ranked
```

Replace `YOUR_CODE` with that bot account's access code.

Games still connect directly to each other. For players behind restrictive
routers, configure the optional TURN relay using the
[relay instructions](README.md#the-relay-optional) and its usage monitoring.
Do not enable a relay until you have reviewed its separate billing/usage settings.

For local development, staging, upgrades, logs and rollback, see
[CLOUDFLARE_DEPLOYMENT.md](CLOUDFLARE_DEPLOYMENT.md).

## 8. Copy Cloudflare → LAN

Run the commands in this section from the repository root, not `server`.

Stop any Worker/application writes to the source database before exporting. If it
is only the unused copy created above, there are no game writes to stop.

In the terminal where you set the credentials, run:

```bash
python3 server/tools/db_migrate.py export d1 \
  --database "$ZENKAIPOWER_D1_DATABASE_ID" \
  --archive cloudflare-transfer.json --quiesced
```

Restore into a **new filename**:

```bash
python3 server/tools/db_migrate.py import sqlite \
  --database server/data/restored.sqlite3 \
  --archive cloudflare-transfer.json

python3 server/tools/db_migrate.py verify sqlite \
  --database server/data/restored.sqlite3 \
  --archive cloudflare-transfer.json
```

If `restored.sqlite3` already exists, choose another unused filename. The importer
will not overwrite it.

Stop any other LAN server using port 8787, then start the restored database:

```bash
python3 server/src/lan_server.py --db server/data/restored.sqlite3
```

To use this local server from a game on the same computer:

```bash
ZENKAIPOWER_LAN=online ZENKAIPOWER_MATCH_URL=http://127.0.0.1:8787 ./play.sh
```

Another computer must use the server computer's reachable address instead of
`127.0.0.1`. Changing the matchmaking URL can require logging in again.

## 9. Troubleshooting

| Problem | What to check |
| --- | --- |
| Authentication/permission failure | Re-enter the token; check its D1 Edit permission and selected account. |
| Database not found | Use the UUID, not `zenkaipower-transfer`; confirm the Account ID belongs to it. |
| Destination is not empty | Create another empty D1 database or choose an unused local filename. |
| Import fails halfway through | Keep the destination unused. D1 imports use multiple transactions; retry into another empty database. Preserve the original/archive. |
| Export detects changes | Stop the source application's writes, then export again. |
| Address already in use | Another LAN server is already using port 8787. Stop that instance first. |
| Game login/ranking fails on Worker | Deploy the full bundle, apply migrations, check `DB`/`COORDINATOR`, and log in again after a URL change. |
| Local and Cloudflare data differ later | Transfers are snapshots; there is no automatic merge or synchronization. |

Archives contain private account credential material. Keep them out of Git and
store them like database backups. When finished, remove the API token from this
terminal's environment:

```bash
unset CLOUDFLARE_API_TOKEN
```

This does not revoke the token; revoke it in the dashboard if you no longer need it.

For exact preservation rules, supported data types, and technical limitations,
see [DB_MIGRATION.md](DB_MIGRATION.md).
