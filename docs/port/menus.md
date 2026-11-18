# Menus: implementation guide

Dragon Net Battle is the port's only in-game online menu. It reconstructs the
original Wii screens from extracted assets. See
[Dragon Net Battle](dragon-net-battle.md) for the screen behavior and extraction
details.

## Main menu and dispatch

`src/menu/menu_a.c` lists the original Dragon Net Battle item (4) when the Wii
artwork is available and `Port_NetMenuListed()` allows it. Its original disc plate,
icon, description, and guide voice are used. The main menu has eleven possible
original items.

Choosing Dragon Net Battle sets `gPortScreen = PORT_SCREEN_DRAGON_NET` and enters
`MPROG_MODE_PORT_SCREEN` (80). `Progress_Main` in `src/menu/menu_a_b.c` dispatches
that mode to `PortScreen_Main` in `src/menu/port_screen.c`, which loads the shared
menu archive and runs `PortTbo_Run` in `src/menu/port_tbo.c`. Leaving the screen
returns to the main menu.

The internal `Tbo`, `Tb_`, and `TBO_` identifiers are used by Dragon Net Battle,
its results, and shared text rendering.

## Native PC keyboard input

External settings > UI > **Native PC Keyboard** enables direct typing in the
Data Center password keyboard and Dragon Net Battle's name, friend-code and
account-code keyboards. It is enabled by default and saved as `native_keyboard`.
Enter (including keypad Enter) activates the selected on-screen key, matching
X/confirm; Backspace follows Y/delete, including the original exit behavior
when the field is empty. Arrow keys navigate the on-screen keyboard. Ctrl+V pastes clipboard text through
the same character validation and insertion path; line breaks are ignored and
do not confirm the entry.

SDL text input supplies case and punctuation according to the host keyboard
layout. Both on-screen keyboards follow Shift and Caps Lock: either alone
shows uppercase, both together show lowercase, and neither shows lowercase. Each screen retains its supported alphabet, validation, text limit and
cursor behavior; unsupported characters are ignored. Controller input remains
available. While typing, printable keys do not also trigger controller bindings
or open online chat. Settings, chat, focus loss and leaving a keyboard discard
queued input. Disabling the option restores the normal configured key bindings.

Run the focused mailbox check with `python3 port/tests/ui/native_keyboard.py`.

Esc goes back in menus, matching right-click. During battle and in the battle pause menu, Esc acts as Start to pause or resume.

## Mouse/Keyboard bindings

In ZENKAI mode, **Options > Controller > Mouse/Keyboard** opens a native Options page using the original plates, focus animations, header, Dende/help window and EXIT footer. Four action rows are visible at once; Up/Down, the wheel or the native scroll chevrons scroll the list. Click or confirm a row, then press a key or mouse button. Esc cancels capture without changing the binding. Delete or Backspace clears the selected action's binding. Arrow keys can be assigned to battle actions and continue to navigate menus outside capture. F1 remains reserved for external settings. **Options > Controller > Clash Input**, immediately after Gamepad, uses a native two-choice selector for gamepads: Rotate Stick uses stick rotation, while Tap Strike uses repeated presses of the mapped Strike action. Keyboard and mouse always use Tap Strike in a clash, regardless of this setting. The local prompt changes to a pulsing Square after keyboard or mouse input. Controller Default restores Rotate Stick.

Each action uses exactly one keyboard key or one mouse button. Assigning one replaces the previous input, and assigning an input already used by another action moves it to the selected action. The list covers Ki Charge, Blaster Wave, Ki Blast, Fight, Descend, Ascend, Guard, Dash, movement directions, Camera Left/Right, Change Camera, Switch Character (L3 + R3) and Transform/Progress (R3). Movement drives the game's left analog stick; horizontal camera controls drive the right. Battle action mappings follow the player's current native controller configuration. Esc retains its menu Back and battle pause/resume behavior.

ZENKAI Controller shows four entries: Vibration, Mouse/Keyboard, Gamepad and Clash Input. The original Control Type entry is hidden in ZENKAI mode; Original mode keeps it. Vibration keeps its native picker and Dende voice callback. Capture blocks gameplay/menu input until the captured input is released. Bindings use the regular ZENKAI Options save flow and checksummed `.sav.options` companion. Older companions retain the default PC bindings until saved again. Dragon Net Battle fights use these Keyboard/Mouse and Gamepad bindings too; they travel into an isolated online session without reading or writing its save card. The external F1 Controls settings still choose the connected controller and its deadzones. Original mode and native controller button mappings remain unchanged.

