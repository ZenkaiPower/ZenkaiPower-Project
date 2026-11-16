# In-game Options and adding settings

The ZENKAI Options menu extends the original Options movie. It preserves the original backdrop, header, Dende, dialogue window, plate decorations and footer timelines. More Options settings use the same runtime owners as the external settings overlay. In ZENKAI mode, native and port Options persist together in the save companion `savegame/zenkaipower.sav.options`. Original mode retains its original menu and navigation.

Launch with `--settings` to enable the F1 external menu. It opens in a separate, movable desktop window and has **Video Settings** and **Cheats** tabs (when cheats are enabled). Video Settings uses the same eight descriptors and `Port_OptionsVideoChange` bridge as the in-game Video Settings page. Changes therefore update the same runtime settings and persistence path, including settings that require a restart to take effect.

## Menu structure

Controller Options contains Vibration, Clash Input, **1P Controller**, and
**2P Controller**, followed by the native Default footer. Each player submenu contains
Mouse/Keyboard and Gamepad. Back from a binding page returns to that player's
submenu; Back again returns to Controller with the player entry selected.
The player submenu hides the parent Default footer; each binding page retains
its own confirmed Default reset, affecting only that player and input type.

Player names match the game's Controller labels in each locale: English and
Italian use 1P/2P, German uses S1/S2, and Spanish and French use J1/J2.
The submenu headings and parent entries share the same localized title IDs.

Both players have independent gamepad and keyboard/mouse bindings, used for their
respective battle input. Player 1 keeps the existing defaults; player 2 starts
with default gamepad bindings and unbound keyboard/mouse actions to avoid
controlling both fighters with the same keys. Bindings can intentionally overlap.
On Android, Mouse/Keyboard remains hidden in both player submenus. The parent
Controller page also includes Virtual Gamepad and scrolls after four rows;
its scroll arrows support touch as well as controller navigation.
Virtual Gamepad uses the native animated Off/On selector and defaults to On.
Hover/Left/Right only changes the provisional choice; confirm applies it and
marks Options dirty, while Back cancels. Controller Default restores On after
confirmation. The setting is saved with the normal ZENKAI Options flow when
leaving Options, not in a separate Android preference file.

The Virtual Gamepad preference uses reserved byte 2 of the companion's native
mirror (`0` = On, `1` = Off), next to Clash Input in byte 1. Existing companions
therefore retain On without a format/version change. The host preserves this
field when loading and rebuilding snapshots on any platform; only Android
shows its menu entry. `Port_VirtualGamepad` / `Port_VirtualGamepadApply` own the
live value. Android publishes battle activity separately from overlay visibility
so tapping the top-center timer can request Start even when Virtual Gamepad is
Off. Pause requests are consumed on the game thread before normal pad exchange.

```text
Options — four visible entries at a time
  Save/Load
  Screen
  Sound
  Controller
  More Options
  EXIT — fixed native footer, outside the scrolling list

More Options
  Video Settings
  Battle Effects
  Visual Settings (Startup Logos, Intro Movie, Language Selection)
  General Settings (Autosave, Game Edition, Network Mode, Classic Mode)
  EXIT — returns to Options

Video Settings — four visible settings at a time
  Resolution
  Aspect Ratio
  2D Filtering
  Fullscreen
  VSync
  Frame Skipping
  Graphics API
  Frame Rate (FPS)
  Default — fixed native footer with Yes/No confirmation
```

More Options appears only when `Port_ZenkaiEnabled()` is true. Cancel in Video or Battle Effects returns to its category in More Options; cancel or EXIT in More Options returns to Options with More Options focused. EXIT in More Options uses the actual native main Options EXIT artwork, not a generated font caption. Video keeps Default, not EXIT or Back.

## Video settings and persistence

Descriptor row IDs are shared between native menu code and the host bridge. Preserve their order when changing the bridge.

| Row | Menu label | Choices | Default | Owner / saved key |
| --- | --- | --- | --- | --- |
| 0 | Resolution | Auto, 1x–8x | Auto (`0`) | `PortVideo.scale` / `scale` |
| 1 | Aspect Ratio | Auto, 4:3, 16:9, 16:10, 21:9, 32:9 | Auto (`0`) | `PortVideo.aspectMilli` / `aspect_milli` |
| 2 | 2D Filtering | Nearest, Bilinear | Bilinear (`1`) | `PortVideo.smooth2d` / `smooth_2d` |
| 3 | Fullscreen | Off, On | On (`1`) | `PortVideo.fullscreen` / `fullscreen` |
| 4 | VSync | Off, On | Off (`0`) | `PortVideo.vsync` / `vsync` |
| 5 | Frame Skipping | Off, On | Off (`0`) | `PortVideo.frameSkip` / `frame_skip` |
| 6 | Graphics API | Vulkan, OpenGL | Vulkan (`0`) | Host backend preference / `gpu_api` |
| 7 | Frame Rate (FPS) | 30, 60, 120, 240 | 30 FPS (`0`) | `gsInterp` / `interp` |

