# System Transparency 1.0.4

## Highlights

- Added an uninstall option to the tray settings menu.
- Added a dark custom confirmation dialog for uninstall.
- Centered the Custom Alpha dialog and uninstall confirmation dialog on screen.
- The uninstall confirmation dialog now uses the current transparency preset.

## What Changed Since 1.0.3

- Added `Setting > Uninstall`.
- Uninstall now asks `Are you sure?` before removing the app.
- Uninstall removes:
  - the installed executable
  - the startup scheduled task
  - app registry settings
  - bundled certificate registrations for `sunwookim05`
- Uninstall now deletes only the executable files and leaves the install directory alone.
- Removed the unfinished context-menu transparency experiment from this release path.

## Download

- `SystemTransparency.exe`: signed single-file app.
- `SystemTransparency.zip`: zipped copy of the signed executable.

## Notes

- First-run setup may require UAC.
- Uninstall runs a temporary batch file after the app exits so the running executable can be deleted.
- Some protected or GPU-accelerated windows may reject transparency changes.
