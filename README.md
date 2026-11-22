# Gear Management Overhaul

Source for the Gear Management Overhaul v0.5.9 native plugin for Door Kickers 2. The DLL implements extended equipment copy/paste, loadout presets, and portrait selection. The Workshop package supplies the game UI and artwork.

## Build and test on Windows

Install [Zig 0.16.0](https://ziglang.org/download/) and make sure `zig` is on PATH. From the repository root in PowerShell, run:

```powershell
.\build.ps1 -RunTests
```

The output is `build/Rake5000GearManagementOverhaul.dll`. The build and tests require no local Door Kickers 2 installation. To try the rebuilt DLL in-game, replace the DLL in the `native` folder of an existing GMO package while the game is closed. Follow the [DK2 Modloader installation instructions](https://github.com/gabrweu/doorkickers-modloader#installation-steps-for-players).

## License

Original GMO source is available under the [PolyForm Noncommercial License 1.0.0](LICENSE). The [Door Kickers 2 Modloader SDK header](https://github.com/gabrweu/doorkickers-modloader) retains its bundled MIT license.