Resolution is the internal rendering multiplier, not a window-size selector. Aspect Ratio stores the width/height ratio multiplied by 1000 (`0` for Auto), rather than the displayed choice index. Existing saved ratio values remain valid. Changing the selected ratio leaves the current window dimensions unchanged; the rendered image, battle camera, and HUD ease to it over 20 frames, the same transition used in menus. Nearest and Bilinear expose the external overlay's Sharp/Smooth 2D filtering setting. Frame Skipping drops rendered pictures when the machine falls behind to help maintain game speed.

Frame Rate (FPS) selects frame interpolation: values `0..3` correspond to 30/60/120/240 FPS. Higher settings smooth rendering without changing battle simulation speed, replay timing or online gameplay. Graphics API changes are saved for the next start; they do not replace the running backend. Its help explicitly states that a restart is required. Existing environment overrides remain governed by their owning subsystems.

Default opens the native reset confirmation, initially selecting No. Confirming resets all eight Video descriptors to the defaults above. Rejecting or cancelling changes none of them. Other port settings, including fonts, effects and audio, are preserved. Confirmed More Options changes mark the game save dirty. Saving happens only when leaving Options for the main menu: Autosave off shows the normal save question; Autosave on saves automatically.

The FPS/network overlay is not part of Video Settings and is not shown in the two-tab F1 menu. Native Video Default does not change its `meter` preference.

## Battle Effects

Battle Effects sits beside Video Settings in More Options, with its own animated header, four-row scrolling list, native Off/On selectors, and Default confirmation. All eight effects default to On, matching the existing renderer defaults. Cancelling returns to Battle Effects in More Options.

| Row | Setting | Existing effect / host mapping |
| --- | --- | --- |
| 0 | Character Outlines | Outline around fighters; `fxOff` bit 1 |
| 1 | Occlusion Tint | See-through tint showing fighters behind scenery; bit 2 |
| 2 | Depth Haze | Distance-based color tint; bit 4 |
| 3 | Bloom | Glare and glow; bit 8 |
| 4 | Depth of Field | Distance blur; bit 16 |
| 5 | Motion Blur | Movement blur; bit 32 |
| 6 | Sharp Stage Blur | `PortVideo.sharpBlur` / `sharp_blur` |
| 7 | Weather | Stage haze, dust and mist; `fxOff` bit 64 |

The `fxOff` flags are inverted: an On choice clears only the corresponding disable bit, while Off sets it. Preserve every unrelated bit. Weather is On/Off only; it does not expose or reset the external overlay's Full/Light amount (bit 128). Sharp Stage Blur controls sampling at higher internal resolutions, preserving a crisper look when On and softer PS2-style sampling when Off; it does not turn stage blur off. All these changes use the existing renderer behavior and apply without a restart.

Battle Effects Default resets its eight controls only. Video Default resets Video only. Neither reset changes the other category, FPS/network overlay, font preferences, audio or other unrelated settings. Existing external Effects controls stay synchronized through the same `PortVideo` setter and `fx_off`/`sharp_blur` keys. Glow/Glare strength sliders are not added to this category.

The native page lifecycle is shared: `sOptionSettingsCategory` selects Video Settings (0), Battle Effects (1), Visual Settings (2), or General Settings (3) while retaining states 23/24/26 for page/reset/picker. `Option_PortSettingCount` and `Option_PortDescriptor` select the category's count and descriptors. The host bridge's historical `Port_OptionsVideoGet/Change` names accept flattened row IDs: Video Settings `0..7`, Battle Effects `8..15`, Visual Settings `16..19`, General Settings `20..21`. `PORT_OPTION_SETTING_COUNT` sizes both snapshots; `sPortOptionOffsets` supplies each category offset. `Port_OptionsDescriptor` resolves either range, including the native choice captions. Category-local row IDs remain separate from these host IDs.

`Port_OptionsVideoDefaults` queues Video only; `Port_OptionsBattleEffectsDefaults` queues effects only; `Option_PortDefaults` dispatches the native reset confirmation to its active category. Every apply starts with a fresh `PortVideo`, modifies only queued fields/bits and saves through `GsGpu_SetSettings` on the render thread. Keep both pending arrays initialized to the no-request sentinel when extending the bridge.

