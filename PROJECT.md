## ZENKAI Power!

This is the repository for the ZENKAI Power! project, including the source code, assets, and tools for building and releasing.

The following instructions are for buiding and releasing a new version.

### Setup

Requires `Docker`.

Build the Docker image for the project.

```powershell
docker build --no-cache --platform linux/amd64 -t zenkaipower .
```

For removing the image, run:

```powershell
docker rmi -f zenkaipower:latest
```

### Prepare

Set `version` in [game-info.json](game-info.json) without a `v` prefix. 

Confirm `source` points to this project's GitHub repository.

### Build 

Build game for all platforms.
```powershell
$version = (Get-Content -Raw game-info.json | ConvertFrom-Json).version
$env:GITHUB_REPOSITORY = 'ZenkaiPower/ZenkaiPower-Project'
$env:ZENKAIPOWER_RELEASE_TAG = "v$version"
.\port\release\build.ps1 all -Release
.\port\release\build.ps1 android -Release
```

### Sign 

Add [online signature](server/data/game_data_signature.json) to the builds.
```powershell
& {
$version = (Get-Content -Raw game-info.json | ConvertFrom-Json).version
$policyOutput = & docker run --rm --platform linux/amd64 --entrypoint python3 `
  --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
  port/tools/make_online_signature.py --stdout `
  --build build/ZenkaiPower-Linux/ZenkaiPower `
  --build build/ZenkaiPower-Windows/ZenkaiPower.exe `
  --build "release/ZenkaiPower-v$version-android-arm64.apk"
if ($LASTEXITCODE -ne 0) { throw 'Signature generation failed; policy was not changed.' }
$policyJson = $policyOutput -join "`n"
$policy = $policyJson | ConvertFrom-Json -ErrorAction Stop
if ($policy.scheme -ne 'zenkaipower-binaries-v2' -or
    $null -eq $policy.signatures -or $policy.signatures -is [array]) {
  throw 'Invalid signature policy; policy was not changed.'
}
[System.IO.File]::WriteAllText(
  (Join-Path $PWD 'server/data/game_data_signature.json'),
  $policyJson + "`n", [System.Text.UTF8Encoding]::new($false))
}
```

### Deploy

Deploy changes to the matchmaking server.
```powershell
docker run --rm -it --platform linux/amd64 --entrypoint python3 `
  --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
  server/tools/deploy_portable.py
```

### Release

Create a new tag in GitHub with the same version as in `game-info.json`. The tag name must be prefixed with `v`.
![](figs/new_tag.png)

Release title:

```
ZENKAI Power! <version with v>
```

Release description follows the format:

```
**ZENKAI Power!**
---
**Version: <version without v>**

⚠️ <Important 1 if any, e.g., "This release must be installed manually by replacing the existing installation files.">
⚠️ <Important 2 if any>

**Added** <if any>
- Item 1 <with subtitems need ":", with no subitems need ".">:
  <figure here if any>
  - Subitem 1.
    <figure here if any>
  - Subitem 2.
    <figure here if any>
- Item 2:
  - Subitem 1.
  - Subitem 2.
- Item 3.

**Changed** <if any>
- Item 1
- Item 2
- Item 3

**Fixed** <if any>
- Item 1
- Item 2
- Item 3
```

### Bot

Compile the bot for all platforms.

```Powershell
docker run --rm --platform linux/amd64 --entrypoint /bin/sh -e ZENKAIPOWER_BOT_CREDENTIAL --mount "type=bind,source=${PWD},target=/src" -w /src zenkaipower port/bot/build.sh all
```

Generate a new bot credential. This will generate [bot.local.json](server/bot.local.json) with the new credential.

```powershell
docker run --rm --entrypoint python3 -e ZENKAIPOWER_BOT_CREDENTIAL `
  --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
  server/tools/bot_config.py --username SparkingBot
```

Deploy the bot to the server.

```powershell
docker run --rm -it --platform linux/amd64 --entrypoint python3 `
  --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
  server/tools/deploy_portable.py
```

Copy the [bot.local.json](server/bot.local.json) to the same directory as the bot executable. The bot will read the credential from this file.

### Server Credentials ⚠️

Server credentials are stored in `server/cloudflare.local.json`. This file is not included in the repository for security reasons. To deploy the server, you must create this file with the following structure:

```json
{
  "account_id": <account_id>,
  "api_token": <api_token>,
  "database_id": <database_id>
}
```

### Factory Reset Server ⚠️

```Powershell
docker run --rm -it --platform linux/amd64 --entrypoint python3 `
  --mount "type=bind,source=${PWD},target=/work" -w /work zenkaipower `
  server/tools/clear_d1.py
```