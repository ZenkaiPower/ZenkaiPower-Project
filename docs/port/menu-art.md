# Menu artwork at 4:3 proportions

The PC option **Esc → UI → Menu art and dialogs in 4:3 (Work in Progress)** (`title_main_chars_43`, default on; previously called **Text and artwork proportion**) keeps completed screens wide with proportional foreground artwork. Unfinished screens automatically use a centred 4:3 page with black side bars. While this option is enabled, **Menus in 4:3** is shown checked and disabled: it represents the automatic fallback, not a whole-page override on completed screens. Turning the first option off restores the second option's saved manual preference. A manually enabled **Menus in 4:3** then applies to every menu page. Battles are unaffected.

## Credits in 4:3

The ending/credits movie (`zs3used.pss` or the optional Japanese `zs3jped.pss`, selected by Game Version and played by `Movie_PlayEnding` in `src/sys/movie.c`) always uses a centred 4:3 page on PC. On widescreen this produces black side bars and preserves the movie's original proportions, independently of the menu-art and whole-menu 4:3 settings. `Port_Credits43Set` scopes this override to the ending playback; it clears after playback ends or START skips it.

## Intro video proportions

The opening movie is played by `Movie_PlayOpening` in `src/sys/movie.c`. With
**Esc → UI → Text and artwork proportion** enabled (the default), the complete
intro uses a centred 4:3 page. The boot logos and initial copyright page use the
same presentation while they are shown. A widescreen output has black bars on
the left and right; the picture keeps its original proportions. This applies to
both regional intros and uses page presentation rather than the Flash foreground
markers.

`Port_Opening43Set(1)` starts the playback override before opening the movie;
`Port_Opening43Set(0)` clears it after `Movie_Run` and `Movie_Close`, including when
playback is skipped. The title screen then returns to the configured page aspect.
With artwork proportion disabled, the intro follows the configured aspect, subject
to the independent **Menus in 4:3** override while menu mode is active.

**Esc → Misc → Game Version** selects the intro assets independently of this layout
setting. **Sparking! METEOR** uses `zs3jpop.pss` with `zs3jpop.adx` when both Japanese
files are available, including through the optional `sparking-meteor.pak`.
`Port_UseJapaneseOpening` in `port/src/plat_file.c` requires the complete video/audio
pair. Otherwise, **Budokai Tenkaichi 3** playback is used: `zs3usop.pss`, paired with
`zs3euop.adx` if available or `zs3usop.adx` otherwise. **Skip intro video** on the Misc
tab skips opening playback; it does not change the layout setting.

For both intro and credits, `Port_PageAspectMilli` in `port/src/plat_stub.c` supplies
the page override to the OpenGL and SDL GPU presentation paths. Each path clears
the surrounding output to black and fits the page inside the window. Credits always
use 4:3; the intro's override depends on the artwork option.

Verify both regional intros at 4:3, 16:9 and ultrawide, with artwork proportion on
and off and whole-menu 4:3 enabled separately. Check normal completion, skipping
during playback, and skipping through **Skip intro video**. In each case, confirm
that the following title/menu uses its configured aspect and has no leftover movie
bars. Also check Japanese-asset fallback when either member of the pair is missing.

## Screens covered

- **Title screen:** selected foreground art and the title menu entries.
- **Main menu:** option artwork, Dragon Balls, guide characters, and the bottom description text.
- **Dragon Story saga selection:** the animated monkeys and branch, guide portraits, saga and level plates, scenario thumbnails with their decorative backing, label bars and NEW badge, the complete achievement panel (caption, digits and percent sign), navigation arrows, and the guide's dialogue text. Its picture background, scenery animation and message-window backing remain full-width.
- **Dragon Story episode selection (after opening a saga):** episode thumbnail plates with NEW badges, large preview with its mask and frame, saga and battle titles, CLEAR badge, navigation arrows, guide portrait and dialogue, and the scrolling story panel with its text and next indicator. The separate `ModeBg` picture and scenery movie remain full-width except for Giru in the GT saga.
- **Dragon History battle outcomes:** win/lose character artwork and captions, episode title and backing, earned/total points with their backings, and the complete reward popup with text. Background pictures and scrolling speed lines remain full-width.
- **Versus Duel menu:** Vegeta and Nappa, complete main-option and battle-type plates, DP choices, battle settings and value panels with their backings, help text, and the reset-confirmation dialog. The picture, clouds and smoke remain wide. Character selection after leaving this menu has separate rules below; team selection is not covered.
- **Versus character selection:** individual foreground artwork uses scoped proportion rules over the wide background. This includes portraits/masks, labels, grids, customization, stage/music controls and item help. The separate team-selection layout is not included.
- **Dragon Net Battle:** home and rewards, Custom/Normal/Ranking Battle selection, friend menus and room-code entry,
  player-name keyboard, settings, ranking pages, matchmaking/VS cards, and post-match results. Foreground
  panels, portraits, Pan/Giru and their facial layers, radar cursors, labels, numbers, native and generated text
  use edge-specific anchors: header/Pan at left, home/settings Giru at right, and shared controls centred.
  Keyboard/name/code pages stretch their complete foreground without forced side bars. Bottom dialogue follows the separate MsgWin rule: wide backing with
  proportional text anchored at the left edge; landscape/space backgrounds remain wide. Connection/account dialogs keep their independent
  proportional window and full-width dimmer.
- **Ultimate Battle:** landing menu, Mission 100, Survival, Disc Fusion, course selection, ranking ladder and results. Includes proportional entries/backings, guide anchors, subtitles and Disc Fusion offscreen option checks. Ranking/course lists, matchups and shared results have dedicated rules; individual in-game visual review remains pending.
- **Ultimate Battle character selectors:** solo and team selection, including portraits and their divider, name/form panels, grids, customization, team slots, guides and item inspection/details. The separate Versus team selector is not covered.
- **Sim Dragon:** right-anchored entry/results; left-anchored day-board monitor, money, day counter and status, with right-anchored actions/backing; centred training/minigame elements over stretched backgrounds/effects; centred pause/item menus and inset-preserving subtitles. The battle introduction uses wide scenery with edge-anchored portraits/dividers and centered VS/BATTLE artwork.
- **Dragon World Tour:** tournament selection/settings and invitations, entrant character selection, brackets and moving chips, guides, VS/result panels, subtitles and rewards. Scenery stays wide; controls use component anchors and bracket elements share a centred transform. See the detailed coverage and visual checklist below.
- **Memory-card dialogs:** the animated dialog window and its text wherever the memory-card flow opens it.
- **Exit confirmation dialog:** the animated window, choices, and text.
- **Shared menu headers:** the `IconWin` heading artwork on screens that use it, including Ultimate Battle, character selection, and Battle Net. This covers the shared header component, not each screen's other artwork.

The screens listed above have explicit component or movie rules; the detailed sections below describe their anchors and exceptions. Unlisted screens receive only the shared header/dialog treatment where applicable. Versus team selection remains outside this coverage. Dragon Story saga-outro movies now use a centered proportion-preserving foreground group (scope 43), with closing-text callbacks flushed inside the group and guide dialogue using `Port_MsgWinDraw43`; visual verification is pending. Source checks have passed, but in-game visual verification remains necessary for the documented layouts and animations.

## How the marker works

The game's menu movies are Flash-like timelines. `FlashSlot_Draw` in `src/sys/gfxm_c.c` can place a marker before a named clip tree and reset it after the tree. `FlashShape_Draw` can mark a standalone image record. In `port/src/gs/gs_draw.c`, markers `0x19`, `0x1A` and `0x1B` narrow the drawn vertices about the centre, left edge and right edge respectively. For menu rendering the scale is `1333.333 / Port_PageAspectMilli()`; at 16:9 it is approximately 0.75. The right-edge and left-edge pivots are page coordinates 512 and 0. No 4:3 scissor is used: artwork can animate into and out of the visible area without being cut off.

`Port_FlashDrawMenuArt(movie, screen, ...)` scopes rules to a known movie: title (1), main menu (2), shared heading window (3), the opted-in memory-card/exit dialogs (4), Dragon Story saga selection (5), episode selection (6), battle outcomes (7), GT background Giru (8), and Versus Duel menu (9). Character selection uses movie scopes 10–12 as described below. Other menu movies are not automatically transformed. A marker never changes the PS2 build because all selection code is under `#ifdef PORT`.

### Artwork staged outside the page

A movie may park clips beyond x=0 or x=512 before a later `fl_*_in` animation. A 4:3 marker narrows their positions around its pivot, which can bring them into the visible wide page early. Dragon Story does this with the main-menu entries, saga carousel and difficulty panel. Their clip trees are hidden until the corresponding `fl_menu_in`, `fl_scenario_in` or `fl_level_in` command starts. The saga and difficulty visibility flags remain set during their exit animation and reset once the menu accepts input again. This phase gate is necessary even when a parked shape is partly inside the original page.

On Dragon Story saga selection, `FlashShape_Draw` also checks the transformed quad **before** the renderer applies the 4:3 marker and skips marked quads entirely outside the original horizontal page. This covers both selected clip trees and standalone selected images. The saga label backing follows the scenario visibility bit, as do the pictures, labels and NEW badge. Decorative frame image 76 is shared by saga selection and difficulty selection: keep it visible while either phase bit is set. Difficulty heading image 82 follows the difficulty bit. This keeps other parked pieces from appearing early without clipping a clip that has begun entering the page.

Dragon Story's `mc_menu_plate_*`, `mc_scenario_*` and `mc_level_plate_*` entries use the **left** pivot throughout entry, idle and exit. The saga label backing, NEW badge, shared decorative frame and difficulty heading use that same pivot. With a centre pivot, x=0 becomes x=64 at 16:9 (x=112 at 21:9): an entry first passing the original page boundary would suddenly appear inside the screen, then move. The left pivot keeps x=0 at the visible edge, so each plate and its nested label slide in from outside. Phase gating alone does not fix that jump once `fl_menu_in` starts.

When extending another screen, inspect its initial and transition frames for parked artwork as well as its settled layout. Identify the `fl_*_in` command that reveals each group and the point when its exit timeline finishes. Gate the complete parent clip tree until entry begins; keep it visible through exit. Keep any visibility rule scoped to that movie. A full-outside quad test catches remaining parked pieces but cannot hide a partly visible group by itself. Do not apply a blanket 4:3 scissor: it can cut off intended entry and exit animations.

## Artwork groups

| Group | Selection | Pivot |
| --- | --- | --- |
| Title art | title clip trees and identified image records | centre |
| Main-menu options and Dragon Balls | complete option clips, balls and standalone holder image 42 | left |
| Main-menu guides | `mc_guide_*` clip trees | centre |
| Dragon Story initial-screen monkeys | complete `mc_saru` tree, including `mc_saru_oyako` and `mc_eda` | left |
| Dragon Story menu entries | complete `mc_menu_plate_*` clips, including nested labels | left |
| Dragon Story guide portraits | `mc_guide_gokuu` and `mc_guide_0*` | centre |
| Dragon Story saga and difficulty groups | `mc_scenario_*`, `mc_level_plate_*`, `mc_icon_new`; movie-local images 37 (saga label bar), 76 (shared decorative frame), 82 (difficulty heading) | left |
| Dragon Story achievement | `mc_tassei_num_*`; movie-local images 77, 78 (caption and panel with percent sign) | right |
| Dragon Story navigation arrows | complete left and right arrow clips | left, matching the carousel |
| Dragon Story episode list and preview | `mc_menu_plate_*` (including thumbnails and NEW badges), `mc_battle_mask`, `mc_image_l_1`, `mc_battle_text_1`, `mc_clear`, both `mc_yajirusi_*` trees; movie-local images 6, 7 | left |
| Dragon Story episode saga header | `mc_scenario_text` and movie-local image 5 (title backing) | centre |
| Dragon Story episode guide | complete `mc_guide_0*` trees, including eyes and mouth; centred between the scenario preview and the right screen edge on widescreen | right |
| Dragon Story scrolling story | `mc_scenario`, `mc_episode_text_*`, `mc_episode_next`, both mirrored root shapes using image 38; separately flushed story glyphs | centre |
| Dragon History outcome characters | complete `mc_chara_*` illustration trees | right |
| Dragon History outcome points | `mc_point_text_*`, `mc_get_point_*`, `mc_total_point_*`; result-movie images 5 and 7 | left |
| Dragon History outcome captions and episode title | `mc_result_text_*`, `mc_battle_text`; result-movie image 3 | left |
| Dragon History reward popup | complete `GetWin_Draw`, including its `Font_FlushAll`, scoped by `Port_GetWinDraw43` | left |
| Versus Duel options and DP | `mc_menu_plate_*`, `mc_mode_*`, `mc_dp_*`; movie-local images 41–43 | left |
| Versus Duel Battle Settings | `mc_battle_set_text_*`, primary `mc_set_*` values; movie-local images 44–46 | centre |
| Versus Duel guides and choice panels | complete `mc_guide_nappa`, `mc_guide_bejita`, settings value-choice panels and `set_4_1p2p`; images 47, 48 | right |
| Versus Duel help/reset dialog | scoped `Port_MsgWinDraw43` / `Port_DialogDraw43` | left help text; centred confirmation window and text |
| Character selection | portraits/masks and shared controls; side labels, grids and customization | left/right portraits, dividers and side UI; centre shared controls |
| Dragon Net header and crest | native title sheets and generated title text, including their entrance transforms | left |
| Dragon Net Pan | body/pointing, eye and mouth layers; identical pivot through entrance, idle and exit | left |
| Dragon Net Giru | home/settings body and eye layers | right |
| Dragon Net battle-page Giru | left-side battle/VS/ranking guide and its eyes | left |
| Dragon Net home/User Settings rows | complete plate, native/font label and attached radar | same centre anchor and resting x=162 (radar x=196) |
| Dragon Net battle/format/friend rows | complete plate, native/font label and attached radar | left, with a right margin and compensated entrance |
| Dragon Net cards | VS and ranking cards, reward/result panels | centre |
| Dragon Net bottom guide text | separately flushed MsgWin glyphs; backing stays full-width | original animated inset; glyph widths preserve proportion |
| Dragon Net keyboard | complete player-name/room-code page, native keyboard and Wii overlays | stretched with the screen, no forced side bars |
| Shared heading | left-side clips and standalone shapes; `mc_window_up_02` separately | left; right corner at right |
| Memory-card and exit dialogs | animated window and separately flushed glyphs; texture 0 dimmer remains full-width | centre, except dimmer |
| Main-menu bottom description | separately flushed glyphs, not its window backing | left |
| Dragon Story saga/episode guide dialogue | `MsgWin` glyphs flushed by `Port_MsgWinDraw43`, not its window backing | left |

