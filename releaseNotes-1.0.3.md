# System Transparency 1.0.3

## Highlights

- Added single-executable first-run setup.
- Added GitHub Releases based auto update support.
- Added a dark tray menu and a dark Custom Alpha dialog.
- Added a Custom Alpha slider with live numeric value updates and live window preview.
- Improved startup behavior by using a highest-privilege scheduled task.
- Improved tray icon recovery after Explorer/taskbar restarts.

## What Changed Since 1.0.2

- Replaced the old installer-centered flow with single-exe self-install behavior.
- The app now installs itself into `%LOCALAPPDATA%\SystemTransparency\SystemTransparency.exe`.
- First-run setup can request UAC, register the bundled certificate, and register startup automatically.
- Added manual update checking from the tray menu.
- Added update logs and a tray shortcut to open them.
- Added support for `Custom Alpha` in the preset menu.
- `Ctrl + Middle Click` now uses the selected preset, including the custom alpha value.
- Reworked the Custom Alpha UI with dark styling, a smoother slider, direct numeric input, and real-time preview.
- Reworked tray menu rendering with a dark owner-drawn style.
- Removed the experimental active-process selection feature.
- Updated README in English to match the current feature set.

## Download

- `SystemTransparency.exe`: signed single-file app.
- `SystemTransparency.zip`: zipped copy of the signed executable.

## Notes

- First-run setup may require UAC.
- Some protected or GPU-accelerated windows may reject transparency changes.
- Auto update only runs from the installed app path.