## Input and scrolling

- Up/Down selects settings and the footer; navigation wraps.
- Resolution, Aspect Ratio and Frame Rate (FPS) initially show only their label. Click/confirm reveals the current value and animated left/right chevrons without changing the value. Left/Right or clicking a chevron then adjusts it. Confirm keeps the changes and hides the controls; Back restores the value from before editing and hides the controls. Row navigation stays locked until editing ends.
- Every two-choice setting opens a native animated selector on click/confirm: 2D Filtering, Fullscreen, VSync, Frame Skipping and Graphics API. These rows show their label without inline value arrows.
- Inside a two-choice selector, Left/Right or mouse hover selects a button. Confirm/click applies that choice and returns; cancel returns without applying it. Mouse wheel switches the provisional choice. Merely hovering never changes the stored setting.
- Confirm on Default opens reset confirmation; confirm on More Options EXIT returns to its parent.
- Mouse movement focuses a row without changing its value. While editing an inline value, hover keeps that row active; clicking other rows, the footer, or scroll chevrons cannot leave editing. Either side of a binary row opens its selector.
- The mouse wheel selects rows when browsing and changes the value while editing an inline option.
- Up/down chevrons scroll only when more entries exist in that direction. Clicking one advances past the corresponding visible boundary.
- Footer hit areas come from the rendered native footer. They do not move with the row viewport.
- Confirmation, value changes and pointer interaction remain blocked during submenu entrance/exit phases.

The main list uses `sOptionTopFirst` and its animated scroll offset. Video uses `sOptionVideoFirst` and `Option_PortVideoEnsure`. Both display four rows; their footer stays outside the list. Video's footer index is `PORT_VIDEO_OPTION_COUNT`, not a descriptor row. More Options has four category rows (Video Settings=0, Battle Effects=1, Visual Settings=2, General Settings=3) and its EXIT footer at index 4. Shorter categories start at the same top row and leave the unused slots empty.

## Native appearance and geometry

Each visible row owns an independent copy of the original Options movie, created from resource section 8. `sOptionPlateMovies` and `sOptionPlateSelected` contain four slots. Advance visible templates each frame and destroy them before releasing the original Options resources.

`Option_PortButton` and `Port_FlashOptionsPlate` reuse the native rounded blue/gold plate, triangular pattern, border, selection icon and caption timelines. Normal captions use text kind 3; focused captions use kind 4, matching the configured Menu font styles used by Dragon Net Battle. Generated captions replace native text inside its timeline, retaining alpha, pulse, scale and offsets. Do not draw a second static caption afterward or replace the decoration with a plain rectangle.

| Element | Native coordinates / transform |
| --- | --- |
| Visible setting/main row `i` | Left `92 - 20*i`; top `72 + 44*i`; body height 32 |
| Row area scissor | x=0..511, y=52..278, including selected icon/shadow |
| Value text | Ink center x=420, y=row top+16 |
| Value chevrons | Centers x=360 and x=480, y=row top+16 |
| List chevrons | Center x=272; y=64 above and y=252 below |
| More Options category row `i` | Left `92 - 20*i`, top `72 + 44*i`; fewer than four entries occupy the top slots |
| Footer | Original `mc_bottom_plate_1` position and movement |

Rows and chevrons use the right foreground anchor (`0x1B`). The list-chevron x=272 is the midpoint of the visible list envelope x=32..512. It centers the arrows within the option list, not the entire window. Mouse coordinates must apply the same transform: `512 + (x - 512) * narrow`, with `narrow = 1333.333 / aspectMilli` when proportional foreground rendering is active, otherwise 1. Chevron artwork and hit regions retain their proportions in widescreen.

Values are centered using actual styled glyph ink bounds from `PortTbo_TextInfo`, not padded texture dimensions or generic font advance. Adding a longer value still requires visual review for clearance between its chevrons. Do not narrow hit geometry twice or treat a power-of-two texture's transparent padding as a visible button.

## Draw order, footers and animation

`Flash_OptionsViewportRow` inserts `Port_OptionsTopDraw` at the original menu-row depth. It runs once per movie pass and disables itself while template rows are drawn to prevent recursion. Both the main list and custom submenu rows use this insertion point. Drawing them after the complete movie puts them over Dende, native dimmers and selectors. Keep Dende in front and retain native overlay order.

