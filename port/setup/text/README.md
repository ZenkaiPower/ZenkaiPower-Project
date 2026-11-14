# Setup text

Edit `en.json` to change the installer's English text. Keys are stable identifiers;
edit their values. The catalog is separate from the game's translations.

Common groups:

- `welcome.*`: the welcome screen.
- `discs.*`: disc names, descriptions and verification states.
- `tasks.*`: the desktop shortcut page.
- `steps.*` and `progress.*`: installation operations and status.
- `installed.*`: the installed-game screen.
- `buttons.*`, `updates.*`, `version.*` and `links.*`: shared controls.
- `failure.*` and `error.*`: setup and extraction errors.

Save UTF-8 JSON and restart Setup. Use `\n` for a line break. Keep formatting
placeholders such as `%s`, `%d`, `%llu`, `%08lX` and `%%` exactly as they are and in
the same order. Setup rejects edits with incompatible placeholders and uses its
built-in value for that entry. Missing keys also use built-in values. Invalid JSON
falls back to the built-in catalog; details are written to the SDL log.

When running from a source checkout, Setup reads `port/setup/text/en.json`.
Builds embed this file as the fallback, so rebuilding includes your edits in the
standalone installer as well.

Release packages on all platforms include the catalog as `en.json` inside
`setup.pak`. An optional loose `setup/en.json` override takes priority and can be
edited without rebuilding. Automatic updates keep an existing loose override;
new keys use the updated installer's built-in defaults.

The game title and version come from `game-info.json` / `game-info.pak`. Operating
system dialogs and diagnostic output from the game or external tools are supplied
by those programs rather than this catalog. Internal disc IDs, filenames, step
identifiers and event protocol strings remain in code so editing text cannot
change installation behavior.
