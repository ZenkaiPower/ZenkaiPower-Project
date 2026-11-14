# Creating native styled text headings

Use this guide to add headings like **Training Settings**: text drawn from a TrueType font,
with the game's gradient and borders, inside the native pause menu. Wording comes from translation JSON.

For editable replacements of existing bitmap labels, see [Menu fonts](menu_font.md).

## How the training heading works

1. The game requests translated wording through `Port_TrainingTextCopy`.
2. `BtlMenu_DrawPart` requests a generated text texture with `PortTbo_TextId(text, 7)`.
3. `tbo_text.c` measures and caches that line. Kind `7` selects the pause heading renderer.
4. `MenuText_RenderPauseHeading` rasterizes Metropolis Bold with the `pause-menu` decoration.
5. The game draws the returned texture ID in a sprite packet using `TBO_PSM`.
6. `gs_draw.c` uploads the texture and applies the native battle UI's widescreen correction.

The ordinary options below the heading still use the game's font commands and selection colors.

## Implementation map

| File | Responsibility |
|---|---|
| `port/src/gs/menu_text.c` | Font loading, glyph layout, gradient, outlines, RGBA rasterization |
| `port/src/gs/menu_text.h` | Host renderer declarations |
| `port/src/gs/tbo_text.c` | Generated text IDs, measurements, texture pixels, versions |
| `port/src/gs/gs_draw.c` | Texture upload and aspect correction |
| `port/src/gs/menu_translation.c` | Catalog lookup, English defaults, copying strings to the game |
| `port/translations/labels/*.json` | Translated wording |
| `src/battle/hud_0.c` | Training menu tree, options, callbacks |
| `src/battle/hud_0_b.c` | Native frame, heading sprite, option drawing and input |
| `include/battle/port_training_options.h` | Reserved training text and heading identifiers |

## 1. Choose the font and decoration

The pause heading uses `port/third_party/fonts/metropolis/Metropolis-Bold.ttf`. Keep the font and its license
in the fonts directory so packaging includes them. A fixed heading face does not need a selectable
entry in `fonts.tsv`; add such an entry only if it should appear in font settings.

Metropolis is by Chris Simpson and is distributed under the Unlicense. The
upstream OpenType file, matching runtime TrueType file, full license and pinned
source attribution are in `port/third_party/fonts/metropolis/`.

`menu_text_row` selects the fixed pause face when `MenuLabel.fontFace` is `2`. It opens
`metropolis/Metropolis-Bold.ttf` under the resolved font directory, including the `ZENKAIPOWER_FONTS` override.

The current heading recipe in `MenuText_RenderPauseHeading` is:

| Property | Value |
|---|---|
| Logical texture | 256 × 32 pixels |
| Capital height / baseline | 18 / 23 pixels |
| Alignment / margin | Center / 4 pixels |
| Fill, top to bottom | `#FFAE20` to `#F42D19` |
| Inner outline | `#302320`, 2 pixels |
| Outer rim | `#C8C4DA`, 2 pixels |
| Raster scale | 4, producing a 1024 × 128 texture |

Decoration widths in `MenuLabel` use thousandths of a logical pixel: `outlineMilli = 2000` means 2 pixels.
Colors use `0xRRGGBB`.

Keep `artWidth` zero for new headings. A nonzero value fits the glyphs to a measured artwork width,
which is useful for replacing an existing sprite but can change a new heading's letter proportions.
The renderer fits an overlong line by reducing both horizontal and vertical scale together.

To create a different style, add a renderer beside `MenuText_RenderPauseHeading`, choose its face
and decoration, and declare it in `menu_text.h`. Initialize a `MenuLabel`, call `menu_text_row`,
then paste its `Row` into an allocated RGBA texture. Follow the existing locking and cleanup:
initialize fonts under `sLock`, release the temporary row pixels, and return pixels owned by the caller.
The renderer expects GS alpha: 128 is opaque.

## 2. Register the generated texture kind

To reuse the pause style, request:

```c
s32 id = PortTbo_TextId(translatedTitle, 7);
```

For a new style, allocate an unused kind in `tbo_text.c` and implement both paths:

- `line_measure`: determine the texture dimensions and metrics; free any temporary raster.
- `TboText_Pixels`: render the same texture and return its dimensions and pixels.

Also extend the allowed kind range in `PortTbo_TextId`. Measurement and rendering must agree on
dimensions. The cache uses the text, kind, font generation, and slot version; request the ID while
the heading is visible rather than retaining a slot ID indefinitely.