Two-choice pages set `gOption->picker=3`, exactly like Stereo/Mono, so the native draw path hides controller-only `mc_select_plate_3/4`. They reuse `mc_select_plate_1/2` and root `fl_value_in` / `fl_value_out`, including the native dimmer and movement. `Port_FlashOptionsVideoPicker` replaces only the active choice captions; native selectors elsewhere are unchanged.

`Flash_OptionsCaptionQuad` centers the generated caption's measured ink in the original caption shape's local rectangle (`FlashShapeQuad`), rather than the outer button's 168×40 rectangle. Apply the original shape matrix (`trainingMatrix`) and live caption `prop` to that quad. This preserves the authored text pivot, focus scaling, movement and entrance/exit transform. Recentring the final draw against the outer button each frame cancels authored text movement; using button coordinates inside the caption clip adds its offset twice. Both approaches must be avoided. Alpha and tint still come from the native on/off caption layers. Mouse hits continue to use the visible native button body, not caption texture padding.

On confirm/cancel, restore `PORT_OPTION_VIDEO_PAGE` immediately and start `fl_value_out`, as native Stereo/Mono does. `Option_PortVideoFocus` then restarts the parent row's focused timeline during the selector's exit instead of waiting until it disappears. Confirm applies the provisional choice; cancel restores the original highlight without applying it. Keep the custom picker captions active while `sOptionPickerClosing` is true, so native words cannot flash during exit. The outer `MFLASH_PAD` input gate and mouse closing flag block new actions until the native transition finishes.

Focus uses `fl_on_start` and `fl_off_start`; a confirmed entry uses `fl_ok`. Opening a custom page first removes the outgoing rows for phases `-16..-1`, then reveals incoming rows during `0..16`. `sOptionFromPage` distinguishes Options from More Options during that outgoing stage. Returning removes the current submenu before restoring its parent. Native root `fl_menu_change` handles the footer/page transition. Keep mouse readiness consistent with these phases.

`Option_PortPrepareFooter` operates on the original footer and changes its focus timeline before the movie draws. It uses `fl_ok` during Default confirmation, then returns to `fl_on_start` after the dialog closes. Footer focus state is separate from the four row-template states.

`Port_FlashOptionsVideoFooter` has three modes:

| Mode | Result |
| --- | --- |
| 0 | Original native footer behavior |
| 1 | Video Default: replace only the caption inside native text timelines |
| 2 | More Options EXIT: preserve native EXIT artwork without font replacement |

Both custom modes keep the native footer transform, visibility, animation and draw depth. Their captured hit region is named `mc_video_default`, including in More Options, so pointer input targets the custom page rather than main-menu EXIT. Do not animate a separate copied footer with a manually applied row offset.

Keep the bottom help window open when entering/leaving More Options, Video or Battle Effects. Change its text through `Option_PortHelp`; calling `MsgWin_Open` again restarts its entrance animation.

## Category headers

`Option_PortHeading` selects “More Options” for its category page and the active category title (“Video Settings” or “Battle Effects”) for the settings page, its two-choice selectors and Default confirmation. Original/main Options returns NULL and retains its native header. Custom pages use `IconWin_SetIcon(0)` to keep the Options emblem instead of accidentally inheriting Controller from their higher state numbers.

`Port_FlashOptionsHeading` scopes the replacement to `IconWin_Draw`. Only the shared heading's title image (screen scope 3, image 4) changes. Render text kind 5, the native-size 512×64 header sheet from `MenuText_RenderWiiLabel`, with the existing orange gradient, brown outline and light rim. `Flash_OptionsHeadingQuad` maps the sheet through the original sprite's actual UV rectangle and then applies its original shape/clip transforms and alpha. Do not independently fit glyph metrics into the title rectangle or scale a tightly packed kind-2 heading to fill it: both introduce a separate size from the native sheet.

The native emblem, underline, clouds, opening motion and idle motion remain part of IconWin. Clear the override immediately after drawing so another screen cannot inherit it. Do not hide the entire header or draw an unanimated overlay label, and do not restart IconWin when changing the category title.

## Help-dialog line length

Use the available pixel width, not a fixed character count. `Option_PortWrapHelp` measures through `Font_GetWidth` and wraps at `MsgWin_PortLineRoom(400)`, matching the existing help margin in the 512-wide native coordinate system. The room depends on where the native text starts; 400 is the right boundary, not a 400-pixel line length.

Prefer concise help that fits one or two lines. The wrapper breaks at spaces, preserves explicit newlines, and splits a word that cannot fit. Proportional fonts give different widths to W and i, so a character limit cannot guarantee a fit. Keep translated help short and check it in the actual selected font.

