# Disc reference checksums

The optional-disc SHA-1 values in `Native_VerifyDisc` were calculated from the
repository owner's local images. They identify matching reference files, not
independent certification of clean dumps. No disc contents are distributed.

Preflight reads small identifying files, like the US program check, rather than
hashing entire multi-gigabyte images. Asset structure checks still run during
extraction. A passing preflight does not guarantee that unrelated files or all
optional assets are undamaged.

| Disc | SHA-1 | Bytes hashed, in order |
| --- | --- | --- |
| PS2 EU, SLES-54945 | `e8c4c81294ee86723da74b3db59d63930ae64817` | Raw `SLES_549.45`, then `BIN/DBZP.BIN` (2,687,584 bytes total) |
| Wii EU, RDSPAF | `a6eb9199a5d19eef10af7cfa30699fb032fb3f4b` | Decompressed `wzs3eu0.afs` (768,000 bytes) |
| PS2 JP, SLPS-25815 | `fe56a52156ef3d6eb0ad14001736203a77ca95b7` | Raw `SLPS_258.15`, then `BIN/DBZP.BIN` (2,678,608 bytes total) |

The Wii archive is extracted by the bundled Dolphin tool into a unique temporary
directory that is removed afterward. ISO and RVZ containers share this reference.
No Python or separately installed tools are needed by the release installer.

The US disc retains its flattened-program and menu-program checksums from the
decompilation configuration. All selected discs must pass before Install becomes
available. The native installer repeats validation before extracting content.
Other program/menu revisions need separately reviewed references.