`TEXT_MAX` currently limits a generated line to 127 UTF-8 bytes plus its terminator.

## 3. Put wording in translation JSON

Add stable IDs to every supported file in `port/translations/labels/`:
`en.json`, `de.json`, `es.json`, `fr.json`, and `it.json`.

For example, the training menu title is:

```json
"training-settings/0/5": {
  "original": "Training Settings",
  "text": "Trainingseinstellungen"
}
```

Keep `original` identical to English in every catalog. Set `text` to the locale's wording.
Update `labels_count` to the actual number of entries and update catalog versions when changing the catalog.

The current training rows are:

| ID suffix | Wording |
|---|---|
| `0/0` | Health Regeneration |
| `0/1` | On |
| `0/2` | Off |
| `0/3` | Ki Regeneration |
| `0/4` | Blast Stock Regeneration |
| `0/5` | Training Settings |

Add English defaults in `tr_required_defaults` in `menu_translation.c` so older installed catalogs
can still open the menu. Add or extend a copy function like `Port_TrainingTextCopy`: it ensures the
translation catalog is loaded, looks up the stable ID, and copies the resolved UTF-8 string into
the caller's buffer with an English fallback.

Pass UTF-8 directly to the generated heading renderer. For normal options using `Font_Print`, convert
UTF-8 into the game's `u16` text buffer, as `Port_TrainingMenuText` does. Copying UTF-8 bytes individually
into `u16` produces broken accented characters. Check that the chosen font contains the needed glyphs;
this renderer does not provide complex-script shaping or bidirectional layout.

## 4. Add the title to the native menu

Use the existing `BtlMenu` engine for its frame, animation, selection, keyboard/controller input, and mouse input.
The training menu represents its title as the first item:

- Set both controller text entries to the reserved negative heading identifier.
- Set `disabled = 1` so navigation skips the title.
- Include the heading row in the item count and menu height.
- Start selection on the first actual option.

`PORT_TRAINING_TITLE_PART` reserves parts 8–11 for the training title and three regeneration titles.
`BtlMenu_DrawPart` handles those parts before trying to draw the original atlas. Extend this mapping
when adding another heading. Keep the width calculation consistent with the heading's logical size.

Keep titles and selectable options distinct: title rows draw a styled sprite; options keep the native
font commands, yellow selected color, blue normal color, and shadow.

## 5. Draw with matching texture coordinates

The current pause heading maps a 1024 × 128 raster onto a 256 × 32 logical sprite. Both axes use
the same 4:1 ratio. In the sprite's `TEX0`:

- Set the reserved `TBO_PSM` format.
- Put the generated text ID in CBP, bits 37–50.
- Set texture dimensions to exponents 10 and 7.
- Enable texture alpha, as the existing heading packet does.

UV coordinates are in sixteenths of a texel and have only 14 bits per component. An edge at
1024 texels becomes 16384 and wraps to zero. Clamp that edge to `0x3FFF`, as `Ps_Uv` and the
training heading packet do. A wrapped coordinate can make an otherwise valid heading disappear.

If changing raster dimensions, update measurement, returned pixel dimensions, `TEX0` exponents,
and UV endpoints together. Do not stretch an arbitrary texture rectangle to fill the menu frame.

## 6. Preserve widescreen proportions

Generated textures use `TBO_PSM`, which normally bypasses battle UI aspect correction because
other generated menu screens already position their own text.

Pause headings need the battle correction. `TboText_IsPauseHeading` identifies kind `7`, and
`GsGpu_Draw` admits those sprites to the native widescreen correction. Without this, a heading
on a 16:9 page is about one-third too wide relative to the native pause UI.

When adding another pause heading kind, include it in this classification. Apply aspect correction
once; combining a manually narrowed sprite with renderer correction makes the text too narrow.

## Review checklist

- Confirm the menu title and every On/Off title are visible.
- Check 4:3 and widescreen presentation, including the longest translated title.
- Confirm letters keep their proportions and borders stay inside the heading row.
- Check accents, fallback wording, and the font's presence in a packaged build.
- Check controller and mouse navigation skips the title and returns to the parent menu correctly.
- For existing sprite replacements, compare the visible text bounds against the original artwork.

`menu_font.py --compare-styles` compares cataloged bitmap replacements; it does not exercise the
new Training Settings menu's drawing packet. That path also needs an in-game visual check.