The shared heading is drawn by `IconWin_Draw` in `src/battle/view_a_c.c`; every caller now gets the same scoped treatment. Character artwork outside the title and main menu retains its previous layout for now. The bottom description is drawn by `MsgWin_Draw` in `src/battle/view_a_b.c`. Dialogs are scoped by `Port_DialogDraw43`, so unrelated callers of `Dialog_Draw` keep their old layout. Flat colours, full-screen pictures and unselected window panels are not character art.

The Dragon Story saga panel is not one parent clip. In its archive movie (`gamedata/pzs3us1/00460.bin`, section 1, resource 10), root shape 36 uses image 37, shape 75 uses image 76, and shape 76 combines images 78 and 77. Marking only `mc_scenario_*` and `mc_tassei_num_*` leaves their backings wide and separates the digits from the percent sign. Apply the same left transform to the saga pieces and NEW badge, and the same right transform to the achievement caption, panel and digits. Do not infer shared parenting from pieces looking like one panel on screen. The achievement group is tied to the right edge, so its backing, caption, digits and percent sign must all use `0x1B`.

Saga-selection guests (`mc_guide_00` through `mc_guide_07`, scope 5) retain their
centered resting positions. Their body at depth 1 moves from x=0 to its settled
translation and back; the settled values are 250, 190, 200, 230, 190, 170, 190 and
190, decoded from archive 460/1/10. Blend an additional leftward offset from zero
at rest to the centered page's full side margin at the parked pose. Apply it to
the parent so the body, eyes and mouth travel together. These portrait trees
bypass the original-page quad rejection, allowing them to animate through the
wide side area until they leave the physical screen. Other saga/level pieces
keep their existing rejection. At 4:3, with artwork proportions disabled, or with
whole-page 4:3 selected, guest placement is unchanged. Verify both introductions
and event visits at 16:9 and ultrawide; visual verification remains pending.

The same movie reuses image 76 for the difficulty selector. Replaying the root timeline to `fl_level_in` leaves shape 75 (image 76) present and introduces shape 80 (image 82, the difficulty heading). A saga-only gate on image 76 makes the difficulty backing disappear. Gate that shared frame on `scenario | difficulty` (phase mask 6), image 37 on scenario (mask 2), and image 82 on difficulty (mask 4). Keep each bit set through its exit animation. Check both direct entry into difficulty and entry after cancelling saga selection.

### Episode selection after opening a saga

`ModeMenu_Draw` in `src/menu/menu_a_d.c` draws the episode list (progress mode 7). Its foreground movie is resource 3 of section 2 in `gamedata/pzs3us1/00460.bin`; it receives movie scope 6. `ModeBg_Draw` uses a separate picture and movie and keeps its wide layout.

The archive's root shapes 5, 6 and 7 use images 5, 6 and 7 for the saga-title backing, thumbnail strip and preview frame. The saga header `mc_scenario_text` and its standalone image 5 backing use the centre marker, keeping the title centred on the screen. Images 6 and 7 receive the same left marker as the list, preview, preview mask, battle title in its preview position, and CLEAR badge. Each thumbnail's NEW badge is nested in its `mc_menu_plate_*` parent and inherits that transform. Both navigation arrows belong to this left-side list, so both use the left pivot. The guide enters from the right and uses the right pivot for the complete portrait tree. Inside a saga on widescreen (`Port_AspectMilli() > 1333`), the guide is centred in the gap between the visible scenario preview and the screen's right edge. Compute the preview's right edge from `mc_battle_mask` x plus its visible 256-unit width, apply its left-anchor scale, and take the midpoint with page x=512. Convert that midpoint back through the guide's right-anchor transform and subtract half its 256-unit width to get the parent position. This replaces the fixed 20% shift and adapts to the widescreen aspect ratio. Blend the adjustment along the original guide entry/exit movement (x=520 to 314; guide 06 uses 518 to 312), keeping parked poses offscreen. Apply it once to the complete guide parent so eyes and mouth follow the body. At 4:3 or narrower the original position remains. This adjustment follows the artwork toggle and does not affect saga-selection guides. Edge pivots preserve horizontal offscreen staging without adding a screen-wide scissor or borrowing saga-selection phase flags.

After an episode is confirmed, `fl_battle_ok` opens the scrolling story panel. Its `mc_scenario` artwork, mirrored root shapes using image 38, `mc_episode_text_*` callback clips and `mc_episode_next` indicator use the centre pivot. `ModeMenu_Draw` wraps its own `Font_FlushAll` in that same marker so the deferred story text follows the panel. It resets the marker before drawing the guide's separate `MsgWin` dialogue, which uses the left-anchored `Port_MsgWinDraw43` wrapper. The battle title `mc_battle_text_1` also changes anchors with its movie placement: x=-65 uses the left pivot beside the preview; the story-panel placement at x=-5 uses the centre pivot. The movie fades the title out before repositioning it and back in afterwards, including on cancel. Distinguish those placements at their midpoint, x=-35, rather than comparing against exactly -65: bit-packed translations can decode slightly above -65, which would incorrectly centre the browsing title on the page instead of on its scenario preview. Select the pivot from that placement rather than leaving it left-anchored throughout; otherwise titles such as “Common Enemy” appear to the left of the centred panel. `fl_battle_cansel` returns to the list using the same scoped rules.

Verify opening each saga, scrolling the list in both directions (including temporary plates 4 and 5), changing preview images, NEW/CLEAR badges, guide eyes and mouth, entering and cancelling the story panel (including battle-title alignment and its fade), scrolling narration, and returning from battle. Check mask edges against the preview frame, and story glyphs against both halves of the panel at 4:3, 16:9 and ultrawide. Battle outcomes use the separate rules below; saga-outro screens use their separate centered movie rule (scope 43) and still require visual verification.

Giru in the Dragon Ball GT saga is the complete `mc_gil` tree in `ModeBg`'s background movie, not an episode-list foreground clip. `ModeBg_Draw` opts in only saga index 5 with movie scope 8; only `mc_gil` uses the left marker. The archive is `gamedata/pzs3us1/00469.bin`, resource 9 (the corresponding second archive is file 500). Clouds (`mc_kumo`), standalone background shapes and the picture remain wide. This shared background treatment also applies wherever the GT `ModeBg` is reused. Verify Giru's full animation loop and both artwork-toggle states.

Both saga-selection chevrons use the carousel's left pivot. A right pivot on `mc_yajirusi_right` separates it from the narrowed carousel on widescreen; it is a carousel control, not an element tied to the screen's right edge.

### Dragon History battle outcomes

`HistResult_Draw` in `src/menu/menu_c_c.c` uses movie scope 7. Its foreground movie is resource 4 of section 3 in `gamedata/pzs3us1/00460.bin`. All four `mc_chara_*` outcome illustration trees use the right pivot. On widescreen, their entry blends from the left pivot into that right pivot along the original horizontal movement, instead of applying the right anchor immediately to a parked pose. The archive starts characters 1–4 at x=-327, -240, -257 and -295; each travels 595 movie units to its resting position. Clamp `(x - parked) / 595` to 0..1 and compensate the right marker by subtracting `512 * (1 / scale - 1) * (1 - progress)` from the parent translation. At entry start this reproduces the left-edge placement; at rest the compensation is zero and the character is right-anchored. The complete illustration tree, including both halves of character 1, receives the adjustment once. At 4:3 no entrance compensation is applied. Earned/total-point labels and digits and their standalone backings (images 5 and 7) use the left pivot. Root shape 9 combines images 10 and 9 into one illustration; selecting the complete character tree keeps both halves together. The win/lose captions and episode title use the left pivot; `mc_result_text_*` trees extend positive horizontal timeline translations by the page-to-4:3 aspect ratio, preserving the resting pose and easing while carrying the exit animation beyond the widescreen right edge; root shape 3 (image 3) is the title backing. Images 0 and 1 belong to the scrolling `mc_line_*` scenery and retain the wide layout. The separate `Sprite_DrawPicture` background remains wide too.

The reward popup is its own movie, created by `GetWin_Init` from resource 5's nested pack (English movie resource 5). `HistResult_Draw` calls `Port_GetWinDrawCentered43` in `src/battle/view_a.c`, wrapping the complete `GetWin_Draw` call in the center marker when the artwork option is enabled and whole-page 4:3 is disabled. This keeps the animated window, title, reward icon, amount and deferred text together through opening, next-reward and closing animations. Other callers of `GetWin_Draw` retain their existing layout.

Verify a loss and all three victory illustration variants, outcome entry and exit, episode titles, earned and total-point counters with different digit counts, and reward popups for money, characters, episodes and other unlocks. Check multiple rewards in succession, both text lines and long names. Repeat at 4:3, 16:9 and ultrawide with the artwork option enabled/disabled and whole-page 4:3 enabled; check that scenery stays wide while foreground groups and reward glyphs remain aligned. The separate saga-completion outro movie uses scope 43 for its complete foreground and preserves guide dialogue proportions through `Port_MsgWinDraw43`.

### Versus Duel menu

`DuelMenu_Draw` in `src/menu/menu_g_c.c` uses movie scope 9. The archive is `gamedata/pzs3us1/00448.bin`, section 1, resource 19. Select whole named plates so their on/off text, icons, arrows and highlights inherit one transform. Main options and battle types enter from the left. DP choices and their panel use the left pivot with the main battle-type menu; right-anchoring that group pushes it over the guides. Expanded value-choice panels and Vegeta/Nappa use the right pivot. Primary settings values (`mc_set_1`, etc., including `mc_set_4as` / `mc_set_4bs`) use the centre pivot with the Battle Settings labels and backing. The expanded choices (`mc_set_1_1`, etc., `mc_set_4a_*`, `mc_set_4b_*`, and `set_4_1p2p`) use the right pivot.

Root shape 41/image 41 backs the left menu. Shapes 42 and 43/images 42 and 43 back the left-anchored DP controls. Shape 44 combines images 46, 45 and 44 for the settings backing and uses the centre pivot. Shapes 45 and 46/images 47 and 48 back the right value panels. These image numbers apply only to the Duel menu movie. The standalone background shapes, `mc_kumo`, `mc_kemuri_*` and `Sprite_DrawPicture` retain their wide presentation.

Duel DP, Battle Settings and expanded values also need phase gates: their parked positions can become visible after narrowing. `sPortDuelPanels` in `menu_g_c.c` supplies bits 1, 2 and 4 to movie scope 9. Each bit is set when its entry is requested and cleared only after the movie accepts input again on another menu level, preserving the exit animation. Gate complete root clips and their standalone backings (images 42–43, 44–46 and 47–48 respectively). Reset the bits when the menu initializes. This prevents values from waiting visibly on screen before their panel opens.

The shared header retains its existing `IconWin` rule. Help text uses `Port_MsgWinDraw43`, which marks its deferred glyph flush with the left pivot. `DuelMenu_UpdateReset` uses `Port_DialogDraw43` for the reset window, choices and deferred text. All of these rules follow the artwork toggle and avoid double narrowing with whole-menu 4:3 enabled.

Verify 1P vs COM, 1P vs 2P (including unavailable-controller styling), COM vs COM, all battle types and DP limits, every settings row and expanded value panel, both player columns, reset confirmation/cancel, guide eye/mouth animation and entry/exit. Check the help text and backing/label alignment at 4:3, 16:9 and ultrawide with both toggle states. Single, team and DP character/stage selection use the rules below.

### Versus character selection

Character selection uses individual artwork rules over its wide background, following the artwork toggle. It does not force a 4:3 viewport or add solid side bars. On widescreen, the original thin divider sprite closes each portrait’s inner edge. Player 1’s portrait and face mask use the left pivot; player 2’s use the right pivot; name/form plates and text, DP readouts, grids and customization use their player's left/right pivot. Shared stage/music controls, timer and VS emblem use the centre pivot.

`CharSel_Draw` uses scope 10 for the shared movie (resource 15, section 2 of `gamedata/pzs3us1/00448.bin`), and scopes 11/12 for the side-only grid movies (24/25) and customization movies (32/33). The side-only movies are transformed as complete groups. Shared movie image 38 is the centred stage-name backing. Name/form callback glyphs are flushed before each clip's marker resets. Custom-character item/stats inspection panels and item details use the full menu page, including their deferred glyphs, without artwork narrowing. The independent whole-menu 4:3 option still applies to the page. Whole-menu 4:3 bypasses the extra artwork transforms.