## Mouse input

External settings > UI > **Mouse Support (Work in Progress)** enables or
disables the in-game mouse controls immediately. It is enabled by default and
saved as `mouse_support`. The external settings window keeps its mouse controls.

The PC title screen and main menu support hover to select, left-click to enter
the pointed option, right-click to go back (quit confirmation on the title),
and the wheel to move through options. The main menu's up/down chevrons also
accept clicks, using the same navigation as the keyboard. Left-click also advances the title intro and Press START prompt.
Unavailable Continue entries cannot be selected. Using the mouse shows the
pointer; keyboard or controller input hides it again when the pointer-hiding setting is enabled. A stationary pointer
does not override keyboard/controller selection. Submenus retain their existing
keyboard/controller controls.

The native Exit Game prompt supports hover and left-click on Exit/Cancel, plus
right-click to cancel. It uses its own captured button regions and an atomic
mouse mailbox, so the opening animation cannot retain an accidental click and
later pointer motion cannot move the position of an earlier click. Underlying
menus do not receive the prompt's clicks. The existing fallback prompt retains
its ImGui mouse controls.

`port/src/gs/gs_input.c` receives events from both graphics backends after the
settings/chat overlay has had first refusal. A locked mailbox transfers them
to `MainMenu_Input` and `Title_Input` without polling the window from the game
thread. Clicks retain their event coordinates and are consumed once. Input during opening,
closing, scrolling, focus loss, or an overlay cannot carry into a later choice.
Window coordinates account for pixel density and the presented picture's bars.

`src/sys/gfxm_c.c` takes hit regions from the selectable plate clips while
drawing the movie, including their animation transforms and widescreen
correction: the main menu anchors left, and the title anchors at the centre. Regions use the stable unselected plate, excluding
the enlarged highlight and glow, and test its actual transformed shape. When
animated regions overlap, the nearest row centre wins. Regions are rebuilt each
draw, including when whole-page 4:3 presentation is selected.

Focused checks live in `port/tests/ui/menu_mouse.c`,
`menu_mouse_geometry.c`, `menu_mouse_navigation.c`, and
`title_mouse_navigation.c`, and `quit_mouse.c`; each file documents its
standalone build command. Check hover/click alignment in both renderers at 4:3,
widescreen, and a scaled desktop, including wheel navigation at both ends of
the visible ring and returning from a submenu.

## Wii screens and drawing

`src/menu/port_tbo.c` contains the Wii home, battle, format, friend battle,
settings, keyboard, VS, and results screens. Its frame loop updates controller
input, sound, fades, animation, accessibility announcements, and drawing using
the game's normal frame machinery.

Artwork IDs are listed in `include/menu/tbo_art.h`. The renderer in
`port/src/gs/gs_draw.c` decodes the Wii resources or loads their extracted copies
under `gamedata/dragon-net-battle`.
`include/menu/tbo_sizes.h` records extracted texture sizes and label bounds;
regenerate it with `python3 port/tools/tbo_sizes.py` after changing extraction.

`src/menu/port_screen.c` provides shared sprite, page, shape, and text helpers.
The Wii artwork uses the original screen coordinates. The first-run language
selector also uses shared page and font helpers.

The heading and message windows reuse the game's `IconWin` and `MsgWin` movies
from the Duel archive. Dialog messages reuse the game's native window animation,
with extracted Giru artwork. The keyboard reuses Data Center's complete Password
Input movie, including its opening and closing animations and key navigation.

## Text, translations, and voices

`port/src/gs/tbo_text.c` supplies translated labels, font rendering, result text,
and disconnect text. Keep it when changing the online menu: the Wii screens and
language selector use this shared functionality.

Original texture labels remain available; edited translations and selected font
settings use rendered text where required. Wii dialog text and English/Japanese
voice tracks share the original line indices. Preserve authored line breaks and
voice-language selection independently of the text language.

The main-menu label catalog now includes only the original eleven items. Changing
its layout requires matching `MENU_LABELS_VERSION` in
`port/src/gs/menu_labels.h` and the header in `port/tools/menu_font.py`, so older
cached indexes are regenerated.

