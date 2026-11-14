# Packaged game data

Both `install.py` and the native release setup now install seekable AFS packages:

```text
gamedata/
  pzs3us0.afs
  pzs3us1.afs
  pzs3us2.afs
  pzs3eu0.afs              optional European selector
  pzs3eu1.afs              optional European language subset
  disc.pak                 executable data, movies and disc streams
  mods/                    optional loose overrides
  game-text.pak            generated labels and language catalogs
  game-text/               optional loose text overrides
  dragon-net-battle.pak    optional extracted Wii art, subtitles and voices
  dragon-net-battle/       optional loose overrides
```

Rebuild the game before using these packages. This changes data storage, not
release signing or server authentication. Packages are uncompressed indexed
archives, not encryption or proof that the data is unmodified.

To convert an existing extraction, close the game and run from the repository root:

```bash
python3 port/tools/pack_gamedata.py gamedata
```

The converter merges partial re-extractions into existing packages. It writes a
new temporary archive, flushes it, and replaces the package before deleting the
numbered loose files. A failed operation retains source files. Other files in a
numbered folder are left alone. Run it only while extraction/editing/gameplay is
stopped. `--keep-loose` keeps the original files for development tools that need
loose inputs; loose files take priority over package entries.

Normal gameplay reads seek directly to an entry within its package and cannot
read into the following entry. Existing stdio-based text/image/sound readers get
an independent temporary file containing only the requested entry. These files
are closed by their existing owners; no complete package is unpacked at startup.
Allow temporary disk space for concurrently open streamed assets.

Mods keep priority: `gamedata/mods/pzs3us1/00448.bin` overrides entry 448 in
`pzs3us1.afs`. External stage/song aliases retain their existing priority.
Translations and custom textures/music/stages are not folded into packages.
`ZENKAIPOWER_DATA` still selects a different data directory. Player progress remains in
`savegame/`, separate from the packages.

The native release setup needs no Python to produce packages. Python extraction
retains the original USA disc archives directly; the native setup does the same,
and builds small packages for the optional European subset. Packaged data still
comes from the player's own disc; the ordinary release ZIP does not bundle it.


## Text generation during setup

After disc data and optional European/Wii resources are available, setup runs the
game executable in `ZENKAIPOWER_PREPARE_TEXT=1` mode. This exits before game-memory and
graphics initialization: no first game launch is needed. It synchronously generates
English UI/dialog text, menu-label metadata, the optional European UI catalogs,
and copies bundled label/port catalogs into the working resource tree. It then
writes `gamedata/game-text.pak` and removes the packaged loose files.

The named text package is uncompressed and supports direct per-file lookup and
catalog enumeration. The game reads it without extracting the full tree. Loose
files under `gamedata/game-text/` override the matching packaged file, so a player
can still supply `game-text/translations/port/pt-br.json`, for example.

Setup restores an existing package into its working tree before regenerating it,
keeping existing loose edits in priority. It replaces the package only after
writing a complete new one. Normal startup keeps the older generation fallback
for installations that have no prepared package.

To prepare/rebuild text resources manually after rebuilding the executable, run
from the repository root:

```bash
ZENKAIPOWER_PREPARE_TEXT=1 ZENKAIPOWER_SETTINGS= ZENKAIPOWER_GS=none ./port/build/zenkaipower_64
```

In a release folder, replace `./port/build/zenkaipower_64` with `./Tenkaichi3Decomp`.
The setup executable invokes this mode automatically on Linux and Windows.

External stage and song additions are currently disabled. Their folders, manifests
and extra-stage environment lists do not add entries to the selection menus.
Original disc stages/music and loose replacements for original assets remain supported.

Successful installation removes empty directories beneath `gamedata/`, including
empty text-generation and numbered-asset folders. Files, nonempty directories,
symlinks, and the data root are retained. Directory symlinks are not followed.


## Disc package

Setup also writes `gamedata/disc.pak`, using the same named-entry format as
`game-text.pak`. It contains `SLUS_216.78`, `BIN/DBZP.BIN`, and the remaining
`DATA/` assets (including optional European opening audio). The original `disc/`
tree is removed after successful packaging when it is empty.

The runtime executable-data loader, file cache, audio/video readers, and data
verification support these packaged entries. Loose `gamedata/disc/...` files
still override packaged entries. Setup merges partial additions into an existing
package before replacing it, retaining older entries that were not re-extracted.
The release's executable-stripping and skeleton tools also accept packaged data.

To convert an existing installation, rebuild the game and run the headless
preparation command above. It packages both text and loose disc assets. Stop the
game first and allow temporary space for the old package, working files, and the
replacement package during an update.


## Font package

Release packaging writes `fonts.pak` beside the executable, replacing the bundled
`fonts/` tree. It includes `fonts.tsv`, font files, and their license files;
required license notices are also kept separately in `licenses/`. Runtime font
selection reads the package directly using the existing logical `fonts/...` paths.
Loose files in `fonts/` still override the corresponding packaged entries, so
custom fonts and a custom `fonts/fonts.tsv` remain possible. The source checkout's
`port/third_party/fonts/` is not modified.

## Dragon Net Battle resources

Wii extraction automatically writes `gamedata/dragon-net-battle.pak` and removes
the packaged loose files. Artwork, animation data, subtitles, and English/Japanese
voices load from this package. Setup preparation also converts existing loose
Dragon Net Battle resources. Loose files override matching package entries.

Use `--keep-loose` with `port/tools/extract_wii_online.py` to retain the extracted
files for inspecting its HTML preview. The texture-size generator also reads
packaged resources. Rebuild the game to enable these loaders.

## Online server address

The release root contains `dragon-net-battle.pak`, with a `server.txt` entry.
The release packager builds it from the source-root `dragon-net-battle.txt`.
This is separate from `gamedata/dragon-net-battle.pak`, which contains Wii assets.
The game reads the root package on startup; `ZENKAIPOWER_MATCH_URL` still overrides it.
Development runs can fall back to the source TXT when no package is present.

## Bundled translations

Releases ship `translations.pak` beside the executable instead of a loose
`translations/` directory. Setup reads its label, UI, and port JSON catalogs
and includes them in `gamedata/game-text.pak`. Source catalogs remain editable
in `port/translations/`. External screen-reader translation catalogs are disabled
and `a11y_*.txt` files are no longer shipped.