`TeamSel_Draw` uses the same scope 10 for resource 15, shared with single selection.
Its team-slot movies (37/38), grids (24/25) and customization movies (32/33)
receive complete left/right transforms through scopes 11/12. This keeps the
team portraits, member slots and DP costs together on each player's side while
stage/music controls remain centred. Deferred callback glyphs flush before the
side transform resets. Team and DP item/stats inspection and item help use the
same scoped wrappers as single selection. Mode 40 already participates in the
artwork policy; these changes complete its draw rules. Whole-menu 4:3 and the
artwork proportion toggle retain the same behavior as single selection.
Check team and DP selection, both player columns, member reordering, total/max
DP, customization and stage confirmation at 4:3, 16:9 and ultrawide; in-game
visual verification is pending.

Verify portrait edges, masks, both player grids, name/form text, customization, stage/music selection and item help across entry/exit at 4:3, 16:9 and ultrawide. Check each portrait’s inner cutoff against its divider throughout entry, idle and exit. The shared movie already draws two copies of shape 21/image 21 at root depths 13/14, normally overlapping at the centre. On widescreen, scope 10 attaches depth 14's copy to the right edge of the 256-unit-wide player 1 sprite, and depth 13's copy to the left edge of player 2's sprite. Each divider follows its portrait's animated x position and receives the same player-edge marker. Their original vertical geometry, alpha and draw order are preserved. No extra divider remains in the centre; the background, grids, labels and other controls retain their previous rules.

The stage-confirmation `mc_vs` emblem uses the centre pivot. Its standalone VS background (image 48) and emblem artwork (image 47) use the same centre pivot when drawn outside a clip group, preserving both layers' proportions throughout the pre-fight animation on widescreen. As `mc_map_name` slides left, its anchor blends from the centre to the left edge over the original 512-unit exit, keeping its parked position outside the screen. Its separate root backing (image 38) follows the same correction from the name clip’s timeline position, so the text and backing leave together. Check stage confirmation and the complete exit at widescreen ratios.

Team and DP selection use the shared rules described above.

Versus item/stats inspection uses scope 13 through `Port_ItemPanelDrawMenuArt`. Its background retains the full-width layout. Complete item rows use the centre pivot, including their backings, icons and text. Name/form backings and text use the left pivot, with both the character name and transformation right-aligned, each with an eight-unit inset. The complete Custom selector (backing, label and arrow) uses the right pivot. The original Ability Limit artwork uses image 15 for the caption, image 16 for the slash, image 17 for large digits and image 18 for small digits. All four images and the digit clips share the right pivot, preserving the original spacing as one readout. Callback glyphs flush before their marker resets. These rules follow the artwork toggle and are bypassed by whole-menu 4:3; other item-panel callers retain their existing layout. Verify both players, occupied/empty/disabled rows, selection movement and opening/closing at 4:3, 16:9 and ultrawide. The item-details page uses scope 14 through `Port_ItemHelpDrawMenuArt`: the complete outer base, item illustration, type/cost icons, name and description share one centre transform. Callback glyphs flush before that marker resets, keeping the entire detail panel at its original proportions and centred without narrowing its contents twice. Image 0 is the black screen dimmer: it temporarily bypasses the centre transform to cover the full screen, then restores the panel transform. Check opening an equipped item, its description and closing back to the list.

### Dialogue and deferred text

Dragon Story's guide dialogue uses `MsgWin`, not `Dialog_Draw`. `HistSel_Draw` sets the current guide's text and calls `Port_MsgWinDraw43(voiceLine)`. This wrapper in `src/battle/view_a_b.c` opts in only that draw, checks **Text and artwork proportion** and **Menus in 4:3**, and restores its previous state afterwards. It uses the left marker (`0x1A`), so dialogue starts at the left edge like the main-menu description. `gMsgWin->side` selects the movie's slide animation; it does not determine the text's layout anchor.

`TextBox_AttachLine` attaches text to `mc_message_text`, but the movie callback only queues glyphs. `MsgWin_Draw` must set the marker immediately around `Font_FlushAll`, after drawing the window movie, then reset it to `0x13`. Marking just the movie or the text clip leaves the actual glyphs stretched. This correction preserves the window's slide animation and the text's line layout while narrowing the emitted glyph positions and widths about the left edge. The message-window backing still spans the wide page.

For another screen, identify whether its dialogue comes from `MsgWin`, `Dialog_Draw`, or another text renderer before choosing a rule. Use the scoped `Port_MsgWinDraw43` call for a `MsgWin` screen that needs this text treatment; confirmation dialogs use `Port_DialogDraw43` for their window and glyphs together. A shared text component does not by itself opt all its callers into the artwork setting.

The initial Dragon Story screen's monkeys are a foreground exception to the wide scenery. In the saga-selection movie, `mc_saru` contains the animated `mc_saru_oyako` monkeys and `mc_eda` branch. Mark the complete parent with the left pivot so the monkeys, their rotation and the branch retain matching proportions throughout the loop. The movie introduces this parent during `fl_menu_in`; its existing timeline controls visibility. Do not mark its children independently or narrow the rest of the background animation. Check the loop and screen transitions with the artwork toggle both enabled and disabled.

## Extending a screen

1. Find its `Flash_Create` and `Flash_Draw` calls. Work out whether the art belongs to its movie, the shared `IconWin`, a `MsgWin`, a dialog, or a `Sprite_DrawPicture` background. The last cannot be split into foreground and background by a Flash marker.
2. Inspect the movie's named clips and shape/image records in the player's own archive. Prefer a complete parent clip (including eyes, mouth, mask and other layers), so its pieces receive one transform. Record any unparented image index **for that movie only**; an image number means something different in another movie.
3. Choose the pivot from the original layout and motion: centre for freestanding art, left/right for art tied to or entering through that page edge. Check the boundary after scaling: `x' = pivot + (x - pivot) * scale`. Culling at the original boundary with a centre pivot can cause a visible jump; hiding more frames does not restore a continuous entry. Keep the chosen pivot through entry, idle and exit. A clip already parented by another selected clip must not reset a different marker before its parent finishes.
4. Scope a special rule to the movie; there is no automatic cross-screen character-art rule. Do not select `mc_bg_*`, all `mc_chara_*` indiscriminately, or an entire movie just because it contains one character.
5. If text is queued by a `TextBox`, mark it when `Font_FlushAll` emits its glyphs; marking only the earlier Flash draw does not affect them. Keep its alignment with the related art or panel.
6. Test 4:3, 16:9 and an ultrawide aspect ratio. Check entry and exit animations, focus changes, masked portraits, screen transitions, and both states of the toggle. Text, masks and backing art should remain aligned throughout. For Dragon Story, include the NEW badge, saga label bar, decorative frame, and achievement values 0, 10 and 100 with the percent sign; check returning from saga selection to the menu as well as entering it. Verify the entire saga group enters through the left screen edge, the achievement panel stays against the right edge, and the difficulty backing, heading and all three choices remain visible. Repeat difficulty entry before and after visiting saga selection, then confirm and cancel it. Check guide dialogue with short and multiline messages, changing guides, and the message-window slide. Also enable **Menus in 4:3** to confirm no second narrowing is applied.

The changes above have passed C syntax and whitespace checks. In-game visual verification of the animations, grouped panels and dialogue at these aspect ratios is still required.

The current rules identify artwork by movie scope, names and a few verified image slots. They are not a universal image/character classifier: art baked into `Sprite_DrawPicture` backgrounds, unnamed shapes outside the shared heading, and 3D character models cannot be separated by these Flash rules. Adding one of those cases requires a separate, carefully scoped rule or asset work.

## Dragon Net Battle dialogs

Connection-lost, connection-error and account-message/confirmation popups draw their dark overlay in
full-screen coordinates, outside the centred menu page. Their native window, text and choices use the
shared proportional dialog path when Text and artwork proportion is enabled. The Giru illustration and
eye use proportional page coordinates as well. This applies on the home page and other Dragon Net pages;
the dark overlay continues to fill the entire wide picture.

### Dragon Net foreground scope

`Ps_Foreground43Begin` / `Ps_Foreground43End` in `src/menu/port_screen.c` scope the Dragon Net
foreground after `Tb_Backdrop` and the shared `IconWin` heading have drawn. The same scope wraps the
post-match result controls and player cards. It is enabled by `title_main_chars_43` and bypassed when
whole-page `menus_43` already provides the 4:3 presentation. Turning the artwork option off retains the
previous Dragon Net layout.

During this scope, page coordinates become identity coordinates, while GS marker `0x19` narrows the
complete foreground about the centre. This handles both raw-screen Wii sprites and page-based text
without scaling either twice. Font batches flush before marker `0x13` restores normal drawing. The scope
also ends temporarily for the bottom MsgWin dialogue: its backing stays wide and its deferred text is
left-anchored, as on Dragon Story and the main menu. It ends before the modal dimmer and screen fades;
neither background nor overlay is narrowed.
The existing shared-header rules run separately so their left and right corners keep their own anchors.

Check home entry/exit motion and focus changes; Pan/Giru layers and blinking; battle-choice panels,
Fighting Points, native versus font labels; Custom/Normal formats; friend host/join and room-code input;
player-name keyboard and cursor; settings; ranking navigation, names and points; opponent-search and VS
cards; and post-match outcomes, player names and rematch/exit controls. Repeat at 4:3, 16:9 and ultrawide,
with the artwork option on/off and whole-menu 4:3 on. Verify connection/account popups dim the complete
screen and the next menu has no leftover transform. Visual checks require a rebuilt executable.

The standalone `port/tests/ui/dragon_net_proportions.c` checks scope activation, identity coordinates,
entrance offsets, scope restoration, and avoiding double scaling under whole-menu 4:3. It does not replace
visual checks of each page's source artwork.

### Port-uploaded artwork and bottom dialogue

Dragon Net's Wii textures, including Pan, Giru, their facial overlays, and generated labels, are uploaded
through the port (`tbo` textures in `gs_draw.c`). The usual exemption from automatic battle-HUD narrowing
must not suppress explicit menu-art markers. The renderer therefore permits these textures through the
proportion transform **only** for marked menu groups (`0x19`/`0x1A`/`0x1B`) with the artwork option enabled
and whole-menu 4:3 disabled. Unmarked port artwork and full-screen backgrounds retain their previous rules.
This is essential: setting a marker in the menu code alone does not guarantee its uploaded textures are scaled.

Dragon Net's bottom text uses `MsgWin_PortDrawString`, which draws a native movie backing and defers glyphs.
`Tb_DrawMessage` temporarily ends the centred foreground scope, calls `Port_MsgWinDrawString43`, then
restores the scope for subsequent foreground labels. The wrapper uses the same **left marker `0x1A`** as
`Port_MsgWinDraw43` on Dragon Story, applying it around `Font_FlushAll` only. The window's backing and slide
animation remain full-width. This applies to the home/reward guide dialogue and other pages with the bottom
message box. It does not change the separately centred connection/account confirmation dialogs.

When checking these screens, verify Pan's body, face, hair and blink layers stay aligned, and Giru's body,
eye and facing animation keep the same aspect throughout entry/exit. Check short and multiline dialogue
against Dragon Story's left anchor; its background should reach both screen edges while its lettering keeps
its original proportions. Toggle the artwork option off and repeat with whole-menu 4:3 enabled to detect
missing or double transforms. Coordinate-scope tests alone cannot detect the renderer's `tbo` exemption;
these cases also require a rebuilt executable and visual checks.

### Dragon Net edge alignment and keyboard presentation

Do not use one centre pivot for every foreground item. The shared heading's left artwork is already
left-anchored: native Wii titles and crest, and generated title text, now also use `0x1A`. Pan's body,
pointing pose, eyes and mouth use that same left pivot. Home/settings Giru and its eye use `0x1B` so the
right-side character stays tied to the screen edge. The battle-page Giru sheet is a left-side guide and
uses `0x1A`. These markers are scoped per component, restoring the centre scope for panels/cards afterward.
`Ps_Foreground43Anchor` changes the pivot only inside an active Dragon Net foreground scope.

The bottom dialogue's starting position is the original left-side MsgWin position, rather than a centred
page offset. Its window movie remains wide and only its flushed glyphs receive `0x1A`; see the existing
Dragon Story MsgWin section above. Do not centre the backing or apply the foreground scale twice.

Player-name, recovery-code and friend-room-code keyboard pages use `Ps_ForegroundStretchBegin`.
The native keyboard movie, input field, generated title, cursor, Pan and Giru stretch together with the
configured screen aspect. Page coordinates become identity coordinates without a narrowing marker, avoiding
mixed stretched art and narrowed text. There is no keyboard-specific presentation override or forced black
side bar. The independent whole-menu 4:3 option remains available if explicitly selected.

Verify title/crest alignment against the shared header line; Pan's left entry and Giru's right entry,
including all face layers; bottom dialogue's first glyph and multiline wrapping against Dragon Story;
and keyboard entry, typing, deleting, cursor motion and closing. Check stretched layout alignment during normal name
entry, recovery-code confirmation and friend-code entry, then confirm the next menu restores its foreground rules.

### Bottom dialogue inset and Fighting Points

