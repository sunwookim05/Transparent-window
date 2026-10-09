/* Standalone Win32 interaction checks; does not install the app or write settings. */
#include <assert.h>
#include "../src/App.c"

static int saves;
static int autoApplications;
static void saveTestSettings(Settings* settings) {
    (void)settings;
    saves++;
}
static void applyTestWindows(App* app) {
    (void)app;
    autoApplications++;
}

static void savePreview(HWND popup) {
    RECT rect;
    BITMAPINFO info = {0};
    BITMAPFILEHEADER header = {0};
    void* pixels;
    HDC screen = GetDC(popup);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap;
    HGDIOBJ previous;
    FILE* file;
    GetClientRect(popup, &rect);
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, null, 0);
    assert(bitmap);
    previous = SelectObject(dc, bitmap);
    SendMessage(popup, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + rect.right * rect.bottom * 4;
    file = fopen(trayPopupPage == TRAY_PAGE_HOME ? "build/trayPopup.home.preview.bmp" :
        trayPopupPage == TRAY_PAGE_ALPHA ? "build/trayPopup.alpha.preview.bmp" : "build/trayPopup.keys.preview.bmp", "wb");
    assert(file);
    fwrite(&header, sizeof(header), 1, file);
    fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
    fwrite(pixels, rect.right * rect.bottom * 4, 1, file);
    fclose(file);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(popup, screen);
}

