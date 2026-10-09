# System Transparency 1.1.1

## Highlights

- Refined tray menu styling with a cleaner dark owner-drawn layout.
- Added popup menu transparency with a new on/off setting enabled by default.
- Added best-effort Windows context menu transparency, including retry handling for Windows 11 popup timing.
- Polished custom alpha, hotkey, and uninstall dialogs with shared dark UI resources.

## Notes

- Windows 11's modern File Explorer context menu can be backed by XAML/composition surfaces, so some system menus may still reject layered-window transparency.
