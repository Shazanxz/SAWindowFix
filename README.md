# SAWindowFix

**English** | [Português](README.pt-BR.md)

An ASI mod for **GTA San Andreas** (and **SA:MP**) that runs the game in a **borderless window** covering the whole monitor. Alt-Tab is instant, the game keeps running in the background, and nothing is minimized or lost when you switch windows.

## Features

- **Borderless window**: looks like fullscreen, behaves like a window. Alt-Tab with no black screen and no minimizing.
- **Keeps running in the background**: no pause, no pause menu when you come back, and the audio keeps playing.
- **Renders at the menu resolution**: the image is stretched to the monitor, so lowering the resolution in the game menu gives you more FPS on weak GPUs.
- **Original 4:3 view**: keeps the stock fullscreen field of view instead of the zoomed-in windowed look.
- **Correct mouse** in the game menu and in SA:MP dialogs and chat, at any resolution.
- **Vsync off**.

## Compatibility

| Game | Status |
|---|---|
| GTA San Andreas 1.0 US | Tested, working |
| Other versions (1.01, 2.0, Steam, ...) | Not supported, nothing is changed |

| Together with | Status |
|---|---|
| SA:MP, SAMPFUNCS, MoonLoader, CLEO | Tested, working |
| Other windowed / no-pause mods | Not compatible (see below) |

You need an **ASI Loader** installed (CLEO and SAMPFUNCS already include one).

Before changing anything, SAWindowFix checks every place it patches. If another windowed, borderless or no-pause mod has already changed one of them, SAWindowFix **changes nothing** and writes in the log which mod conflicts. Keep only one of them.

## Installation

1. Download `SAWindowFix.asi` from [Releases](../../releases).
2. Copy it to your GTA San Andreas folder, next to `gta_sa.exe`.
3. Start the game. It opens directly in a borderless window.

`SAWindowFix.ini` is created automatically on first run.

## Configuration

```ini
[SAWindowFix]
; Write SAWindowFix.log next to the .asi (0 = off, 1 = on)
Log=0
```

- **Log**: `1` writes `SAWindowFix.log` next to the `.asi`. Turn it on when reporting a problem and attach the file.

The log records:
- the mod version and environment (game path, Windows version, monitor)
- every patch applied
- device resets, with their cause
- the FPS every 10 seconds
- crash details (module + offset, registers, stack)

## Tips

- **More FPS**: choose a lower resolution in the game menu (e.g. `1280x720`). The game renders fewer pixels and the image is stretched to the monitor.
- **Short freeze after Win+L, UAC or Ctrl+Alt+Del**: this is Direct3D 9 recovering its device. It is normal and cannot be avoided.

## How it works

The mod patches `gta_sa.exe` in memory when it loads:

- **Window**: forces the D3D9 present parameters to windowed mode just before the device is created, turns the window into a borderless popup, and hooks `IDirect3DDevice9::Reset`/`Present` so it stays that way after a device reset or the ESC menu.
- **No pause**: the focus-loss handlers in the window procedure are neutralized, so `ForegroundApp` is never cleared and the main loop keeps running.
- **Stretched rendering**: RenderWare's client-size checks are skipped, so the backbuffer keeps the menu resolution while the window covers the monitor. Mouse messages are rescaled in the game's `PeekMessageA` import before any window procedure (SA:MP included) sees them.

Each patch site is compared with the original bytes before anything is written. Calls are redirected instead of NOPed, so other mods that hook the same calls can still chain to them.

## Building

Requires Visual Studio 2019+ (the "Desktop development with C++" workload) and CMake 3.20+.

```sh
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Output: `build/Release/SAWindowFix.asi`. Add `-DGTA_SA_DIR="C:/Games/GTA San Andreas"` to copy it to the game after every build.

The version is generated from git: each commit bumps the patch number (`1.0.1`, `1.0.2`, ...), a `vX.Y.Z` tag sets a new base, and builds with uncommitted changes get `-dev`.

## License

Licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE).

- **Allowed**: using, studying, modifying, forking and sharing the mod and its code, for any noncommercial purpose.
- **Not allowed**: selling it or using it for any commercial purpose.
- **Required**: anyone who shares the mod or code derived from it must include the license and keep the credit line:
  `Required Notice: Copyright (c) 2026 Shazanxz (https://github.com/Shazanxz)`

Suggestions, issues and pull requests are welcome.

## Credits

- [**Shazanxz**](https://github.com/Shazanxz) - development and maintenance
- [**mrxenginner**](https://github.com/mrxenginner) - first version and improvements
- [**ThirteenAG**](https://github.com/ThirteenAG) - inspiration, through the [WindowedMode Plugin](https://github.com/ThirteenAG/III.VC.SA.WindowedMode)
