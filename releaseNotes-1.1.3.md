# System Transparency 1.1.3

## Hotfix

- Fixed the tray icon right-click menu closing immediately after opening.
- Uses the standard Win32 tray menu focus handoff after `TrackPopupMenu`.
- Keeps popup/context menu transparency conservative so classic menus continue to open reliably.

## Notes

- Windows 11's modern File Explorer context menu can be backed by XAML/composition surfaces, so some system menus may still reject layered-window transparency.
