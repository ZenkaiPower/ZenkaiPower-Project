# Battle HUD element and texture map

The battle HUD combines five shared texture banks, character-specific portraits, shared inline command icons,
and text or geometry generated at runtime. This reference maps the visible elements to their banks and drawing
code. Texture indices below are **zero-based**, in the runtime `gHud->res[]` order.

The local previews contain **all 53 entries in the five shared HUD banks**: 46 images and seven palette-only
entries, plus seven images showing alternate palettes. An additional 47-entry auxiliary bank contains pause
artwork and other reusable UI pieces: 38 images and nine palette-only entries. Portraits and inline controller
icons have separate runtime resources; their sources are listed below, rather than being mislabeled as HUD-bank
textures. The previews show the original installed US assets, before font or texture-pack replacement.

## Open the texture sheets

| Sheet | Preview | Content |
| --- | --- | --- |
| R0 | [Gauges, timer, team and palette variants](../../port/build/battle-hud-reference/gauges.png) | Health, Ki, Blast Stock, status icons, team and timer |
| R1 | [Battle announcements](../../port/build/battle-hud-reference/announcements.png) | Ready, Fight, K.O., results and lightning streaks |
| R2 | [Combo and event messages](../../port/build/battle-hud-reference/combo.png) | Hit and damage digits, Counter, MAX POWER, Lock on, First Attack, BOOST |
| R3 | [Prompts](../../port/build/battle-hud-reference/prompts.png) | Analog-stick animation, burst, pointer and directional markers |
| R4 | [Captions](../../port/build/battle-hud-reference/captions.png) | Logo and Press START caption |
| P | [Pause and auxiliary art](../../port/build/battle-hud-reference/pause-parts.png) | Pause captions, panel pieces, Dragon Balls, numbers and palettes |
| All | [Compact overview](../../port/build/battle-hud-reference/overview.png) | Thumbnail view of the six sheets |

Each image is also available separately under [the texture directory](../../port/build/battle-hud-reference/textures).
The [manifest](../../port/build/battle-hud-reference/manifest.json) records every entry, dimensions, texture format,
palette variants, source-file hash, and direct `HudSprite_InitTex`, `SetTex` and `SetUv` callsites.

Checkerboards are preview backgrounds, not part of the PNGs. Raw images preserve transparency; GS alpha 128
is converted to PNG alpha 255. Contact sheets use nearest-neighbor scaling to expose the original texels.

## Resource bank lookup

The shared assets are in **`pzs3us1.afs`, entry 5 (`00005.bin`)**. `Hud_Init` selects the banks in a different
order from their file sections:

| Runtime bank | File section | Native parts | Entry count |
| --- | --- | --- | --- |
| R0 | 1 | `HudGauge`, `HudTimer`, `HudTeam` | 25 |
| R1 | 4 | `HudNotice` | 9 |
| R2 | 5 | `HudCombo` | 8 |
| R3 | 2 | `HudPrompt` | 9 |
| R4 | 3 | `HudCaption` | 2 |
| P, auxiliary export name | 6, also referenced by section 7 | Pause and reusable UI artwork; not `gHud->res[5]` | 47 |

Source: [`Hud_Init`](../../src/battle/hud_a.c), texture structs in
[`hud_a.h`](../../include/battle/hud_a.h), decoding and uploads in
[`hud_c_c.c`](../../src/battle/hud_c_c.c) and [`hud_d.c`](../../src/battle/hud_d.c).

`HudSprite_InitTex(sprite, bank, texture, sub)` uses the image of entry `texture` and the palette of entry
`texture + sub`. Palette-only entries are therefore real resources, not empty or corrupt textures.

## Visible battle elements

