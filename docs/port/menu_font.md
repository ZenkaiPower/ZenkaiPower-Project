# Menu fonts

Three settings in F1 > UI draw text from a TrueType font instead of the game's own lettering. The defaults are DejaVu Sans Condensed Bold for menus and battle text, and
DejaVu Sans Condensed for dialogue / UI. Each can be changed independently,
including selecting "Original". Saved font choices take precedence over these defaults.

| setting | what it replaces |
|---|---|
| Menu font | the labels of the menus (pictures on the disc: "Dragon History", "1P VS COM", ...) and the names of added stages and songs |
| Battle font | Battle announcements, combo HUD, timer digits, Wins label, and blast stock digits |
| Dialogue / UI font | the game's shared text font (`Font_Print`): dialogue, subtitles, explanations, ordinary menu text |

So the Dialogue / UI font does change the game's ordinary `Font_Print` text: that is what it is for. Placement,
line breaks, colours, shadows and the button icons stay the game's; only the glyph pictures are drawn again.

## The fonts

The fonts are in `port/third_party/fonts/` (in a release: `fonts/` next to the program), one folder per family
with its licence, and the list `fonts.tsv`:

```
# file <tab> name in the list <tab> use: 1 = Menu, 2 = Dialogue / UI, 4 = Battle, added up for several
dejavu/DejaVuSansCondensed-Bold.ttf	DejaVu Sans Condensed Bold	1
dejavu/DejaVuSansCondensed.ttf	DejaVu Sans Condensed	2
anton/Anton-Regular.ttf	Anton	4
```

To offer one font in several settings, add their numbers: `3` for Menu and Dialogue / UI, `7` for all three. The
column is the one it was before the Battle font came (1, 2, 3 = both), so lines added to an earlier list keep
their meaning, and a line without it is offered for Menu and Dialogue / UI as before. DejaVu Sans Condensed Bold is
first: it is the lettering of the Dragon Net Battle's translated labels while the Menu font is "Original".
To add a font, put the `.ttf` there and add a line; restart the game. A font has to be a static TrueType file
(glyf outlines; not `.otf`, not a collection, not a variable font), have a capital `H` (the letter height is taken
from it) and be at most 16 MB. Choices are kept by file path, not by their place in the list.

A font file is checked when it is chosen and at every start. A missing, damaged or unsupported file
uses "Original" temporarily. Menu and Battle font choices retain the requested font ID and retry
about once a second while loading is pending. Restoring the font file or the required label data lets
the game apply the requested font again without restarting. Changing unrelated settings preserves
that request. Selecting "Original" from an active custom font still saves "Original" normally.
The log reports the first failure for a font and logs its name when it loads successfully.

## The menu labels

The bundled menu and battle translations are in `port/translations/labels/` (`translations/labels/` in a release).
At startup, the game copies missing locale files into `gamedata/game-text/translations/labels/` and discovers
languages from the `labels/`, `ui/`, and `port/` catalogs. A language without its own label catalog uses English
menu and battle labels. On the first launch, a native language list appears before game data is loaded when at least
two languages are available. With only one language, the list is skipped. The choice is saved in `settings.dat`
and can be changed in F1 > UI > Language; restart to load another language.
The texture/layout index in `labels.tsv` is shared by all languages. `ZENKAIPOWER_MENU_LANG=es` overrides the saved choice
for one launch. F1 > UI > **Show language selector at startup** can show the list on later normal launches.
See [Translations](translations.md) for catalog locations, editing, and fallback rules.

Which texture has which label, and how large its letters are, is measured on your own disc data. The game does it
itself (`port/src/gs/menu_labels.c`), in a source installation and in a release alike: when it starts with a
window and `gamedata/game-text/labels.tsv` is not there, or does not have the current header, it reads the HUD
resource `gamedata/pzs3us1/00005.bin` and eleven menu archives, `00448.bin` ... `00472.bin`, and writes `gamedata/game-text/`:

- `labels.tsv` and `name-styles.tsv`: what the game reads; `labels.tsv` stores label IDs and layout, while wording stays in the selected translation JSON;
- `labels.json`: editable texture positions and styles. One that is already there is kept as it is.

This normally runs in the background: the game does not wait for it. Until it is
done "Menu font" and "Battle font" offer only "Original"; then the fonts appear in the list, and a font that the settings file
already names is taken into use between two frames (a menu on screen at that moment changes its lettering).
The log reports how many labels it made from the disc; the measured time varies.
Each file is written under another name and renamed when complete: a list is never half there.

