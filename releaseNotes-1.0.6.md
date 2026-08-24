# System Transparency 1.0.6

## Highlights

- Fixed configurable hotkey recording and custom hotkey activation.

## What Changed Since 1.0.5

- Hotkey matching now uses the key state tracked by the low-level keyboard hook instead of relying only on `GetAsyncKeyState`.
- `Shift`, `Alt`, `Ctrl`, and `Win` are now all tracked consistently for both recording and activation.
- Custom modifier combinations such as `Shift + Win + Mouse Wheel` now replace the default opacity-adjust shortcut.

## Download

- `SystemTransparency.exe`: signed single-file app.
- `SystemTransparency.zip`: zipped copy of the signed executable.

## Notes

- First-run setup may require UAC.
- Some protected or GPU-accelerated windows may reject transparency changes.