int main(int argc, char** argv) {
    App app = new_App();
    WNDCLASSW wc = {0};
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    HWND popup;
    HWND slider;
    KBDLLHOOKSTRUCT key = {0};
    MSLLHOOKSTRUCT mouse = {0};
    MSG msg;
    app.settings.reset(&app.settings);
    app.settings.save = saveTestSettings;
    app.applyExplorerAutoAll = applyTestWindows;
    appContext = &app;
    InitCommonControlsEx(&icc);
    wc.lpfnWndProc = trayPopupProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = L"TrayPopupTest";
    assert(RegisterClassW(&wc));
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 224, 432, null, null, wc.hInstance, (LPVOID)TRAY_PAGE_ALPHA);
    assert(popup);
    trayPopupWindow = popup;
    if (argc > 1 && strcmp(argv[1], "--preview") == 0)
        savePreview(popup);
    slider = GetDlgItem(popup, ID_ALPHA_SLIDER);
    assert(slider && GetDlgItem(popup, ID_TRAY_BACK));
    assert(!GetDlgItem(popup, ID_HOTKEY_APPLY_LABEL));
    assert(SendMessage(slider, TBM_GETRANGEMIN, 0, 0) == 60);
    assert(SendMessage(slider, TBM_GETRANGEMAX, 0, 0) == 255);
    assert(GetWindowLong(slider, GWL_STYLE) & TBS_VERT);
    SendMessage(slider, TBM_SETPOS, true, 315 - 123);
    SendMessage(popup, WM_VSCROLL, TB_THUMBTRACK, (LPARAM)slider);
    assert(app.settings.preset == PRESET_CUSTOM && app.settings.customAlpha == 123);
    assert(saves == 1 && autoApplications == 1);
    SendMessage(popup, WM_VSCROLL, TB_ENDTRACK, (LPARAM)slider);
    assert(saves == 1);

    DestroyWindow(popup);
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 436, 432, null, null, wc.hInstance, (LPVOID)TRAY_PAGE_KEYS);
    assert(popup);
    trayPopupWindow = popup;
    assert(!GetDlgItem(popup, ID_ALPHA_SLIDER));
    assert(!GetDlgItem(popup, ID_HOTKEY_APPLY_RECORD));
    assert(GetWindowLong(GetDlgItem(popup, ID_HOTKEY_APPLY_LABEL), GWL_STYLE) & WS_TABSTOP);
    if (argc > 1 && strcmp(argv[1], "--preview") == 0)
        savePreview(popup);

    SendMessage(GetDlgItem(popup, ID_HOTKEY_APPLY_LABEL), BM_CLICK, 0, 0);
    assert(hotkeyRecordingAction == HOTKEY_ACTION_APPLY);
    SendMessage(popup, WM_TRAY_RECORD, app.settings.restoreModifiers, HOTKEY_ACTION_APPLY);
    assert(saves == 1 && hotkeyRecordingAction == HOTKEY_ACTION_APPLY);
    SendMessage(popup, WM_TRAY_RECORD, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, HOTKEY_ACTION_APPLY);
    assert(app.settings.applyModifiers == (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT));
    assert(saves == 2 && hotkeyRecordingAction == HOTKEY_ACTION_NONE);

    /* Hooks queue UI work, consume the recording gesture and cancel with Escape. */
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_ADJUST_LABEL, 0);
    app.ctrlDown = true;
    assert(mouseHook(HC_ACTION, WM_MOUSEWHEEL, (LPARAM)&mouse) == 1);
    assert(PeekMessage(&msg, popup, WM_TRAY_RECORD, WM_TRAY_RECORD, PM_REMOVE));
    DispatchMessage(&msg);
    assert(app.settings.adjustModifiers & HOTKEY_MOD_CTRL);
    assert(saves == 3 && hotkeyRecordingAction == HOTKEY_ACTION_NONE);
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_RESTORE_LABEL, 0);
    key.vkCode = VK_ESCAPE;
    assert(keyboardHook(HC_ACTION, WM_KEYDOWN, (LPARAM)&key) == 1);
    assert(PeekMessage(&msg, popup, WM_COMMAND, WM_COMMAND, PM_REMOVE));
    DispatchMessage(&msg);
    assert(IsWindow(popup) && hotkeyRecordingAction == HOTKEY_ACTION_NONE && saves == 3);

    SendMessage(popup, WM_COMMAND, ID_HOTKEY_APPLY_LABEL, 0);
    SendMessage(popup, WM_ACTIVATE, WA_INACTIVE, 0);
    assert(!PeekMessage(&msg, popup, WM_CLOSE, WM_CLOSE, PM_REMOVE));
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_APPLY_LABEL, 0);
    assert(hotkeyRecordingAction == HOTKEY_ACTION_NONE);
    SendMessage(popup, WM_TRAY_RECORD, HOTKEY_MOD_WIN, HOTKEY_ACTION_APPLY);
    assert(saves == 3); /* Stale recording messages cannot change settings. */
    SendMessage(popup, WM_ACTIVATE, WA_INACTIVE, 0);
    assert(PeekMessage(&msg, popup, WM_CLOSE, WM_CLOSE, PM_REMOVE));
    DispatchMessage(&msg);
    assert(!IsWindow(popup) && !trayPopupWindow && !hotkeyDialogWindow);

    app.trayWindow = CreateWindowW(L"STATIC", L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, null, null, null);
    assert(app.trayWindow);
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 224, 432, app.trayWindow, null, wc.hInstance, (LPVOID)TRAY_PAGE_ALPHA);
    assert(popup);
    trayPopupWindow = popup;
    assert(SendMessage(GetDlgItem(popup, ID_ALPHA_SLIDER), TBM_GETPOS, 0, 0) == 315 - 123);
    slider = GetDlgItem(popup, ID_ALPHA_SLIDER);
    SendMessage(slider, TBM_SETPOS, true, 60);
    SendMessage(popup, WM_VSCROLL, TB_THUMBTRACK, (LPARAM)slider);
    assert(app.settings.customAlpha == 255);
    SendMessage(slider, TBM_SETPOS, true, 255);
    SendMessage(popup, WM_VSCROLL, TB_THUMBTRACK, (LPARAM)slider);
    assert(app.settings.customAlpha == 60);
    SendMessage(slider, WM_KEYDOWN, VK_UP, 0);
    assert(app.settings.customAlpha == 61);
    SendMessage(slider, WM_KEYDOWN, VK_DOWN, 0);
    assert(app.settings.customAlpha == 60);
    DestroyWindow(popup);
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 436, 248, app.trayWindow, null, wc.hInstance, (LPVOID)TRAY_PAGE_HOME);
    assert(popup);
    trayPopupWindow = popup;
    assert(GetDlgItem(popup, ID_TRAY_ALPHA_MENU) && GetDlgItem(popup, ID_TRAY_KEYS_MENU));
    assert(!GetDlgItem(popup, ID_ALPHA_SLIDER) && !GetDlgItem(popup, ID_HOTKEY_APPLY_LABEL));
    if (argc > 1 && strcmp(argv[1], "--preview") == 0)
        savePreview(popup);
    SendMessage(popup, WM_COMMAND, ID_TRAY_MORE, 0);
    assert(!IsWindow(popup) && !trayPopupWindow && !hotkeyDialogWindow);
    assert(PeekMessage(&msg, app.trayWindow, WM_TRAY_MORE, WM_TRAY_MORE, PM_REMOVE));
    DestroyWindow(app.trayWindow);
    puts("Tray popup interaction checks passed.");
    return 0;
}