Dragon Net's backing remains a wide native MsgWin movie. To keep its text inside the backing's shaped
upper-left corner, `MsgWin_PortDrawClip` compensates the clip translation before narrowing glyphs:
`adjusted.x = (original.x + inset) / scale - inset`. After the left marker is applied, the first glyph
retains its original animated inset. Only glyph widths and relative positions narrow; the text origin
no longer drifts left of the backing. This compensation is scoped to `Port_MsgWinDrawString43`; other
screens retain their existing MsgWin policy.

The Fighting Points panel, label and digits on Ranking Battle/View Ranking focus, and ranked-format
selection, use the left anchor together. The same label/digit textures inside VS/result player cards
retain those cards' own layout and are not forced to the screen edge.

### Dragon Net entry margins and offscreen staging

Battle/format/friend rows use a left pivot for their complete plate and label, with the radar
cursor following the same offset. `Tb_RowInset` reserves at least 16 screen units at the right edge
for the complete resting row artwork. It accounts for aspect ratio and the lowest staggered battle row;
no texture/font squashing is used. This inset applies only when proportional widescreen art is active.

`Tb_RowX` compensates each entrance from its resting position to the original offscreen boundary:
when the source animation's leading edge reaches x=512, the transformed leading edge is also x=512.
Rows parked beyond that point stay hidden, as do delayed battle rows before `Tb_In(i * 2)` starts.
They enter continuously from the right and settle with a margin, instead of touching the screen edge
or jumping into view after a centre/left transform. Native/font labels and radar retain plate alignment.
Check all rows on home, battle and format pages at 16:10, 16:9 and ultrawide, including page re-entry.

### Matching home and User Settings rows

The first Dragon Net screen uses User Settings as its alignment reference: both rest at x=162 for the
461-unit plate and x=196 for the radar, with the same centre marker in proportional mode. Labels retain
their original offsets within that plate. This shares their right edge at 4:3 and widescreen instead of
applying the battle-row inset to only the home page. `Tb_HomeRowX` remaps the native home entrance to this
resting point and compensates its offscreen boundary for the centre transform; parked rows remain gated.
Battle/format/friend menus keep their independent inset rule.

The native keyboard contains static backing shapes as well as animated key clips. The complete native
keyboard now receives a 20-frame smooth vertical offset from below the screen during entry, reversed on
exit, through `Port_DcNameDraw`'s existing offset argument. Its backing, keys, highlights and delete mark
travel together while the original key timelines still advance. This prevents a static keyboard backing
from appearing in its resting position before the key entrance. No aspect override or black bars are added.
Check opening and cancelling every name/code keyboard, including repeated visits and confirmation paths.

### Keyboard Giru and heading exceptions

Keyboard keys, backing, cursor and input frame remain stretched. Giru and its eye are scoped separately
with `Ps_Foreground43Begin`/`End`, using the left pivot for their keyboard-page position and entrance.
The top title uses a separate centred proportional scope, for both native and generated heading text.
Each exception restores `Ps_ForegroundStretchBegin` afterward so the keyboard does not inherit its transform.
These exceptions follow Text and artwork proportion; no whole-page aspect override or side bars are added.
Check player-name, recovery-code and friend-code headings, Giru's eye/body alignment throughout entry/exit,
and that keyboard keys and input frame still stretch together.

Battle-mode/format/friend selection rows have an additional 120-unit rightward resting offset
(`TB_WII_MODE_ROW_OFFSET_X`). Plates, labels and radar move together in 4:3 and widescreen. The entrance
compensation keeps their offscreen boundary unchanged. Home and User Settings retain their shared alignment;
Fighting Points and the header retain their independent anchors.

### Giru returning from the keyboard

The name/recovery keyboard path uses User Settings' current motion-track position as its endpoint,
rather than a hardcoded x/y. Because Settings Giru is right-anchored and keyboard Giru is left-anchored,
its keyboard-path x includes `512 * (1 / scale - 1) * (1 - travel)`. At travel=0 the displayed position
matches the right-anchored Settings sprite exactly; at travel=1 no compensation remains at the keyboard.
Closing retraces the same path, with body/eye sharing both coordinates and idle bob. Friend-code entry
keeps its separate left-side entrance. Check confirm/cancel at 4:3, 16:9 and ultrawide with the option on/off.


## Ultimate Battle

Ultimate Battle now opts into **Text and artwork proportion** through movie scope 15;
Sim Dragon's day board and event movies use scope 16. The extra transforms are bypassed
when **Menus in 4:3** is enabled or the artwork option is disabled. PS2 drawing paths
retain their original calls. `include/menu/port_ultimate_art.h` supplies the scoped draw,
message and confirmation wrappers used only by these screens.

Covered callers are the Ultimate Battle landing menu (`menu_p_c.c`), course selection
(`menu_n_b.c`), ranking ladder (`menu_n_c.c`), Mission 100 (`menu_o_d.c`), survival
selection (`menu_s_b.c`), Sim Dragon entry/day (`menu_r_d.c`, `menu_q_b.c`), their result
screens (`menu_n_d.c`, `menu_p_b.c`, `menu_s_c.c`, `menu_r_c.c`), and Disc Fusion
(`menu_o_b.c`). Shared `IconWin` headings retain their existing heading rules.

The local US archive `pzs3us1.afs`, entry 451, was inspected after unpacking its sections:
1 contains the landing menu, 2 mission/survival selection, 3 results, 5 Sim Dragon entry,
and 6 Disc Fusion. Android 17 and 18 use the complete `mc_guide_17go` and
`mc_guide_18go` trees, including eyes and mouths, with the right pivot. Named option,
mission, ranking, history and window plates, portrait/frame groups, counters, arrows,
and result captions use the left pivot. Nested selected clips inherit their parent's
marker rather than applying a second transform. Sim Dragon's named status plates,
timer, cards, Popo, monitor and event controls also use the left pivot in scope 16.
These are explicit foreground-name rules in `FlashClip_MenuArtMarker`, not a rule
for every clip in the movie. `mc_bg_*` scenery and separate `Sprite_DrawPicture`
backgrounds keep their wide layout. Unnamed standalone shapes have no new image-slot
rules; artwork found outside the named foreground trees needs movie-specific inspection.

TextBox callbacks flush their queued glyphs before the selected clip's marker resets,
so names, descriptions and result labels follow their owning group. Guide messages
use `Port_MsgWinDraw43`: the backing stays wide and text uses the shared left pivot.
Disc Fusion and Sim Dragon confirmation dialogs use `Port_DialogDraw43`, preserving
the window/text proportions while the dimmer covers the full screen.

Validation: PORT C syntax checks and whitespace checks. No game rebuild was performed.
In-game verification remains required at 4:3, 16:9 and ultrawide: enter and leave every
submenu, change mission/course and ranking focus, inspect survival and all result pages,
advance a Sim Dragon day through training/cards/events, and confirm/cancel Disc Fusion.
Check parked clips before entry, portrait masks and frames, guide eyes/mouths,
multiline callback text, and message/confirmation transitions. Repeat with artwork
proportion disabled and with whole-menu 4:3 enabled to check for double scaling.


### Ultimate Battle landing screen alignment

The first screen (`UbMenu_Draw`, `menu_p_c.c`) uses its own scope 17, replacing
scope 15 for this caller. Its complete option plates and selection icon share the **centre** pivot.
Android 17 (`mc_guide_17go`) uses the **left** pivot and Android 18
(`mc_guide_18go`) uses the **right** pivot, preserving their placement relative
to their respective screen borders as the aspect widens. Each complete guide
tree, including eyes and mouth, receives one transform throughout its animation. In archive 451, section 1,
resource 1, image slots 2–7 (entry backing/text strips and icon) also receive
that centre pivot when drawn outside a selected tree. This keeps the entry
backgrounds proportional and aligned with the labels, without narrowing a
selected child twice. The separate picture and `mc_bg_*` scenery stay wide;
the shared heading and bottom help retain their existing component rules.
Other Ultimate Battle screens keep their separate rules while they are reviewed
step by step. Check all four entries, including the locked third entry, and
both guides through entry, idle and exit with the artwork option on and off.


Ultimate Battle subtitles use `Port_MsgWinDrawInset43`, preserving the native
animated text inset inside the wide message backing. The callback compensates
its x translation before glyph narrowing, using the same inset calculation as
Dragon Net dialogue. This keeps the first glyph clear of the curved border;
glyph sizes and line spacing retain their proportional treatment. The wrapper
is scoped to Ultimate Battle, leaving other native message callers unchanged.
Check short and multiline subtitles during both window entry and exit.


### Survival selection with Android 17

`SurvSel_Draw` uses scope 18 for archive 451, section 2, resource 2.
Android 17's complete guide tree uses the left pivot. Entry/window plates,
mission portraits/frames, labels, arrows and score/time/rank readouts use the
left pivot. Standalone foreground image slots 4–22 and 26–28 receive the
same left pivot, keeping the large entry backing proportional with the rows.
Scenery slots 0–3 and `mc_bg_*` stay wide; guide slots 23–25 inherit their
parent's left transform. This rule is specific to Survival's movie, leaving
the landing screen and other selection movies separate. Check the course list,
selection details and return animation, including deferred text, with artwork
proportion enabled/disabled and whole-menu 4:3 enabled independently.


### Mission 100 and Disc Fusion selection

These follow Survival's left-anchored layout: Android 17's complete tree,
entry plates, labels and readouts preserve their proportions against the left
screen edge. `MisSel_Draw` uses scope 19 (archive 451, section 2, resource 1);
`DiscFusion_Draw`'s main movie uses scope 20 (section 6, resource 1).
Their standalone backing rules are movie-specific: Mission 100 images 4–32
and 36–38; Disc Fusion images 4–11 and 15–17. Guide images (Mission 100
33–35, Disc Fusion 12–14) inherit the complete guide's left marker instead.
Scenery images 0–3 and `mc_bg_*` retain their wide presentation. Disc Fusion's
second movie retains its previous drawing rules. Callback text flushes with
its owning foreground clip; existing subtitle inset and confirmation rules apply.
Verify scrolling missions, portrait/frame alignment, focused/unfocused entries,
Disc Fusion selection/confirmation and returning to the landing menu, with both
artwork-toggle states and whole-menu 4:3. No game rebuild was performed.


Disc Fusion's option trees (`mc_disk_plate*`, scope 20) are hidden while their
original x translation is parked at or beyond 512. A corresponding original-page
outside-quad check also excludes fully parked foreground backing shapes before
narrowing. Timelines still advance normally, and the rule affects neither scenery
nor other movies. Verify both options entering and leaving: parked options must
not wait visibly on widescreen before their native entry movement begins. There
is no full-page 4:3 scissor; partially visible animated quads remain drawable.


### Sim Dragon entry screen

`SimTop_Draw` uses scope 21 (archive 451, section 5, resource 1). Its complete
Android 18 tree, menu plates, ranking rows and portraits, arrows, stars and
readouts use the **right** pivot. Standalone panel/entry images 4–30 and 35–39
share that pivot, preserving the background's proportions alongside its entries.
Guide pieces 31–34 inherit the guide transform; scenery images 0–3 and
`mc_bg_*` retain their wide layout. Callback text flushes before its group's
marker resets. This changes the Sim Dragon entry screen only; the day board
and result screens retain their separate scopes. Verify the initial menu,
ranking list and explanation panel through entry, scrolling and exit, with
artwork proportion both enabled and disabled and whole-menu 4:3 separately.


### Ultimate Battle character selectors

`SoloSel_Draw` (`menu_l_b.c`, Survival/Sim Dragon) and `UbTeamSel_Draw`
(`menu_m_b.c`, team selection) follow the existing Versus character selector's
component rules. Both use archive 451, section 4. Resource 31 receives scope 22:
the selected character, name/form backings and callback text, and DP readout use
the left pivot; complete Android guide trees use the right pivot. Background
pictures and `mc_bg_*` animation stay wide. This is a separate scope from Versus
10: its movie-specific divider depths and stage/music image slots do not apply.

Side-only movies use scope 23, a complete left-anchored group: resource 11's
grid, mask, chips, cursors and arrows; resource 17's customization/costume panels;
and, for team selection, resource 21's team slots and DP values. Deferred text
flushes before each group's marker resets. Shared name/form callbacks flush
inside their selected clips too. Item inspection and item details reuse
`Port_ItemPanelDrawMenuArt` and `Port_ItemHelpDrawMenuArt`, exactly as the
existing selector does. Guide subtitles use the Ultimate Battle inset wrapper.
All extra transforms follow the artwork toggle and bypass whole-menu 4:3.

Check solo and team selection at 4:3, 16:9 and ultrawide: portrait edges, grid
scrolling/masks, names/forms, team slots/DP, costumes, customization, item
inspection/details and cancelling back, including entrance/exit animations.
Check both Android guides and both artwork-toggle states. Source syntax and
whitespace checks do not replace in-game visual verification; no rebuild was run.


Ultimate Battle's character-selector divider is shape/image 8 in resource 31,
not Versus shape/image 21. Image 17 is the selected character texture and must
retain its native position within `mc_single_chara_l`; it receives no divider
translation. In scope 22 on widescreen, the divider's horizontal centre
follows `mc_single_chara_l`'s animated x plus the portrait's 256-unit width,
with the same left pivot as the portrait. This closes the figure at its inner
edge instead of leaving the divider in the middle of the wide page. Its vertical
geometry, alpha and draw order stay native; it is omitted when the portrait is
hidden. Verify solo/team selectors through portrait entrance and exit as well
as at rest; 4:3 and the artwork-disabled path retain the original placement.


