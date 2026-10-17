# dirt-d3d9-sortmodes

> **Built with an AI model.** A large language model diagnosed the problem and wrote this code. I directed the work, ran every test on my own PC and decided which approaches to try.
>
> - **Tool:** [Claude Code](https://claude.com/claude-code) by Anthropic, in the Claude desktop app.
> - **Models:** Claude Opus 5.5 (`claude-opus-5-5`) did the WinDbg crash-dump analysis, found the root causes and wrote the proxy, Makefile and documentation. Claude Fable 5.1 (`claude-fable-5-1`) did the first look at the crash logs.
> - **What it found:** DiRT has two separate bugs triggered by a TV that reports a long, unsorted mode list. It crashes when modes arrive out of order. A long list also overflows its 100 KB hardware-settings memory pool, so the audio settings are lost and the game crashes at startup.
>
> Review the code before you use it, as you would any third-party DLL.

A minimal Direct3D 9 proxy DLL that returns display modes sorted by width, height and refresh rate. Every other call goes straight to the real `d3d9.dll` in the Windows system folder.

It works around old games that crash when the driver reports modes out of order. Colin McRae: DiRT (2007) is one: it crashes at startup on an LG TV that lists 1600x1200 after 2560x1600.

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

Run `mingw32-make install`, or copy `d3d9.dll` next to the game's exe by hand. Optionally add `d3d9_sortmodes.ini` there too:

```ini
[modes]
; only_aspect=16:9   ; keep only modes with this exact aspect ratio
refresh=60           ; list each resolution once, at the rate closest to this
log=1                ; write the mode list to d3d9_sortmodes.log
```

`refresh` matters for DiRT. The game keeps its detected mode list in a fixed 100 KB memory pool. A display reporting hundreds of modes fills it, the audio settings after it are lost, and the game crashes at startup.

To uninstall, delete `d3d9.dll`, plus the `.ini` and `.log` files if present.

## Build

Use 32-bit MinGW-w64, for example WinLibs i686, unpacked to `C:\mingw32`. It includes GNU Make as `mingw32-make`. Run these from the project folder in PowerShell, Command Prompt or Git Bash:

```bash
C:\mingw32\bin\mingw32-make.exe            # build d3d9.dll
C:\mingw32\bin\mingw32-make.exe install    # copy it into DiRT's folder
C:\mingw32\bin\mingw32-make.exe clean      # delete the build
```

Override the defaults on the command line when needed:

```bash
C:\mingw32\bin\mingw32-make.exe MINGW=D:/tools/mingw32 GAME_DIR="D:\Games\DiRT" install
```

- `install` also copies `d3d9_sortmodes.ini`, but only when the game folder has none yet, so your settings are never overwritten.
- The build stops with an error if `MINGW` points at a 64-bit compiler, because the game is 32-bit.

The result depends only on `KERNEL32.dll` and `msvcrt.dll`, which ship with Windows.

## License

MIT. See [LICENSE](LICENSE).
