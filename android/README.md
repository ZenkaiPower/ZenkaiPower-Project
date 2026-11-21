# Android ARM64 release

Builds this repository's game engine as native AArch64 code and packages it with
SDL3 and a small Android launcher. The initial target is an offline fight using a
physical controller or touch controls. The launcher installs original disc images
directly, using the same extraction and checksum validation as the desktop setup.

Requires Android 10/API 29 or newer, ARM64, and a working Vulkan driver. Direct
Mobile GPU tuning and online interoperability
still need broader validation. The desktop OpenGL
backend is not an Android fallback.

Device bring-up: Samsung Galaxy M52 (SM-M526B), Android 13, 4 KB pages.
The engine loads at a relocated address, validates the packed USA disc data,
initializes Vulkan and AAudio, and renders the startup and Versus menus in
landscape. Device testing includes battles and Kid Gohan particle effects;
suspend/resume and netplay still need broader validation.
Packed-file temporary streams use the app's private cache, and the loader
synchronizes its paths with SDL's cached environment.

## Startup and home-screen shortcuts

Setup's **Startup and shortcuts** section can add **Budokai Tenkaichi 3** and
**Sparking! METEOR** home-screen shortcuts using title logos prepared from the
installed discs. This section appears only after the base game is installed; the
METEOR shortcut option appears only after the Japanese disc is installed. The corresponding logo must be available; the Japanese logo
requires the Japanese disc. Android's launcher asks before adding a shortcut.
These shortcuts start the game after checking and preparing the selected folder.
They do not replace the app drawer icon.

**Start game automatically when opening the app** is off by default and persists
across launches. It starts a ready installation after folder preparation; missing
files or permissions leave setup available. Long-press the app icon and choose
**Setup** to bypass auto-start and change this setting. This controls app opening,
not device boot.

## Build on Windows

Install Docker Desktop with Linux containers. The unified root `zenkaipower` image
includes Python, JDK 17, Android SDK platform 35 and build tools. The complete
engine and APK build runs in Docker; no host Java or Android SDK is required.

```powershell
.\android\build.ps1
```

The first build downloads upstream LLVM 21.1.0, the reference project's NDK
sysroot and SDL3 build, Rust 1.94.0, the pinned nod disc reader and its locked
dependencies, plus Gradle dependencies. Downloads and engine objects
use the Docker volume `zenkaipower-android-toolchain` (the NDK has filenames that
differ only by case). The staged source and cached objects live in the Docker
volume `zenkaipower-android-work`; the engine and maps are copied back to
`port/build/android/`. `-EngineOnly` and `-ApkOnly` select individual stages.

Every Gradle APK build generates the launcher icon and setup/preparation header
icon from `assets/zp-avatar.png`, including `-ApkOnly` builds. Changing the avatar
regenerates all launcher densities and the adaptive icon foreground. Generated
images stay under `android/app/build/`; checked-in fallback icons are unchanged.
Direct Gradle builds require Python with Pillow (`python3-pil` in the Docker
image); set `PYTHON` to override the Python executable.
`ZENKAIPOWER_JOBS` limits compiler parallelism. Unchanged sources are cached between builds.
Builds fail on unresolved runtime symbols.
Unused original PS2 SDK dispatch-table entries receive diagnostic traps, like
the desktop port; the build verifies that compiled code never references those
missing symbols and records them in `port/build/android/unused-sdk.txt`.

On Linux/macOS with Docker running:

```sh
sh android/build.sh
```

The debug APK is `release/android-arm64/ZenkaiPower-debug.apk`.
Generated native libraries, SDL Java sources, assets, Gradle caches and local
SDK paths are ignored by Git.

### Signed release

Set the release version in the root `game-info.json`. Both `versionName` and
`versionCode` are derived from it automatically, including on debug builds.
The code is `major * 1000000 + minor * 1000 + patch`: `1.0.1` becomes `1000001`.
Repeated builds of the same version keep the same code; increasing the version
raises it. Use numeric `MAJOR.MINOR.PATCH`, with minor and patch each in
`0..999`; the derived code must be in Android's `1..2100000000` range.