`Tb_Speak` announces page titles, selection changes, keyboard input, and dialogs.
The game's message window exposes its text to the existing screen reader.
`port/src/a11y_game.c` mirrors the main-menu structure; keep its item count and
field offsets synchronized with `include/menu/menu_a.h`.

## Online sessions and results

Matchmaking and P2P networking remain in `port/src/gs/net.c` and `net_match.c`.
The Wii menus use these services for LAN searches, public queues, and friend room
codes. These services are shared by the Wii screens.

Launching a session restores the game's startup state and enters its normal
versus character selection and battle. When it ends, the menu resumes its saved
state; `Port_NetSessionsEnded()` and `Port_NetTakeEndReason()` distinguish return
from a completed session and an unexpected connection failure.

`PortTbo_ResultRun` renders the Wii results screen, point changes, mutual rematch
selection, and independent End Battle exits. Statistics reporting is shared with
the matchmaking server and remains part of this flow.

## Validation

Check the main-menu entry and return path, both local-player roles, custom/normal
and friend battle, keyboard entry/exit, independent results exits, rematches,
Giru error dialogs, text languages, English/Japanese voices, and accessibility.
Build the PC target and run the menu-label and UI-translation checks. Keep the
original PS2 behavior outside port-only code unchanged.

Mouse-wheel navigation suppresses pointer hover selection until the pointer
moves at least four game-picture pixels from its latest scroll position. Small
wheel/trackpad motion does not steal selection back; clicks remain available.

Versus Duel mouse navigation covers the mode, battle-type, DP and rule choices.
The shared single/team selectors support hovering and clicking character/form
chips, item-set and costume plates, team slots and the team-ready action. Stage
chips and the music entry support clicking; the wheel changes rows or songs,
and visible up/down chevrons can be clicked. Right-click cancels through the
native menu state machine. Locked/empty entries retain native availability
checks. Hit regions use the last drawn clip transforms, including animation
and widescreen anchors; they reset each draw. Selector mouse input is disabled
online. All of this follows the existing UI tab Mouse Support setting.

Character selection mouse interactions use individual 64-pixel costume buttons. Clicking a custom-set inward chevron opens its equipped items; clicking an item row opens its details. Item panels keep separate hit regions for each player. Stage thumbnails take priority over the music shortcut when their regions overlap. Versus settings accept clicks on both rule labels and their displayed values.

Stage selection only focuses music over the visible music-name line, below the preview; moving within that line does not restart its highlight animation. The equipped-item panel's corner chevron closes it. Battle Settings value hit areas follow the original 256-pixel buttons (128 for the two-player transformation choices) and their right anchor, and the reset confirmation supports mouse hover, click, and right-click cancellation.

The native mouse cursor hides after 10 seconds without mouse motion. Moving restores it on menus; battle gameplay always hides it, including while moving. The local battle pause menu permits the cursor again, subject to the same inactivity timeout.

Dragon History supports mouse selection of the main menu, difficulty, and unlocked episode thumbnails. Click a side saga card or its arrow to rotate the saga carousel, and click the central card to enter it. The wheel navigates difficulty/menu rows vertically and saga/episode choices horizontally; right-click goes back. Click to advance guide dialogue, episode descriptions, results, and saga closing text. Hit regions follow the current Flash animation and left widescreen anchor, and opening/closing animations and frozen screens discard pending input. Focused checks: `python3 port/tests/ui/history_mouse.py` and `menu_mouse_geometry.c`.

The UI tab's Mouse Support setting also enables left-click mashing in all loading minigames. During battle, left mouse holds Square and right mouse holds Circle (for both taps and long presses), using the normal controller input, recording, and online exchange. A fresh left-click skips battle entrance/winner scenes or advances story battle dialogue; Story Battle cinematic phases use the native scene-skip path even while the fight sequence is active; a running event task by itself does not disable attacks. Ordinary combat speech keeps attack controls. Confirm also advances story dialogue from the keyboard/controller. Attacks are disabled while paused, loading battle assets, or using settings/chat. Buttons held across a screen/context change must be released before acting in the next context. Focused input and context checks: `port/tests/ui/game_mouse.c` and `game_mouse_context.c` (standalone build commands are in each file).

