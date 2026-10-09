# System Transparency 1.1.9

## Fixes

- Fixed Custom Alpha slider and shortcut clicks dismissing the tray menu. Interactive menu input is now intercepted before the native menu loop processes it.
- Kept native menu capture during slider dragging instead of replacing and releasing it.
- Display the actual alpha value from 60 to 255, with endpoint labels, instead of a percentage. Glass displays 150, Soft 200, Ghost 80, and Solid 255.
- Preserve the existing `Setting > Preset > Custom Alpha` and `Setting > Hotkeys` submenu structure, immediate saving, shortcut conflict checks, and Escape cancellation.
- Updated English and Korean documentation.

## Validation

Build and native-menu interaction tests passed using actual injected mouse clicks and drags, including menu persistence, shortcut recording, cancellation, opacity updates, and original window-state restoration. The interaction test needs access to an interactive Windows desktop.