Build from the shared release entry point on Windows:
`./port/release/build.ps1 android -Release`.
On Linux use `sh android/build.sh --release`.
The versioned artifact is `release/android-arm64/ZenkaiPower-v1.0.0-android-arm64.apk`
(the version follows `game-info.json`). Approve the final APK alongside desktop
builds with `python port/tools/make_online_signature.py --build release/android-arm64/ZenkaiPower-v1.0.0-android-arm64.apk`.
The release build also creates `release/ZenkaiPower-v1.0.0-android-arm64.zip`,
containing the versioned APK and its SHA-256 file under `android-arm64/`.
This adds its engine, metadata and SDL signature to the existing policy without
removing desktop approvals. Deploy the matchmaking Worker afterward to publish
the updated policy. APK signing and online binary approval are separate steps.
Rebuilding changes the online signature even when the version number stays the
same. Debug and storage-check APKs are not automatically approved for online
play. If a build is refused, `adb logcat -s zenkaipower` distinguishes an unapproved
build from missing/unreadable files or an unreachable verification service.

Create a private signing key once (never replace it for app updates):

```powershell
.\android\create-release-key.ps1
.\android\build.ps1 -Release
```

The signed, non-debuggable APK is `release/android-arm64/ZenkaiPower-android.apk`
(app name from `subtitle`, version from `version` in the root `game-info.json`). Keep a private backup of both
`%USERPROFILE%/.android/zenkaipower-release.jks` and
`%USERPROFILE%/.android/zenkaipower-release.properties`. They are outside the
repository. An existing key will never be overwritten by the helper.
For another machine, set `ZENKAI_SIGNING_PROPERTIES` to a private Java properties
file with `storeFile`, `storePassword`, `keyAlias` and `keyPassword`. Use an
absolute keystore path with forward slashes. Linux release builds use
`sh android/build.sh --release`. `-ApkOnly -Release` repackages the last native build.

Debug and release builds use different signing keys. Android cannot install
one over the other: first copy game files and saves to the chosen shared game
folder, then remove the old app and install the release. Future release updates
use the same key and install normally. Uninstalling the app does not remove the
chosen shared folder, but Android does remove the old app-specific storage.

