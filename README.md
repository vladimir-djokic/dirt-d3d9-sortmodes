# dirt-d3d9-sortmodes

> **Built with an AI model.** A large language model diagnosed the problems and wrote this code. I directed the work, ran every test on my own PC and decided which approaches to try.
>
> - **Tool:** [Claude Code](https://claude.com/claude-code) by Anthropic, in the Claude desktop app.
> - **Models:** Claude Opus 5.5 (`claude-opus-5-5`) did the WinDbg crash-dump analysis, found the root causes and wrote the proxy, Makefile and documentation. Claude Fable 5.1 (`claude-fable-5-1`) did the first look at the crash logs.
> - **What it found:** DiRT and GRID both break on a TV that reports a long, unsorted list of display modes. DiRT crashes when modes arrive out of order. In both games, a long list overflows a small hardware-settings memory pool and the game crashes at startup. Each game also needs one edit to its own files, for an unrelated bug. See [Which fixes each game needs](#which-fixes-each-game-needs).
>
> Review the code before you use it, as you would any third-party DLL.

A minimal Direct3D 9 proxy DLL that returns display modes sorted by width, height and refresh rate, optionally filtered to a shorter list. Every other call goes straight to the real `d3d9.dll` in the Windows system folder.

It makes these Codemasters games run on modern PCs with large displays:

- **Colin McRae: DiRT** (2007)
- **Race Driver: GRID** (2008)

Both are tested with the Steam versions on a Ryzen 7 5700X (8 cores, 16 threads), an RTX 3080 and an LG 4K TV that reports 217 display modes.

## Which fixes each game needs

| | DiRT (2007) | GRID (2008) |
|---|---|---|
| This DLL | Yes | Yes |
| `d3d9_sortmodes.ini` | `refresh=` your in-game refresh rate | `only_aspect=16:9` and `refresh=` your in-game refresh rate |
| Edit to the game's files | [CPU core-count fix](#dirt-cpu-core-count-fix), for crashes when a race loads | [Resolution cap fix](#grid-resolution-cap-fix), for resolutions stuck at 1280 wide |

## Files

- `d3d9_sortmodes.cpp` — the proxy, about 200 lines.
- `d3d9.def` — the exported functions.
- `d3d9_sortmodes.ini` — default settings.
- `Makefile` — build, install and clean. The build produces `d3d9.dll`, which is not checked in.

## Download

Prebuilt DLLs are on the [Releases](https://github.com/vladimir-djokic/dirt-d3d9-sortmodes/releases) page. GitHub Actions builds them from this repository's source. Each release lists the SHA-256 checksum and a VirusTotal scan link. To confirm a DLL was built here, run:

```bash
gh attestation verify d3d9.dll -R vladimir-djokic/dirt-d3d9-sortmodes
```

## Install

Copy `d3d9.dll` and `d3d9_sortmodes.ini` next to the game's exe, or run `mingw32-make install` (see [Build](#build)). Then set up `d3d9_sortmodes.ini` for your game:

```ini
[modes]
only_aspect=16:9   ; keep only modes with this exact aspect ratio
refresh=60         ; list each resolution once, at the rate closest to this
log=1              ; write the mode list to d3d9_sortmodes.log
```

- **`refresh`** keeps the mode list short. Both games store detected modes in a small, fixed memory pool: 100 KB in DiRT and 16 KB in GRID. A display reporting hundreds of modes overflows it, the settings stored after it are lost, and the game crashes at startup.
- **Set `refresh` to the rate saved in the game's settings.** If the game's saved mode, such as 3840x2160 at 120 Hz, isn't in the list, it falls back to a lower resolution on every launch.
- **`only_aspect`** shortens the list further. GRID's 16 KB pool needs it on displays with many modes.
- **`log=1`** is useful for the first launch. Turn it off once everything works.

To uninstall, delete `d3d9.dll`, plus the `.ini` and `.log` files if present.

## DiRT (2007)

Recommended `d3d9_sortmodes.ini`, with `refresh` matching your in-game rate:

```ini
[modes]
refresh=60
log=0
```

### DiRT: CPU core-count fix

This DLL gets DiRT to the menus. On a modern CPU, the game can still crash as soon as a race loads. That's a separate bug, and you fix it by editing one line in the game's own files.

**Cause:** DiRT picks a thread layout from the number of logical processors it detects. With 8 or more, it uses its 8-core layout. On a Ryzen 7 5700X (8 cores, 16 threads), that layout makes a background task request entry 2 of a table that holds only one entry, and the game crashes during race loading. The game's 4-core layout avoids it.

**Fix:** back up the files first. Then, in the DiRT folder, open `system\hardware_settings_restrictions.xml` and find the rule for 8 or more cores:

```xml
<restrict_if name="cores" op="int_ge" value="8">
```

Replace the `threadStrategy` line under it with the 4-core line from the same file. Change this line:

```xml
<threadStrategy parallelUpdateRender="true" workerMapFile="system/workerMap8Core.xml" forceFeedbackProcessor="3" dvdStorageProcessor="4" dataSetMonitorProcessor="5" renderProcessor="0" updateProcessor="2" fileStreamProcessor="7" />
```

to this:

```xml
<threadStrategy parallelUpdateRender="true" workerMapFile="system/workerMap4Core.xml" forceFeedbackProcessor="3" dvdStorageProcessor="3" dataSetMonitorProcessor="3" renderProcessor="0" updateProcessor="2" fileStreamProcessor="3" />
```

Make the same change to the `threadStrategy` line in DiRT's saved settings file, `hardware_settings_config.xml`. In the Steam version it's in the `hardwaresettings` folder inside the game folder. Changing the rule alone isn't enough, because the game keeps using the layout in the saved settings until it rewrites them.

Steam's "Verify integrity of game files" restores the original rules file. If races crash again after a verify, redo the edit.

## GRID (2008)

Recommended `d3d9_sortmodes.ini`, with `refresh` matching your in-game rate:

```ini
[modes]
only_aspect=16:9
refresh=120
log=0
```

GRID doesn't need the CPU core-count fix. Its rules give every CPU with 4 or more cores the 4-core layout.

### GRID: resolution cap fix

Without this edit, GRID offers no resolution wider than 1280 pixels, even with this DLL installed.

**Cause:** GRID reads the graphics card's memory through DirectDraw and stores it as a signed 32-bit number. On a card with 2 GB or more, the value overflows and turns negative. GRID then treats the card as having 270 MB or less, and hides every resolution wider than 1280 pixels.

**Fix:** back up the file first. Then, in the GRID folder, open `system\hardware_settings_restrictions.xml` and change this line:

```xml
<res mem="270" maxWidth="1280" />
```

to this:

```xml
<res mem="270" maxWidth="4096" />
```

GRID re-detects your hardware on the next launch, and the full list appears in its options. As with DiRT, Steam's "Verify integrity of game files" restores the original file.

## Build

Use 32-bit MinGW-w64, for example WinLibs i686, unpacked to `C:\mingw32`. It includes GNU Make as `mingw32-make`. Run these from the project folder in PowerShell, Command Prompt or Git Bash:

```bash
C:\mingw32\bin\mingw32-make.exe            # build d3d9.dll
C:\mingw32\bin\mingw32-make.exe install    # copy it into DiRT's folder
C:\mingw32\bin\mingw32-make.exe clean      # delete the build
```

`install` targets DiRT's Steam folder by default. For GRID, or a game in another location, pass `GAME_DIR`:

```bash
C:\mingw32\bin\mingw32-make.exe GAME_DIR="C:\Program Files (x86)\Steam\steamapps\common\Grid" install
```

- `install` also copies the default `d3d9_sortmodes.ini`, but only when the game folder has none yet, so your settings are never overwritten. Adjust it for your game afterwards.
- Set `MINGW` if your compiler isn't in `C:\mingw32`, for example `MINGW=D:/tools/mingw32`.
- The build stops with an error if `MINGW` points at a 64-bit compiler, because both games are 32-bit.

The result depends only on `KERNEL32.dll` and `msvcrt.dll`, which ship with Windows.

## License

MIT. See [LICENSE](LICENSE).