The native cursor uses the purple text palette over empty space and the orange text palette over supported clickable controls, including navigation arrows and the exit confirmation. Pressing preserves the current palette until release and compresses the pointer around its fixed hotspot. A press on a clickable control stays orange even if activation clears hover or changes menus. Hover feedback persists while stationary and clears on movement, input handoff, or menu transitions.

Hover colors are published after a complete game-thread hit check, so motion between window and game frames does not flash purple. Fresh clicks survive battle/loading context changes while older held buttons still require release. Mouse scene skips also work when an intro/winner sequence has no native skip callback. Regression check: `python3 port/tests/ui/battle_mouse_scene.py`; `game_mouse.c` additionally checks the full controller-read path.

During the fight sequence, combat mouse bindings take priority over battle speech/subtitles. Visible combat text does not switch the pointer to dialogue mode or disable punching/ki blasts. Pause and asset-loading restrictions still apply.

Player 1 keyboard defaults: F = Triangle (I also remains available), Space = Cross (X), with K retained as a secondary Cross/confirm key (including the pause menu), Shift = L2, Q = L1, E = L2, Ctrl = L3, Alt = R3. Both left/right modifier keys work; Right Shift no longer defaults to Select. Previous default bindings migrate to these keys while customized bindings remain. E and right-side modifiers provide secondary shortcuts. Mouse buttons send their assigned button immediately and keep it held until release.

Battle/loading mouse buttons are sampled from SDL's pumped physical button state on the window thread, independently of menu event consumption, while button events preserve quick taps. The hidden battle pointer is confined to the game window so clicks cannot drift onto the desktop. Pause, settings, chat, focus loss, and disabling Mouse Support release confinement. `game_mouse.c` covers physical-state-only attack and scene/dialogue clicks, plus held buttons across unrelated consumed UI events.

Memory-card save/load dialogs support mouse hover and left-click on their animated Yes/No buttons, plus left-click to acknowledge informational messages. Card operations and confirmation timing retain their native validation; right-click does not cancel these questions. The previous menu mouse state is restored when the flow ends. Regression check: `python3 port/tests/ui/memory_card_mouse.py`.

Ultimate Battle supports mouse hover and left-click selection throughout Mission 100, Survival, Sim Dragon, and the Disc Fusion ladder/course menus. The wheel navigates lists and character rows; click Mission 100's side arrows to change pages. Shared solo/team selectors support character chips, forms, custom sets, costumes, equipment inspection, and team completion. Right-click follows the native back/remove action. Click to advance guide dialogue, expanded mission/course panels, and results; use the wheel to review result pages. Sim Dragon supports its board actions, training choices, options/quit dialogs, card and shell selections, and timed rock swings. Hit regions follow the animated art and widescreen anchors; frozen screens, entry/exit animations, and selector reel transitions discard input. Regression checks: `python3 port/tests/ui/ultimate_mouse.py` and `menu_mouse_geometry.c`.

Sim Dragon card minigame art remains hidden while staged outside the native page during entrance and exit animations, so widescreen scaling does not expose waiting cards at the screen edge.

Options supports mouse hover/click on main/page entries, Save/Load, screen and
sound pickers, controller/player choices, vibration, and reset Yes/No prompts.
Volume digits can be clicked directly; wheel and chevrons adjust the selected
row. BGM rows select on hover and play on click, with wheel/chevron scrolling.
Key-assignment labels select on hover and cycle their mapping on click through
the native assignment/swap logic. Right-click goes back (including nested
controller settings). Screen adjustment stays 4:3: clicking outer edges moves
the position, the wheel moves vertically, and a center click confirms.
Animation, fade, freeze and memory-card input gates remain in effect; card
flows retain their own mouse handling. Regression: `python3 port/tests/ui/options_mouse.py`.

The BGM Test cyan scrollbar thumb can be dragged to scroll the song list, preserving the selected visible row. Releasing the mouse stops dragging without playing a song.

Title-screen New Game memory-card questions retain mouse selection while the card flow owns input. Dialog choices use rendered button bounds; Options arrows retain their parent hit bounds through nested icon drawing.