`Option_PortHelp` alternates two UTF-16 buffers when the source message changes, preserving the screen reader's ability to distinguish selections without reopening the window. Strings begin with `0xFEFF` and are null terminated. The current help source is ASCII English; `Option_PortWrapHelp` widens bytes and does not decode arbitrary UTF-8. Add proper decoding when wiring localized text, following the existing Dragon Net help path rather than copying UTF-8 bytes directly.

## Code map

| File | Responsibility |
| --- | --- |
| `include/menu/port_video_options.h` | Port-only IDs, settings descriptors, values, defaults, help and counts |
| `include/menu/port_options_page.h` | Native state handling, viewports, templates, pointer input, help and footer preparation |
| `src/menu/menu_x_c.c` | Options lifecycle, draw insertion, mouse readiness, native Default dialog lifecycle |
| `src/sys/gfxm_c.c` | Row insertion, native caption/header replacement, footer handling and chevron drawing |
| `port/src/gs/ui.cpp` | Render-thread settings bridge and external settings UI |
| `port/src/gs/ui.h` | `PortVideo` fields and renderer settings API |
| `port/src/gs/gs_draw.c` | Renderer/window setting application and persistence |
| `port/src/gs/tbo_text.c` | Styled text textures and actual ink measurements |
| `src/battle/view_a_b.c` | Native dynamic message-window drawing and line room |

Port-only IDs are `PORT_OPTION_MORE` (main cursor 6), `PORT_OPTION_MORE_PAGE` (25), `PORT_OPTION_VIDEO_PAGE` (23), `PORT_OPTION_VIDEO_RESET` (24), and `PORT_OPTION_VIDEO_PICKER` (26). Handle these before the original switch and do not pass the added main cursor to an original numbered-row lookup. If ZENKAI is disabled while a custom page is active, return to the original Controller entry and restore native plates.

## Adding another Video setting

1. Define the setting contract: owner, legal stored values, default, validation, immediate versus restart application, and persistence key. Reuse an existing owner and setter where possible.
2. Append a descriptor in `port_video_options.h` with label, value array, name array, count, default value and concise help. Increment `PORT_VIDEO_OPTION_COUNT`. Preserve existing row IDs and keep counts consistent with both arrays.
3. Extend `sNativeVideoPending` in `ui.cpp`, explicitly initializing every pending slot to `-1`. Array storage uses the shared count, but an omitted initializer would become zero and apply an unwanted change. Negative stored values need a separate pending flag rather than this sentinel.
4. Extend both the application switch and current-value snapshot in `native_video_apply`. Get a fresh `PortVideo` and modify only requested fields. For a setting owned outside `PortVideo`, call the owning subsystem and use the existing settings store. Do not reset unrelated fields.
5. Verify defaults: `Port_OptionsVideoDefaults` queues each descriptor's actual default value. It must reset the new setting only after native confirmation.
6. A descriptor with exactly two choices automatically uses the native selector; descriptors with more choices use inline arrows. Review navigation, scrolling, help and footer indexing. They use `PORT_VIDEO_OPTION_COUNT` and `PORT_VIDEO_VISIBLE_ROWS`; no extra movie is needed while only four rows are visible. Keep pointer and drawing coordinates identical.
7. Update the external overlay to use the same setting. Avoid static UI caches that become stale when the native menu changes a preference; Graphics API reads its saved preference each time.
8. Extend tests for every value, wrapping, default/reset cancellation, queued changes, preservation of unrelated fields and external/native agreement. Update the table above.

For example, the implemented 2D filtering descriptor represents values `0/1` as Nearest/Bilinear, defaults to 1, and maps row 2 to `video.smooth2d`. `GsGpu_SetSettings` applies and saves `smooth_2d`; its value is also stored in the ZENKAI settings extension.

`Port_VideoOptionIndex` returns the nearest supported value, useful for externally supplied aspect ratios. Validate the row before calling it. Display indices are not necessarily stored values; never save an index in place of its descriptor value.

### Threading and save ownership

The game-thread getter returns pending values when present, so rapid clicks cycle correctly before a render frame applies them. The changer only queues requests. A spinlock protects current/pending state. `native_video_apply` runs at the start of `frame_build`; SDL window changes, renderer operations and settings writes happen there, not on the game thread.

Most rows apply through `GsGpu_SetSettings`. Graphics API and Frame Rate use the backend/interpolation owners and `Port_SettingSave`, followed by `Port_SettingsWrite` when no renderer setter already performs that write. Do not duplicate persistence or overwrite unrequested settings.

Native game structures use 32-bit pointers; the host UI uses C++ host types. Keep bridge arguments/results small typed integers. Do not pass `PortVideo`, C++ objects or pointers through untyped variadic calls.

