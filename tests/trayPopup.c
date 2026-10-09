/* Standalone Win32 interaction checks; does not install the app or write settings. */
#include <assert.h>
#include "../src/App.c"

static int saves;

static void saveTestSettings(Settings* settings) {
    (void)settings;
    saves++;
}
static void applyTestWindows(App* app) {
    (void)app;

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
    file = fopen("build/trayPopup.keys.preview.bmp", "wb");
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

    KBDLLHOOKSTRUCT key = {0};
    MSLLHOOKSTRUCT mouse = {0};
    MSG msg;
    app.settings.reset(&app.settings);
    app.settings.save = saveTestSettings;
    app.applyExplorerAutoAll = applyTestWindows;
    appContext = &app;
    InitCommonControlsEx(&icc);
    /* The original Custom Alpha panel keeps live preview and explicit confirmation. */
    wc.lpfnWndProc = alphaWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = L"CustomAlphaTest";
    assert(RegisterClassW(&wc));
    alphaDialogValue = 150;
    alphaDialogOk = false;
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 260, 285, null, null, wc.hInstance, null);
    assert(popup);
    assert(GetDlgItem(popup, ID_ALPHA_SLIDER));
    assert(GetDlgItem(popup, ID_ALPHA_OK) && GetDlgItem(popup, ID_ALPHA_CANCEL));
    SendMessage(GetDlgItem(popup, ID_ALPHA_SLIDER), WM_LBUTTONDOWN, 0, MAKELPARAM(20, 0));
    assert(alphaDialogValue == 255);
    SendMessage(GetDlgItem(popup, ID_ALPHA_SLIDER), WM_LBUTTONUP, 0, MAKELPARAM(20, 1000));
    assert(alphaDialogValue == 60);
    SendMessage(popup, WM_COMMAND, ID_ALPHA_CANCEL, 0);
    assert(!IsWindow(popup) && !alphaDialogOk && saves == 0);
    wc.lpfnWndProc = trayPopupProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = L"TrayPopupTest";
    assert(RegisterClassW(&wc));
    popup = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
        0, 0, 436, 432, null, null, wc.hInstance, null);
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
    assert(saves == 0 && hotkeyRecordingAction == HOTKEY_ACTION_APPLY);
    SendMessage(popup, WM_TRAY_RECORD, HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT, HOTKEY_ACTION_APPLY);
    assert(app.settings.applyModifiers == (HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT));
    assert(saves == 1 && hotkeyRecordingAction == HOTKEY_ACTION_NONE);

    /* Hooks queue UI work, consume the recording gesture and cancel with Escape. */
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_ADJUST_LABEL, 0);
    app.ctrlDown = true;
    assert(mouseHook(HC_ACTION, WM_MOUSEWHEEL, (LPARAM)&mouse) == 1);
    assert(PeekMessage(&msg, popup, WM_TRAY_RECORD, WM_TRAY_RECORD, PM_REMOVE));
    DispatchMessage(&msg);
    assert(app.settings.adjustModifiers & HOTKEY_MOD_CTRL);
    assert(saves == 2 && hotkeyRecordingAction == HOTKEY_ACTION_NONE);
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_RESTORE_LABEL, 0);
    key.vkCode = VK_ESCAPE;
    assert(keyboardHook(HC_ACTION, WM_KEYDOWN, (LPARAM)&key) == 1);
    assert(PeekMessage(&msg, popup, WM_COMMAND, WM_COMMAND, PM_REMOVE));
    DispatchMessage(&msg);
    assert(IsWindow(popup) && hotkeyRecordingAction == HOTKEY_ACTION_NONE && saves == 2);

    SendMessage(popup, WM_COMMAND, ID_HOTKEY_APPLY_LABEL, 0);
    SendMessage(popup, WM_ACTIVATE, WA_INACTIVE, 0);
    assert(!PeekMessage(&msg, popup, WM_CLOSE, WM_CLOSE, PM_REMOVE));
    SendMessage(popup, WM_COMMAND, ID_HOTKEY_APPLY_LABEL, 0);
    assert(hotkeyRecordingAction == HOTKEY_ACTION_NONE);
    SendMessage(popup, WM_TRAY_RECORD, HOTKEY_MOD_WIN, HOTKEY_ACTION_APPLY);
    assert(saves == 2); /* Stale recording messages cannot change settings. */
    SendMessage(popup, WM_ACTIVATE, WA_INACTIVE, 0);
    assert(PeekMessage(&msg, popup, WM_CLOSE, WM_CLOSE, PM_REMOVE));
    DispatchMessage(&msg);
    assert(!IsWindow(popup) && !trayPopupWindow && !hotkeyDialogWindow);

    puts("Tray popup interaction checks passed.");
    return 0;
}
