# System Transparency 1.1.11

- Keep Custom Alpha and Hotkeys menus open while clicking, dragging, and recording shortcuts by handling input inside the native menu loop.
- Apply tray menu opacity immediately on opening, including when external popup transparency is disabled.
- Refresh transparency when external popup menus reopen.
- Include the corrected executable and increment the version so existing installations receive the fixes through Check for Updates.

Validation: Windows tray interaction tests passed with actual mouse navigation, slider clicks and drags, all three shortcut recording rows, and without the global mouse hook. Idle menu opacity and window restoration checks passed.