### Sim Dragon day board alignment

The board movie in `SimDay_Draw` now uses scope 24 (US archive 474, resource 1).
`mc_sim_monita` keeps the complete scenario monitor/frame/screen together at
the left pivot. `mc_point_plate` (money) and `mc_icon_timer` (day counter,
including lamps and digits), plus `mc_time_text`, share that left pivot.
`mc_sim_botan*` action buttons use the right pivot. Left-side health/stats retain
their left anchor. These board components keep their original left-anchored placement; the separate menu/event/card movies retain scope 16.
Scenery remains wide and queued text flushes inside its selected component.
Check monitor content/frame alignment, money and all day-counter lamps, all
four action choices, status panels, and board entry/exit at widescreen and
ultrawide with the proportion option on/off and whole-menu 4:3 independently.


The Sim Dragon action column's dark backing is an unnamed root shape, shape/image
6 in archive 474 resource 1 (root depth 29, native x=419), outside the button
trees. Scope 24 gives this image the same right pivot as `mc_sim_botan*`, so its
width and position follow the proportional buttons. Other scenery remains wide.
Check the backing through the column's opening/closing animation as well as at rest.


### Sim Dragon training and minigames

The card, Popo and shot/training movies in `SimDay_Draw` use scope 25, a
complete **centre** transform. This includes the minigames driven through the
shot movie (rock/shell games), keeping their playfield, objects, controls and
local text aligned as a unit. These movies come from archive 474 resources
5–12 (rank-dependent cards), 13 (Popo) and 14 (shot/minigames). They are separate
from the board scenery, so centring them does not narrow the landscape or move
the board's left status panels and right action column. Deferred glyphs flush
before the centre marker resets. The artwork option controls this treatment;
whole-menu 4:3 bypasses the extra transform. Verify each training/minigame,
including moving objects, input feedback and transitions back to the board,
at 4:3, 16:9 and ultrawide. No rebuild was performed.


Minigame backgrounds use movie-specific exceptions to the centred foreground:
card movies use scope 25, Popo scope 26 and shot/rock/shell games scope 27.
All three stretch image 0 (black dimmer) across the page. Card backgrounds are
images 13/15; Popo's palace and shared backing are images 6/13; shot-game
backgrounds are images 6/14. These images temporarily bypass the centre marker,
then restore it for cards, characters, targets and text. The minigame-start backing (image 1) and thin animated effect strips (image 3)
also stretch; the lettering images retain their centred proportions. Verify the palace and green playfield span the output width throughout
their native animation while their gameplay objects remain centred.
Whole-menu 4:3 still controls the overall page independently.


### Sim Dragon battle introduction

The VS / BATTLE-number screen uses movie scope 44 (archive 474, resource 2) and retains the configured widescreen page. Like Versus Duel, complete `mc_single_chara_l` and `mc_single_chara_r` trees use the left/right pivots; `mc_vs`, round-number clips and standalone images 9–12 use the center pivot. The two root shape-13/image-15 divider instances track the inner edge of their respective portrait using the same side pivot. Image 15 is 32 texels wide with an opaque stripe only in texels 0–7; align its visible center at u=4 (u=28 when flipped), rather than the full quad center at u=16. Thunder scenery and the animated battle-number band retain full width. The previous transition into a complete 4:3 page is no longer activated.

Verify portrait entrance, both dividers, the VS logo, BATTLE digits and their backing, confirm/timeout and outgoing fade at 4:3, 16:9 and ultrawide. Check both artwork-toggle states and whole-page 4:3 behavior. Syntax checks pass; in-game visual verification is pending.


### Sim Dragon results alignment

`SimResult_Draw` (`menu_r_c.c`) uses scope 28 for archive 451, section 3,
resource 3. Like the Sim Dragon entry screen, Android 18 and the result
foreground use the **right** pivot: result/bonus rows, captions, rank, time,
score, rewards and callback text. Standalone foreground images 4–48 share
that pivot, including the large backing and time punctuation, so they align
with the proportional rows. Images 0–3 and `mc_bg_*` scenery stay wide.
Queued glyphs flush inside their selected clip. This is specific to Sim Dragon
results; other Ultimate Battle result screens retain their own rules. Verify
win/lose results, bonus scrolling, totals and rewards, plus opening/closing,
with artwork proportion on/off and whole-menu 4:3 enabled independently.


### Sim Dragon pause and item menu

The Return to Game / Confirm Z Items / End Sim Dragon menu uses scope 30
(`SIMDAY_FL_MENU`, archive 474 resource 15). Its complete panel, entry labels,
highlight and item-list callback text share one centre transform. Glyphs flush
before that transform resets. Image 3 is the black dimmer and temporarily
bypasses the marker to cover the wide page; the panel images and labels remain
proportional. This follows the artwork toggle and bypasses extra narrowing when
whole-menu 4:3 is enabled. Check opening/closing, all three entries, disabled
items and the item list at widescreen and ultrawide.


Subtitle inset compensation also converts the font's horizontal clipping bounds
into pre-narrowing coordinates (`clip.x / scale`). Font clipping happens before
the GS proportion transform; retaining the original right bound after shifting
the inset truncates otherwise valid lines. The callback uses a local draw-state
copy, preserves vertical bounds, and leaves the stored TextBox state unchanged.
This applies to Ultimate Battle inset subtitles and Dragon Net's inset dialogue.
Check long and multiline subtitles, including a line near the right boundary,
with 16:9/ultrawide output and during message-window movement.


## Dragon World Tour

Dragon World Tour opts into the artwork proportion setting through
`include/menu/port_tour_art.h`. Whole-menu 4:3 bypasses the extra narrowing;
PS2 paths retain their native draws. The extracted US archives were inspected
for the movie names: archive 450 contains the menu/entrant sections, and
archives 454–458 contain the five tournament bracket layouts. `TourBg_Draw`
and its drifting clouds, plus separate background pictures, remain wide.

| Component | Movie scope and source | Anchor |
| --- | --- | --- |
| Tournament menu/settings | 31; archive 450, section 1, resource 15 | Right controls, clock and helicopter; left `mc_guide_satan` |
| Invitation flyer | 34; archive 450, section 1, resource 18 | Centre |
| Entrant portrait/name/form | 35; archive 450, section 2, resource 3 | Left |
| Entrant slots and decision labels | 35; same movie | Right |
| Entrant grid and customization | 23; section 2, resources 10 and 16 | Complete left group |
| Entrant guides | 37; tournament-specific section 2 movie | Left, original position |
| Bracket tree and moving chips | 33; tournament archive resources 10, 13 and 14 | Centre, with one shared pivot |
| Bracket VS and title/result movies | 33; resources 16 and 4 | Centre |
| Bracket guide | 38; resource 29 | Left, original position |

`TourMenu_Draw` scopes complete option/tournament/title plates, difficulty
Dragon Balls, entrant count, arrows, clock and prize labels. Main-movie standalone
foreground image slots 7–30 receive the right marker too, keeping backings with
controls. The complete Satan guide inherits its left marker, including facial
layers; the complete clock tree uses the right marker; the complete `mc_heri_body` helicopter tree uses the right marker, including its rotor.

`EntrySel_Draw` follows the existing character-selector layout, using a separate
scope rather than importing Versus-specific image/depth IDs. Its standalone
divider image 4 and name/form images 6–9 follow the left marker; entrant
panel/badge images 0, 1 and 18 use the right marker. Grid/mask/cursor
and customization movies are transformed as complete groups. Item inspection
and item details reuse `Port_ItemPanelDrawMenuArt` and
`Port_ItemHelpDrawMenuArt`. These rules also apply to other modes that reuse
this entrant selector.

`Bracket_Draw` preserves the original movie order, including `BRK_VS_FRONT`.
The tree and its two moving-chip movies use the same centre transform so the
chips remain on their bracket paths during scrolling and match progression.
The title, result banners and VS panels are separate foreground movies and
receive that centre transform too. The bracket guide uses the same centre
pivot, keeping the announcer with the bracket rather than the right screen edge.
Callback glyphs flush before each whole-movie or selected-clip marker resets.
Guide dialogue uses the inset-preserving message wrapper, including the wider
font clipping bounds. Reward popups use `Port_GetWinDrawCentered43` with its centre
anchor and deferred text handling.

Validation: PORT C syntax checks for the three changed menu callers and Flash
renderer, plus whitespace checks. No game rebuild or in-game visual verification
was performed. Verify all five tournament types at 4:3, 16:9 and ultrawide:
menu entry/exit, invitation open/close, tournament information, difficulty and
entrant-count selection; each entrant slot, grid scroll, portrait/name/form,
costume/customization and item details; bracket scrolling, moving chips,
round transitions, VS panels, win/lose/championship banners and rewards.
Include guide facial animation, long subtitles, locked choices, cancel/back,
both artwork-toggle states, and whole-menu 4:3 to catch double scaling. Check
parked menu pieces before their entry animations; movie-specific phase gates
may be needed if visual verification exposes early-visible artwork.


### Dragon World Tour menu staging and invitation effects

Satan uses its original left anchor and `mc_timer` uses the right anchor, including all
clock digits. Invitation scope 34 centres the flyer while image slots 0, 1, 2
and 5 bypass the marker for the full-width background effect; flyer images 3/4
retain their proportions. The centre marker is restored after each effect draw.

`TourMenu` tracks five visibility bits for the main options, tournament names,
information, difficulty and entrant count. Each bit is set when its native
entry command starts (`fl_menu_in`, `fl_taikaisentaku_in`,
`fl_taikai_setumei_in`, `fl_nanidoi_in`, `fl_ninzu_in`). Groups remain visible
during exit and bits for later levels clear once the movie accepts input again
at an earlier level. Complete plates, tournament titles/icons, difficulty balls
and labels, arrows and entrant digits respect these gates. Right-anchored controls retain their original right-side offscreen staging;
scope 31 does not cull them at the old 4:3 boundary or add a 4:3 scissor.

Verify the first greeting and invitation before the menu opens, then enter and
cancel every level repeatedly: tournament names and difficulty must not wait
visibly on screen before entry. Check their backing/label alignment throughout
the animation, the right clock, Satan's left position and invitation effects
at 16:9 and ultrawide with both artwork-toggle states. C syntax and whitespace
checks passed; no rebuild or in-game validation was performed.


### World Tour right-side controls and helicopter

Tournament/menu plates, settings values, difficulty balls, names, arrows and
standalone foreground backings share the right pivot. Their native parked
translations (main options x=540, tournament rows x=569, difficulty balls
x=576/646/716) remain beyond the wide screen edge under that pivot, so entry
starts offscreen and moves continuously to rest. The phase gates still hide
inactive groups. The former original-page outside-quad cull is removed from
scope 31 to avoid a jump when a right-anchored group crosses x=512.

Satan retains his original left-anchored position, with no added inset. The
complete `mc_heri_body` helicopter tree uses the right pivot, preserving its
position relative to the screen's right edge and keeping the body and rotor
proportional. Its original vertical entrance remains native. Check these complete animations,
including cancel/re-entry and the helicopter loop, at 4:3, 16:9 and ultrawide.


### World Tour entrant selector alignment

The announcer/guide movie uses a complete left transform at its original position (scope 37). The character grid, mask, cursor, arrows and customization
use a complete left transform (scope 23). Each flushes deferred text before resetting its marker.
The portrait keeps its original left-side placement. Its divider is shape/image
4 in archive 450, section 2, resource 3, not the portrait image 5. On widescreen,
only the divider's horizontal centre follows the animated portrait x plus its
256-unit width, using the same left pivot. The screen's left border and this
inner divider therefore enclose the character image. The entrant panel/badges
remain right-anchored; names/forms stay with the left portrait.
Verify the announcer's entrance and facial layers, grid scrolling and masks,
all entrant slots, customization and the divider through portrait entry/exit.
No rebuild was performed; C syntax and whitespace checks passed.


### World Tour title presentation

The opening tournament-title illustration keeps its centred proportions
(scope 36). Its black background, image 0, stretches across the configured page. Other headings
and result banners retain their proportional foreground treatment (scope 33). There is no tournament-specific 4:3 page override, black
side bars or bar animation. The independent whole-menu 4:3 setting still applies
if enabled. Sim Dragon's battle-introduction bars and animation are unchanged.

The World Tour bracket screen's announcer uses scope 38, using the left pivot at its original position. The bracket, moving entrant
chips and result panels retain their centre pivot over the wide scenery. This does not change the entrant-selection
announcer's separate left anchor or the subtitle window's wide backing/inset.
Check bracket scrolling and announcer entry/exit with the artwork option enabled.


### World Tour matchup presentation

VS and post-battle result panels use the configured page with side-specific
proportional foreground artwork (scope 39), without tournament-specific side bars. The VS
visibility gates still hide inactive layers, and the bracket stays hidden while
the matchup is visible. On closing, the normal bracket returns after the VS
root timeline stops. There is no side-bar timing or guide delay.

### World Tour guide inset and opening timing

Entrant guides (scope 37) and bracket guides (scope 38) use the left pivot
at their original movie positions, with no added horizontal offset. Complete
body and facial layers share the transform throughout their native animations.
Character grid/customization options use the left pivot; entrant slots retain
their separate right-side layout.

The tournament guide follows its native entrance and speech timing, with no
bar-related pause or delay. Matchup visibility remains independent of guide
position. Verify the title-to-announcer handoff, both guide contexts, long
subtitles and character selection at 4:3, 16:9 and ultrawide.


### Removing parked VS layers from the bracket