This is a directly installable APK. Shared-folder access uses Android's folder
picker and a [persisted read/write grant](https://developer.android.com/training/data-storage/shared/documents-files#persist-permissions)
for that folder only. The app does not
request all-files access or legacy storage permissions. A native rebuild is
required for this change; APK packaging rejects older engine/loader/installer
libraries that do not link the folder adapter (`-ApkOnly` cannot migrate them).

## Install from disc images

1. Install and open the APK. Android recognizes it as a game; the icon comes
   from `assets/zp-avatar.png`.
2. Choose a local folder such as `Internal storage/ZenkaiPower` in the Android
   folder picker and allow access to that folder. This applies to Android 10+;
   no storage settings screen or device-wide file permission is needed.
3. Select the **parent** folder for `gamedata` and `savegame`, not either child.
   The launcher remembers the choice; **Choose game folder** changes it later.
4. Tap **Install Budokai Tenkaichi 3 — USA (SLUS-21678)** and select your original
   USA PS2 ISO. Installation checks the disc, extracts the packaged archives,
   decodes touch-button artwork and generates `game-text.pak` on the phone.
5. Optionally install each additional disc with its own button:
   - **Europe PS2 (SLES-54945):** official Spanish, German, French and Italian
     translations, language selector and opening soundtrack.
   - **Europe Wii (RDSPAF):** Dragon Net Battle artwork, animations, text and
     voices for the LAN/online menus; accepts ISO and RVZ.
   - **Japan PS2 (SLPS-25815):** Sparking! METEOR title artwork, anime soundtrack
     and other Japanese content.
6. Select **Play**. Installed discs display **Reinstall** for repairs.

Additional discs require a prepared USA installation. Each disc is checked
against the same region/program/archive checksums as the desktop setup.
The file picker reads local images in place; it does not copy the whole ISO.
PS2 and Wii readers use owned, seekable descriptor streams, without reopening
`/proc/self/fd` paths or treating neighboring descriptors as split-image parts.
Keep the app open during installation; progress and cancellation are available.
All installation stages use the portrait setup progress screen. After a successful
installation, setup restarts automatically to reload the installed content.
Text preparation briefly opens an isolated game process and returns to the
launcher when finished. Errors remain visible in the launcher. PS2 images must
be uncompressed ISO files. No Python, 7-Zip or desktop executable is required.
The launcher also recognizes a desktop installation already placed in the
chosen folder. ZIP import and the old installation-copy controls are removed.
Existing text packages remain available until a replacement is published. If an
older interrupted installer removed the text pak, the launcher regenerates it
from the verified installed USA data. Preparation uses a separate process and
per-request result files, so it cannot reuse a running battle activity or stale
completion status.

The launcher follows the desktop setup's light paper colors, blue controls and
green verified-disc indicators. The optional `setup/cover.jpg` appears below **Play** as soon as the game
folder is loaded, without waiting for disc verification. Verified disc icons open edition details, with front,
back and disc artwork; tap an image to open the pinch-to-zoom viewer. Unverified
discs use neutral placeholders. Artwork is optional: place JPEG/PNG files in `setup/` inside the selected game folder,
as described in [release instructions](../port/release/README.md#optional-installer-artwork).
Missing files leave neutral icons, hide the cover and show unavailable edition
images. Artwork is read through the selected folder's storage access and is not
embedded in the APK. Restart setup after changing files.

The game runs directly from the chosen folder. Its standard folders are `gamedata/`, `licenses/`, `logs/`, `net_session/`,
`savegame/`, `setup/`, `textures/` and `tools/`. Bundled fonts, translations and
game information live in `fonts.pak`, `translations.pak` and `game-info.pak`,
using the same desktop package format. The shared setup catalog is in `setup.pak`
and the server address is in `dragon-net-battle.pak`. Android engine maps are packaged as
`setup/engine.pak` and expanded only into the private app cache. Saves and relative desktop file paths resolve
there. Touch-button artwork is read directly from `gamedata/touch-buttons.pak`.
Both packed game data and an extracted desktop data tree are recognized.
No disc data is bundled with the APK.

Choose internal shared storage or a mounted SD-card folder outside Android's
protected directories. Cloud document providers are not supported. Both the
launcher and native engine resolve game paths through the selected document
tree, opening seekable file descriptors directly. Installation, mods, logs and
saves use that folder in place; the game is not copied into app-owned storage.
Directory lookups and read-only archive descriptors are cached for one second;
in-app writes and replacements invalidate them immediately. Archive readers have
independent seek positions, and native AFS/PAK indexes are reused until the file's
identity, size or modification time changes. Background disc verification runs
at low priority, yields between batches, and reads packed entries directly.
Native archive streams keep independent positions using `pread` on retained
descriptors; asset loads do not reopen the archive through shared-storage FUSE.
Archive replacement is checked periodically; already-open readers retain their
original file, as with normal POSIX file handles.
The app remembers the folder grant across restarts. If access is revoked, the
folder is moved, or the SD card is removed, select the folder again. Existing
installations from the all-files version must select their same folder once
after updating; their game files and saves remain in place.

The document provider does not offer POSIX atomic replacement. When publishing
a replacement file, the adapter first renames the old file to a unique sibling
backup, publishes the completed temporary file, then removes the backup. Failed
publication restores the old name; after interruption, opening a missing target
restores its backup. Files are never overwritten by copying through private
storage. Cross-directory moves support an unchanged filename and an unoccupied
destination (including migration of old logs); other cross-directory renames
return `EXDEV`.

Engine diagnostics are written to `logs/zenkaipower_log.txt` in the chosen game folder and
to `adb logcat -s zenkaipower`. The Android system records native crash tombstones. The
game runs in a separate process so exiting it does not terminate the launcher.
Installer events are saved in `logs/setup-android.log`.

Device installer checks use a separate test APK and isolated cache folders:

```powershell
python port/tests/android/installer_fixtures.py
# Add --boot <owner-extracted boot-icons.bin> to compare all 18 touch images.
# Inside the build container, run android.py installer-tests, then copy the
# generated android/app/src/androidTest/jniLibs back to the checkout.
.\android\gradlew.bat -p android assembleDebugAndroidTest
adb install -r android/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
adb shell am instrument -w com.zenkaipower.android.debug.test/com.zenkaipower.android.InstallerChecks
```

These checks reject invalid discs and malformed DOL metadata, compare Japanese
and Wii ISO/RVZ extraction against desktop fixtures, and generate a fresh text
package from a read-only view of installed data. Test assets and libraries are
ignored by Git and excluded from the release APK.

Folder-grant checks use a unique temporary child of the folder already selected
in the launcher. Build `python port/tools/android.py storage-tests` inside the
Android toolchain environment (the container wrapper copies the test library),
then install the instrumentation APK as above and run:

```sh
adb shell am instrument -w com.zenkaipower.android.debug.test/com.zenkaipower.android.StorageChecks
```

These checks exercise Java descriptor seeking/appending, save replacement and
interruption recovery, native relative/absolute file access and SDL I/O against
the selected tree. They never use the player's save filenames. Verify on-device
with all-files access disabled; repeat after restarting the app, with an SD-card
folder, and after revoking the folder grant. The tree's game data and saves must
remain after uninstalling the app.

Folder setup, packed/loose readiness and migration checks:
`python port/tests/android/game_folder.py` (JDK 17+).

## Port implementation

USB OTG and Bluetooth gamepads use SDL's existing Android controller support.
Physical controllers are listed before the touch pad, so the default player 1
slot uses a physical controller when connected, including after touch play.
Generic DragonRise USB PS2-style pads (`0810:0001`) have an Android mapping for
their numbered buttons, D-pad and both analog sticks. Detection and mapping
results are recorded in `logs/zenkaipower_log.txt`.

For wireless ADB on Android 11+, connect the PC and phone to the same Wi-Fi,
enable **Developer options > Wireless debugging**, then choose **Pair device
with pairing code**. Run `adb pair PHONE_IP:PAIRING_PORT` on the PC and enter
the displayed code. Return to the main Wireless debugging screen and use its
**IP address & port** with `adb connect PHONE_IP:CONNECTION_PORT`. These two
ports usually differ. Check `adb devices`, then connect the USB gamepad through
an OTG adapter. Official instructions: https://developer.android.com/tools/adb#connect-to-a-device-over-wi-fi

Touch gamepad controls appear automatically during combat and hide in menus,
loading screens, cutscenes and the battle pause menu. Touch on the left half and drag
to move the left analog stick; release to center it. The right side provides
Triangle, Circle, Cross and Square animate using their original pressed textures.
L1 sits above L2 on the left, with L3 at the bottom left; R1 sits above R2 on the
right, with R3 at the bottom right. Start and Select sit along the bottom center.
Multiple fingers can move and hold different buttons together.
In Character Reference's Model View, drag across the screen to orbit the camera.
During Sparking! Changes battles, drag an empty area on the right half of the
screen to turn the camera horizontally while unlocked. Buttons and the left
movement stick remain usable with other fingers.
Controls release when the app loses focus or is paused.
Android's **Options > Controller > Virtual Gamepad** provides an On/Off selector
(default On), saved through the game's normal Options save flow. Mouse/Keyboard
is hidden in the Android Controller menu. Tap the battle timer at the top center
to pause, including when Virtual Gamepad is Off or a physical pad is connected.
With Virtual Gamepad and the game's Controller Vibration option enabled, battle
rumble also drives the Android device's vibrator. Devices with amplitude control
follow the motor strength; other devices use their default vibration strength.
Vibration stops when virtual controls turn off, the battle ends, the app pauses,
or the window loses focus. Devices without a vibrator continue normally.

Outside combat, slide one finger
to move the mouse pointer without clicking. Tap
once to left-click; tap with two fingers together to right-click (Back/Cancel).
Dragging or holding a touch does not click when released. Physical mice and
pens continue to use SDL's normal input handling.

The data exporter extracts the original button artwork (including pressed face
buttons from the battle recovery prompts) into `gamedata/touch-buttons.pak` in the
ZIP. No disc artwork is bundled with the APK. Existing installations can export
these separately with `python port/tools/android_buttons.py --data <gamedata>
--output <gamedata>/touch-buttons` and copy that directory into the phone's
installed `gamedata` directory. On the next launch it is converted to
`touch-buttons.pak`. Text labels are used if the artwork is absent.

`port/tools/android.py` reuses the existing game preparation, PS2 floating-point
constants, `ptr32.py`, software float and renderer sources. Android-specific IR
fixups correct AArch64 narrow stores and move emulated registers into arrays.
The engine is linked below 4 GB and supplied with relocation and game-data maps;
`app/src/main/cpp/loader.c` reserves low memory, loads the engine, relocates its
32-bit pointers when needed, and runs constructors after relocation.

The Android memory branch and AArch64 rollback context are conditional; desktop
builds retain their existing paths. Memory mapping and rollback tracking use the
device's page size, and native ELF segments are linked with 16 KB alignment.

See [NOTICE.md](NOTICE.md) for the reference project's attribution and licenses.

Android rendering resolution offers 0.5x, 1x, 1.5x, 2x, 2.5x and 3x. The default is 1.5x.

Android Video Settings hides the Graphics API selector; the renderer uses Vulkan.

Two shortcut buttons to the right of L2 hold L2 + D-pad Up and L2 + D-pad Down.
Their artwork combines the original L2 icon with the extracted directional arrows.

Encrypted saves and Options use anonymous temporary streams in the app cache;
the authenticated final files are written to the chosen game folder. Android
checks for this path are in `port/tests/android/save_stream.py`.

Existing loose fonts, translations and generated disc/text/button resources are
merged into their desktop-format packages. Input files are removed only after
package verification; unknown nonempty user folders are preserved. Empty folders
created by older Android builds are removed. Mod overrides belong under
`gamedata/mods/`; `songs/` and `stages/` remain optional desktop features rather
than directories created by Android. Pak conversion checks:
`python port/tests/android/game_pak.py`.

## VS Code F5 (Windows)

Select **Build and run Android** in Run and Debug, then press **F5**. The
configuration uses the existing Docker build to compile the engine and package
the debug APK, installs it with ADB, and opens the Android launcher. This is a
build-and-launch configuration; it does not attach a native debugger.

Docker Desktop must be running. Install Android platform-tools and put `adb` on
PATH, or set `ANDROID_HOME` / `ANDROID_SDK_ROOT`. Connect an arm64 Android device
with USB debugging enabled and authorize the computer, or start an arm64
emulator. With multiple devices, the terminal prompts you to select one. Set
`ANDROID_SERIAL` before starting VS Code, or pass `-Serial`, to skip the picker.

The same flow is available through **Tasks: Run Task ? Build and run Android**
or `powershell -File android/run.ps1`. Use `-SkipBuild` to install and launch the
existing `build/android-arm64/ZenkaiPower-debug.apk` without rebuilding. The
debug app uses `com.zenkaipower.android.debug` and a **(Debug)** label, so it
installs alongside the release app. Its settings and folder grants are separate;
choose the game folder on its first launch. Rebuilding updates the debug app
without clearing its data. The release app and its data are preserved.

### Touch direction gestures

With the touchscreen gamepad active, drag in empty space on the right half of
the screen to send a right-stick direction. After a short movement threshold,
the dominant direction is held at full strength until release. There is no
visible right joystick or variable sensitivity. Release and drag again to send
another direction. Up/down selects the switch candidate when the battle rules
allow it. Buttons and left-stick movement remain independent.
