# Content tools

`build.py` validates `assets/characters/*/manifest.json`, deduplicates binary
payloads and emits `port/build/generated/content_assets.inc`. Both native build
entry points call it automatically before compilation. Generated output is
rewritten only when its contents change, preserving incremental compilation.

A manifest has schema version 1, an optional `costumes` mapping from character
ID to the total costume count, and a `resources` list. Each resource declares:

- `target`: requested path relative to the installed game-data root.
- `base`: existing original pack used for nonvisual entries.
- `asset`: binary source pack relative to the manifest directory.
- `replace_entries`: inclusive first/last pack entries to replace.

Targets must be unique and cannot be used as other resources' bases. Asset
paths must remain inside the content directory. The builder checks pack bounds
and replacement ranges. The runtime honors explicit loose mod overrides first.