| Element | Resource | Drawing or update source | Important details |
| --- | --- | --- | --- |
| Main health and Ki panel frame | R0:6 | [`HudGauge_Init`, `HudGauge_Draw*`](../../src/battle/hud_b.c) | One shared tree is drawn for both sides; side 1 is mirrored |
| Health-panel flash/mask | R0:7 | [`hud_b.c`](../../src/battle/hud_b.c) | White silhouette; runtime alpha/state determine the effect |
| Current health, trailing health and remaining-bar markers | R0:8 + palettes 9–13 | [`hud_a_d.c`](../../src/battle/hud_a_d.c), [`hud_b.c`](../../src/battle/hud_b.c) | One atlas, multiple colors and cropped widths; not separate full bar images |
| Current Ki | R0:8, sub 1 | `HudGauge_UpdateKi` in [`hud_b.c`](../../src/battle/hud_b.c) | Five cropped bar sprites |
| Trailing Ki | R0:8, sub 2 | `HudGauge_UpdateKi` | Separate delayed layer beneath current Ki |
| Ki reserve/red layer | R0:8, sub 3 | `HudGauge_UpdateKi` | Pulsing red layer; distinct from current Ki |
| MAX POWER duration gauge | R0:8, sub 5 | `HudGauge_UpdateMaxPower` | Five timer segments |
| Ki/MAX POWER lamps | R0:17 | `HudGauge_UpdateKiLamps`, `HudGauge_UpdateMaxPowerLamps` | Yellow/cyan diagonal lamps in the atlas |
| Blast Stock number | R0:14 | `HudGauge_UpdateBlast` | Eight cells for 0–7 |
| Blast Stock frame and progress fill | R0:15 | `HudGauge_Init`, `HudGauge_UpdateBlast` | Fill width changes with progress toward the next stock |
| Blast Stock number glow | R0:18 | `HudGauge_UpdateBlast` | Cyan versions of the 0–7 digits |
| MAX POWER lightning/aura | R0:16; additional plain-colored geometry | `HudGauge_UpdateAura` | Animated sprites, rotation, tint and alpha are runtime effects |
| Four status-modifier icons | R0:19–22 | `HudGauge_UpdateStatIcons` | Fist, lightning bolt, armor and aura artwork; color indicates positive/negative state |
| Status frames and backplates | R0:15 | `HudGauge_UpdateStatIcons` | Shared atlas regions, repeated for up to four icons |
| Active fighter portrait | Character/object `faceRes` | `HudGauge_DrawFace` in [`hud_b.c`](../../src/battle/hud_b.c) | Separate character data, not a fixed common HUD texture |
| Reserve/switch target portrait | Member `faceRes`, entry 0 | `HudTeam_Draw*` in [`hud_a_b.c`](../../src/battle/hud_a_b.c) | Current/previous portraits can overlap during the flip animation |
| Team panel and target-health frame | R0:23 | [`HudTeam_Init`](../../src/battle/hud_a_b.c) | Long reserve panel plus narrow end piece |
| Reserve indicators, switch gauge, target health | R0:24 | [`hud_a_b.c`](../../src/battle/hud_a_b.c) | Green/magenta strips, masks and circular-arrow artwork |
| Timer digits/infinity | R0:0; palette entries 1–2 | [`HudTimer_UpdateDigits`](../../src/battle/hud_e_c.c) | White, yellow and red palettes; infinity occupies two cells |
| Timer frame / connection or FPS frame | R0:3 | [`HudTimer_Init`, `HudTimer_LayoutConnectionFrame`](../../src/battle/hud_e_c.c) | Port status panel reuses the original timer frame at a smaller scale |
| Wins/count mode label and Dragon Ball mark | R0:4–5 | [`hud_e_c.c`](../../src/battle/hud_e_c.c) | Mode-dependent timer replacement or marks |
| Connection percentage / FPS string | `Font_PrintAt` output | `HudTimer_Draw` in [`hud_e_c.c`](../../src/battle/hud_e_c.c) | Generated text; no separate texture saying a fixed percentage or FPS value |
| Ready/Fight/K.O./result words | R1:0–6 | [`hud_d_b.c`](../../src/battle/hud_d_b.c), [`hud_e.c`](../../src/battle/hud_e.c) | Atlas rows and animated pieces; Battle font can replace the lettering |
| Announcement lightning streaks | R1:7 | Same notice code | Overlaid strips and jitter; not part of the word's glyph texture |
| Replay/camera mark | R1:8 | `HudNotice_ShowReplayMark` and notice initialization | Independent 32×32 picture |
| Combo hit digits | R2:0 | [`HudCombo_SetHitDigits`](../../src/battle/hud_c_b.c) | Up to three digits, with runtime UV selection |
| Combo damage digits | R2:1 | [`HudCombo_SetDamageDigits`](../../src/battle/hud_c_b.c) | Up to five digits; rolling values are generated from atlas cells |
| Hits label and impact graphic | R2:2 | [`hud_c_b.c`](../../src/battle/hud_c_b.c) | Two pieces in the same sheet |
| Counter/MAX POWER/Lock on/First Attack/BOOST | R2:3–7 | [`HudCombo_UpdateMessage`](../../src/battle/hud_c_b.c) | Event selects the message; runtime tint/animation can change its appearance |
| Technique or skill-name line | `BtlText_DrawEntryName` output | [`hud_c_b.c`](../../src/battle/hud_c_b.c) | Skill text from the fighter's script; not one static HUD-bank image per move |
| Analog rotation/press prompt | R3:1–6 | [`hud_e_b.c`](../../src/battle/hud_e_b.c) | Animation frames selected by prompt state |
| Prompt burst, hand pointer and direction markers | R3:0,7,8 | [`HudPrompt_*`](../../src/battle/hud_e_b.c) | Multiple cue/command modes reuse these |
| Command/button icons | `FontIcon_GetRes()` + `FontIcon_GetDefs()` | [`hud_e_b.c`](../../src/battle/hud_e_b.c), [`col_c_b.c`](../../src/battle/col_c_b.c) | Separate shared inline-icon resource; controller type and command select the image |
| Caption logo / Press START | R4:0–1 | [`hud_a_c.c`](../../src/battle/hud_a_c.c) | Mode/replay-dependent caption root; not always shown in a normal fight |
| Wide health-bar extensions | R0 panel/atlas pieces plus plain strips | `HudGauge_DrawWideFrame` in [`hud_b.c`](../../src/battle/hud_b.c) | Composed at runtime; no dedicated widescreen texture |
| Fighter names and large side-panel portraits | Generated font text and separate portrait files | [`fight_panels_ui.cpp`](../../port/src/gs/fight_panels_ui.cpp) | Port presentation layer, separate from the five original HUD banks |
| Pause/result/training menu text and panels | Auxiliary P pieces, font commands and generated headings | [`hud_0.c`](../../src/battle/hud_0.c), [`hud_0_b.c`](../../src/battle/hud_0_b.c), [`hud_0_c.c`](../../src/battle/hud_0_c.c) | Separate battle menu system, not another ordinary HUD gauge |
| Android touch gamepad and combo buttons | `gamedata/touch-buttons.pak` | [`VirtualPadView.java`](../../android/app/src/main/java/com/zenkaipower/android/VirtualPadView.java) | Android view overlay; separate from HUD sprite textures |