The VS movie is drawn only during an actual matchup/result phase. Its native
resting or closed timeline can leave name bars and tinted side panels parked
outside the original page; drawing that inactive movie on a wide bracket page
exposes those pieces. `Bracket_DrawVs` now gates the complete movie. Visibility
starts on resumed results or when the moving chips launch the next matchup,
stays through `fl_out`, and clears when the closing root timeline stops. This
removes the empty name bars and vertical side overlays from the normal bracket
without changing the backdrop or adding a scissor. Confirm first entry, dismissing
post-battle results, and a second matchup: the next opening must still play.


The bracket tree and both moving-chip movies are hidden while the VS matchup
movie is visible, including its entrance, result display and closing animation.
Their timelines still advance; only drawing is gated. When the closing VS root
stops, its visibility state clears before the bracket draw, restoring the tree
and chips together. The scenery, matchup, title and dialogue retain their own
draw order. Verify opening the next matchup and returning from a completed
battle: no bracket lines or entrant chips should appear behind the fighters.


### World Tour welcome illustration

The welcome artwork on entering the main tournament menu uses standalone
images 1 and 2 in archive 450, section 1, resource 15. Both halves use the
centre pivot in scope 31, preserving their matching proportions through the
native welcome animation. They are separate from the right-anchored menu
plates and helicopter tree. This follows the artwork option and bypasses
extra narrowing under whole-menu 4:3. Check the initial welcome before the
options appear, including first-visit and returning-visit flows at widescreen.


### Tournament intro and matchup entrances

The opening-title artwork keeps its centred proportions during a new tournament's
initial fade and sequence 0x33–0x34. Scope 36 applies one centre transform to
the intro elements, bypassing it only for black background image 0. After the
intro, scope 40 supplies proportional title art and the right-aligned header. No page override or
black side bars are introduced.

Matchups before and after battle use scope 39, following the Versus Duel
character selector's side anchors: left portrait, name/form backings and text,
player badge and result caption use the left pivot; the corresponding right
components use the right pivot. The VS emblem stays centred. Complete named
trees inherit one transform and callback glyphs flush before resetting it.
Unparented name/form backing images 7–10 select their page side from their
native quad position, using this tournament movie's own IDs rather than Versus
image numbers. Edge pivots preserve offscreen staging: left-entering pieces
stay beyond the left edge and right-entering pieces beyond the right edge
until their native movement reaches the visible page. The existing VS phase
gate and hidden bracket/chip movies remain in effect through closing.
Verify normal entry/exit and resumed post-battle results, both players' long
names/forms, badges and captions at 4:3, 16:9 and ultrawide, with the artwork
option on/off and whole-menu 4:3 independently. No rebuild was performed.


Tournament matchup scope 39 also centres standalone VS decoration images 1
and 4 (resource 16), not only the named `mc_vs` clip. This keeps the complete
emblem/backing proportional together through its animation. Pieces inside a
selected clip inherit its marker and are not narrowed twice. Check the emblem
and decoration during both pre-battle and post-battle matchup transitions.


### Tournament headers and VS portrait dividers

After the opening illustration (initial fade and sequence 0x33–0x34), the
bracket title movie uses scope 40. Its default centre transform preserves the
centred tournament title and result artwork. The complete `mc_menu_title_plate`
(top tournament name and round) overrides that transform with the right pivot.
Returning from a clip or standalone shape restores the centre marker, and
callback text flushes before its group resets. Image 0 remains a wide dimmer.

In matchup scope 39, the movie's two native shape/image-3 black dividers
follow the inner edges of `mc_single_chara_l` and `mc_single_chara_r`, using
the corresponding left/right pivots. As in Versus, the later root divider copy
belongs to the left portrait. The left divider follows portrait x+256 and the
right divider portrait x; both retain their native height, alpha and draw order.
Only the dividers move: portrait positions remain unchanged. Check the top
header, centred post-intro title and both portrait closures during entry/exit,
including resumed results, at 16:9 and ultrawide. Syntax/whitespace checks pass;
no rebuild or in-game visual verification was performed.


Scope 40 also gives header backing image 9 and standalone tournament/round
text images 36/37 the right pivot, matching `mc_menu_title_plate`. These layers
can draw outside the named plate; leaving them under the default centre marker
separates their positions and causes the overlapping header seen on widescreen.
Selected parent trees still inherit one marker, avoiding double narrowing.
Verify the full top strip during its entry/fade and at rest.


Tournament intro scope 36 now centres and preserves the proportions of all
intro artwork, including the animated emblem and its effects. Only image 0,
the full-screen black background, bypasses the centre marker; the marker is
restored immediately afterward. Deferred text flushes before scope reset.
This does not add page bars or alter Sim Dragon. Verify the full intro loop
and transition to the post-intro title at widescreen and ultrawide.


### Tournament intro circle, flash and header visibility

During intro scope 36, the top `mc_menu_title_plate` and standalone header
images 9/36/37 are hidden until the post-intro scope 40 takes over. The white
opening flash (`mc_flash_ef`, images 27–30) and golden victory flash
(`mc_yusyo_flash_ef`, images 19–22) bypass the center marker in both intro
and result scopes, filling the wide page with their native animation.

The opening circle is `mc_taikaimoji_waku` (ring image 23 and fill image 24).
Its native scale runs from 3.65152 to 1 around local center (128,128). Normalize
that zoom to a clamped travel value and multiply the whole clip's scale by
`1 + (aspect / 1333.333 - 1) * travel`, preserving its transformed center.
Both axes scale together, extending the entrance beyond the wide viewport
while retaining the original final size. The separate image-15 victory halo
keeps its authored animation. Check the circle entrance and both flashes at
16:9 and ultrawide; in-game visual verification is pending.


### Tournament backdrop emblem

`TourBg_Draw` opts its scenery movie into scope 41. Only the central decoration
uses the centre pivot: the complete `mc_taikaimoji_red` tree and its standalone
circle/frame layers. The optional decoration occupies image slots 3–5, or 7–9
for the Big Tournament backdrop, as assigned by `TourBg_Init`. All pieces share
one centre transform, keeping the emblem circular and its glyph proportional.
Background pictures, clouds and other scenery remain wide. This also covers
other screens reusing `TourBg`; absent optional textures remain skipped normally.
Verify the complete decoration at rest and during its loop in the matchup and
bracket screens, with the artwork option on/off and whole-menu 4:3 separately.


### Centred tournament rewards

Tournament reward popups use `Port_GetWinDrawCentered43`: the complete native
`GetWin_Draw`, including window artwork, reward icon, caption, amount and
`Font_FlushAll`, shares the centre pivot. Opening, next-reward and closing
animations keep their native timeline. This follows the artwork option and
bypasses extra narrowing under whole-menu 4:3. Story rewards retain their
separate left-anchored wrapper. Check Z Points, items and Dragon Ball rewards,
including consecutive rewards and closing back to the bracket.


## Automatic 4:3 fallback for unfinished screens

With **Menu art and dialogs in 4:3** enabled, `Port_Menus43` now returns the
current screen's effective whole-page policy. Completed screens return false,
so their scoped artwork rules run over the configured wide page. Unfinished
screens return true, giving them a 4:3 page and bypassing extra artwork
narrowing. `Port_PageAspectMilli` applies this only during menu mode; it does
not constrain battles. Intro/credits presentation overrides retain their own
rules.

The completed-mode list lives in `include/menu/port_menu_art_policy.h` and is
queried through `Port_MenuArtScreenCovered` in `menu_a.c`. It currently includes:

- Title and main menu (1, 4).
- Dragon History saga/episode selection and battle outcomes (6–8).
- Ultimate Battle / Mission 100 / Survival / Sim Dragon / Disc Fusion and their
  implemented selectors/results (13–25), plus the course character selector (29).
- Dragon World Tour menu, entrant selection and bracket/results (33–35).
- Versus Duel and the single-character selection paths (38–40).
- Dragon Net Battle (80).

All other modes fall back to 4:3, including future/unknown modes. In particular,
the initial Disc Fusion ranking/course layouts (26–28, 30), separate Versus team
selector (41), training, evolution/item shops, Dragon Library, Game Options and
Shenron remain on the fallback until their complete layouts have been reviewed.
Having a shared proportional heading/dialog is insufficient to opt a screen in.
The broader initial rules documented above may still exist on unfinished screens,
but are bypassed while that screen uses the whole-page fallback.

`Port_Menus43Configured` reads the saved manual `menus_43` preference separately.
The UI checks and disables the second checkbox while the first is enabled and
explains that unfinished screens use 4:3. It does not overwrite the saved manual
preference. If the first option is turned off, the second becomes editable again.
This also handles existing settings files without migration: completed screens
remain wide under automatic mode even if the stored manual setting was true.

Validation: the standalone `port/tests/ui/menu_art_fallback.c` was linked against
the actual menu-aspect block extracted from `plat_stub.c`. It checks completed,
unfinished and unknown modes, battle isolation, and manual preference restoration.
C and UI C++ syntax checks and whitespace checks passed. No game rebuild was
performed. In-game checks remain: move between a completed menu and an unfinished
one, open Esc on both, toggle the first option, verify the second checkbox's state,
and enter/leave a fight. Promote a mode into the completed list only after its
full layout has proportion rules.


### Work-in-progress label and animated menu aspect changes

The toggle is labelled **Menu art and dialogs in 4:3 (Work in Progress)**.
Switching between a completed wide menu and an unfinished 4:3 menu now eases
the presentation aspect over 20 frames (about one third of a second at 60 Hz).
The black side bars grow inward from both output borders for 4:3 and retreat
back to the borders for wide pages. Manual menu-aspect changes use the same
transition. Changing target mid-transition starts from the current aspect,
without snapping back to an endpoint.

`Gfx_BeginFrame` calls `Port_MenuAspectFrame` once; presentation getters never
advance the animation. Both render backends read the same interpolated
`Port_PageAspectMilli`. The screen's effective fallback/artwork policy still
comes from `Port_Menus43`. Battle presentation bypasses this menu animation;
intro, credits and Sim Dragon's battle-introduction override keep their own
rules. Returning to menu mode starts a fresh menu transition.

The fallback test now checks intermediate entrance/exit aspects, final values,
and repeated-getter stability as well as saved preferences and battle isolation.
Behavior checks, C/C++ syntax checks and whitespace checks pass. No rebuild was
performed. Verify completed↔unfinished navigation, quick reversal, manual
checkbox changes and fullscreen/aspect changes in-game.


### Proportions during bar transitions and title entrances

The GS menu-art transform uses the currently displayed page aspect, including
intermediate side-bar animation frames, rather than the final configured aspect.
This keeps marked artwork's horizontal/vertical proportions stable while the
presentation viewport widens or narrows. Whole-page fallback menus leave their
foreground and backgrounds in the same page coordinates so they resize together
with the black bars. Applying an additional foreground correction cancels that
movement and separates menu elements during the transition. The message-window
text inset and clipping use the displayed page aspect too, matching the GS
transform. Battles retain their existing HUD aspect calculation.

The title screen's two option trees also compensate their centre-pivot entrance
inset. Their native parked x positions are -453 and 450, with resting x=0.
The extra translation blends from the corresponding screen edge at the parked
pose to zero at rest, using the displayed page aspect. Whole plates—including
backings and labels—move together from beyond the visible screen instead of
appearing inside the wide page before their entrance finishes. Existing phase
gates still hide the options before `fl_menu_in`.

Check both directions of the bar animation, interrupted/reversed transitions,
long text and uploaded artwork, plus title option entry/exit at 16:9 and
ultrawide. Renderer and game C syntax checks and whitespace checks passed;
no rebuild or in-game visual verification was performed.


### Shared aspect for menu positioning

Flash entrance/exit offsets, Dragon History guide placement, tournament circle
positioning, and Dragon Net row/Giru pivot corrections use `Port_PageAspectMilli`,
matching the renderer throughout side-bar transitions. The configured output aspect
is not the visible page aspect during these frames.

`port/tests/ui/dragon_net_row_aspect.c` exercises the actual Dragon Net row helpers
with a fixed ultrawide output and changing page aspects in both directions. It
checks that parked left- and centre-anchored rows remain at the visible edge and
that the home row retains its resting position. It does not replace in-game
verification of Flash art, masks, or Giru animations.

### Episode clear difficulty badges

Dragon History save mode 10 is covered by the proportion policy: its page has no foreground artwork, and McFlow already draws its dialogs through `Port_DialogDraw43`. It retains the selected page aspect through the save transition instead of briefly selecting the whole-page 4:3 fallback.

The PC episode preview badge (`mc_clear`, image 34) keeps its original ring and palette. On entering the episode menu, `Port_StoryBadge` derives small and medium stars from the loaded indexed texture in memory, using nearest-neighbor sampling; Hard uses the untouched original. No PNG assets are loaded. The selected episode's highest recorded clear difficulty selects Easy/small, Normal/medium or Hard/original. Beating a lower difficulty does not downgrade a record.

`HistResult_Init` records difficulty only on a win. A port extension at save offset 0x3E00 stores magic `ZHD1` and 9 × 16 one-byte records (0 unknown, 1..3 Easy..Hard), within the existing checksum-protected 0x4000-byte block. Existing clears without difficulty records are registered as Easy on entering the episode menu; later wins can upgrade them. The migration is persisted with the next normal save. Runtime texture checks cover unchanged borders, distinct sizes and restoration when switching episodes; save checks cover highest-difficulty retention, independent episodes, invalid indices and full-block copying. In-game visual verification is pending.