In ZENKAI mode, the PC Options menu includes a font-rendered More Options entry containing Video Settings, with Resolution (Auto, 1x–8x), Aspect Ratio, 2D Filtering, Fullscreen, VSync, Frame Skipping, Graphics API and Frame Rate (FPS) controls. Video scrolls through four visible rows. More Options has an EXIT footer; Video retains Default. Its added rows reuse the native Options blue/gold decorations and the Dragon Net Battle caption styles. See [Adding in-game settings options](settings-options.md) for the descriptors, renderer bridge, input, drawing, and verification workflow.

ZENKAI Options displays four of Save/Load, Screen, Sound, Controller and More Options at a time, with native scrolling chevrons and EXIT fixed in its original footer. More Options contains Video Settings; its native EXIT returns to Options, while cancel in Video returns to More Options. Original-mode Options is unchanged.

Video help uses measured glyph widths and the native dialogue margin, wrapping at words rather than a fixed character limit. See [In-game Options and adding settings](settings-options.md) for the complete menu hierarchy, setting defaults and keys, native animations/layering, pointer geometry, help wrapping, and extension/testing instructions.

Video settings with two choices use the native animated two-button selector, like Stereo/Mono. Hovering selects provisionally, click/confirm applies, and cancel preserves the previous value. Fullscreen uses Off/On; Graphics API uses Vulkan/OpenGL. Frame Rate (FPS) shows numeric choices without a repeated FPS suffix.

More Options and Video Settings have orange titles inside the native IconWin header timeline. Choice captions use the native caption shape pivot and transform, preserving centering and motion. Returning from a choice selector restores the Video row focus as the selector exits, matching Stereo/Mono; further input stays blocked until the exit finishes. See [the settings guide](settings-options.md) for implementation details and regression checks.


The FPS/network statistics overlay remains available through Show FPS in the external settings window; it is not part of native Video settings or its Default reset.