The texture numbers are those of the USA release (SLUS-21678), so all twelve archives are checked by size and CRC-32
before their contents are parsed. With any other disc, a missing archive or a folder that cannot be written to,
nothing is made, the log has one line saying why, and "Menu font" offers only "Original"; the game is otherwise
unaffected. (It is tried again at the next start.) `ZENKAIPOWER_MENU_FONT_ANY_DISC=1` leaves the check out, for a changed
archive: where its textures are numbered differently, labels are missing or wrong.

A run whose buttons are recorded or played back, or that is re-run blank by blank (`ZENKAIPOWER_PAD_REC`, `ZENKAIPOWER_PAD_PLAY`,
`ZENKAIPOWER_PAD_TABLE`, `ZENKAIPOWER_SYNCTEST`), makes the list at start, before anything is drawn: its pictures do not depend on
how fast the list was made. During an online session nothing is taken into use; a list finished then waits for
the session's end. Without a window (`ZENKAIPOWER_GS=none`) nothing of this runs.

The list is derived from the disc and is never part of the repository or of a package. Before this was in the
game it was a step of `install.py` (a Python tool with a C helper built by gcc), which a release could not run;
the files the game writes are the same bytes as that tool's, and a `game-text/` folder made by it goes on working.

## Edit translations and labels

When no English UI catalog is present, the game extracts its UTF-16 UI, mode and story dialogue text into
`gamedata/game-text/translations/ui/en.json` on startup. Copy it to
`ui/<locale>.json`, set `locale` and `name`, then edit `text` while keeping each ID
and `original`. The selected language's catalog is loaded on the next launch;
translated lines are measured and drawn by the game's UI text routines.
See [Translations](translations.md) for the matching and fallback rules.

The 64-bit port still passes text through game font routines that store pointers
in 32-bit fields. The UI translation loader therefore keeps translated UTF-16
strings in memory addressable by those routines using `Port_LowAlloc` and frees
them with `Port_LowFree`. Ordinary host `malloc` is safe for catalog metadata
and English strings used only for lookup, but using it for a string returned to
the game can truncate the pointer and crash in `Font_CountLines` (for example,
when a translated memory-card dialog opens). The standalone comparison tool
uses `UI_TEXT_HOST_ALLOC` because it does not call the game's font routines.
`port/tests/menu_font/ui_translation.c` checks that returned translated pointers
fit in 32 bits.

Port-added interface messages have their own catalogs in
`gamedata/game-text/translations/port/`. The English template is
`port/translations/port/en.json`; currently it defines the quit confirmation
message. A selected locale's port catalog is loaded at startup, with English
fallback for missing messages.

To change wording, edit `text` in `gamedata/game-text/translations/labels/<locale>.json` and restart the game.
Normal and selected states each have their own entry; edit both to keep their wording the same. Use `\n` for two lines.
The bundled label and port catalogs have `translation_version` (which the translator can increment when revising
the translation), `labels_version` (to increment when source IDs or original wording change), and `labels_count`
(the number of entries in `labels`). These fields are informational; extracted UI catalogs may omit `labels_count`.
`schema_version` is checked by the loader and must match the supported JSON format. The translation JSON files are
preserved when the game starts; a valid locale catalog in any of the three folders appears in the language lists
on the next start.

To change a label's layout or style, edit `gamedata/game-text/labels.json`, then rebuild the texture/layout index:

   ```sh
   python3 port/tools/menu_font.py
   ./play.sh
   ```

No game rebuild or archive repacking is needed. The tool turns `labels.json` into `labels.tsv` and
`name-styles.tsv`; it reads only that JSON and needs only Python. The `text` fields in older `labels.json` files are
ignored; wording comes from the selected translation JSON.
For a release, take
`port/tools/menu_font.py` from the source and name the folder: `python3 menu_font.py --out <game>/gamedata/game-text`.
The game itself never reads `labels.json` and never writes over one. To go back to the disc's labels, delete
`labels.json` and `labels.tsv`: the game writes both again at its next start.
If a local `labels.json` lacks battle labels, delete it and `labels.tsv`, then start the game to generate the
current catalog. Copy any edits you want to keep into the new JSON and run the tool again.

To compare original artwork with replacement text for menu and battle styles, run from the repository root:

```sh
python3 port/tools/menu_font.py --compare-styles
```