## R0 texture inventory

![Gauge and timer textures](../../port/build/battle-hud-reference/gauges.png)

| Entry | Dimensions | Content |
| --- | --- | --- |
| 0 | 128×128 | Timer digits 0–9 and infinity, white palette |
| 1 | Palette only | Yellow timer palette for image 0, sub 1 |
| 2 | Palette only | Red timer palette for image 0, sub 2 |
| 3 | 64×64 | Timer/status frame pieces |
| 4 | 64×64 | Wins label |
| 5 | 32×32 | Dragon Ball mark |
| 6 | 256×64 | Main health/Ki frame |
| 7 | 256×64 | Main frame silhouette/mask |
| 8 | 256×64 | Health and Ki shapes, base palette |
| 9 | Palette only | Image 8 sub 1: green health / yellow Ki |
| 10 | Palette only | Image 8 sub 2: red health / cyan Ki trail |
| 11 | Palette only | Image 8 sub 3: orange health / white Ki shape, tinted red for reserve |
| 12 | Palette only | Image 8 sub 4: yellow health / magenta Ki shape |
| 13 | Palette only | Image 8 sub 5: blue health / cyan MAX POWER shape |
| 14 | 128×64 | Blast Stock digits 0–7 |
| 15 | 64×64 | Blast progress frame/fill and status-frame pieces |
| 16 | 256×128 | Lightning/aura animation atlas |
| 17 | 64×64 | Yellow/cyan lamps and horizontal magenta highlight |
| 18 | 128×64 | Blast Stock glow digits 0–7 |
| 19 | 32×32 | Fist status icon |
| 20 | 32×32 | Lightning-bolt status icon |
| 21 | 32×32 | Armor status icon |
| 22 | 32×32 | Aura status icon |
| 23 | 128×32 | Team/target panel frame pieces |
| 24 | 128×32 | Team progress, health, masks and switch-arrow pieces |

## Frequently reused UV regions

Coordinates are texels, with the same argument order as `HudSprite_SetUv`: **u0, u1, v0, v1**.
The width of a fill's UV rectangle changes as its displayed value changes.

| Piece | Image | UV rectangle or cell rule |
| --- | --- | --- |
| Full health-bar body | R0:8 | `(0,160,0,16)`; palette depends on remaining health bars |
| Health trailing/foreground tip layer | R0:8 | `(0,160,48,64)` before runtime width cropping |
| Remaining-health marker | R0:8 | `(0,16,16,32)` |
| Ki/MAX POWER segment | R0:8 | `(16,32,16,33)`; update code also uses a 14-texel-wide crop `(16,30,16,33)` |
| Blast Stock digit n | R0:14 or 18 | `u=(n%4)*32`, `v=(n/4)*32`, cell 32×32 |
| Timer digit n | R0:0 | Same four-column 32×32 cell layout |
| Infinity | R0:0 | `(64,128,64,96)` |
| Status-frame outline | R0:15 | `(0,42,22,44)` |
| Status-frame backplate | R0:15 | `(0,36,44,64)` |
| Team main frame | R0:23 | `(0,94,0,20)` |
| Team target-health frame | R0:23 | `(96,126,0,20)` |
| Green team strip | R0:24 | `(0,32,0,16)` |
| Magenta team strip | R0:24 | `(64,96,0,16)` |
| Circular switch mark | R0:24 | `(64,80,16,32)` |

