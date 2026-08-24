# System Transparency

🌍 [English](https://github.com/sunwookim05/Transparent-window/blob/main/README.md) | [한국어](https://github.com/sunwookim05/Transparent-window/blob/main/translations/ko.md)

System Transparency is a lightweight Windows tray utility that makes Explorer and selected system windows feel cleaner by applying adjustable transparency in real time.

It is written in C with the Win32 API. The app is distributed as a single executable: on first run it can install itself into the user profile, register the required certificate, create a highest-privilege startup task, and keep itself updated from GitHub Releases.

<p align="center">
  <img src="https://img.shields.io/badge/C-100%25-blue?style=flat-square" />
  <img src="https://img.shields.io/badge/Win32-API-blue?style=flat-square&logo=windows" />
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011-blue?style=flat-square" />
  <img src="https://img.shields.io/github/v/release/sunwookim05/Transparent-window?label=release&style=flat-square" />
  <img src="https://img.shields.io/github/license/sunwookim05/Transparent-window?style=flat-square" />
</p>

## Features

- Automatic Explorer transparency for supported Windows Explorer and menu windows.
- Manual transparency toggle with configurable hotkeys:
  - default `Ctrl + Middle Click`: apply the selected preset to the active window.
  - default `Win + Middle Click`: restore the active window to full opacity.
  - default `Ctrl + Win + Mouse Wheel`: adjust the active window opacity step by step.
- Tray menu with dark owner-drawn styling.
- English and Korean UI language support.
- Transparency presets:
  - `Solid`: 255
  - `Soft`: 200
  - `Glass`: 150
  - `Ghost`: 80
  - `Custom Alpha`: user-defined value from 60 to 255.
- Custom Alpha dialog with:
  - dark UI
  - draggable slider
  - live numeric value
  - live preview on the selected window while adjusting.
- Dark uninstall confirmation dialog from the tray settings menu.
- Single-exe first-run setup:
  - copies itself to `%LOCALAPPDATA%\SystemTransparency\SystemTransparency.exe`
  - requests UAC when required
  - registers the bundled certificate
  - creates a Windows scheduled task with highest privileges for startup.
- GitHub Releases based auto update.
- Update log shortcut from the tray menu.

## Installation

1. Download `SystemTransparency.exe` from the latest release.
2. Run it once.
3. Approve the UAC prompt when the app performs first-run setup.
4. After setup, the installed copy starts from:

```text
%LOCALAPPDATA%\SystemTransparency\SystemTransparency.exe
```

The app then runs from the system tray and automatically starts on future logins.

## Usage

Right-click the tray icon to open the menu.

- `Setting > Explorer Auto Transparency`: enable or disable automatic transparency for supported Explorer windows.
- `Setting > Run at Startup`: enable or disable the startup scheduled task.
- `Setting > Hotkeys...`: record the modifier keys used for the manual transparency shortcuts.
- `Setting > Language`: choose System default, English, or Korean.
- `Setting > Preset`: choose the transparency level used by automatic mode and the apply-preset shortcut.
- `Setting > Preset > Custom Alpha...`: choose a custom opacity value from 60 to 255.
- `Setting > Uninstall`: remove the installed executable, startup task, app registry settings, and bundled certificate registration.
- `Check for Updates`: manually check the latest GitHub Release.
- `Open Log`: open the updater log file.

## Updating

System Transparency checks GitHub Releases and installs a newer `SystemTransparency.exe` when a newer version is available.

The update process is designed for the single-exe distribution model. The running app downloads the release asset, schedules replacement of the installed executable, and restarts into the updated version.

## Build

This project uses GCC/MinGW and the Windows resource compiler.

```powershell
windres res\app.rc -O coff -o build\app_res.o
gcc src\main.c src\App.c src\Installer.c src\Updater.c src\Settings.c src\Transparency.c src\Tracker.c src\thread.c src\System.c src\Scanner.c src\console.c src\algorithm.c build\app_res.o -Iinc -o SystemTransparency.exe -mwindows -luser32 -lshell32 -lcomctl32 -lpsapi -lwinhttp -lshlwapi -ldwmapi -Wall -Wextra
```

## Notes

- Some protected, game, GPU-accelerated, or security-sensitive windows may reject transparency changes.
- The app is optimized for Windows Explorer and common shell windows.
- Administrator rights are required for first-run setup, certificate registration, and highest-privilege startup registration.

## License

MIT License. See [LICENSE](LICENSE).

This project includes source code and structural patterns derived from [Object-Oriented-C-Language](https://github.com/sunwookim05/Object-Oriented-C-Language), licensed under the MIT License.

## Author

- GitHub: [sunwookim05](https://github.com/sunwookim05)
- Repository: [sunwookim05/Transparent-window](https://github.com/sunwookim05/Transparent-window)
