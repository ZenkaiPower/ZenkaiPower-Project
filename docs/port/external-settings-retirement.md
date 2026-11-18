# F1 settings

The Dear ImGui settings window is opt-in in debug builds; distribution builds disable it. In debug builds, launch with `--settings` to enable F1, the configured overlay key or controller button, and the START + SELECT controller chord. The menu has Video Settings and, when cheats are enabled, Cheats.

Video Settings uses the same eight descriptors, runtime owners and change bridge as the in-game Video Settings page. In ZENKAI mode, these settings are captured in the checksummed `.sav.options` companion during the normal game-save flow. Other preference persistence remains opt-in through `ZENKAIPOWER_SETTINGS=<path>`; enabling the F1 window does not create `savegame/settings.dat`.

The release build still compiles out cheats. Development builds can expose the Cheats tab by launching with `--cheats`.