## R1 announcement inventory

![Announcement textures](../../port/build/battle-hud-reference/announcements.png)

| Entry | Dimensions | Content |
| --- | --- | --- |
| 0 | 512×64 | Ready! |
| 1 | 512×64 | Fight! |
| 2 | 512×64 | K.O.! |
| 3 | 512×128 | Success / Perfect! in separate rows |
| 4 | 512×128 | Ringout! / Time Up! in separate rows |
| 5 | 512×64 | Winner! |
| 6 | 512×64 | Loser |
| 7 | 512×128 | Lightning streak variants |
| 8 | 32×32 | Replay/camera mark |

## R2 combo inventory

![Combo textures](../../port/build/battle-hud-reference/combo.png)

| Entry | Dimensions | Content |
| --- | --- | --- |
| 0 | 128×128 | Red hit-count digits |
| 1 | 128×128 | Small orange damage digits |
| 2 | 128×64 | Hits! label and impact star |
| 3 | 128×64 | Counter! |
| 4 | 128×64 | MAX POWER! |
| 5 | 128×64 | Lock on |
| 6 | 128×64 | First Attack! |
| 7 | 128×64 | BOOST! |

## R3 prompts and R4 captions

![Prompt textures](../../port/build/battle-hud-reference/prompts.png)

| Entry | Dimensions | Content |
| --- | --- | --- |
| R3:0 | 64×64 | Burst/impact marker |
| R3:1–6 | 64×64 each | Analog-stick animation and tilted/pressed states |
| R3:7 | 32×32 | Pointing hand |
| R3:8 | 64×64 | Four triangular direction markers |
| R4:0 | 256×128 | Original game logo |
| R4:1 | 512×32 | Press START button caption |

![Caption textures](../../port/build/battle-hud-reference/captions.png)

## Auxiliary pause bank

[Open the complete auxiliary sheet](../../port/build/battle-hud-reference/pause-parts.png).
P is an export label, not a sixth `gHud->res` bank. These assets are included to distinguish battle-menu art
from the live battle HUD and from shared controller-icon resources.

| Entries | Content |
| --- | --- |
| P:0 | 256×256 caption atlas: 1P PAUSE, 2P PAUSE, EXIT?, COM Settings, Training Content |
| P:1–6 | 32×32 border, panel, separator and small decorative pieces |
| P:7–13 | 32×32 one-star through seven-star Dragon Balls |
| P:14–15 | 32×32 blue circular star markers |
| P:16–18 | 32×32 gray, ochre and cyan panel masks/fills |
| P:19 | 32×32 energy/lightning symbol |
| P:20–28 | 32×32 cyan digits 1–9 |
| P:29–37 | 32×32 yellow digits 1–9 |
| P:38–46 | Palette-only entries; swatches are shown separately |

## Portraits, icons and generated content

The export is complete for the common banks above, **not a dump of every fighter portrait or every runtime
font-icon resource**. The portrait on screen depends on the selected fighter/form; controller icons depend on
the controller layout and command. Resolve these through the runtime sources in the element map rather than
assigning them a shared R0–R4 index. See [`HudGauge_DrawFace`](../../src/battle/hud_b.c),
[`HudTeam_Draw*`](../../src/battle/hud_a_b.c), and [`FontIcon_GetRes`](../../src/battle/col_c_b.c).

Plain rectangles, runtime color tint, animated fills, font-rendered names, connection percentages and FPS strings
do not have one independent static PNG each. The original bank previews also differ from the enhanced Menu/Battle
font replacements; that path is described in [Menu font](menu_font.md).

## Regenerate the reference

The PNGs, raw source file and manifest live under ignored `port/build/battle-hud-reference/`; the document and
exporter are repository sources. Given an installed/extracted US archive entry:

```powershell
python port/tools/export_battle_hud_textures.py --file path/to/pzs3us1/00005.bin
```

Requires Pillow. The exporter uses the same GS texture-layout decoder as `port/tools/menu_font.py` and reads
only the supplied file. For animation, HUD state and implementation notes, see [Battle HUD](../systems/hud.md).
