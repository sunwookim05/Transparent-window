/* Native-menu checks, without installation or registry writes. */
#include <assert.h>
#include "../src/App.c"
static int saves, ticks, mode;
static boolean checked;
static void saveSettings(Settings* s) { (void)s; saves++; }
static void updateWindows(App* app) {
    if (inlineMenuWindow) applyExplorerAutoWindow(inlineMenuWindow, (LPARAM)app);
}
static void CALLBACK checkMenu(HWND owner, UINT msg, UINT_PTR timer, DWORD time) {
    (void)msg; (void)timer; (void)time;
    if (++ticks > 60) { EndMenu(); return; }
    EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)appContext);
    if (!inlineMenuWindow) return;
    assert(appContext->transparency.getWindowAlpha(&appContext->transparency, inlineMenuWindow) == getCurrentAlpha(appContext));
    RECT rect;
    assert(GetMenuItemRect(owner, mode == 1 ? inlineAlphaMenu : inlineKeysMenu, 0, &rect));
    if (mode == 1) {
        POINT top = {rect.left + 40, rect.top + 60}, bottom = {rect.left + 40, rect.bottom - 24};
        ScreenToClient(inlineMenuWindow, &top); ScreenToClient(inlineMenuWindow, &bottom);
        SendMessage(inlineMenuWindow, WM_LBUTTONDOWN, 0, MAKELPARAM(top.x, top.y));
        assert(appContext->settings.customAlpha == 255 && inlineDragging);
        SendMessage(inlineMenuWindow, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(bottom.x, bottom.y));
        SendMessage(inlineMenuWindow, WM_LBUTTONUP, 0, MAKELPARAM(bottom.x, bottom.y));
        assert(appContext->settings.customAlpha == 60 && !inlineDragging);
        assert(appContext->transparency.getWindowAlpha(&appContext->transparency, inlineMenuWindow) == 60);
        SendMessage(inlineMenuWindow, WM_KEYDOWN, VK_UP, 0);
        assert(appContext->settings.customAlpha == 61);
    } else {
        POINT point = {rect.left + 40, rect.top + 40}; ScreenToClient(inlineMenuWindow, &point);
        SendMessage(inlineMenuWindow, WM_LBUTTONDOWN, 0, MAKELPARAM(point.x, point.y));
        SendMessage(inlineMenuWindow, WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
        assert(inlineRecordingAction == HOTKEY_ACTION_APPLY);
        int before = saves;
        SendMessage(owner, WM_TRAY_RECORD, appContext->settings.restoreModifiers, HOTKEY_ACTION_APPLY);
        assert(saves == before && inlineRecordingAction == HOTKEY_ACTION_APPLY);
        SendMessage(owner, WM_TRAY_RECORD, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, HOTKEY_ACTION_APPLY);
        assert(saves == before + 1 && inlineRecordingAction == HOTKEY_ACTION_NONE);
        startInlineRecording(inlineMenuWindow, ID_INLINE_ADJUST);
        appContext->ctrlDown = true;
        MSLLHOOKSTRUCT mouse = {0}; MSG queued;
        assert(mouseHook(HC_ACTION, WM_MOUSEWHEEL, (LPARAM)&mouse) == 1);
        assert(PeekMessage(&queued, owner, WM_TRAY_RECORD, WM_TRAY_RECORD, PM_REMOVE)); DispatchMessage(&queued);
        assert(appContext->settings.adjustModifiers & HOTKEY_MOD_CTRL);
        startInlineRecording(inlineMenuWindow, ID_INLINE_RESTORE);
        KBDLLHOOKSTRUCT key = {0}; key.vkCode = VK_ESCAPE;
        assert(keyboardHook(HC_ACTION, WM_KEYDOWN, (LPARAM)&key) == 1 && inlineRecordingAction == HOTKEY_ACTION_NONE);
        startInlineRecording(inlineMenuWindow, ID_INLINE_RESTORE);
        key.vkCode = VK_MENU;
        assert(keyboardHook(HC_ACTION, WM_SYSKEYDOWN, (LPARAM)&key) == 1 && appContext->altDown);
        inlineRecordingAction = HOTKEY_ACTION_NONE;
        assert(keyboardHook(HC_ACTION, WM_SYSKEYUP, (LPARAM)&key) == 1 && !appContext->altDown && !recordedModifierKeys);
    }
    assert(IsWindow(inlineMenuWindow)); checked = true; EndMenu();
}
int main(void) {
    App app = new_App(); app.settings.reset(&app.settings);
    app.settings.save = saveSettings; app.applyExplorerAutoAll = updateWindows; appContext = &app; ensureUiResources();
    WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "TopLevelWindowForOverflowXamlIsland"; assert(RegisterClassA(&wc));
    HWND overflow = CreateWindowExA(0, wc.lpszClassName, "", WS_POPUP, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    assert(overflow && isPopupMenuWindow(overflow));
    app.settings.preset = PRESET_CUSTOM; app.settings.customAlpha = 60; applyPopupTransparency(&app, overflow);
    assert(app.tracker.windows[0].originalAlpha == 255);
    app.settings.customAlpha = 230; applyExplorerAutoWindow(overflow, (LPARAM)&app);
    assert(app.transparency.getWindowAlpha(&app.transparency, overflow) == 230);
    app.tracker.restoreAll(&app.tracker, &app.transparency);
    assert(!(GetWindowLong(overflow, GWL_EXSTYLE) & WS_EX_LAYERED));
    SetWindowLong(overflow, GWL_EXSTYLE, GetWindowLong(overflow, GWL_EXSTYLE) | WS_EX_LAYERED);
    assert(SetLayeredWindowAttributes(overflow, RGB(1,2,3), 180, LWA_ALPHA | LWA_COLORKEY));
    assert(applyTrackedAlpha(&app, overflow, 60)); assert(app.tracker.windows[0].originalAlpha == 180);
    app.tracker.restoreAll(&app.tracker, &app.transparency);
    BYTE alpha; DWORD flags; COLORREF key;
    assert(GetLayeredWindowAttributes(overflow, &key, &alpha, &flags));
    assert(alpha == 180 && flags == (LWA_ALPHA | LWA_COLORKEY) && key == RGB(1,2,3)); DestroyWindow(overflow);
    wc.lpfnWndProc = trayWindowProc; wc.lpszClassName = "InlineMenuTestOwner"; assert(RegisterClassA(&wc));
    app.trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL); assert(app.trayWindow);
    for (mode = 1; mode <= 2; mode++) {
        resetMenuItems(); inlineAlphaMenu = CreatePopupMenu(); inlineKeysMenu = CreatePopupMenu();
        appendDarkMenu(inlineAlphaMenu, MF_STRING, ID_INLINE_ALPHA, L"", false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_APPLY, tr(STR_HOTKEY_APPLY), false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_RESTORE, tr(STR_HOTKEY_RESTORE), false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_ADJUST, tr(STR_HOTKEY_ADJUST), false, false);
        ticks = 0; checked = false; inlineMenuWindow = NULL;
        SetTimer(app.trayWindow, 900, 30, checkMenu);
        TrackPopupMenu(mode == 1 ? inlineAlphaMenu : inlineKeysMenu, TPM_RETURNCMD | TPM_NONOTIFY, 100, 100, 0, app.trayWindow, NULL);
        KillTimer(app.trayWindow, 900); EnumThreadWindows(GetCurrentThreadId(), detachInlineMenus, 0);
        assert(checked); DestroyMenu(inlineAlphaMenu); DestroyMenu(inlineKeysMenu); inlineAlphaMenu = inlineKeysMenu = NULL;
    }
    app.tracker.restoreAll(&app.tracker, &app.transparency); DestroyWindow(app.trayWindow);
    puts("Native inline menus, opacity updates, recording and restoration checks passed."); return 0;
}