More Options also contains **Battle Effects**: Character Outlines, Occlusion Tint, Depth Haze, Bloom, Depth of Field, Motion Blur, Sharp Stage Blur and Weather. Each uses the native Off/On selector. The category has its own header, four-row scrolling list and Default confirmation; resets affect only that category. Weather amount remains controlled by the external Effects tab. See [Battle Effects](settings-options.md#battle-effects) for names, effect bits, defaults and bridge details.

More Options also includes **Visual Settings** (Startup Logos, Intro Movie and Language Selection) and **General Settings** (Autosave, Game Edition and Network Mode: Online/LAN), in that display order. Game Edition requires extracted Japanese assets; Network Mode requires extracted Wii assets; Autosave and the General Settings category remain accessible. Unavailable entries stay visible and dimmed. Their stored row IDs remain stable even though Game Edition moved to General Settings. See [settings details and availability checks](settings-options.md#visual-settings-and-general-settings).

In ZENKAI mode, the full More Options record is stored in `savegame/zenkaipower.sav.options` and committed after the game save succeeds. Confirmed changes are saved only when leaving Options for the main menu: with Autosave off, a save prompt appears; with Autosave on, saving is automatic. Autosave is also read from the companion before the boot memory-card flow decides whether to prompt. The startup language selector can create a language-only companion record before the first game save; it is not treated as full Options data and is upgraded on a later full save. Selector hover does not save. Successful game saves refresh the startup settings cache; cancelled saves do not. See [ZENKAI settings persistence](settings-options.md#saving-more-options-in-zenkai-mode).

Launch with `--settings` to enable the F1 settings window in debug builds only. It contains Video Settings and, when cheats are enabled, Cheats. Video changes use the same runtime settings bridge as the in-game Video Settings page. See [In-game Options and adding settings](settings-options.md) for its choices and persistence behavior.

On Linux, both renderers try native Wayland first and fall back to X11 when it is unavailable. An explicit `SDL_VIDEO_DRIVER` setting (or legacy `SDL_VIDEODRIVER`) takes precedence. Android and other platforms retain their normal backend selection.

Linux x64 initializes SDL video on the process main thread before starting the game/render workers. This lets libdecor initialize its GTK plugin on the required thread and provide Wayland window decorations. Headless/non-GPU runs skip this initialization. Fullscreen still hides decorations; F11 returns to the decorated window.

### Training and World Tournament mouse support

Training supports pointer focus and left-click confirmation on its top menu, classes, and lesson rows. Use the wheel or lesson chevrons to scroll; click to advance lesson introductions, explanation pages and completion dialogue. Right-click follows the native Back action. Character, stage and music selection reuse the existing shared selectors.

World Tournament / Dragon World Tour supports the entry type and tournament lists, difficulty selection, entrant count (wheel or native side chevrons), tournament descriptions and guide dialogue. Entrant selection supports character chips, forms, custom item sets, the custom-set inspection arrow, item details and costumes. Native restrictions still apply to prize tournaments and Yamcha's random entrants. In the bracket view, use the wheel to pan and left-click to advance/start the next match. A dialogue-ending click cannot also activate the next menu.

Title-screen attract battles accept a fresh left-click as Start to skip back out of the preview, including during CPU combat. The opening movie replay reached after title-screen inactivity uses the same mouse-skip context as the startup opening. Clicks remain edges, so holding the mouse cannot skip subsequent screens automatically.

Hit regions use captured native clip transforms and their widescreen anchors. Entry/exit animations, frozen screens, memory-card flows and entrant reel transitions gate menu input. Targeted checks: `training_tournament_mouse.py`, `game_mouse_context.c`, and the existing shared-selector/mouse geometry tests.

Tournament difficulty spheres are directly clickable. Their hit areas follow the rendered selected/unselected artwork, including nested shape offsets and widescreen positioning. Prize tournaments retain their native fixed-difficulty restriction.

Tournament bracket side arrows support click-to-pan and hold-to-pan, using their rendered bounds after bracket scrolling and widescreen centering. Hover changes cursor feedback; arrow clicks never confirm the next match. The wheel remains available for panning.


### Battle pause and remaining menu mouse support

Battle pause menus support pointer focus, left-click confirmation, right-click Back, and wheel navigation on the pad that owns the pause menu. Disabled rows retain their native restrictions. Skill lists support clicking page tabs, pointing at skill rows, and wheel scrolling; right-click closes the list. Opening/closing animations reject clicks. Online input affects only the local player's menu.

The following screens also use the shared Mouse Support setting:

- **Dragon Net Battle:** home, battle formats, friend actions and settings rows support hover and click; the wheel navigates rows and ranking pages. Name and friend-code entry support the native on-screen keyboard and its page switch. Right-click follows native Back/cancel behavior. Account dialogs keep their existing mouse selectors. Result-screen rematch/leave buttons accept local mouse input and retain the native peer/disconnect rules.
- **Character Reference:** select characters and detail actions, click list and character-navigation chevrons, scroll biographies, advance voice/commentary pages, choose costumes, and right-click out of the model viewer.
- **Data Center:** select top-level actions, enter passwords with the on-screen keyboard (wheel switches keyboard pages), choose storage/character rows and custom-item details, and browse replay slots with the wheel or chevrons. Save/overwrite/cancel prompts remain native dialogs.
- **Evolution Z:** select top-level actions, characters, custom sets, editable item slots, list rows/tabs and Z-point digits. The shop supports purchase/collection lists, tabs, item details and confirmation choices. Left-click advances guide explanations; right-click backs out through the native flow.
- **Shenron:** select wishes, scroll with the wheel or chevrons, select Yes/No/Details, dismiss item details and acknowledge rewards. The native ownership checks and memory-card save flow remain in control.

Native Flash controls use their composed clip transforms and the same left/center/right widescreen anchors as drawing. Password/name keys use rendered shape bounds, including the wider bottom-row buttons. Dragon Net Battle records row bounds while drawing, including entrance offsets. Modal dialogs consume their own mouse edges; screen adapters must return before reading mouse input while a shared dialog owns the selection. Enabling mouse input clears pending edges, so enable once on entry rather than on every frame.

Targeted adapter and positioning checks: `python3 port/tests/ui/remaining_mouse.py`. Existing Training/Tournament, Ultimate Battle, Dragon History, battle mouse, Options and geometry checks cover the shared input paths. These isolated checks do not rebuild or launch the full game; visual animation and actual disc-specific layouts still need in-game verification.


Evolution Z chevrons use their rendered arrow bounds for hover/click feedback. In the Item Shop, the native `mc_menu_yajirusi_up/down` arrows navigate both purchase and collection lists. Distribute Z Points uses the selected digit's `mc_yajirusi_num_up/dwn` children to increase/decrease the amount without confirming payment. In the item editor, only the left/right children of `mc_custom_yajirusi` cycle the editing mode; clicking the mode caption does not cycle it. These actions feed native directional input, preserving limits, mode wrapping and navigation sounds. Digit and editor arrows have parent-qualified capture names so repeated child names cannot select another control.


Data Center's “Load replay data?” memory-card question reads the previous frame's dialog button regions before the replay browser clears its capture registry. The “Erase Password Character data?” question dispatches mouse input explicitly because it belongs to the character browser rather than a memory-card flow. Both retain native Yes/No results, disabled/animation gates and confirmation sounds. `memory_card_mouse.py` checks Yes, No and clicks outside the buttons for these paths using mocked records; it does not touch save files.


Character Reference's introduction chevrons scroll the biography in the same increments as the mouse wheel. View Model's costume chevrons send native Up/Down presses without opening the viewer. The character list's cyan thumb supports grab-and-drag scrolling like BGM Test: its rendered bounds follow the live size/position and widescreen anchor, the grab point stays under the pointer, the selected visible row stays fixed, and dragging clamps to the list endpoints without opening a character. Leaving the list or releasing the button ends the drag. Arrow icons also use rendered bounds so their nested shape offsets and visibility match hit testing.

Evolution Z’s item selector also uses the native `mc_menu_yajirusi_up/down` chevrons for mouse navigation. Their visible bounds provide hover feedback, and clicks send Up/Down without confirming/equipping an item.

Evolution Z’s “Use Z Points?” and “Remove all items?” questions dispatch the shared Yes/No mouse adapter before the screen resets its capture registry. Native dialog results still control payment/removal, delays and sounds; the underlying amount/item editor does not consume confirmation clicks.

Dragon Net Battle’s access-code storage confirmation supports mouse hover and Yes/No clicks through its private dialog instance. The account’s native accept/discard handling and error prompts remain responsible for the result.


### Online character selectors and returning from a session

The shared single-character and team selectors support mouse controls in Dragon Net Battle (LAN and online, including Normal, Custom, ranked and friend battle formats). Only the local player's reel, forms, saved characters, custom sets, item details, costumes and team entries accept the pointer. Stage and music controls accept mouse input only for the designated stage picker. Finished selections can use native Back while waiting for the other player; native online restrictions still apply.

Online pointer handlers inspect the drawn controls and generate native controller navigation. Those buttons enter the existing 18-byte pad packet before input-delay scheduling and rollback recording. The pointer never directly changes online selection state, and remote coordinates are not transmitted. A click on another entry navigates to it and confirms only when the native cursor arrives; navigation waits for each acknowledged step to avoid overshooting under input delay. Keyboard/controller input cancels queued mouse navigation. Offline selectors keep their direct pointer focus behavior.

The session's boot-state/return-state swap also saves and restores host-side mouse ownership and its active selector callback. Returning from battle restores the original menu's mouse-enabled state, releases combat mouse context, clears pending clicks/navigation and waits for the next frame's control regions. This prevents mouse support from staying disabled after the online result screen closes.

Targeted checks: `net_selection_targets.py` covers both native selector adapters and local/stage-picker ownership; `net_selection_mouse.c` exercises delayed pad navigation, click confirmation, controller override, context changes and session restoration without starting a network game. Full LAN/online visual testing requires an updated game build.

Returning from a port settings category first slides its rows out, then slides
More Options back in from its entry position. Input stays blocked until those
rows finish entering. Returning from More Options restores the native top-page
state and restarts `fl_menu_change`, including the focused row's
`fl_on_start`, so the parent Options rows and EXIT follow the native return
animation. The bottom help window stays open throughout these transitions.

Pointer selection uses the main menu's visible unselected plate rather than its
transparent texture padding. Changing pointer focus clears the previous plate
immediately, including when a dimmed neighbor scrolls the list. Team member
cards use their centred 64-by-72 bounds, including the next empty member slot.
Pause-menu hit testing reverses the renderer's widescreen scaling around the
page centre so View Skills tabs and rows align with the displayed controls.
Focused checks: `port/tests/ui/menu_mouse_geometry.c`,
`port/tests/ui/menu_mouse_navigation.c`, `python3 port/tests/ui/team_selection_mouse.py`
and `python3 port/tests/ui/pause_mouse.py`.

View Skills supports tapping its scrollbar track and dragging its thumb with a mouse or touchscreen. Scrolling stays within the active tab and keeps the selected skill visible.