The command writes `port/build/menu-font/style-comparison.png`, names the fonts used, and applies current
`labels.json` edits before rendering. It uses the first Menu font in `fonts.tsv` (currently DejaVu Sans Condensed
Bold) by default and needs the extracted disc data, Pillow, and a C compiler. Battle rows are included when the
Battle font listed in `fonts.tsv` is present; some styles have several examples.
Set the PNG path with `--output path/to/comparison.png`.

| field | effect |
|---|---|
| `original` | Reference wording painted in the artwork; runtime text comes from the selected translation JSON |
| `enabled` | `false` keeps the original sprite |
| `italic` | `true` slants this label; default `false`. Enabled for “Press START button” and battle announcements. |
| `font_face` | `battle` uses the separate Battle font setting; `selected` uses the chosen Menu font; `pause-menu` uses fixed Metropolis Bold |
| `color_style` | A shared palette such as `purple`, `main-heading`, or one of the `notice-*` styles |
| `shadow` | `false`, `true`, `hard`, or `translucent-hard`; the last uses a partly transparent bottom-right shadow |
| `cap_height`, `baseline` | Letter height and baseline, in original texture pixels |
| `align`, `margin` | `left` or `center`, with horizontal margin |
| `width_scale` | Horizontal scale, 0.5–2.0; default 1.0 |
| `art_width` | Measured width of the original letter fill |

Leave texture identity and cell bounds unchanged: `hash`, `texture_width`, `texture_height`, `x`, `y`,
`width` and `height`. Edit the JSON, not the generated `labels.tsv` or `name-styles.tsv`.

Unchanged labels match the original artwork's letter width and intended height. Translated labels and added
names use natural font proportions, shrinking to fit. Text supports up to two lines and 255 UTF-8 bytes.
Characters absent from the selected font are skipped. Empty labels, or labels with no supported characters,
keep the original sprite.
Complex-script shaping and bidirectional text are not supported.

## Text styles

`pause-menu` styles the five battle pause headings with `fonts/metropolis/Metropolis-Bold.ttf`: an orange-to-red fill, a dark inner outline, and a pale lavender outer rim. The finished headings fit the original texture lettering's visible bounds, including its position and decoration. English, Spanish, German, French, and Italian wording is supplied by the label translations. When European language artwork is installed, fitting uses that localized texture; otherwise it uses the USA texture. It follows the Menu font setting; Original preserves the sprite artwork. Existing generated catalogs regenerate on startup; delete an older `labels.json` to include these headings in editable settings.

Each label's `color_style` references a shared entry in `color_styles` at the top of `labels.json`.
Styles apply across selectable typefaces. Colors are defined explicitly; artwork is measured only for layout.

| Type | Used for | Default fill, top → bottom | Outline |
|---|---|---|---|
| `pause-menu` | Battle pause headings (Metropolis Bold) | Orange `#FFAE20` to red `#F42D19` | Dark `#302320` with pale lavender `#C8C4DA` rim |
| `purple` | Most unselected labels and unselected song names | Light gray `#CBCBCB` | Purple `#460059` |
| `main-menu` | Unselected main-menu labels | Light gray `#CBCBCB` | Lighter purple `#5E236D` |
| `main-heading` | Large headings in the main and submenu screens | Gold `#FFBE2F` → red `#FF2222` | Dark brown `#7C3025`, darkening at the base |
| `orange` | Selected labels, stage names and character-preview numbers | Yellow `#F4EF89` → orange `#D2684B` | Dark brown `#2C151B` |
| `title-prompt` | “Press START button” | Cream `#EFEFDF` → yellow-green `#B1B360` | Dark red `#3F1511` |
| `notice-blue` | Ready! | Icy white → blue | Deep blue |
| `notice-orange` | Fight! | Cream → orange | Dark brown |
| `notice-red` | K.O.! | Pink → red | Dark red |
| `notice-silver` | Success, Perfect! | White → blue-gray | Slate blue |
| `notice-green` | Ringout!, Time Up! | Pale green → green | Dark green |
| `notice-gold` | Winner! | Cream → gold | Dark brown |
| `notice-purple` | Loser | Pale lavender → purple | Dark purple |
| `combo-red-digits` | Red combo digits | Red `#EA1C25` → `#DE161C` | Black `#000000` |
| `combo-orange-digits` | Orange combo digits | Gold `#FFBE2F` → red `#FF2222` | Black `#000000` |
| `combo-hits` | Hits label | Cream `#FFF5DF` → yellow `#D8D701` | Brown `#663D12` |
| `combo-message` | Combo messages | Orange `#FE5D0D` → yellow `#FFFE01` → orange `#FE3C0F` | Brown `#663D12` |
| `combo-power` | Power messages | Blue `#77BAFD` → white `#FFFFFF` → blue `#5CACFD` | Dark blue `#142956` |
| `timer-white` | White timer digits | Gray `#EEEEEE` → `#C4C4C4` | Dark gray `#242424` |
| `timer-yellow` | Yellow timer digits | Yellow `#EEEE6C` → `#C4C424` | Dark gray `#262618` |
| `timer-red` | Red timer digits | Red `#EE6464` → `#C42424` | Dark gray `#291B1B` |
| `win-counter` | Wins counter | White `#F2F2F2` → gray `#D8D8D8` → `#969696` | Gray `#444444` |
| `blast-stock-counter` | Blast stock digits | Orange `#FF8B00` → yellow `#FFFE00` → orange `#FE7A02` | Black `#000000` |