### Saga outro overlays and portrait exits

Outro scope 43 draws image 0 (the shared solid-color overlay used by the opening black screen and narration dimmer) at full page width. The saga title, illustration and narration retain the centered proportion transform. Each `mc_guide_00`..`mc_guide_08` parent receives the left or right anchor according to its body shape in timeline slot 0, whose translation identifies the selected left/right animation. The complete parent includes eyes and mouth. The native parked poses now stay beyond the physical screen edge instead of appearing inside centered side margins; no visibility cutoff is added. After drawing each portrait tree, restore the centered movie marker. Source checks passed; in-game verification remains pending.

### Ultimate Battle exit save

On PC, a Z-point payout to save offset 0x3028 marks Ultimate Battle as needing an exit save. The shared `UbScore_Transfer` covers Mission 100, Survival, Sim Dragon and both Disc Fusion result modes; score-counting transfers do not mark it. The flag persists across submenu and battle hand-offs. After any landing-menu goodbye script finishes fading out and the landing screen is freed, the Dragon History mode-0 McFlow save runs over a black background before the main menu opens: Autosave handles it silently, otherwise "Save game data?" is shown. Saving or declining completes that visit. Dialog resources are loaded only for an exit with awarded points and freed after the flow. In-game verification is pending.

### Mission 100 result layout

`MisResult_Draw` uses dedicated scope 45 for archive 451, section 3, movie 1. The complete Android 17 tree uses the left anchor, matching Mission 100 selection and Disc Fusion instead of the shared result scope's right-guide rule. All named foreground groups use the same left pivot: mission title, base and bonus rows, rank/record, time and score, payout arrows/amounts, reward text and Exit. Standalone images 2–38 and 42–45 also use that pivot so punctuation and backing strips align with their readouts. Scenery images 0/1 and `mc_bg_*` groups remain wide; guide images 39–41 inherit their parent. Callback glyphs flush before the marker resets. Syntax checks passed; the score, bonus, conversion/reward pages and entry/exit animations still need in-game visual verification.

### Disc Fusion modes: fonts, translation and widescreen

Ranking Battle (475) and Course Battle (476) are compressed whole-file resources, so the font catalog accepts outer index 0 for these files. Their disc sizes/CRCs are checked before extraction. Catalog version v15 regenerates the runtime texture-label index, adding only the Ranking Battle and Course Battle headings. Other bitmap labels retain their original artwork. Wording uses the normal translation catalog and Menu font selection; existing course/character names remain TextBox text. Both whole-file packs use runtime translated fonts rather than the European banked-menu artwork walker.

Course selection uses scope 46, ranking scope 47, the ranking chip transition scope 48, and the full matchup scope 49. List/score panels, standalone labels/backings, and Android 17 use the left pivot; scenery stays wide. The matchup uses edge-anchored portraits/dividers and a centered VS emblem. Both result modes share Mission 100's result scope 45. Modes 26–30 are covered by the aspect policy, avoiding an automatic whole-page 4:3 fallback.

The shared dialog scope extends each Yes/No plate's native motion toward the actual widescreen edges. Open positions remain x=88.5 / 308, while travel toward the native parked positions -130 / 517 receives the center-pivot inset compensation continuously. This applies to the complete button, including its translated text, without hiding it at an arbitrary cutoff.

Validation: the native label generator was checked against the actual USA disc archives under AddressSanitizer/UndefinedBehaviorSanitizer. Syntax checks and 4:3/16:9/21:9/32:9 dialog endpoint checks passed. In-game visual verification of closing dialogs, list scrolling, ranking transitions, matchups and result pages remains pending.

Ranking Battle's challenger announcement uses the center pivot for standalone image 30, including its animated copies. Image 24 is the full-page black dimmer and bypasses foreground narrowing; after it draws, restore the ranking movie's wide base marker. List panels and Android 17 retain their left anchor.

### Tournament finals cheat

The launch-only Cheats tab includes **Tournament: Start in finals**, off by default and inactive online. Only new brackets apply it, after entrant shuffling and match initialization. Earlier matches 0–13 are resolved in order: players beat CPUs, and when two players meet the lower player number advances. Winners are propagated through the native bracket links, with losers marked and chip positions updated. Play begins at round 3 / match 14, the bracket final; the seeded champion match 15 remains part of normal progression. Existing tournaments and entrant health are unchanged. Bracket checks cover 1–16 players, champion preservation and disabled-cheat behavior; launch/online gating checks also pass.

### Evolution Z main screen

`EvoTop_Draw` (progress mode 48) opts archive 452, section 1, movie resource 6
into scope 50. The complete `mc_guide_cririn` tree uses the right pivot, keeping
his body, eyes and mouth aligned. The menu plates and explanation controls use
the left pivot; standalone foreground images 4–6 and 7–12 follow the same right
and left rules. Callback text flushes before each marked clip restores the wide
layout. The separately drawn scenery, cloud clips and lantern decoration remain
full-width, preserving the authored cloud scissor regions. The shared heading
retains its own artwork policy, and guide dialogue uses `Port_MsgWinDraw43`.
Both artwork proportions and whole-menu 4:3 follow the existing options.
Character customization and Item Shop use their own rules below. Check all three menu choices, the full explanation sequence,
Krillin's facial animation, cloud clipping, save dialogs, and entry/exit at 4:3,
16:9 and ultrawide. In-game visual verification is pending.

### Evolution Z character customization

`EvoZ_Draw` (mode 49, archive 452 section 3) uses scope 51 for movie 13.
The portrait, name/form plates and ability diagram use the left pivot; Z-point,
experience, capacity and editing controls use the right pivot. Standalone
foreground quads follow the same rules; black dimmer image 0 covers the full
page. Divider shape/image 41 at root depth 3 follows the left portrait’s inner
edge with the same left pivot, including its entry/exit animation. Named groups keep their
children together, including callback-rendered names, with a font flush before
restoring the background marker. Background scenery remains full-width.

The character reel (movie 23) uses whole-movie left scope 11, keeping its
backing, mask, cells, cursor and arrows aligned to the left. The item/slot list
(movie 31) uses component scope 53, matching the battle selector's view-only
item panel: full-width backing, centred complete item rows, and right-anchored
Ability Limit artwork/digits. The complete equipment-action selector (Change
Equipment, etc.) uses the left pivot. The item-selection frame and header
backings (images 9/10) expand across the page while their options, labels,
icons and scrollbar retain centred proportions. Tabs use the centre
pivot. Callback glyphs flush inside each marked group.

The list backing's native x=520 parked position stays offscreen because it no
longer receives a whole-movie centre transform. The four tabs wait at x=-77,
-72, -67 and -62 and settle at 23, 78, 133 and 188. Their complete trees, including
NEW badges, gain an extra leftward offset proportional to their remaining
native travel and aspect excess; open positions and animation easing remain
unchanged. The vertical list scissor is unchanged. Password windows use whole-
movie centre scope 52 only from this caller. Its solid black dimmer (image 0)
temporarily uses the full-width marker, then restores the centred password
window and text. Item details use the existing
centred item-help wrapper, and equip/conversion confirmations use the standard
proportional dialog wrapper.
All transforms follow the artwork setting and bypass whole-menu 4:3.
Verify character/form changes, reel scrolling, all slot/item tabs, equipping,
Z-point conversion, portrait/divider alignment, passwords, item details and confirmation
entry/exit at 4:3, 16:9 and ultrawide. In-game visual verification is pending.

The `mc_ability_limit_up` movie in customization scope 51 deliberately has no
single group anchor: image 0 is the full-screen dimmer, image 39 is the wide
banner backing, and image 40 is the centred proportional announcement text.
Capacity backing/caption/slash images 34–36 and digit clips/images 37–38 use
the right pivot together, matching the normal right-side Ability Limit readout.
This avoids stretching the announcement or applying one transform to its mixed
full-width, centre and right components.

Ability Limit Up lettering has three image-40 layers at local depths 10–12 in
`mc_ability_limit_up`. All wait at x=550, then settle at 4.4, 17.2 and -21.2.
Scope 51 extends each layer's rightward travel by
`256 * (aspect / 1333.333 - 1) * clamp((x - settled) / (550 - settled), 0, 1)`
before applying the centre marker. This puts the parked texture beyond the
wide page, retains the native easing and each settled pose, and preserves the
proportions of the lettering throughout its animation.

### Evolution Z Item Shop

`Shop_Draw` (mode 50, archive 452 section 2) keeps the native ordering of the
message window, guide and six movies. Scopes 54–59 cover the main movie (6),
buy list (17), collection list (19), guide (9), confirmation (29) and completion
caption (33), respectively. Main menu plates use the left pivot; money and
collection counters use the right pivot. Window/flower image 2, Oolong's body
and hands (images 1/3), and clouds (image 0) use the same centre pivot. The
shop-only background wrapper applies that identical proportional mapping to
native x=144..464, which contains the baked window, while stretching the room
outside that region to the page edges. Picture tiles are split at both region
boundaries, with matching source UVs so the background has no gaps or doubled
frames. The window scissor's x bounds receive the same centre mapping; its y
bounds stay unchanged. Other picture callers retain their original geometry.
The complete Launch guide movie uses the right pivot, including facial layers.

Buy/collection panel backings stay expanded, while complete rows, prices,
item icons, labels, tabs and scroll controls keep centred proportions. Callback
glyphs flush before each component restores the wide marker; list scissor Y
ranges remain unchanged. The buy page's background at root depth 2 travels
from x=-520 to zero. Its centred list foreground receives matching extra
wide-page travel. The acquired-item row (`mc_list_plate_6`) and Unlocked
captions (`mc_font_get*`) are excluded from this list correction: they animate
independently after the list has parked. They retain their centred open poses,
with the acquired-item row extending its own x=566 waiting position. Tabs use
the same parked-position correction as customization. The confirmation window
uses a whole centre transform and extra leftward motion based on its native
backing travel (-256.55 to 123.45); its offset is restored after drawing.
The completion caption's displacement from its native x=-1 resting pose
scales with the page aspect so copies enter/leave beyond the visible page.
Guide text uses the shared proportional MsgWin wrapper, and item descriptions
use the centred item-help wrapper with a full-width dimmer. All rules follow
the artwork setting and bypass whole-menu 4:3.

Verify both shop pages, all tabs, owned/unknown/unavailable rows, price and money
readouts, guide changes and facial animation, purchase confirmation/cancel,
completion messages, item descriptions, and full entry/exit at 4:3, 16:9 and
ultrawide. In-game visual verification is pending.

### Ultimate Training

`Train_Draw` (`src/menu/menu_h_d.c`, progress mode 44) uses movie scope 60.
The source movie is archive 462, section 1, resource 1
(`UltimateTraining_top_PS2_.fod`). Its cloud/scenery layer fills the wide page;
foreground illustrations, plates, arrows, screenshot controls and class labels
keep their proportions. The complete Saiyaman and Videl guide trees and the
class title use the left pivot. The menu/lesson rows, their arrows and clear
badge and the circular emblem use their original right pivot. The rectangular
gold backing behind Videl remains through 16:9 and is hidden on wider pages.
Through 16:9 the option group and emblem move left to connect to it. The mirrored
right backing receives the same shift at rest, tapered to zero over its
898-to-698 entrance so its parked pose stays outside the right edge.

`Train_DrawBg` explicitly resets the GS marker to full width before drawing
its eight landscape tiles. The `mc_ch_goten` movie contains a second tree/cliff
cutout (shape/image 2, character texture resource entry 1), drawn over the tree
already baked into the landscape. This scenery image must also use full width,
then restore the enclosing right marker before the next flying Goten copy.
Narrowing the cutout with the character group creates a second tree beside the
original. The scenery cutout receives no flight offset or interpolation.

The cloud tree explicitly uses the full-width marker. Its scroll phase retains
the fractional texel in the GS UV packet (1/16 texel precision), avoiding the
whole-texel jumps exposed by stretching. Divide its scroll increment by the
page's width ratio to retain the original movement speed. Both changes are
scoped to proportional widescreen Training; native 4:3 keeps its UV timing.
Goten and the other Videl illustration use the right pivot. Guide eyes and mouths inherit their parent
transform. Lesson-list callback text flushes before the foreground marker is
restored, and spoken subtitles use the shared inset-preserving message window.

Entry/exit staging follows the actual movie coordinates. Saiyaman travels from
x=-259 to 0; his 256-wide quad remains outside the left edge when parked.
Videl's guide travels from x=-241 to -21. On widescreen only, extend the parked
position by 16 units to x=-257, tapering the correction to zero at rest using
`clamp((-21 - x) / 220, 0, 1)`. Apply it to the complete parent tree on drawing,
without mutating the timeline, so forward entry and reverse exit use the same
path. The right-side Videl illustration parks at x=516.3. Goten's parent contains the tree/cliff scenery cutout (image 2) and
independent flying copies (images 1 and 3). Their child timelines
park at x=320.1 or about -312.4, relative to the parent's x=132.3; anchoring only
the parent is insufficient. Map their complete horizontal flight between the two offscreen endpoints:
the right endpoint receives +61 units, and the left receives
`-(77 + 512 * (aspect / 1333.333 - 1))` units. Interpolate the correction linearly
across the full -312.4 to 320.1 path, without endpoint clamps. This preserves
the movie's original easing and a consistent speed scale throughout the flight,
rather than accelerating only near the edges. This keeps both complete flying quads
outside the physical page while parked and preserves the scenery image (2).
The spatial correction affects drawing only. `Port_FlashAdvanceTraining`
separately paces Goten's child movie by the inverse of the horizontal path
expansion, preserving the original 4:3 flight velocity rather than a fixed percentage
while the root menu, guides, input and other transitions advance normally.
On drawing, interpolate flying-shape matrices between the previous and current
authored poses using the fractional clock remainder. Use the interpolated X
for the widescreen path correction too, so skipped timeline ticks do not hold
the rendered position. This changes drawing only, with one child-tick of
interpolation latency. Do not interpolate pending seeks, changed shape IDs or
loop teleports (more than 128 units on either axis).
Pending child timeline seeks are handled immediately. The slower pacing is
scoped to proportional widescreen Training; native 4:3 and other menus retain
their original animation rate.

