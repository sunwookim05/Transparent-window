# System Transparency 1.1.6

## Highlights

- Split the tray popup into separate Opacity and Shortcuts menus with Back navigation.
- Added a dark vertical opacity slider with a live percentage display (24–100%). Drag upward for more opacity or downward for more transparency; arrow keys adjust precisely.
- Made shortcut combination text directly clickable to start recording, replacing separate Record buttons.
- Save opacity and completed shortcut recordings immediately. Explorer windows update when automatic transparency is enabled.
- Reject duplicate Apply/Restore combinations and keep recording active so another combination can be entered.
- Added keyboard focus indicators, Tab navigation, Escape cancellation, and automatic dismissal when recording is inactive.
- Kept existing settings, language options, update checks, and uninstall actions accessible through More.
- Updated English and Korean usage documentation and added isolated Win32 interaction checks.

## Usage

Right-click the tray icon, then choose Opacity or Shortcuts. In Shortcuts, click a combination, hold modifier keys, and middle-click for Apply/Restore or scroll for Adjust. Click the combination again or press Escape to cancel. Use Back to return to the first menu.

## Notes

- Opacity still ranges from 60 to 255 internally and is shown as a rounded percentage.
- Popup/system-menu transparency remains best effort; some Windows 11 composition surfaces may reject it.
