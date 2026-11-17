# Built-in character content

Binary source assets and a manifest live under
`assets/characters/future-gohan/`. The manifest declares resource targets,
original base packs, replacement entry ranges, and costume counts. The generic
runtime (`port/src/plat_content.c`) contains no character-specific file IDs.
`ChrTbl_WrapCostume` asks this registry for additional costume selections.

Desktop and Android compilation invoke `port/tools/assets/build.py`. This
validates manifests, deduplicates binary payloads, and writes embedding code to
`port/build/generated/content_assets.inc`. This directory is ignored by Git.
The checked-in binary model assets are embedded directly during compilation.
Installed game-data files are not modified.

The resource cache and fingerprint reader both use the same content loader.
Loose files under `gamedata/mods` retain precedence. The loader composes an
independent seekable temporary pack with the installed game's original data;
only the manifest's visual entries are replaced. Base-resource cycles and
invalid pack offsets are rejected during asset generation.

Future Gohan's third and fourth outfits have separate clean starting and torn
damaged models for both forms. Both outfits have a missing left arm and retain
the full blue sleeve. Outfit 3 uses orange clothing; outfit 4 uses outfit 2's
yellow-and-blue colors. The asset README lists their registration and contents.

Run loader, portrait-preservation, costume-registration and boundary-read tests:

```sh
python3 port/tests/content/outfits.py
```

These tests compile a native harness with address and undefined-behavior
sanitizers and use synthetic original packs; they do not need installed data.
Visual review must additionally cover both forms and damage states in-game.

For developer visual review, a non-release executable supports
`ZENKAIPOWER_DEMO=1`, `ZENKAIPOWER_CHARA=159` (or `160`),
`ZENKAIPOWER_COSTUME=2`, and `ZENKAIPOWER_OUTFIT_REVIEW=1`.
The last flag creates two idle teams so health and switch portraits remain
visible alongside the model. Set `ZENKAIPOWER_VARIANT=1` for the damaged slot;
unset it for the normal slot. The demo override re-finalizes the starting form
after changing the roster, ensuring the requested model actually loads.

Use `ZENKAIPOWER_COSTUME=3` to visually review outfit 4.

Availability requires `PORT_ZENKAI_EXTRA_OUTFITS=1` (the default) and
`Port_ZenkaiPackEnabled()`. Classic Mode and original mode retain the original
two costumes and original resources. Set `-DPORT_ZENKAI_EXTRA_OUTFITS=0` to
exclude built-in outfit payloads from compilation. The resource cache discards
old streams when pack availability changes. Visual review must use an active
offline Zenkai save with Classic Mode off.

Base Future Gohan's normal grab uses the Super Saiyan form's attacker, paired
victim and recovery motions (IDs `0x96..0x98`) when
`PORT_ZENKAI_FUTURE_GOHAN_GRAB=1` and the offline Zenkai pack is active.
This independent feature applies to every outfit, including the original two;
Classic Mode retains the original grab. Donor motion entries are cloned from
the installed game's animation pack at runtime and released at battle cleanup.
Other animations, attacks, voices and parameters retain the base form's data.
