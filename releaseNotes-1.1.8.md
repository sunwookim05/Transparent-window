# System Transparency 1.1.8

## Changes

- Adjust Custom Alpha directly inside `Setting > Preset > Custom Alpha`, using a vertical slider in the native tray submenu.
- Record shortcuts directly inside `Setting > Hotkeys` by clicking the combination text. No separate settings window or Record button is required.
- Keep the menu open while dragging the slider or recording a shortcut. Escape cancels recording; duplicate Apply/Restore combinations are rejected.
- Support Up/Down for precise opacity adjustment and Home/End for the slider endpoints.
- Include the app's own tray menus and recognized hidden-tray-icon overflow hosts in popup transparency handling.
- Update recognized popup/overflow targets when the selected opacity changes, including raising opacity after lowering it.
- Capture original window opacity and layered attributes before applying changes. Restore tracked windows on normal application exit.
- Correct destroyed-window cleanup and avoid popup frame refreshes that interfere with layered opacity.
- Update English and Korean documentation and add real native-menu interaction and restoration checks.

## Behavior changes

Custom Alpha changes save immediately and update enabled automatic transparency targets. The previous standalone numeric input, selected-window preview, and OK/Cancel rollback controls have been removed. Opacity remains 60–255 internally and is displayed as a rounded 24–100% value.

## Validation and limitations

Build and native Win32 menu tests passed for dragging, opacity changes, shortcut recording, cancellation, conflict checks, and original alpha/color-key/style restoration. Modern Windows 11 composition surfaces can still reject layered transparency; the real hidden-icon overflow surface needs confirmation on the target desktop. Restoration applies to normal application exit.