In ZENKAI mode, all native and More Options values are authoritative in the checksummed `.sav.options` companion. Normal play has no external settings-file startup cache: defaults apply unless the active ZENKAI companion supplies a value. A new numeric key normally uses a default when absent without changing the file format. Renaming keys or changing stored meanings requires a compatibility decision. `ZENKAIPOWER_SETTINGS=<temporary path>` remains available only for isolated persistence checks; never point it at a player's normal preferences. See [External settings retirement](external-settings-retirement.md).

## Adding another category inside More Options

For a descriptor-driven category like Battle Effects, extend the shared category selection, descriptor lookup and flattened host bridge. A category needing a different flow can instead use an unused port-only page state. Add its entry inside More Options. Extend its category labels, input wrap, mouse rows, help, draw loop and return destination together. Keep EXIT in the native footer and preserve outgoing-before-incoming transitions. Gate input, hit areas, drawing, resources and any mode-switch fallback with `Port_ZenkaiEnabled()`.

Implement the category's own descriptors and bridge to its owning subsystem; renderer effect controls can reuse `PortVideo`, while unrelated settings belong to their own owner. Share resources, but give simultaneously drawn rows independent mutable animation state. Reuse the native style, proportional anchors and original menu-row insertion depth. Release any added resources on leaving Options.

For localization, register stable IDs and English defaults in the existing menu translation catalog and route labels/help through the existing text-copy bridge. Descriptors alone do not register translations. Do not use translated labels as settings keys or reorder descriptor IDs by language.

## Verification without a full game build

```sh
python3 port/tests/ui/options_video.py
python3 port/tests/ui/options_layers.py
python3 port/tests/ui/options_mouse.py
python3 port/tests/ui/options_mouse_geometry.py
python3 port/tests/ui/options_value_alignment.py
python3 port/tests/ui/options_help.py
python3 port/tests/ui/memory_card_mouse.py
cc -std=gnu89 -fsyntax-only -fno-builtin -Iinclude -Iport/include \
  -include port_compat.h src/menu/menu_x_c.c src/sys/gfxm_c.c
```

`options_video.py` exercises production input helpers and the host bridge with mocked renderer/dialog owners, including More Options/Video/Battle Effects navigation, all eight effect mappings, category-specific reset isolation, Weather amount preservation, settings application, scroll boundaries, provisional binary choices, mouse confirmation, selector cancellation, Default confirmation outcomes, mode changes and unrelated-field preservation. `options_layers.py` checks insertion ordering and recursion protection. Mouse tests cover routing and rendered geometry. Alignment tests check styled ink bounds. `options_help.py` checks proportional-width wrapping and content preservation, including explicit line breaks and overlong words.

These standalone checks do not prove live pixel alignment, driver behavior or persistence after a real restart. After the next authorized game build, inspect 4:3 and widescreen, all scroll positions, long values, selected icons, footer artwork, Dende's foreground order, native dimmers, focus pulses and submenu transitions. Check that the bottom dialog does not reanimate. Test pointer, keyboard and controller navigation, and confirm native/external settings agree. Check both graphics backends and restart-only API changes. Reset cancellation must leave every setting unchanged.

`port/tests/ui/menu_mouse_geometry.c` also checks generated picker ink against the original caption shape centre through multiple native scales and translations, using the production quad builder and native transform. It verifies both centering and continuing authored movement.

## Visual Settings and General Settings

More Options also contains **Visual Settings** and **General Settings**, using the same native headers, row animations, help window, four-row viewport, mouse geometry and category-specific Default confirmation.

| Category | Setting | Choices | Default | Saved keys |
| --- | --- | --- | --- | --- |
| Visual Settings | Startup Logos | Off / On | On | Inverse of `skip_logos`; the copyright page is always skipped |
| Visual Settings | Intro Movie | Off / On | On | Inverse of `skip_intro` |
| Visual Settings | Language Selection | Off / On | On | `language_prompt_always` |
| General Settings | Autosave | Off / On | Off | `save_quick` (stored row 21) |
| General Settings | Game Edition | Sparking! METEOR / Budokai Tenkaichi 3 | Sparking! METEOR | `game_version` (0 / 1; stored row 19) |
| General Settings | Network Mode | Online / LAN | Online | `dragon_net_mode` (1 / 2; stored row 20) |

