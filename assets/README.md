# Shared application assets

`zp-avatar.png` is the application icon used by Windows and Android builds.

Optional installer artwork is kept locally in the ignored `assets/setup/`
directory. Installers read artwork from `setup/` inside the installed game folder.
See the [release artwork instructions](../port/release/README.md#optional-installer-artwork).

`characters/` contains built-in character content as binary assets and JSON
manifests. Native builds embed them automatically; generated C files stay in
`port/build/generated/`. See [built-in content](../docs/port/future-gohan-outfit.md).