The unnamed rectangular gold root backing (image 5, including its mirrored
copy) is omitted above 16:9. Through 16:9 the left backing and circular emblem
use the left pivot; option plates, arrows and the clear badge move left by the
matching aspect expansion. The plate correction tapers to zero at the parked
right-side pose, keeping entry and exit outside the screen. Above 16:9 the
options and emblem retain their original right-side positions. Shift only the
outer `mc_yajirusi_up/down/right` groups; their `mc_yajirusi_icon_*` children
inherit that shift, keeping the up/down chevrons centered on the lesson list.
Native 4:3
retains its original layout.
The two root preview-frame halves (image 7)
move together from the left despite one being mirrored: x=-449 / 63 to 30 / 542.
They share a right pivot at rest and an additional left travel compensation of
`64 + 512 * (aspect / 1333.333 - 1)` at the parked pose, tapered over 479 units.
This prevents the blank plate/frame from waiting visibly on widescreen. The screenshot and its placeholder (`mc_ss_speace` / `mc_ss_dammy`) share
the ornate frame's placement on widescreen. Root depth 14 is the left frame
half (shape/image 7); its inner opening starts at (+128,+32). Place both image
parents at that offset, using exactly the same horizontal travel compensation
as the frame. The screenshot therefore rests at x=158 when the frame rests at
x=30, and its rendered centre stays aligned during entry, explanation-page
changes and exit. Do not give the image an independent travel correction or
leave the placeholder at its separate native x=128.95. Native 4:3 stays unchanged.

Artwork correction respects the existing proportional-art toggle and whole-menu
4:3 setting. Modes 44 and 45 are marked covered. The first Training option opens the
shared `CharSel` character/stage selector (mode 45, archive 462 section 2), which
already draws through scopes 10/11/12. It uses those existing foreground,
portrait entry/exit, stage and item-panel rules instead of forcing a whole-menu
4:3 fallback. Geometry checks for 4:3, 16:9,
21:9 and 32:9:

```sh
cc -std=c99 -Iinclude port/tests/ui/training_art.c -lm -o /tmp/training-art
/tmp/training-art
python3 port/tests/ui/training_flight.py
```

After rebuilding, visually check first entry, both guide poses and facial
animation, top/class/lesson transitions, explanation screenshots, lesson-list
scroll clipping, and every return/exit at widescreen and ultrawide.

### Data Center top menu

`DcMenu_Draw` (`src/menu/menu_z_c.c`, progress mode 53) uses movie scope 61.
The source is archive 460 (AFS index 459), section 1, resource 5,
`data_center_top_PS2_.fod`. `Port_DataCenterArtMarker` anchors the complete
`mc_guide_blma` tree to the left and `mc_menu_plate_1/2/3` trees to the right.
Bulma's blinking eyes and talking mouth inherit the guide transform; option
backings, captions and selection icons inherit their plate's transform. The
separate background picture, standalone shape/image 0 and `mc_compane_*`
animated scenery remain full width. The existing shared heading preserves its
proportions, and guide subtitles use the inset-preserving message window.
Save dialogs continue through the shared dialog renderer.

The native entry and reverse exit need no extra travel: Bulma's parent goes
from x=-129 to 11, and her texture's visible alpha ends at x=105, leaving the
parked portrait completely outside the left edge. The plates go from x=521
to 171; their leftmost backing coordinate is -3, leaving the parked group
outside the right edge. Keep the original timing and apply the anchor once
to each complete group, including its nested animation.

Modes 53–56 are marked covered. Native 4:3, the proportional-art toggle and
the whole-menu 4:3 setting retain their existing behavior.

Check marker inheritance, offscreen staging and continuous entry across 4:3,
16:9, 21:9 and 32:9 without rebuilding the game:

```sh
cc -std=c99 -fsanitize=undefined -Iinclude port/tests/ui/data_center_art.c -o /tmp/data-center-art
/tmp/data-center-art
```

In-game verification: enter and leave Data Center, cycle all three options,
check Bulma's eyes/mouth and dialogue placement, and visit a submenu then return.

### Data Center submenus

The same archive (460 / AFS index 459) supplies these additional movie scopes:

| Screen | Scope | Section / movie resource | Layout |
| --- | --- | --- | --- |
| Password keyboard | 62 | 3 / 17 | Wide keyboard, passcode rows, backings and effects; left-anchored Bulma |
| Password character results | 63 | 3 / 18 | Left status/name panel; right portrait, result rows and scrollbar |
| Saved custom characters | 64 | 2 / 5 | Left actions, stats and names; right portrait, list, arrows and scrollbar |
| Character item panel | 65 | 2 / 9 | Left name/form plates; centered item rows; right capacity; wide backing and dimmer |
| Replay list | 66 | 4 / 1 | Centered controls, chips, slot digits and popups; wide scenery, outer frame and dimmer |

Password entry follows Dragon Net Battle's `Tb_DrawWiiName` / `Port_DcNameDraw`:
the native keyboard stretches across the page as a unit. Do not individually
narrow its keys, captions, delete symbol or backing. Data Center keeps its own
passcode rows and native entry/exit timelines; the online name keyboard retains
its input-only masking and separate vertical offset. Bulma's body, eyes and
mouth share one left-anchored tree. Both password pages and the character list
use inset-preserving subtitles.

`Port_DataCenterSubmenuMarker` and `Port_DataCenterSubmenuImage` distinguish
foreground trees and standalone backings from `mc_compane_*` scenery. Scopes
63/64 use the original side anchors throughout entry and exit: right-entering
portraits start at x=516, list rows at x=544, and scrollbars beyond x=822.
The item panel starts beyond x=520. Replay controls and the keyboard enter
vertically, preserving the original offscreen Y positions and motion. Names
and other callback text flush under the enclosing foreground marker.

The character list's password popup uses the existing scope-52 password-window
wrapper; item help and confirmation/card dialogs retain their existing shared
proportion rules. Test all password pages, custom-character actions, the item
panel, password popup, replay empty/occupied slots and entry/exit at 16:9 and
ultrawide ratios after rebuilding. The focused `data_center_art.c` checks cover
submenu markers, keyboard/backing consistency and page coverage.

On the password-results/Battle Registration page (scope 63), standalone name
bar images 25/26 use the right anchor with `mc_name_text_r` and `mc_form_text_r`.
The left name/form text retains its left anchor. The two-line Battle
Registration heading is cataloged as `battle-registration-heading/0/0` from
archive index 459, section 3, resource 4, texture 2 (256x64). Its translation
uses a newline so the existing runtime font renderer preserves the two-line
layout. Catalog v16 regenerates cached label metadata for this heading.

Data Center password-registration cancel and character-list confirmation dialogs
use `Port_DialogDraw43(0)`, sharing the centered proportions and font flush of
memory-card prompts. Both the message and Yes/No controls stay in that same
scope; native 4:3 and the whole-menu fallback retain their original draw.

Data Center's item panel (scope 65) follows Evolution Z's item-editor
composition (scope 53): the dimmer and main backing remain full width,
complete item rows (labels, potara icons, costs and selection effects) use the
center pivot, and Ability Limit captions/digits use the right pivot. Name/form
plates retain the left pivot. The different texture IDs are mapped explicitly:
0/2 wide, 3–6 right, and item artwork from 7 onward centered. Data Center calls
`Port_ItemHelpDrawMenuArt` for item details, sharing Evolution Z's centered
name, icon, description and backing rules, including its full-width dimmer.

The Fighter Catalog item panel's centered rows need additional offscreen travel.
Scope 65 reads the main backing at root depth 3: x=520 when parked, 0 at
rest. Blend an extra `256 * (aspect / 1333.333 - 1)` over that 520-unit
travel and apply it once to each outer centered clip and standalone centered
shape. Nested captions/icons inherit their parent correction. This keeps the
closed item panel outside the right edge, preserves its open layout, and
retraces the same path on exit. The full-width backing, left name plates and
right-anchored capacity controls retain their existing transforms.

After flushing a complete Data Center list/registration/item foreground group
(scopes 63–65), restore the default drawing environment. List text can leave
its font scissor active; without this reset, following name/form backing shapes
are cropped at the list boundary even in native 4:3. Reset only at the outer
group boundary so nested callbacks keep their list clipping while drawing.

Password results reuse backing shapes 25/26 for two different name positions.
Use the left pivot while the shapes are in the lower reveal area (native y>150),
matching `mc_name_text_l`/`mc_form_text_l`; use the right pivot when they move to
the upper registration list, matching the `_r` text. Applying either fixed pivot
to both phases separates one pair. Announcement image 33 (A New Warrior Appears!)
uses the center pivot for all its animated copies. Native 4:3 keeps its authored
positions; these markers affect proportional widescreen rendering only.

Catalog v17 also handles older prepared game-text packages whose translation
catalog lacks Battle Registration, Ranking Battle or Course Battle. Missing
required heading rows receive in-memory English defaults before the label index
is regenerated; existing translations and files are preserved. This prevents an
old package from making index generation fail and silently retaining an old
index without the Battle Registration font replacement. `menu_font.py` writes
the same v17 header as the runtime generator.

Fighter Catalog's Z Item, Password and Erase Data captions use runtime menu
fonts and translation entries under `fighter-actions-normal/0/0..2` and
`fighter-actions-selected/0/0..2`. Archive index 459, section 2, resource 4
holds the normal/selected caption sheets at texture entries 2/3 (256x128).
Use the standard purple/orange option styles and 32-pixel caption rows.
Catalog v18 adds these six rows, including defaults for older text packages;
missing selected translations inherit the matching normal caption.

Fighter Catalog action captions occupy the right end of their 256-pixel atlas
rows. Catalog v19 keeps each measured inset as a cropped row origin (161, 127,
112 for Z Item, Password, Erase Data) rather than replacing it with a generic
left margin. Both selection states use the same bounds. This keeps font-rendered
captions inside the visible rounded end of bars that extend off the left edge.

### Character Reference

Mode 60 uses scope 67 for the character list, detail menu, introduction,
voice playback, commentary and costume selection. Chi-Chi follows the left edge in the list and the right edge during commentary,
including entrance/exit animation. The portrait and its frame retain their
proportions at the left edge; list rows, options,
name/form plates and scrollbar use the right edge. The library scenery fills
the page. The introduction frame spans the page and its dimmer covers the full
widescreen page (the native 4:3 overlay retains its original right-half span). Introduction text, the blue heading and voice indicator remain
centered and proportional. Introduction scroll arrows follow the centered
panel; list and costume arrows follow their right-side controls. Subtitles use
the shared proportional text inset, and the existing 3D model viewer keeps its
wide camera view. Native edge entrance/exit positions remain outside the page;
4:3 and the menu-proportion setting retain their existing behavior.

Validation: `port/tests/ui/character_reference_art.c` checks anchors and parked
positions at 4:3, 16:9, 21:9 and 32:9. Visual verification after rebuilding should
include opening/closing each detail option, scrolling the list/introduction,
changing characters, and returning from the model viewer.

### Options

Mode 62 uses scope 68 for the main Options menu, screen previews, sound and
volume controls, music list and controller editor. Dende and bottom actions
anchor left; main rows and editing controls anchor right. The music list and
its scroll arrows anchor left, with the BGM Test backing stretched across the page, while screen-type previews remain centered.
Screen Position adjustment previews, their alignment target and help panel
retain the original centered 4:3 proportions. Save/Load and other choice buttons use the centered proportional layout.
Their left and right entrance/exit paths extend on widescreen so parked poses
stay hidden.
Page dimmer image 35 fills the output even when nested in a right-anchored
menu clip. Shared subtitle insets and proportional reset dialogs retain readable text;
manual save/load continues to use the existing memory-card layout.

`port/tests/ui/options_art.c` checks component anchors and entrance positions
at 4:3, 16:9, 21:9 and 32:9. After rebuilding, visually check page transitions,
volume adjustment, music scrolling, controller editing and reset prompts.

Screen Adjust (Options state 9) and its reset prompt (state 11) are marked
unsupported by the coverage policy, activating automatic whole-page 4:3
fallback. Returning to the Screen menu restores its widescreen coverage.

### Shenron

Mode 70 uses the battle scene presentation while Shenron runs: the 3D camera
uses the full configured aspect ratio. Scope 69 centers the wish list and
confirmation menu with original UI proportions, flushing callback text before
restoring the render marker. Item details use the existing proportional item
window; subtitles and system dialogs use their shared menu layouts. Reward
windows retain battle presentation's proportional 2D behavior. Scene mode is
restored before mode 71, whose save screen uses the shared memory-card dialogs.

Visual checks after rebuilding: summon each dragon, scroll wishes, open and
close confirmation and item details, receive a reward, and complete saving.