Game Edition is dimmed and cannot be activated without extracted Japanese soundtrack assets, checked by `PortSongs_SoundtrackAvailable`. Network Mode is dimmed and cannot be activated without extracted Wii artwork, checked by `Port_WiiArtAvailable`. Availability is published atomically from the render thread, so native input and drawing do not load disc assets. Default skips unavailable settings. These checks reuse the existing extraction availability checks; they do not inspect mounted discs.

Startup settings apply on the next launch. Language Selection Off uses the saved language; the first launch still asks when no valid language has been saved. In ZENKAI mode, that locale is available from the options companion before the game save is loaded. Network Mode reuses the existing online/LAN handling; the LAN host address remains configurable in external settings.

Category counts and flattened offsets live in `sPortOptionCounts` and `sPortOptionOffsets`: Video Settings 0–7, Battle Effects 8–15, Visual Settings 16–18, General Settings 19–21. These flattened IDs remain stable for saved data. General Settings displays the rows in a different order: Autosave uses row 21, Game Edition row 19, then Network Mode row 20. `Option_PortSettingRow` maps displayed rows to those stored rows; use it for descriptor lookup, input, availability and drawing rather than adding the visible row directly to the category offset. `Port_OptionsCategoryDefaults` resets only the selected category, and `Port_OptionsAvailable` guards both native input and host writes. Disabled rows use the same RGB `0x58/0x80` tint as disabled Dragon Net Battle controls, applied to both artwork and captions without reducing alpha. They accept pointer/controller focus and play normal navigation/confirm sounds, but activation cannot open a selector or change a value. Focus uses the native selected timeline with the disabled tint retained; native row animations and decorations remain intact. Keep render loops bounded by the category count as well as the four-row viewport.

General Settings remains accessible without Wii assets; only Network Mode is disabled. Network Mode uses the native two-button selector. With Wii assets available, a legacy Disabled (`dragon_net_mode=0`) preference migrates to Online. Autosave uses the existing `save_quick` setting and is included only in General Settings Default.

## Saving More Options in ZENKAI mode

In ZENKAI mode, Options persist in `savegame/zenkaipower.sav.options`, alongside `zenkaipower.sav`. The binary companion's full record uses the main save's checksummed, atomic writer. Version 3 retains 40 explicit little-endian binding storage values for compatibility, with 17 active actions and three reserved slots. Each action uses a keyboard key or mouse button exclusively. Version 2 companions migrate by dropping vertical-camera/Pause bindings and preferring the mouse button when an old action has both inputs. Version 1 companions still load with default PC bindings. The `ZOPT` envelope contains a schema/version header, the 52-byte More Options payload and 160 bytes of native Options: voice/controller flags, both players' button maps, camera choices, screen position, sound mode and volumes. The native mirror's reserved second byte stores Clash Input (0 Rotate Stick, 1 Tap Strike); older companions default to Rotate Stick. Version 7 adds the locale and startup-prompt metadata. Version 8 appends the
online player name. Version 9 appends player 2's 40 keyboard/mouse storage values
and 17 gamepad bindings, leaving all earlier offsets intact. Existing versions
1?8 retain player 1 bindings and load player 2 defaults. Name/language updates
preserve both players' saved bindings. Native save-format fields remain as a compatibility mirror; the companion takes precedence on load. Unrelated flag bits, progression and the two unknown native words at `0x168C` are not overwritten.

The boot language selector runs before the first game save and before normal Options loading. In ZENKAI mode it reads the locale from the companion; if no full Options record exists, choosing a language can create a language-only version 7 record. That record is not treated as native Options or bindings, and a later full Options save replaces it with the complete record. The startup selector preference can also be retained in this minimal record. Autosave is read from the full companion before the boot memory-card flow makes its save/load decision, so the saved `save_quick` choice is available before the game save is loaded; a missing or language-only record falls back to the default.

`Port_OptionsSaveLoad` runs after successful game loading and mode commit. It restores native Options before `SndOpt_Apply` and queues port settings for the render thread. If the companion is absent, legacy native fields and valid embedded `ZOP1`/`ZOP2` More Options records supply migration values. Unknown or corrupt companions are ignored without overwriting them during loading. A recognized embedded block is cleared when preparing a later save, before the native checksum. Original mode and isolated online-session cards never read or write the companion.

`Port_OptionsSaveSync` stages a separate snapshot and marks Options dirty when native or port settings change. It never writes the full Options record. The existing native Options exit flow controls the Save question or Autosave; returning through submenus does not save. After the main save succeeds, `Port_OptionsSaveCommit` writes the full companion. A companion write failure enters the existing save-error flow and does not publish the startup cache. The main save and full companion are individually atomic, not a single filesystem transaction; keep them together when copying saves. The early language-only record is the exception: it supports startup selection before a game save exists.

