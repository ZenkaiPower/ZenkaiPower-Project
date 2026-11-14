# Development-only Dolphin disc tool

The Windows and Linux executables in this directory are used by developer
extraction utilities and synthetic-disc conversion tests. Game releases use the
statically linked nod reader and do not distribute DolphinTool or its runtimes.

`dolphin-tool-source.tar.gz` is the complete matching source used to build both
executables, including pinned dependencies, licenses, the documented MSVC
compatibility patch, build recipes and binary hashes. Its checksum is recorded
in `binaries.json`. Extract it and follow its README.md to rebuild the tools.
The recipes are also retained in `build/` for review.

If distributing these development binaries separately, retain their notices
and make their matching source available. See SOURCE.md for provenance.
