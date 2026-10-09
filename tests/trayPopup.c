/* Native-menu checks, without installation or registry writes. */
#include <assert.h>
#include "../src/App.c"
static int saves, ticks, mode;
static boolean checked;
static int inputStage;
static boolean selectedPopup;
static HMENU lastOpenedMenu, waitingMenu;
static int navigationLevel;
static LRESULT CALLBACK testOwnerProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_INITMENUPOPUP) lastOpenedMenu = (HMENU)w;
    if (msg == WM_MENUSELECT) selectedPopup = (HIWORD(w) & MF_POPUP) != 0;
    return trayWindowProc(hwnd, msg, w, l);
}
static void pressMenuKey(WORD key) {
    LPARAM flags = 1 | ((LPARAM)MapVirtualKey(key, MAPVK_VK_TO_VSC) << 16);
    assert(PostMessage(appContext->trayWindow, WM_KEYDOWN, key, flags));
}
static void clickShortcutRow(HWND owner, UINT index) {
    RECT rect; assert(GetMenuItemRect(owner, inlineKeysMenu, index, &rect));
    INPUT pointer[2] = {0};
    pointer[0].type = pointer[1].type = INPUT_MOUSE;
    pointer[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    pointer[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SetCursorPos(rect.left + 40, rect.top + 40);
    assert(SendInput(2, pointer, sizeof(INPUT)) == 2);
}
static void saveSettings(Settings* s) { (void)s; saves++; }
static void updateWindows(App* app) {
    if (inlineMenuWindow) applyExplorerAutoWindow(inlineMenuWindow, (LPARAM)app);
}
static BOOL CALLBACK checkVisibleMenuAlpha(HWND hwnd, LPARAM param) {
    App* app = (App*)param;
    if (IsWindowVisible(hwnd) && isOwnTrayMenu(hwnd))
        assert(app->transparency.getWindowAlpha(&app->transparency, hwnd) == getCurrentAlpha(app));
    return true;
}
static void CALLBACK checkMenu(HWND owner, UINT msg, UINT_PTR timer, DWORD time) {
    (void)msg; (void)timer; (void)time;
    if (++ticks > 60) { EndMenu(); return; }
    EnumThreadWindows(GetCurrentThreadId(), checkVisibleMenuAlpha, (LPARAM)appContext);
    /* Navigate the production root -> Setting -> Preset/Hotkeys hierarchy. */
    if (!inlineMenuWindow) {
        if (waitingMenu) {
            if (lastOpenedMenu == waitingMenu) return;
            waitingMenu = NULL;
        }
        UINT target = navigationLevel == 0 ? 3 : navigationLevel == 1 ? (mode == 1 ? 7 : 3) : 4;
        if (selectedPopup && selectedInlineMenu == lastOpenedMenu && selectedInlineItem == target) {
            waitingMenu = lastOpenedMenu;
            navigationLevel++;
            pressMenuKey(VK_RETURN);
        } else {
            pressMenuKey(VK_DOWN);
        }
        return;
    }
    assert(appContext->transparency.getWindowAlpha(&appContext->transparency, inlineMenuWindow) == getCurrentAlpha(appContext));
    RECT rect;
    assert(GetMenuItemRect(owner, mode == 1 ? inlineAlphaMenu : inlineKeysMenu, 0, &rect));
    if (mode == 1) {
        POINT top = {rect.left + 40, rect.top + 60}, bottom = {rect.left + 40, rect.bottom - 24};
        INPUT pointer = {0}; pointer.type = INPUT_MOUSE;
        if (inputStage == 0) {
            SetCursorPos(top.x, top.y);
            pointer.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            assert(SendInput(1, &pointer, sizeof(pointer)) == 1);
            inputStage = 1;
            return;
        }
        if (inputStage == 1) {
            assert(appContext->settings.customAlpha == 255 && inlineDragging);
            SetCursorPos(bottom.x, bottom.y);
            pointer.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            assert(SendInput(1, &pointer, sizeof(pointer)) == 1);
            inputStage = 2;
            return;
        }
        assert(appContext->settings.customAlpha == 60 && !inlineDragging);
        assert(appContext->transparency.getWindowAlpha(&appContext->transparency, inlineMenuWindow) == 60);
        SendMessage(inlineMenuWindow, WM_KEYDOWN, VK_UP, 0);
        assert(appContext->settings.customAlpha == 61);
    } else {
        if (inputStage == 0) {
            clickShortcutRow(owner, 0);
            inputStage = 1;
            return;
        }
        if (inputStage == 2) {
            assert(inlineRecordingAction == HOTKEY_ACTION_RESTORE);
            inlineRecordingAction = HOTKEY_ACTION_NONE;
            clickShortcutRow(owner, 2);
            inputStage = 3;
            return;
        }
        if (inputStage == 3) {
            assert(inlineRecordingAction == HOTKEY_ACTION_ADJUST);
            inlineRecordingAction = HOTKEY_ACTION_NONE;
            checked = true; EndMenu(); return;
        }
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
        clickShortcutRow(owner, 1);
        inputStage = 2;
        return;
    }
    assert(IsWindow(inlineMenuWindow)); checked = true; EndMenu();
}
int main(void) {
    POINT originalCursor; GetCursorPos(&originalCursor);
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
    wc.lpfnWndProc = testOwnerProc; wc.lpszClassName = "InlineMenuTestOwner"; assert(RegisterClassA(&wc));
    app.trayWindow = CreateWindowA(wc.lpszClassName, "Menu interaction check", WS_OVERLAPPEDWINDOW, 100, 100, 160, 100, NULL, NULL, wc.hInstance, NULL); assert(app.trayWindow);
    ShowWindow(app.trayWindow, SW_SHOW);
    SetForegroundWindow(app.trayWindow);
    app.mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHook, GetModuleHandle(NULL), 0);
    assert(app.mouseHook);
    for (mode = 1; mode <= 2; mode++) {
        app.settings.popupTransparency = mode == 1;
        ticks = 0; inputStage = 0; checked = false; inlineMenuWindow = NULL; inlineActiveMenu = NULL;
        selectedPopup = false; lastOpenedMenu = waitingMenu = NULL; navigationLevel = 0;
        SetCursorPos(100, 100);
        SetTimer(app.trayWindow, 900, 30, checkMenu);
        SendMessage(app.trayWindow, WM_TRAY, 0, WM_RBUTTONUP);
        KillTimer(app.trayWindow, 900); EnumThreadWindows(GetCurrentThreadId(), detachInlineMenus, 0);
        assert(checked);
    }
    UnhookWindowsHookEx(app.mouseHook);
    SetCursorPos(originalCursor.x, originalCursor.y);
    app.tracker.restoreAll(&app.tracker, &app.transparency); DestroyWindow(app.trayWindow);
    puts("Native inline menus, opacity updates, recording and restoration checks passed."); return 0;
}