During ZENKAI play, numeric keys owned by More Options use a runtime overlay in `plat_settings.c`. After both saves succeed, `Port_SettingsGameCommit` publishes that overlay to the startup cache. This cache supplies startup/backend choices before a game is loaded; the companion is authoritative on load. Graphics API still requires a restart. Cancelling an inline edit restores its initial value; Confirm keeps it pending for the normal save flow.

Checks: `options_video.py` covers native/port round trips, migration, schema rejection, dirty tracking, failed publication, missing-disc preservation and Original mode isolation. `port/tests/save/options_file.c` uses temporary files to check versioned save IO, corruption detection, failed atomic replacement and preservation of unrelated save bytes. `options_settings.c` checks runtime/cache isolation. No player save files are used.

Startup Logos, Intro Movie and Language Selection use positive On=show/play semantics. Defaults are On, On and On respectively. The copyright page is always skipped, regardless of the Startup Logos setting; the legacy `skip_copyright` preference is retained for compatibility. The ZOP2 decoder also accepts ZOP1 saves and inverts their three legacy skip values, preserving saved startup behavior. Existing preference keys remain compatible.


## Classic Mode (save-owned General Settings option)

General Settings always displays Classic Mode after Network Mode as its fourth
row. It is enabled only while the current save can still disable the Zenkai pack. Its descriptor-only
flattened row is `PORT_CLASSIC_OPTION_ROW` (22). The existing 22 persisted ZOP rows
stay unchanged: the choice belongs to the main save, not the Options companion.
It is exposed only in the in-game menu and managed on the game thread.

The native On/Off picker displays the inverse of the pack state: Off keeps the
pack enabled; On requests original progress and mechanics. Help reads:
"Change the game to classical progress and mechanics."
Confirming On closes the picker, then opens the native Yes/No dialog:
"Enable Classic Mode? You cannot turn it off again."
No is selected by default. Cancel/No changes nothing; Yes commits the permanent
pack-disabled flag and marks Options dirty for the normal game-save flow.

After earning positive Zeni or enabling Classic Mode, the entry stays visible
and focusable but is dimmed and cannot open its picker. Older saves without the
earnings record also disable it. The row count and Default footer stay unchanged.
Default never changes this save-owned choice. The setter rechecks eligibility
before committing. The renderer bridge skips this row when resetting category
defaults; native picker captions use its shared descriptor.
See [the pack contract](zenkai-pack.md) for feature IDs and saved layout.

## Controller binding implementation

`port_keyboard_options.h` owns the parent and player submenu navigation, native
plates, pointer input, help, and per-player binding pages. `sOptionPcPlayer`
selects player 0/1; `sOptionPcPlayerMenu` distinguishes the player submenu from
the parent while retaining the native Controller state and page. New player
entry IDs are cursor IDs, not native movie row indices. Keep the displayed row
count, hit testing, focus wrapping and Android scrolling in agreement.

`PortInput_PlayerGet/Set` read or write a specified player's bindings under the
input mutex. `PortInput_PlayerDefaults` resets one player/input type. Legacy
`PcGet/Set` and `GamepadGet/Set` continue to address player 1 for online input
export. Binding-name and capture APIs encode the player as
`action + player * PORT_PC_STORAGE_ACTIONS`; the capture stores this player
before accepting SDL events. Duplicate assignment clears only that player's
other action. Accepting a gamepad binding also assigns that physical gamepad to
the selected player. If the other player was using it, their controller slot is
swapped with the selected player's previous slot, so one gamepad does not
unexpectedly control both players. Cancel and Delete do not change assignments.
The assignment is applied on the game thread after capture, outside the binding
mutex; a disconnected gamepad is ignored. Both menu and battle polling use the
same assigned controller slot, independently of the player currently being
edited in Options.

Reserved native companion byte 3 stores `slot + 1` for player 1 in the low nibble
and player 2 in the high nibble. Zero retains the original first/second gamepad
defaults, so old saves remain compatible. Normal Options snapshots, saves and
loads preserve these assignments alongside both players' bindings. Existing
bindings made before device assignment was implemented need one action rebound
with the intended gamepad to record its player assignment.

Checks: `keyboard_options.py` covers submenu navigation, 2P capture selection,
mouse input and Default confirmation. `android/controller_options.py` checks
both platform layouts. `player_bindings.py` exercises actual SDL capture,
per-player keyboard/gamepad battle mapping, cancellation and isolated defaults.
`save/options_file.c` checks both players' save round trips and exact v8 file
migration; `options_video.py` checks the shared snapshot/apply bridge.
