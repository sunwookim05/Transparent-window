# System Transparency 1.1.10

## Fixes

- Fixed interactive submenu detection in the actual tray context-menu hierarchy. Native menu notifications are now enabled, and menu attachment/transparency updates use a timer callback that runs during the menu loop.
- Keep the Custom Alpha submenu open when clicking or dragging the vertical slider.
- Keep the Hotkeys submenu open when clicking any of the three shortcut combinations to record a new shortcut.
- Apply the selected opacity to the app's root tray menu and all open submenus, independently of the external popup transparency setting.
- Retain direct alpha values from 60 to 255, immediate saving, recording cancellation, and shortcut conflict checks.
- Updated English and Korean documentation.

## Validation

Build and regression tests passed through the production tray menu: navigate Setting > Preset > Custom Alpha and Setting > Hotkeys, inject actual mouse clicks/drags, test all three shortcut rows, and verify opacity on visible tray menus with external popup transparency both enabled and disabled. Window-state restoration checks also passed.
