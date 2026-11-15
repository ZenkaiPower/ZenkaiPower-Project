# Giant-compatible attacks

## Scope and behavior

These attacks explicitly support a giant as the target of their catch or cinematic
interaction. This is the complete list of 24 character/form-and-attack combinations
carrying flag `0x08000000` in the locally supplied US game data, across all 161
character/form entries. Ordinary beams and projectiles can also damage giants
without this flag, so this table is not a list of every attack that can hurt them.

The first and second offensive blasts are technique slots 2 and 3; the Ultimate
is slot 4. Support skills (Blast 1 in the game's terminology) are not those slots.

| Character / form | Attack | Slot |
|---|---|---|
| Goku (Early) | Spirit Bomb | Ultimate |
| Goku (End) | Super Spirit Bomb | Ultimate |
| Goku (End) Super Saiyan 3 | Dragon Fist | Ultimate |
| Goku (GT) | Dragon Fist | Second offensive blast |
| Goku (GT) Super Saiyan 3 | Dragon Fist | Ultimate |
| Goku (GT) Super Saiyan 4 | Dragon Fist | Ultimate |
| Kid Goku | Penetrate! | Ultimate |
| Teen Gohan Super Saiyan 2 | Father-Son Kamehameha | Ultimate |
| Gohan (Adult) Super Saiyan | Bros. Kamehameha | Ultimate |
| Yamcha | Soukidan | Ultimate |
| Trunks Super Saiyan | Heat Dome Attack | Ultimate |
| Goten Super Saiyan | Bros. Kamehameha | Ultimate |
| Gotenks Super Saiyan | Galactic Donuts | Second offensive blast |
| Yajirobe | Miracle Ka-Blam Slash | Ultimate |
| Tapion | Brave Sword Attack | Ultimate |
| Majuub | Chocolate Beam | Second offensive blast |
| Captain Ginyu | Body Change | Ultimate |
| Guldo | Guldo Special | Ultimate |
| Super Buu | Chocolate Beam | Second offensive blast |
| Super Buu Gotenks Absorbed | Galactic Donut Volley | First offensive blast |
| Super Buu Gohan Absorbed | Chocolate Beam | Second offensive blast |
| Super Garlic Jr. | Dead Zone | Ultimate |
| Broly Legendary Super Saiyan | Omega Blaster | Ultimate |
| General Blue | Skewer! | Second offensive blast |

## Zenkai cinematic sizing

The giant changes below require `PORT_ZENKAI_GIANTS=1` (the default). Setting it
to `0` restores original giant behavior; the attack compatibility table is unchanged.

Offline Zenkai giants use a 2.5x model multiplier during normal gameplay. During
an active scripted technique camera cut, their model multiplier temporarily
returns to 1x (the original giant size, not a human-sized replacement). Both sides
follow this rule, including giant-versus-giant fights. Props and technique effects
also use the original multiplier during the cut. Normal scaling returns on the
next pose/effect update after the cut ends, including a held cut being released.

Camera offsets use their original values. Gameplay body dimensions remain stable;
the temporary change affects model poses and effect sizing. Original mode and
online sessions retain original sizing throughout.

This applies to active technique cinematics generally, including a giant's own
attack, rather than only the attacks in the table. Data compatibility does not
prove every cinematic looks correct with the mod; in-game verification is needed.

## Evidence and reproduction

- `src/battle/btl_char_hit.c`: rush connection rejects giant targets unless the
  attack has `0x08000000`.
- `src/battle/btl_char_coll_b.c`: giant guard handling checks the same flag for
  catch reactions `0x1D`, `0x1E`, and `0x22`.
- `include/battle/btl_tech_b.h`: `BtlSuperData.flags` starts at offset `0x0C`,
  three 32-bit values indexed by technique slot minus 2.
- For character ID `c`, read game file `0x590 + c * 10`, model-pack entry `0x18`
  for the technique table. Read the first five move headings from English entry
  `0x2D`; headings 3 through 5 name the two offensive blasts and Ultimate.
- Character/form names follow [the roster mapping](tournament-character-families.md).

The table reflects the inspected local assets. Re-extract it if movesets or
compatibility flags in the game data change.