Colors in the JSON use RGB triples, such as `[203, 203, 203]` for `#CBCBCB`.
Existing catalogs keep their saved style values; editing a shared style changes all labels using it.
Added song and stage names share the corresponding fill and outline colors.

| Style field | Effect |
|---|---|
| `top`, `bottom` | Fill gradient; equal RGB triples give a solid fill |
| `middle` | Optional center fill color; blends from `top` through `middle` to `bottom` |
| `outline` | Inner outline RGB color |
| `outline_width` | Optional width, 0–8 original texture pixels; default automatic, usually 3 |
| `outer_outline` | Optional outer outline RGB color |
| `outer_outline_width` | Width, 0–8 original texture pixels; default 0 (disabled) |
| `outline_bottom` | Optional color at the bottom of the inner outline; announcement rims use the solid `outline` color |
| `bevel_width` | Optional border bevel width, 0–4 original texture pixels; default 0 |
| `bevel_light`, `bevel_dark` | Highlight and recess colors for menu borders; announcement faces use directional shading |
| `depth` | Solid diagonal extrusion behind the letters, 0–8 original texture pixels; default 0 |
| `mid_gradient` | `true` sharpens a two-color fill transition around the middle; default `false` |
| `shadow` | Offset shadow; `"hard"` draws a short solid black shadow and a slight light edge on the top and left; default `true` |

## Files and rendering

- `port/third_party/fonts/` (release: `fonts/`): the typefaces, their licences and `fonts.tsv`. `ZENKAIPOWER_FONTS` names
  another folder; an empty value switches all three settings off.
- `gamedata/game-text/` (or `$ZENKAIPOWER_DATA/game-text`): the labels made from the disc; ignored by Git, never packaged.
  `ZENKAIPOWER_MENU_FONT` names another folder; an empty value disables menu and battle label replacement and translation
  catalog loading.
- `port/src/gs/menu_labels.c`: makes the list. Its catalogue (`ml_catalog`) identifies the archive, section and
  texture of each label, and maps label IDs to styles; `port/translations/labels/en.json` holds the English wording. At startup the game
  copies it to `gamedata/game-text/translations/labels/en.json` if absent and reads that copy for the labels and name strips.
  To add a menu, add its sheets to `ml_catalog` and its labels to `en.json`.
- `port/tools/menu_font.py`: an edited `labels.json` into the files the game reads; `--compare-styles` renders a PNG.
  `--out` names the folder.
- `port/src/gs/menu_text.c`: the font check, the drawing and the list of fonts, for Vulkan and OpenGL. Text is
  rasterized at 2–4 times the PS2 resolution; the game's internal resolution setting separately allows 1–8 times.
- `port/src/gs/ui.cpp`: the three settings, and the names of added songs and stages.
- `src/battle/col_c.c` `Font_UploadTexture`: under `#ifdef PORT`, hands the game's glyph table to the renderer.
- `port/tests/menu_font/fuzz.c`: damaged font files against the font check, with the sanitizers.
- `port/tests/menu_font/labels.c`: the maker of the list on its own (to compare its files), and against damaged
  menu archives, with the sanitizers.

The internal `text_screen_size` setting defaults to `1` and chooses shared UI glyph and label raster quality
from the displayed picture's physical pixel size, including letterboxing, independently of the Resolution option.
It keeps the existing 2–4x font raster limits and refreshes cached textures after resizing. Setting it to `0`
restores resolution-based raster quality. Text placement and the final scene render resolution remain unchanged.
Generated in-game Options captions also use this raster scale while keeping their layout measurements fixed.

