# Desktop setup storage locations

Folder selection is temporarily hidden in desktop setup. To restore it, set
`kStorageLocationSelection` to `true` in `port/setup/setup.cpp`. Existing
`locations.ini` settings remain active; new installations use Local by default.
The controls described below apply when folder selection is enabled.

The desktop release setup offers independent **Game data location** and
**Save game location** choices on the disc-selection and installed-game pages.
Both default to **Local**:

| Choice | Local | Custom |
|---|---|---|
| Game data | `gamedata/` beside the executable | The selected folder directly contains extracted archives and generated resources. |
| Save games | `savegame/` beside the executable | The selected folder directly contains save files and their `.options` companions. |

Select **Custom**, click **Browse...**, and choose a folder. **Apply locations**
saves the choices; installation and Play also apply pending choices. Setup
checks that custom folders are writable. Selecting another data folder refreshes
disc availability; an empty folder needs installation before Play.

Changing a location does not move or delete existing files. To reuse existing
data or saves, select the folder that already contains them. Returning to Local
uses the original folders again.

## Persistence and launching

Choices are stored in `locations.ini` beside the executable. Custom paths are
absolute UTF-8 paths; empty entries mean Local:

```ini
data=/path/to/game data
saves=/path/to/save games
```

The native installer uses the selected data folder for USA archives, optional
discs, installation markers, and text preparation. The game reads this file
during early startup, including direct launches and title launchers.
Saves retain the existing flat layout: `game.sav`, `zenkaipower.sav`, and
`zenkaipower.sav.options`. The online account file uses the save folder too.

Explicit `ZENKAIPOWER_DATA` and `ZENKAIPOWER_SAVE_DIR` overrides take precedence
over the corresponding entries. Existing `ZENKAIPOWER_SAVES` overrides retain
their legacy isolated-card behavior. Android storage and source-checkout
installation retain their existing paths.

## Verification

```sh
python3 port/tests/setup/storage_locations.py
```

Checks cover default paths, custom paths with spaces, persistence, flat saves,
legacy overrides, invalid selections, and switching to Local. The installer
retry test also checks disc detection after changing the data location and
reloading persisted choices.
