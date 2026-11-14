# Setup updates

The top-right **Check for updates** button queries the latest published stable release of
the GitHub repository identified by `source` in `game-info.json`, read from the packaged
`game-info.pak`. This is the only repository configuration: the API endpoint, asset URL
validation, and release-page link all derive from it. Checks and downloads run off the UI
thread. No token is needed.
The installed version comes from `version.txt` in the packaged `game-info.pak` (`game-info.json`
in the source tree). Numeric tags such as `v1.0.1` and `v1.0.10` are compared numerically;
`latest` and prerelease suffixes are not supported as version numbers.

To publish an update:

1. Increase `version` in `game-info.json`, commit, and build the release archives.
2. Tag that commit with the matching version, such as `v1.0.1`.
3. Publish a stable GitHub release marked Latest and upload
   `ZenkaiPower-v<version>-linux-x64.zip` and `ZenkaiPower-v<version>-windows-x64.zip`.
   The Linux archive uses a `ZenkaiPower-Linux/`
   ZIP root; the Windows archive uses `ZenkaiPower-Windows/`.
   The updater requires the asset's `sha256:` digest supplied by GitHub's release API.

The user clicks **Download and update** to install. Setup downloads over HTTPS, verifies
GitHub's SHA-256 digest, validates ZIP paths, and stages files on the installation filesystem.
A copy of the current setup acts as a helper, waits for setup to exit, backs up replaced files,
installs the update, and restarts setup. It refuses to update while the installed game runs.
Ordinary installation failures trigger rollback. This is not a power-loss-safe transaction.
Old release files absent from the new archive are retained.

`gamedata/`, `savegame/`, user texture packs, and bot preferences are excluded from updates.
Only program files, bundled resources, libraries, tools and licenses are replaced. Staging and
backups are removed after a successful update. On Windows a cleanup process waits for the
helper to exit before removing its executable and DLLs. Failed updates retain `.zenkai-update-*`
inside the release directory for recovery. Helper failures are recorded there in `update-error.txt` and
reported with a message box. Do not run multiple updaters against the same installation.

Linux needs `curl`, `sha256sum`, and `unzip` on PATH. Windows needs `curl.exe` and Windows
PowerShell (`Get-FileHash` and `Expand-Archive`). No Python is needed on the user's machine.
Missing tools and network failures are reported without changing installed files. HTTP 404
means there is no published stable release or the repository is unavailable; GitHub rate
limits may also prevent a check. Source checkouts can check versions and open the release page,
but in-place installation is enabled only in packaged releases.

The digest protects against a corrupt download; it is obtained from the same GitHub account
as the archive, not from an independently signed manifest.

Tests (inside the release build container, where SDL3 is installed):

```sh
g++ -std=c++17 -O1 port/tests/setup/updater.cpp -lSDL3 -lpthread -o port/build/setup-updater-test
python3 port/tests/setup/updater.py port/build/setup-updater-test
```