The internal `text_antialias` setting defaults to `1`. It enables smooth glyph coverage and linear sampling for
generated text independently of the artwork's 2D filtering option. Setting it to `0` uses binary glyph coverage
and nearest sampling. Font and generated-caption caches refresh when it changes.

Texture-pack replacements take precedence over the font settings. They do not touch the game's state: an online
match is the same with any font on either side. `ZENKAIPOWER_FONT_LOG=1` prints every decoded texture and the time the
text took.

The release build runs `hud-name-test` against packaged fonts without disc data. The original HUD frame contour
has a separate integration test requiring an extracted USA disc. Build it with
`make -f port/release/tests.mk hud-frame-test`, then run
`port/build/hud-frame-test /path/to/extracted/gamedata`. This test is not part of the disc-free release pipeline.

Only Disc Fusion's Ranking Battle and Course Battle headings are added to label index v15; their other bitmap labels retain the original artwork. Existing indices regenerate automatically when the version changes; these modes use the same Menu font and translation catalog as other screens.

ZENKAI training pause menus include a **Training Settings** submenu with Health, Ki, and Blast Stock Regeneration. Its heading and each On/Off screen use fixed Metropolis Bold and the `pause-menu` decoration; options retain native pause-menu colors and navigation.

For an implementation guide covering generated headings, translation JSON, texture coordinates, and widescreen proportions, see [Creating native styled text headings](native_text_headings.md).


## View Skill List translations

The USA character packs already contain Western skill-list scripts. At catalog loading,
`UiText_ExtractSkills` reads their text sections and creates
`gamedata/game-skills.pak`, containing `{en,es,de,fr,it}.json`. These entries are read through
the virtual paths `game-text/translations/skills/<locale>.json` without restoring loose files.
Older loose skill catalogs migrate into this package after successful publication. A European disc import is
not required for these scripts. Both loose character packs and `pzs3us1.afs` are supported.
The extractor reads only script sections and stores identical costume scripts once.

Character pack entries are `0x58F + character * 10 + costume`, for characters 0?160 and
costumes 0?7. English uses section `0x2D`; Spanish, German, French, and Italian use
`0x30`, `0x31`, `0x32`, and `0x33`. Generated IDs are `skills/<character>/<costume>`.
These files are generated from the player's disc resources and must not be committed.

The skill catalogs are loaded alongside UI translations. `BtlMenu_SetScript` and
`BtlMenu_SetScript2` translate the complete UTF-16 script before the skill-list and move-name
parsers read it. This covers page headings, move names, and description/instruction text.
Missing translations keep the original script.

Each JSON entry contains the original and translated **whole script**, including its
control syntax. When editing wording, preserve command prefixes, masks, button tokens,
page markers, and the terminating script command. Existing edited catalogs are preserved;
delete `game-skills.pak` to regenerate all skill catalogs from the disc. Ship rebuilt code and refresh the
installed resources when updating an older release.

## Character Reference voice credits

Character IDs 0?160 use `character-reference/voice/000` through `160` in the
`port/translations/port/<locale>.json` catalogs. The same extracted names and
JP/US markers are supplied for English, German, Spanish, French, and Italian.
These catalogs ship in `translations.pak`; the HTML is not needed by the game.

Regenerate the credits from the saved source snapshot with:

```sh
python port/tools/character_voice_actors.py characters.html
```

The importer maps article entries to the game's character/form IDs, updates all
five catalogs, and generates `port/src/gs/character_voice_actors.inc` as native
fallback wording. Review `SOURCE_ROWS` if the article's order changes. Names are
copied as supplied, including incomplete or misspelled source credits. In this
snapshot Super Saiyan Future Gohan lists only ?Kyle Hebert?.

Character Reference uses the resolved credit string instead of the disc's single
actor name. Every comma-space separator in the source credits becomes a line break in the
JSON and native fallback text. Character Reference displays those line breaks directly.

UI font rendering omits registered trademark (U+00AE), trademark (U+2122), and copyright (U+00A9) symbols, including the original character-name font's U+3327 TM encoding. The filter applies to glyph drawing and width measurement in both game font commands and native TrueType text. Source strings and translation IDs remain intact; bitmap artwork is unchanged.
