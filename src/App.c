#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <commctrl.h>

#include "App.h"
#include "Installer.h"
#include "Updater.h"

#define WM_TRAY (WM_USER + 1)
#define WM_TRAY_MORE (WM_USER + 2)
#define WM_TRAY_RECORD (WM_USER + 3)
#define ID_TRAY_MORE 1301
#define ID_TRAY_HINT 1302
#define ID_TRAY_ALPHA_TITLE 1303
#define ID_TRAY_HOTKEY_TITLE 1304
#define ID_TRAY_ROW_TITLE 1310
#define ID_TRAY_ALPHA_MENU 1320
#define ID_TRAY_KEYS_MENU 1321
#define ID_TRAY_BACK 1322
#define TRAY_PAGE_HOME 0
#define TRAY_PAGE_ALPHA 1
#define TRAY_PAGE_KEYS 2
#define TRAY_ID 1
#define TRAY_RETRY_TIMER_ID 100
#define TRAY_STATUS_TIMER_ID 101

#define ID_SETTING_EXPLORER  10
#define ID_SETTING_STARTUP   11
#define ID_SETTING_HOTKEYS   12
#define ID_SETTING_FOLDER    13
#define ID_SETTING_RESET     14
#define ID_SETTING_UNINSTALL 15
#define ID_SETTING_LANGUAGE  16
#define ID_SETTING_POPUPS    17

#define ID_PRESET_SOLID     20
#define ID_PRESET_SOFT      21
#define ID_PRESET_GLASS     22
#define ID_PRESET_GHOST     23
#define ID_PRESET_CUSTOM    24

#define ID_UPDATE_CHECK     30
#define ID_LOG_OPEN         31

#define ID_LANG_SYSTEM      40
#define ID_LANG_ENGLISH     41
#define ID_LANG_KOREAN      42

#define ID_ALPHA_EDIT       1001
#define ID_ALPHA_SLIDER     1002
#define ID_ALPHA_OK         1003
#define ID_ALPHA_CANCEL     1004
#define ID_CONFIRM_YES      1101
#define ID_CONFIRM_NO       1102
#define ID_HOTKEY_APPLY_LABEL    1201
#define ID_HOTKEY_RESTORE_LABEL  1202
#define ID_HOTKEY_ADJUST_LABEL   1203
#define ID_HOTKEY_APPLY_RECORD   1211
#define ID_HOTKEY_RESTORE_RECORD 1212
#define ID_HOTKEY_ADJUST_RECORD  1213
#define ID_HOTKEY_OK             1221
#define ID_HOTKEY_CANCEL         1222

#define HOTKEY_ACTION_NONE    0
#define HOTKEY_ACTION_APPLY   1
#define HOTKEY_ACTION_RESTORE 2
#define HOTKEY_ACTION_ADJUST  3

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19

#define UI_BG RGB(18, 18, 20)
#define UI_PANEL RGB(30, 30, 34)
#define UI_PANEL_HOVER RGB(42, 42, 48)
#define UI_BORDER RGB(64, 64, 72)
#define UI_TEXT RGB(242, 242, 246)
#define UI_MUTED RGB(170, 170, 178)
#define UI_ACCENT RGB(94, 174, 255)
#define UI_MENU_BG RGB(22, 22, 25)
#define UI_MENU_HOVER RGB(44, 44, 50)
#define UI_MENU_TEXT RGB(238, 238, 242)

static App* appContext = null;
static BYTE alphaDialogValue = 150;
static boolean alphaDialogOk = false;
static HWND alphaPreviewWindow = null;
static BYTE alphaPreviewOriginal = ALPHA_OPAQUE;
static boolean alphaControlsUpdating = false;
static HBRUSH alphaDarkBrush = null;
static HBRUSH alphaEditBrush = null;
static HFONT uiFont = null;
static boolean confirmDialogOk = false;
static BYTE confirmDialogAlpha = ALPHA_OPAQUE;
static boolean statusMenuOpen = false;
static HWND hotkeyDialogWindow = null;
static DWORD hotkeyDialogApplyModifiers = HOTKEY_MOD_CTRL;
static DWORD hotkeyDialogRestoreModifiers = HOTKEY_MOD_WIN;
static DWORD hotkeyDialogAdjustModifiers = HOTKEY_MOD_CTRL | HOTKEY_MOD_WIN;
static int hotkeyRecordingAction = HOTKEY_ACTION_NONE;

static HWND trayPopupWindow = null;
static HFONT trayHeadingFont = null;
static HFONT trayValueFont = null;
static HBRUSH trayPanelBrush = null;
static int trayPopupPage = TRAY_PAGE_HOME;

typedef struct {
    WCHAR text[160];
    boolean checked;
    boolean submenu;
    boolean separator;
} MenuItemData;

static MenuItemData menuItems[64];
static int menuItemCount = 0;

static void resetMenuItems(void) {
    menuItemCount = 0;
}

static MenuItemData* newMenuItem(const WCHAR* text, boolean checked, boolean submenu, boolean separator) {
    MenuItemData* item;

    if (menuItemCount >= 64)
        return null;

    item = &menuItems[menuItemCount++];
    lstrcpynW(item->text, text, sizeof(item->text) / sizeof(item->text[0]));
    item->checked = checked;
    item->submenu = submenu;
    item->separator = separator;
    return item;
}

static void appendDarkMenu(HMENU menu, UINT flags, UINT_PTR id, const WCHAR* text, boolean checked, boolean submenu) {
    AppendMenuW(menu, flags | MF_OWNERDRAW, id, (LPCWSTR)newMenuItem(text, checked, submenu, false));
}

static void appendDarkSeparator(HMENU menu) {
    AppendMenuW(menu, MF_OWNERDRAW | MF_DISABLED, 0, (LPCWSTR)newMenuItem(L"", false, false, true));
}

typedef enum {
    STR_APP_NAME,
    STR_LICENSE,
    STR_SETTING,
    STR_EXPLORER_AUTO,
    STR_POPUP_TRANSPARENCY,
    STR_RUN_AT_STARTUP,
    STR_HOTKEYS,
    STR_LANGUAGE,
    STR_LANGUAGE_SYSTEM,
    STR_LANGUAGE_ENGLISH,
    STR_LANGUAGE_KOREAN,
    STR_OPEN_INSTALL_FOLDER,
    STR_RESET_SETTINGS,
    STR_UNINSTALL,
    STR_PRESET,
    STR_PRESET_SOLID,
    STR_PRESET_SOFT,
    STR_PRESET_GLASS,
    STR_PRESET_GHOST,
    STR_CUSTOM_ALPHA,
    STR_CHECK_FOR_UPDATES,
    STR_OPEN_LOG,
    STR_DEVELOPED_BY,
    STR_EXIT,
    STR_ALREADY_CURRENT,
    STR_UPDATE_FAILED,
    STR_UPDATE_SKIPPED,
    STR_ALPHA_VALUE,
    STR_OK,
    STR_CANCEL,
    STR_ALPHA_RANGE,
    STR_CONFIRM_TITLE,
    STR_CONFIRM_QUESTION,
    STR_YES,
    STR_NO,
    STR_UNINSTALL_FAILED,
    STR_HOTKEY_APPLY,
    STR_HOTKEY_RESTORE,
    STR_HOTKEY_ADJUST,
    STR_RECORD,
    STR_HOLD_MIDDLE,
    STR_HOLD_WHEEL,
    STR_HOTKEY_CONFLICT,
    STR_HOTKEY_LIMIT,
    STR_MIDDLE_CLICK,
    STR_MOUSE_WHEEL,
    STR_NONE
} StringId;

static LanguageMode effectiveLanguage(App* self) {
    LANGID lang;

    if (self && self->settings.language != LANGUAGE_SYSTEM)
        return self->settings.language;

    lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_KOREAN ? LANGUAGE_KOREAN : LANGUAGE_ENGLISH;
}

static const WCHAR* textFor(LanguageMode language, StringId id) {
    boolean ko = language == LANGUAGE_KOREAN;

    switch (id) {
        case STR_APP_NAME: return L"System Transparency";
        case STR_LICENSE: return ko ? L"MIT 라이선스" : L"Licensed under MIT";
        case STR_SETTING: return ko ? L"설정" : L"Setting";
        case STR_EXPLORER_AUTO: return ko ? L"Explorer 자동 투명화" : L"Explorer Auto Transparency";
        case STR_POPUP_TRANSPARENCY: return ko ? L"팝업 메뉴 투명화" : L"Popup Menu Transparency";
        case STR_RUN_AT_STARTUP: return ko ? L"시작 시 실행" : L"Run at Startup";
        case STR_HOTKEYS: return ko ? L"단축키..." : L"Hotkeys...";
        case STR_LANGUAGE: return ko ? L"언어" : L"Language";
        case STR_LANGUAGE_SYSTEM: return ko ? L"시스템 기본값" : L"System default";
        case STR_LANGUAGE_ENGLISH: return L"English";
        case STR_LANGUAGE_KOREAN: return L"한국어";
        case STR_OPEN_INSTALL_FOLDER: return ko ? L"설치 폴더 열기" : L"Open Install Folder";
        case STR_RESET_SETTINGS: return ko ? L"설정 초기화" : L"Reset Settings";
        case STR_UNINSTALL: return ko ? L"제거" : L"Uninstall";
        case STR_PRESET: return ko ? L"프리셋" : L"Preset";
        case STR_PRESET_SOLID: return ko ? L"불투명" : L"Solid";
        case STR_PRESET_SOFT: return ko ? L"부드럽게" : L"Soft";
        case STR_PRESET_GLASS: return ko ? L"유리" : L"Glass";
        case STR_PRESET_GHOST: return ko ? L"희미하게" : L"Ghost";
        case STR_CUSTOM_ALPHA: return ko ? L"사용자 지정 투명도..." : L"Custom Alpha...";
        case STR_CHECK_FOR_UPDATES: return ko ? L"업데이트 확인" : L"Check for Updates";
        case STR_OPEN_LOG: return ko ? L"로그 열기" : L"Open Log";
        case STR_DEVELOPED_BY: return ko ? L"sunwookim05 제작" : L"Developed by sunwookim05";
        case STR_EXIT: return ko ? L"종료" : L"Exit";
        case STR_ALREADY_CURRENT: return ko ? L"최신 버전입니다." : L"Already up to date.";
        case STR_UPDATE_FAILED: return ko ? L"업데이트 실패. 로그를 확인하세요." : L"Update failed. See log.";
        case STR_UPDATE_SKIPPED: return ko ? L"업데이트를 건너뛰었습니다." : L"Update skipped.";
        case STR_ALPHA_VALUE: return ko ? L"값" : L"Value";
        case STR_OK: return ko ? L"확인" : L"OK";
        case STR_CANCEL: return ko ? L"취소" : L"Cancel";
        case STR_ALPHA_RANGE: return ko ? L"60에서 255 사이 값을 입력하세요." : L"Enter a value between 60 and 255.";
        case STR_CONFIRM_TITLE: return ko ? L"System Transparency 제거" : L"Uninstall System Transparency";
        case STR_CONFIRM_QUESTION: return ko ? L"정말 제거할까요?" : L"Are you sure?";
        case STR_YES: return ko ? L"예" : L"Yes";
        case STR_NO: return ko ? L"아니요" : L"No";
        case STR_UNINSTALL_FAILED: return ko ? L"제거 작업을 만들지 못했습니다." : L"Failed to create uninstall task.";
        case STR_HOTKEY_APPLY: return ko ? L"프리셋 적용" : L"Apply preset";
        case STR_HOTKEY_RESTORE: return ko ? L"불투명하게 복원" : L"Restore opacity";
        case STR_HOTKEY_ADJUST: return ko ? L"투명도 조절" : L"Adjust opacity";
        case STR_RECORD: return ko ? L"녹화" : L"Record";
        case STR_HOLD_MIDDLE: return ko ? L"키를 누른 채 중간 클릭..." : L"Hold keys, then middle click...";
        case STR_HOLD_WHEEL: return ko ? L"키를 누른 채 휠 사용..." : L"Hold keys, then use mouse wheel...";
        case STR_HOTKEY_CONFLICT: return ko ? L"프리셋 적용과 불투명 복원은 같은 중간 클릭 단축키를 사용할 수 없습니다." : L"Apply preset and Restore opacity cannot use the same middle-click shortcut.";
        case STR_HOTKEY_LIMIT: return ko ? L"보조 키는 최대 4개까지 사용할 수 있습니다." : L"Use up to 4 modifier keys.";
        case STR_MIDDLE_CLICK: return ko ? L"중간 클릭" : L"Middle Click";
        case STR_MOUSE_WHEEL: return ko ? L"마우스 휠" : L"Mouse Wheel";
        case STR_NONE: return ko ? L"없음" : L"None";
    }

    return L"";
}

static const WCHAR* tr(StringId id) {
    return textFor(effectiveLanguage(appContext), id);
}

static void ensureUiResources(void) {
    if (!alphaDarkBrush)
        alphaDarkBrush = CreateSolidBrush(UI_BG);
    if (!alphaEditBrush)
        alphaEditBrush = CreateSolidBrush(UI_PANEL);
    if (!uiFont)
        uiFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static BOOL CALLBACK applyDialogFontProc(HWND child, LPARAM lParam) {
    SendMessageW(child, WM_SETFONT, (WPARAM)lParam, true);
    return true;
}

static void polishDialog(HWND hwnd) {
    BOOL dark = true;

    ensureUiResources();
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &dark, sizeof(dark));

    if (uiFont) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)uiFont, true);
        EnumChildWindows(hwnd, applyDialogFontProc, (LPARAM)uiFont);
    }
}

static void centerWindow(HWND window) {
    RECT rect;
    RECT work;
    POINT point;
    HMONITOR monitor;
    MONITORINFO info;
    int width;
    int height;
    int x;
    int y;

    GetWindowRect(window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    GetCursorPos(&point);
    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    info.cbSize = sizeof(info);

    if (GetMonitorInfoA(monitor, &info))
        work = info.rcWork;
    else
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);

    x = work.left + ((work.right - work.left) - width) / 2;
    y = work.top + ((work.bottom - work.top) - height) / 2;

    SetWindowPos(window, null, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void positionPopupWindow(HWND window, POINT anchor) {
    RECT rect;
    RECT work;
    HMONITOR monitor;
    MONITORINFO info;
    int width;
    int height;
    int x;
    int y;

    GetWindowRect(window, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    monitor = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
    info.cbSize = sizeof(info);

    if (GetMonitorInfoA(monitor, &info))
        work = info.rcWork;
    else
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);

    x = anchor.x + 8;
    y = anchor.y + 8;

    if (x + width > work.right) x = anchor.x - width - 8;
    if (y + height > work.bottom) y = anchor.y - height - 8;
    if (x < work.left) x = work.left;
    if (y < work.top) y = work.top;

    SetWindowPos(window, null, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void applyAlphaPreview(HWND dialog) {
    Transparency transparency = new_Transparency();

    transparency.apply(&transparency, dialog, alphaDialogValue);
    transparency.refresh(&transparency, dialog);

    if (alphaPreviewWindow) {
        transparency.apply(&transparency, alphaPreviewWindow, alphaDialogValue);
        transparency.refresh(&transparency, alphaPreviewWindow);
    }
}

static BYTE sliderPointToAlpha(HWND slider, int y) {
    RECT rect;
    int height;
    int value;
    int top;
    int bottom;

    GetClientRect(slider, &rect);
    top = rect.top + 12;
    bottom = rect.bottom - 12;
    height = bottom - top;

    if (height <= 1)
        return alphaDialogValue;

    if (y <= top) return 255;
    if (y >= bottom) return 60;

    value = 255 - ((255 - 60) * (y - top) + height / 2) / height;
    if (value < 60) value = 60;
    if (value > 255) value = 255;

    return (BYTE)value;
}

static int alphaToSliderY(HWND slider, BYTE alpha) {
    RECT rect;
    int top;
    int bottom;

    GetClientRect(slider, &rect);
    top = rect.top + 12;
    bottom = rect.bottom - 12;

    return bottom - ((int)(alpha - 60) * (bottom - top)) / (255 - 60);
}

static void updateAlphaControls(HWND dialog, BYTE alpha) {
    char text[16];
    HWND slider;
    HWND edit;

    if (alphaControlsUpdating)
        return;

    alphaControlsUpdating = true;
    alphaDialogValue = alpha;
    snprintf(text, sizeof(text), "%u", alphaDialogValue);
    edit = GetDlgItem(dialog, ID_ALPHA_EDIT);
    SetWindowTextA(edit, text);
    InvalidateRect(edit, null, true);
    UpdateWindow(edit);
    slider = GetDlgItem(dialog, ID_ALPHA_SLIDER);
    InvalidateRect(slider, null, false);
    UpdateWindow(slider);
    alphaControlsUpdating = false;

    applyAlphaPreview(dialog);
}

static LRESULT CALLBACK alphaSliderProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    HWND dialog = GetParent(hwnd);
    PAINTSTRUCT ps;
    RECT rect;
    RECT track;
    RECT active;
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ oldBrush;
    HGDIOBJ oldPen;
    int y;

    (void)id;
    (void)ref;

    if (msg == WM_LBUTTONDOWN || (msg == WM_MOUSEMOVE && (w & MK_LBUTTON))) {
        SetCapture(hwnd);
        updateAlphaControls(dialog, sliderPointToAlpha(hwnd, (short)HIWORD(l)));
        return 0;
    }

    if (msg == WM_LBUTTONUP) {
        if (GetCapture() == hwnd)
            ReleaseCapture();

        updateAlphaControls(dialog, sliderPointToAlpha(hwnd, (short)HIWORD(l)));
        return 0;
    }

    if (msg == WM_ERASEBKGND)
        return 1;

    if (msg == WM_PAINT) {
        HDC dc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &rect);

        brush = CreateSolidBrush(UI_BG);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);

        track.left = rect.left + ((rect.right - rect.left) / 2) - 3;
        track.right = track.left + 6;
        track.top = rect.top + 12;
        track.bottom = rect.bottom - 12;

        brush = CreateSolidBrush(UI_BORDER);
        pen = CreatePen(PS_SOLID, 1, UI_BORDER);
        oldBrush = SelectObject(dc, brush);
        oldPen = SelectObject(dc, pen);
        RoundRect(dc, track.left, track.top, track.right, track.bottom, 6, 6);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        y = alphaToSliderY(hwnd, alphaDialogValue);
        active = track;
        active.top = y;

        brush = CreateSolidBrush(UI_ACCENT);
        pen = CreatePen(PS_SOLID, 1, UI_ACCENT);
        oldBrush = SelectObject(dc, brush);
        oldPen = SelectObject(dc, pen);
        RoundRect(dc, active.left, active.top, active.right, active.bottom, 6, 6);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        brush = CreateSolidBrush(UI_TEXT);
        Ellipse(dc, track.left - 5, y - 7, track.right + 5, y + 7);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, alphaSliderProc, id);

    return DefSubclassProc(hwnd, msg, w, l);
}

static void makeTrayIconData(App* self, NOTIFYICONDATAA* data) {
    ZeroMemory(data, sizeof(*data));

    data->cbSize = sizeof(*data);
    data->hWnd = self->trayWindow;
    data->uID = TRAY_ID;
    data->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data->uCallbackMessage = WM_TRAY;

    data->hIcon = (HICON)LoadImage(GetModuleHandle(null), MAKEINTRESOURCE(102), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);

    if (!data->hIcon)
        data->hIcon = LoadIcon(null, IDI_APPLICATION);

    strcpy(data->szTip, "System Transparency");
}

static boolean addTrayIcon(App* self) {
    NOTIFYICONDATAA data;
    makeTrayIconData(self, &data);

    if (Shell_NotifyIconA(NIM_ADD, &data)) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconA(NIM_SETVERSION, &data);
        self->trayIconAdded = true;
        KillTimer(self->trayWindow, TRAY_RETRY_TIMER_ID);
        return true;
    }

    self->trayIconAdded = false;
    SetTimer(self->trayWindow, TRAY_RETRY_TIMER_ID, 2000, null);
    return false;
}

static void removeTrayIcon(App* self) {
    NOTIFYICONDATAA data;
    makeTrayIconData(self, &data);

    Shell_NotifyIconA(NIM_DELETE, &data);
    self->trayIconAdded = false;
}

static boolean isTrayContextMenu(LPARAM l) {
    UINT event = LOWORD(l);
    return l == WM_RBUTTONUP || l == WM_CONTEXTMENU ||
        event == WM_RBUTTONUP || event == WM_CONTEXTMENU;
}

static DWORD runCommand(string command) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdLine[2048];
    DWORD exitCode = 1;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    lstrcpynA(cmdLine, command, sizeof(cmdLine));

    if (!CreateProcessA(null, cmdLine, null, null, false, CREATE_NO_WINDOW, null, null, &si, &pi))
        return 1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode;
}

static boolean getInstalledExePath(string out, DWORD outSize) {
    Installer installer = new_Installer();
    return installer.getInstalledExePath(&installer, out, outSize);
}

static void registerStartupTask(void) {
    char installed[MAX_PATH];
    char command[2048];

    if (!getInstalledExePath(installed, sizeof(installed)))
        return;

    snprintf(command, sizeof(command), "schtasks /delete /tn \"%s\" /f", APP_TASK_NAME);
    runCommand(command);

    snprintf(command, sizeof(command),
        "schtasks /create /tn \"%s\" /tr \"\\\"%s\\\"\" /sc onlogon /delay 0000:10 /rl HIGHEST /f",
        APP_TASK_NAME, installed);
    runCommand(command);
}

static void unregisterStartupTask(void) {
    char command[256];
    snprintf(command, sizeof(command), "schtasks /delete /tn \"%s\" /f", APP_TASK_NAME);
    runCommand(command);
}

static void openInstallFolder(void) {
    char installed[MAX_PATH];
    char folder[MAX_PATH];

    if (!getInstalledExePath(installed, sizeof(installed)))
        return;

    lstrcpynA(folder, installed, sizeof(folder));
    char* slash = strrchr(folder, '\\');
    if (slash)
        *slash = '\0';

    ShellExecuteA(null, "open", folder, null, null, SW_SHOWNORMAL);
}

static boolean getLogPath(string out, DWORD outSize) {
    char tempPath[MAX_PATH];

    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    return (size_t)snprintf(out, outSize, "%s%sUpdate.log", tempPath, APP_NAME) < outSize;
}

static void openLog(void) {
    char path[MAX_PATH];

    if (!getLogPath(path, sizeof(path)))
        return;

    ShellExecuteA(null, "open", path, null, null, SW_SHOWNORMAL);
}

static boolean getCurrentExePath(string out, DWORD outSize) {
    DWORD len = GetModuleFileNameA(null, out, outSize);
    return len > 0 && len < outSize;
}

static void deleteCertificate(void) {
    runCommand("certutil -delstore TrustedPublisher \"sunwookim05\"");
    runCommand("certutil -delstore Root \"sunwookim05\"");
}

static void deleteSettings(void) {
    RegDeleteTreeA(HKEY_CURRENT_USER, APP_REG_KEY);
}

static boolean createUninstallBatch(string currentExe, string installedExe) {
    char tempPath[MAX_PATH];
    char batchPath[MAX_PATH];
    FILE* file;
    DWORD pid = GetCurrentProcessId();
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char cmdLine[MAX_PATH + 16];

    if (!GetTempPathA(sizeof(tempPath), tempPath))
        return false;

    if ((size_t)snprintf(batchPath, sizeof(batchPath), "%s%sUninstall.bat", tempPath, APP_NAME) >= sizeof(batchPath))
        return false;

    file = fopen(batchPath, "w");
    if (!file)
        return false;

    fprintf(file, "@echo off\n");
    fprintf(file, "set \"PID=%lu\"\n", (unsigned long)pid);
    fprintf(file, "set \"CURRENT=%s\"\n", currentExe);
    fprintf(file, "set \"INSTALLED=%s\"\n", installedExe);
    fprintf(file, ":wait\n");
    fprintf(file, "tasklist /FI \"PID eq %%PID%%\" | findstr \"%%PID%%\" >nul\n");
    fprintf(file, "if not errorlevel 1 (\n");
    fprintf(file, "  timeout /t 1 /nobreak >nul\n");
    fprintf(file, "  goto wait\n");
    fprintf(file, ")\n");
    fprintf(file, "for /l %%%%I in (1,1,10) do (\n");
    fprintf(file, "  del /f /q \"%%CURRENT%%\" >nul 2>nul\n");
    fprintf(file, "  del /f /q \"%%INSTALLED%%\" >nul 2>nul\n");
    fprintf(file, "  if not exist \"%%CURRENT%%\" if not exist \"%%INSTALLED%%\" goto done\n");
    fprintf(file, "  timeout /t 1 /nobreak >nul\n");
    fprintf(file, ")\n");
    fprintf(file, ":done\n");
    fprintf(file, "del \"%%~f0\"\n");
    fclose(file);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    snprintf(cmdLine, sizeof(cmdLine), "cmd.exe /c \"%s\"", batchPath);

    if (!CreateProcessA(null, cmdLine, null, null, false, CREATE_NO_WINDOW, null, null, &si, &pi))
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static void drawDarkButton(DRAWITEMSTRUCT* draw, const WCHAR* label, boolean accent) {
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ oldBrush;
    HGDIOBJ oldPen;
    RECT textRect = draw->rcItem;
    COLORREF background = (draw->itemState & ODS_SELECTED) ? UI_PANEL_HOVER : UI_PANEL;
    COLORREF border = accent ? UI_ACCENT : UI_BORDER;

    FillRect(draw->hDC, &draw->rcItem,
        GetParent(draw->hwndItem) == trayPopupWindow &&
        ((draw->CtlID >= ID_HOTKEY_APPLY_RECORD && draw->CtlID <= ID_HOTKEY_ADJUST_RECORD) ||
         (draw->CtlID >= ID_HOTKEY_APPLY_LABEL && draw->CtlID <= ID_HOTKEY_ADJUST_LABEL)) ?
        trayPanelBrush : alphaDarkBrush);

    brush = CreateSolidBrush(background);
    pen = CreatePen(PS_SOLID, 1, border);
    oldBrush = SelectObject(draw->hDC, brush);
    oldPen = SelectObject(draw->hDC, pen);
    RoundRect(draw->hDC, draw->rcItem.left, draw->rcItem.top, draw->rcItem.right, draw->rcItem.bottom, 8, 8);
    SelectObject(draw->hDC, oldPen);
    SelectObject(draw->hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, UI_TEXT);
    InflateRect(&textRect, -8, 0);
    DrawTextW(draw->hDC, label, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static DWORD getCurrentModifiers(App* self) {
    DWORD modifiers = 0;

    if ((self && self->ctrlDown) || (GetAsyncKeyState(VK_CONTROL) & 0x8000)) modifiers |= HOTKEY_MOD_CTRL;
    if ((self && self->altDown) || (GetAsyncKeyState(VK_MENU) & 0x8000)) modifiers |= HOTKEY_MOD_ALT;
    if ((self && self->shiftDown) || (GetAsyncKeyState(VK_SHIFT) & 0x8000)) modifiers |= HOTKEY_MOD_SHIFT;
    if ((self && self->winDown) || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) modifiers |= HOTKEY_MOD_WIN;

    return modifiers;
}

static int countHotkeyModifiers(DWORD modifiers) {
    int count = 0;

    if (modifiers & HOTKEY_MOD_CTRL) count++;
    if (modifiers & HOTKEY_MOD_ALT) count++;
    if (modifiers & HOTKEY_MOD_SHIFT) count++;
    if (modifiers & HOTKEY_MOD_WIN) count++;

    return count;
}

static boolean modifiersMatch(DWORD current, DWORD expected) {
    DWORD mask = HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_WIN;
    return expected != 0 && (current & mask) == expected;
}

static void modifiersToText(DWORD modifiers, WCHAR* out, size_t outSize) {
    out[0] = '\0';

    if (modifiers & HOTKEY_MOD_CTRL) lstrcatW(out, L"Ctrl + ");
    if (modifiers & HOTKEY_MOD_ALT) lstrcatW(out, L"Alt + ");
    if (modifiers & HOTKEY_MOD_SHIFT) lstrcatW(out, L"Shift + ");
    if (modifiers & HOTKEY_MOD_WIN) lstrcatW(out, L"Win + ");

    if (out[0] == '\0')
        lstrcpynW(out, tr(STR_NONE), (int)outSize);
    else
        out[wcslen(out) - 3] = '\0';
}

static void shortcutToText(DWORD modifiers, const WCHAR* action, WCHAR* out, size_t outSize) {
    WCHAR modText[96];
    modifiersToText(modifiers, modText, sizeof(modText) / sizeof(modText[0]));
    swprintf(out, outSize, L"%s + %s", modText, action);
}

static void updateHotkeyDialogLabels(void) {
    WCHAR text[128];

    if (!hotkeyDialogWindow)
        return;

    shortcutToText(hotkeyDialogApplyModifiers, tr(STR_MIDDLE_CLICK), text, sizeof(text) / sizeof(text[0]));
    SetWindowTextW(GetDlgItem(hotkeyDialogWindow, ID_HOTKEY_APPLY_LABEL), text);

    shortcutToText(hotkeyDialogRestoreModifiers, tr(STR_MIDDLE_CLICK), text, sizeof(text) / sizeof(text[0]));
    SetWindowTextW(GetDlgItem(hotkeyDialogWindow, ID_HOTKEY_RESTORE_LABEL), text);

    shortcutToText(hotkeyDialogAdjustModifiers, tr(STR_MOUSE_WHEEL), text, sizeof(text) / sizeof(text[0]));
    SetWindowTextW(GetDlgItem(hotkeyDialogWindow, ID_HOTKEY_ADJUST_LABEL), text);
}

static void invalidateHotkeyButtons(HWND hwnd) {
    InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_APPLY_RECORD), null, true);
    InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_RESTORE_RECORD), null, true);
    InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_ADJUST_RECORD), null, true);
    if (hwnd == trayPopupWindow) {
        InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_APPLY_LABEL), null, true);
        InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_RESTORE_LABEL), null, true);
        InvalidateRect(GetDlgItem(hwnd, ID_HOTKEY_ADJUST_LABEL), null, true);
        InvalidateRect(hwnd, null, false);
    }
}

static void setHotkeyRecording(HWND hwnd, int action) {
    const WCHAR* text;

    hotkeyRecordingAction = action;
    if (action == HOTKEY_ACTION_APPLY)
        text = tr(STR_HOLD_MIDDLE);
    else if (action == HOTKEY_ACTION_RESTORE)
        text = tr(STR_HOLD_MIDDLE);
    else if (action == HOTKEY_ACTION_ADJUST)
        text = tr(STR_HOLD_WHEEL);
    else
        text = L"";

    SetWindowTextW(GetDlgItem(hwnd,
        action == HOTKEY_ACTION_APPLY ? ID_HOTKEY_APPLY_LABEL :
        action == HOTKEY_ACTION_RESTORE ? ID_HOTKEY_RESTORE_LABEL :
        ID_HOTKEY_ADJUST_LABEL), text);
    invalidateHotkeyButtons(hwnd);
}

static void finishHotkeyRecording(DWORD modifiers) {
    if (!hotkeyDialogWindow || hotkeyRecordingAction == HOTKEY_ACTION_NONE || modifiers == 0)
        return;

    if (countHotkeyModifiers(modifiers) > HOTKEY_MAX_MODIFIERS) {
        MessageBoxW(hotkeyDialogWindow, tr(STR_HOTKEY_LIMIT), tr(STR_APP_NAME), MB_OK | MB_ICONWARNING);
        return;
    }

    if (hotkeyRecordingAction == HOTKEY_ACTION_APPLY)
        hotkeyDialogApplyModifiers = modifiers;
    else if (hotkeyRecordingAction == HOTKEY_ACTION_RESTORE)
        hotkeyDialogRestoreModifiers = modifiers;
    else if (hotkeyRecordingAction == HOTKEY_ACTION_ADJUST)
        hotkeyDialogAdjustModifiers = modifiers;

    if ((modifiers & HOTKEY_MOD_WIN) && appContext)
        appContext->winUsed = true;

    hotkeyRecordingAction = HOTKEY_ACTION_NONE;
    updateHotkeyDialogLabels();
    invalidateHotkeyButtons(hotkeyDialogWindow);
}

static LRESULT CALLBACK confirmWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE:
            ensureUiResources();

            CreateWindowW(L"STATIC", tr(STR_CONFIRM_QUESTION), WS_VISIBLE | WS_CHILD | SS_CENTER,
                24, 30, 252, 24, hwnd, null, null, null);
            CreateWindowW(L"BUTTON", tr(STR_YES), WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_OWNERDRAW,
                56, 82, 82, 30, hwnd, (HMENU)ID_CONFIRM_YES, null, null);
            CreateWindowW(L"BUTTON", tr(STR_NO), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                162, 82, 82, 30, hwnd, (HMENU)ID_CONFIRM_NO, null, null);

            {
                Transparency transparency = new_Transparency();
                transparency.apply(&transparency, hwnd, confirmDialogAlpha);
                transparency.refresh(&transparency, hwnd);
            }
            polishDialog(hwnd);
            return 0;

        case WM_ERASEBKGND: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect((HDC)w, &rect, alphaDarkBrush);
            return 1;
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)alphaDarkBrush;

        case WM_CTLCOLORSTATIC:
            SetTextColor((HDC)w, UI_TEXT);
            SetBkColor((HDC)w, UI_BG);
            return (LRESULT)alphaDarkBrush;

        case WM_DRAWITEM:
            if (w == ID_CONFIRM_YES || w == ID_CONFIRM_NO) {
                drawDarkButton((DRAWITEMSTRUCT*)l, w == ID_CONFIRM_YES ? tr(STR_YES) : tr(STR_NO), w == ID_CONFIRM_YES);
                return true;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(w) == ID_CONFIRM_YES) {
                confirmDialogOk = true;
                DestroyWindow(hwnd);
                return 0;
            }

            if (LOWORD(w) == ID_CONFIRM_NO) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, w, l);
}

static boolean askConfirm(HWND owner, BYTE alpha) {
    WNDCLASSA wc = {0};
    HWND window;
    MSG msg;

    confirmDialogOk = false;
    confirmDialogAlpha = alpha;

    wc.lpfnWndProc = confirmWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "ConfirmWindow";
    RegisterClassA(&wc);

    window = CreateWindowExA(WS_EX_DLGMODALFRAME, wc.lpszClassName, "",
        WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 310, 165,
        owner, null, wc.hInstance, null);

    if (!window)
        return false;

    centerWindow(window);
    SetWindowTextW(window, tr(STR_CONFIRM_TITLE));
    EnableWindow(owner, false);
    ShowWindow(window, SW_SHOWNORMAL);

    while (IsWindow(window) && GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(owner, true);
    SetForegroundWindow(owner);

    return confirmDialogOk;
}

static void uninstallApp(App* self, HWND owner) {
    char currentExe[MAX_PATH];
    char installedExe[MAX_PATH];
    BYTE confirmAlpha;

    confirmAlpha = self->settings.preset == PRESET_CUSTOM ?
        self->settings.customAlpha :
        self->transparency.presetToAlpha(&self->transparency, self->settings.preset);

    if (!askConfirm(owner, confirmAlpha))
        return;

    if (!getCurrentExePath(currentExe, sizeof(currentExe)))
        return;

    if (!getInstalledExePath(installedExe, sizeof(installedExe)))
        lstrcpynA(installedExe, currentExe, sizeof(installedExe));

    unregisterStartupTask();
    deleteCertificate();
    deleteSettings();

    if (!createUninstallBatch(currentExe, installedExe)) {
        MessageBoxW(owner, tr(STR_UNINSTALL_FAILED), tr(STR_APP_NAME), MB_OK | MB_ICONERROR);
        return;
    }

    self->shuttingDown = true;
    removeTrayIcon(self);
    PostQuitMessage(0);
}

static boolean isAutoTarget(App* self, HWND hwnd) {
    return self->transparency.isTarget(&self->transparency, hwnd);
}

static boolean isPopupMenuWindow(HWND hwnd) {
    char cls[128];
    LONG exStyle;
    LONG style;

    if (!IsWindow(hwnd))
        return false;

    GetClassNameA(hwnd, cls, sizeof(cls));
    if (!strcmp(cls, "#32768"))
        return false;

    exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    style = GetWindowLong(hwnd, GWL_STYLE);

    if (!(style & WS_POPUP) || (exStyle & WS_EX_APPWINDOW))
        return false;

    return strstr(cls, "Popup") ||
        strstr(cls, "Flyout") ||
        strstr(cls, "Menu");
}

static BYTE getCurrentAlpha(App* self) {
    if (self->settings.preset == PRESET_CUSTOM)
        return self->settings.customAlpha;

    return self->transparency.presetToAlpha(&self->transparency, self->settings.preset);
}

static void applyPopupTransparency(App* self, HWND hwnd) {
    BYTE alpha;

    if (!self || !self->settings.popupTransparency || !isPopupMenuWindow(hwnd))
        return;

    alpha = getCurrentAlpha(self);
    self->transparency.apply(&self->transparency, hwnd, alpha);
    self->transparency.refresh(&self->transparency, hwnd);
}

static void showStatusMenu(HWND owner, POINT point, const WCHAR* message) {
    HMENU menu = CreatePopupMenu();
    HBRUSH menuBrush = CreateSolidBrush(UI_MENU_BG);
    MENUINFO menuInfo = {0};

    ensureUiResources();
    resetMenuItems();

    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask = MIM_BACKGROUND;
    menuInfo.hbrBack = menuBrush;
    SetMenuInfo(menu, &menuInfo);

    appendDarkMenu(menu, MF_STRING | MF_DISABLED, 0, message, false, false);

    statusMenuOpen = true;
    SetTimer(owner, TRAY_STATUS_TIMER_ID, 3000, null);
    TrackPopupMenu(menu, TPM_NONOTIFY, point.x, point.y, 0, owner, null);
    KillTimer(owner, TRAY_STATUS_TIMER_ID);
    statusMenuOpen = false;

    DestroyMenu(menu);
    DeleteObject(menuBrush);
}

static LRESULT CALLBACK alphaWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    HWND edit;
    HWND slider;
    char text[16];
    int value;

    switch (msg) {
        case WM_CREATE:
            ensureUiResources();

            CreateWindowW(L"STATIC", tr(STR_CUSTOM_ALPHA), WS_VISIBLE | WS_CHILD, 18, 14, 170, 20, hwnd, null, null, null);

            slider = CreateWindowExA(0, "STATIC", "", WS_VISIBLE | WS_CHILD | SS_NOTIFY,
                26, 42, 54, 166, hwnd, (HMENU)ID_ALPHA_SLIDER, null, null);
            SetWindowSubclass(slider, alphaSliderProc, 1, 0);

            CreateWindowW(L"STATIC", L"255", WS_VISIBLE | WS_CHILD, 92, 48, 36, 18, hwnd, null, null, null);
            CreateWindowW(L"STATIC", L"60", WS_VISIBLE | WS_CHILD, 92, 184, 36, 18, hwnd, null, null, null);
            CreateWindowW(L"STATIC", tr(STR_ALPHA_VALUE), WS_VISIBLE | WS_CHILD, 104, 98, 42, 20, hwnd, null, null, null);
            edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_NUMBER,
                150, 94, 70, 24, hwnd, (HMENU)ID_ALPHA_EDIT, null, null);
            snprintf(text, sizeof(text), "%u", alphaDialogValue);
            SetWindowTextA(edit, text);

            CreateWindowW(L"BUTTON", tr(STR_OK), WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_OWNERDRAW,
                38, 224, 82, 28, hwnd, (HMENU)ID_ALPHA_OK, null, null);
            CreateWindowW(L"BUTTON", tr(STR_CANCEL), WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                136, 224, 82, 28, hwnd, (HMENU)ID_ALPHA_CANCEL, null, null);

            applyAlphaPreview(hwnd);
            polishDialog(hwnd);

            SetFocus(slider);
            return 0;

        case WM_ERASEBKGND: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect((HDC)w, &rect, alphaDarkBrush);
            return 1;
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)alphaDarkBrush;

        case WM_CTLCOLORSTATIC:
            SetTextColor((HDC)w, UI_TEXT);
            SetBkColor((HDC)w, UI_BG);
            return (LRESULT)alphaDarkBrush;

        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)w, UI_TEXT);
            SetBkColor((HDC)w, UI_PANEL);
            return (LRESULT)alphaEditBrush;

        case WM_DRAWITEM:
            if (w == ID_ALPHA_OK || w == ID_ALPHA_CANCEL) {
                const WCHAR* label = (w == ID_ALPHA_OK) ? tr(STR_OK) : tr(STR_CANCEL);
                drawDarkButton((DRAWITEMSTRUCT*)l, label, w == ID_ALPHA_OK);
                return true;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(w) == ID_ALPHA_EDIT && HIWORD(w) == EN_CHANGE) {
                if (alphaControlsUpdating)
                    return 0;

                GetWindowTextA(GetDlgItem(hwnd, ID_ALPHA_EDIT), text, sizeof(text));
                value = atoi(text);

                if (value >= 60 && value <= 255) {
                    updateAlphaControls(hwnd, (BYTE)value);
                }
            }

            if (LOWORD(w) == ID_ALPHA_OK) {
                GetWindowTextA(GetDlgItem(hwnd, ID_ALPHA_EDIT), text, sizeof(text));
                value = atoi(text);
                if (value < 60 || value > 255) {
                    MessageBoxW(hwnd, tr(STR_ALPHA_RANGE), tr(STR_APP_NAME), MB_OK | MB_ICONWARNING);
                    return 0;
                }

                alphaDialogValue = (BYTE)value;
                alphaDialogOk = true;
                DestroyWindow(hwnd);
                return 0;
            }

            if (LOWORD(w) == ID_ALPHA_CANCEL) {
                if (alphaPreviewWindow) {
                    Transparency transparency = new_Transparency();
                    transparency.apply(&transparency, alphaPreviewWindow, alphaPreviewOriginal);
                    transparency.refresh(&transparency, alphaPreviewWindow);
                }

                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (w == VK_ESCAPE) {
                SendMessage(hwnd, WM_COMMAND, ID_ALPHA_CANCEL, 0);
                return 0;
            }
            if (w == VK_RETURN) {
                SendMessage(hwnd, WM_COMMAND, ID_ALPHA_OK, 0);
                return 0;
            }
            break;

        case WM_CLOSE:
            if (alphaPreviewWindow) {
                Transparency transparency = new_Transparency();
                transparency.apply(&transparency, alphaPreviewWindow, alphaPreviewOriginal);
                transparency.refresh(&transparency, alphaPreviewWindow);
            }

            DestroyWindow(hwnd);
            return 0;
    }

    return DefWindowProc(hwnd, msg, w, l);
}

static boolean askAlpha(HWND owner, BYTE* alpha) {
    WNDCLASSA wc = {0};
    HWND window;
    MSG msg;
    INITCOMMONCONTROLSEX icc;
    POINT point;

    alphaDialogValue = *alpha;
    alphaDialogOk = false;
    alphaPreviewWindow = null;
    alphaPreviewOriginal = ALPHA_OPAQUE;

    GetCursorPos(&point);
    alphaPreviewWindow = GetAncestor(WindowFromPoint(point), GA_ROOT);
    if (alphaPreviewWindow == owner || alphaPreviewWindow == GetDesktopWindow())
        alphaPreviewWindow = GetForegroundWindow();

    if (alphaPreviewWindow && alphaPreviewWindow != owner) {
        Transparency transparency = new_Transparency();
        alphaPreviewOriginal = transparency.getWindowAlpha(&transparency, alphaPreviewWindow);
    } else {
        alphaPreviewWindow = null;
    }

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    wc.lpfnWndProc = alphaWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "AlphaInputWindow";
    RegisterClassA(&wc);

    window = CreateWindowExA(WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME, wc.lpszClassName, "",
        WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 260, 285,
        owner, null, wc.hInstance, null);

    if (!window)
        return false;

    positionPopupWindow(window, point);
    SetWindowTextW(window, tr(STR_CUSTOM_ALPHA));
    EnableWindow(owner, false);
    ShowWindow(window, SW_SHOWNORMAL);

    while (IsWindow(window) && GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(owner, true);
    SetForegroundWindow(owner);

    if (alphaDialogOk)
        *alpha = alphaDialogValue;

    alphaPreviewWindow = null;
    return alphaDialogOk;
}

static BOOL CALLBACK enumExplorerWindows(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return true;

    if (!self->settings.explorerAuto)
        return true;

    if (!isAutoTarget(self, hwnd))
        return true;

    BYTE alpha = getCurrentAlpha(self);
    self->transparency.apply(&self->transparency, hwnd, alpha);
    self->tracker.track(&self->tracker, &self->transparency, hwnd);

    return true;
}

static BOOL CALLBACK applyExplorerAutoWindow(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (!IsWindowVisible(hwnd))
        return true;

    if (isAutoTarget(self, hwnd)) {
        BYTE alpha = self->settings.explorerAuto ?
            getCurrentAlpha(self) :
            ALPHA_OPAQUE;

        self->transparency.apply(&self->transparency, hwnd, alpha);
        self->transparency.refresh(&self->transparency, hwnd);

        if (self->settings.explorerAuto)
            self->tracker.track(&self->tracker, &self->transparency, hwnd);
    }

    return true;
}

static void applyExplorerAutoAll(App* self) {
    EnumWindows(applyExplorerAutoWindow, (LPARAM)self);
}

static void drawTrayMenuItem(DRAWITEMSTRUCT* draw, MenuItemData* item) {
    RECT rect = draw->rcItem;
    RECT fillRect = rect;
    RECT textRect = rect;
    HBRUSH brush;
    HPEN pen;
    HGDIOBJ oldBrush;
    HGDIOBJ oldPen;
    HGDIOBJ oldFont = null;
    boolean selected = (draw->itemState & ODS_SELECTED) ? true : false;
    boolean disabled = (draw->itemState & ODS_DISABLED) ? true : false;

    if (uiFont)
        oldFont = SelectObject(draw->hDC, uiFont);

    brush = CreateSolidBrush(UI_MENU_BG);
    FillRect(draw->hDC, &rect, brush);
    DeleteObject(brush);

    if (item->separator) {
        RECT line = rect;
        line.left += 14;
        line.right -= 14;
        line.top += (line.bottom - line.top) / 2;
        line.bottom = line.top + 1;
        brush = CreateSolidBrush(UI_BORDER);
        FillRect(draw->hDC, &line, brush);
        DeleteObject(brush);
        if (oldFont)
            SelectObject(draw->hDC, oldFont);
        return;
    }

    if (selected && !disabled) {
        InflateRect(&fillRect, -5, -3);
        brush = CreateSolidBrush(UI_MENU_HOVER);
        pen = CreatePen(PS_SOLID, 1, UI_MENU_HOVER);
        oldBrush = SelectObject(draw->hDC, brush);
        oldPen = SelectObject(draw->hDC, pen);
        RoundRect(draw->hDC, fillRect.left, fillRect.top, fillRect.right, fillRect.bottom, 8, 8);
        SelectObject(draw->hDC, oldPen);
        SelectObject(draw->hDC, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    if (item->checked) {
        RECT mark = rect;
        mark.left += 11;
        mark.right = mark.left + 12;
        mark.top += 10;
        mark.bottom = mark.top + 12;

        brush = CreateSolidBrush(UI_ACCENT);
        pen = CreatePen(PS_SOLID, 1, UI_ACCENT);
        oldBrush = SelectObject(draw->hDC, brush);
        oldPen = SelectObject(draw->hDC, pen);
        Ellipse(draw->hDC, mark.left, mark.top, mark.right, mark.bottom);
        SelectObject(draw->hDC, oldPen);
        SelectObject(draw->hDC, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
    }

    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, disabled ? UI_MUTED : UI_MENU_TEXT);

    textRect.left += 34;
    textRect.right -= item->submenu ? 28 : 14;
    DrawTextW(draw->hDC, item->text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    if (item->submenu) {
        RECT arrowRect = rect;
        arrowRect.left = arrowRect.right - 22;
        arrowRect.right -= 8;
        SetTextColor(draw->hDC, UI_MUTED);
        DrawTextW(draw->hDC, L">", -1, &arrowRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }

    if (oldFont)
        SelectObject(draw->hDC, oldFont);
}

static void CALLBACK winEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time) {
    char cls[128];
    App* self = appContext;

    (void)hook;
    (void)child;
    (void)tid;
    (void)time;

    if (!self || !IsWindow(hwnd))
        return;

    GetClassNameA(hwnd, cls, sizeof(cls));

    if (event == EVENT_OBJECT_SHOW && obj == OBJID_WINDOW && IsWindowVisible(hwnd) && isPopupMenuWindow(hwnd)) {
        applyPopupTransparency(self, hwnd);
        return;
    }

    if (obj == OBJID_WINDOW && IsWindowVisible(hwnd) && isAutoTarget(self, hwnd)) {
        if (!strcmp(cls, "TaskSwitcherWnd") || !strcmp(cls, "MultitaskingViewFrame"))
            return;

        if (!self->settings.explorerAuto)
            return;

        self->transparency.apply(&self->transparency, hwnd, getCurrentAlpha(self));
        self->tracker.track(&self->tracker, &self->transparency, hwnd);
    }

    if (event == EVENT_OBJECT_DESTROY && obj == OBJID_WINDOW)
        self->tracker.remove(&self->tracker, hwnd);
}

static void updateTrayAlpha(HWND hwnd) {
    WCHAR text[96];
    swprintf(text, 96, L"%u%%", (unsigned)((getCurrentAlpha(appContext) * 100 + 127) / 255));
    SetWindowTextW(GetDlgItem(hwnd, ID_ALPHA_EDIT), text);
}

static void paintTrayCard(HDC dc, RECT rect, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 14, 14);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void showTrayPopupPage(App* self, int page, const POINT* position);

static LRESULT CALLBACK shortcutButtonProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    (void)ref;
    if (msg == WM_SETCURSOR && LOWORD(l) == HTCLIENT) {
        SetCursor(LoadCursor(null, IDC_HAND));
        return true;
    }
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, shortcutButtonProc, id);
    return DefSubclassProc(hwnd, msg, w, l);
}

static LRESULT CALLBACK trayPopupProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    App* self = appContext;
    if (!self)
        return DefWindowProcW(hwnd, msg, w, l);

    switch (msg) {
        case WM_CREATE: {
            HWND slider;
            const int labels[] = {ID_HOTKEY_APPLY_LABEL, ID_HOTKEY_RESTORE_LABEL, ID_HOTKEY_ADJUST_LABEL};
            const StringId titles[] = {STR_HOTKEY_APPLY, STR_HOTKEY_RESTORE, STR_HOTKEY_ADJUST};
            trayPopupPage = (int)(INT_PTR)((CREATESTRUCT*)l)->lpCreateParams;
            ensureUiResources();
            if (!trayPanelBrush)
                trayPanelBrush = CreateSolidBrush(UI_PANEL);
            if (!trayHeadingFont)
                trayHeadingFont = CreateFontW(-17, 0, 0, 0, FW_SEMIBOLD, false, false, false, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            if (!trayValueFont)
                trayValueFont = CreateFontW(-26, 0, 0, 0, FW_SEMIBOLD, false, false, false, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            CreateWindowW(L"STATIC", trayPopupPage == TRAY_PAGE_ALPHA ?
                (effectiveLanguage(self) == LANGUAGE_KOREAN ? L"불투명도" : L"Opacity") :
                trayPopupPage == TRAY_PAGE_KEYS ?
                (effectiveLanguage(self) == LANGUAGE_KOREAN ? L"단축키" : L"Shortcuts") : tr(STR_APP_NAME), WS_CHILD | WS_VISIBLE,
                22, 18, 480, 26, hwnd, (HMENU)ID_TRAY_ALPHA_TITLE, null, null);
            if (trayPopupPage == TRAY_PAGE_ALPHA) {
            CreateWindowW(L"STATIC", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"불투명도" : L"Opacity",
                WS_CHILD | WS_VISIBLE | SS_CENTER, 40, 76, 140, 20, hwnd, null, null, null);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
                40, 100, 140, 36, hwnd, (HMENU)ID_ALPHA_EDIT, null, null);
            CreateWindowW(L"STATIC", L"100%", WS_CHILD | WS_VISIBLE | SS_CENTER,
                40, 145, 140, 18, hwnd, null, null, null);
            slider = CreateWindowW(TRACKBAR_CLASSW, L"Opacity", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_VERT | TBS_NOTICKS,
                98, 166, 32, 140, hwnd, (HMENU)ID_ALPHA_SLIDER, null, null);
            SendMessage(slider, TBM_SETRANGE, true, MAKELPARAM(60, 255));
            SendMessage(slider, TBM_SETPAGESIZE, 0, 15);
            /* Native vertical trackbars increase downward; invert the position for opacity. */
            SendMessage(slider, TBM_SETPOS, true, 315 - getCurrentAlpha(self));
            CreateWindowW(L"STATIC", L"24%", WS_CHILD | WS_VISIBLE | SS_CENTER,
                40, 308, 140, 18, hwnd, null, null, null);
            updateTrayAlpha(hwnd);
            }
            if (trayPopupPage == TRAY_PAGE_KEYS) {
            hotkeyDialogWindow = hwnd;
            hotkeyDialogApplyModifiers = self->settings.applyModifiers;
            hotkeyDialogRestoreModifiers = self->settings.restoreModifiers;
            hotkeyDialogAdjustModifiers = self->settings.adjustModifiers;
            hotkeyRecordingAction = HOTKEY_ACTION_NONE;
            for (int i = 0; i < 3; i++) {
                int y = 88 + i * 84;
                CreateWindowW(L"STATIC", tr(titles[i]), WS_CHILD | WS_VISIBLE,
                    38, y, 356, 20, hwnd, (HMENU)(INT_PTR)(ID_TRAY_ROW_TITLE + i), null, null);
                CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    38, y + 24, 356, 32, hwnd, (HMENU)(INT_PTR)labels[i], null, null);
                SetWindowSubclass(GetDlgItem(hwnd, labels[i]), shortcutButtonProc, 1, 0);
            }
            updateHotkeyDialogLabels();
            CreateWindowW(L"STATIC", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"키 조합을 클릭해 변경하세요" : L"Click a shortcut to change it",
                WS_CHILD | WS_VISIBLE, 22, 48, 390, 20, hwnd, (HMENU)ID_TRAY_HOTKEY_TITLE, null, null);
            }
            if (trayPopupPage == TRAY_PAGE_HOME) {
                CreateWindowW(L"BUTTON", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"불투명도  >" : L"Opacity  >",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    22, 64, 388, 48, hwnd, (HMENU)ID_TRAY_ALPHA_MENU, null, null);
                CreateWindowW(L"BUTTON", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"단축키  >" : L"Shortcuts  >",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    22, 122, 388, 48, hwnd, (HMENU)ID_TRAY_KEYS_MENU, null, null);
            }
            if (trayPopupPage != TRAY_PAGE_HOME) {
            CreateWindowW(L"STATIC", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"변경 시 자동 저장" : L"Saved automatically",
                WS_CHILD | WS_VISIBLE, 22, 348, trayPopupPage == TRAY_PAGE_ALPHA ? 180 : 390, 20, hwnd, (HMENU)ID_TRAY_HINT, null, null);
            CreateWindowW(L"BUTTON", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"<  뒤로" : L"<  Back",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                22, 378, trayPopupPage == TRAY_PAGE_ALPHA ? 176 : 388, 32, hwnd, (HMENU)ID_TRAY_BACK, null, null);
            } else {
            CreateWindowW(L"BUTTON", effectiveLanguage(self) == LANGUAGE_KOREAN ? L"더 보기" : L"More",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                22, 192, 268, 32, hwnd, (HMENU)ID_TRAY_MORE, null, null);
            CreateWindowW(L"BUTTON", tr(STR_EXIT), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                302, 192, 108, 32, hwnd, (HMENU)3, null, null);
            }
            polishDialog(hwnd);
            SendMessage(GetDlgItem(hwnd, ID_TRAY_ALPHA_TITLE), WM_SETFONT, (WPARAM)trayHeadingFont, true);
            SendMessage(GetDlgItem(hwnd, ID_ALPHA_EDIT), WM_SETFONT, (WPARAM)trayValueFont, true);
            return 0;
        }
        case WM_VSCROLL:
            if ((HWND)l == GetDlgItem(hwnd, ID_ALPHA_SLIDER)) {
                BYTE alpha = (BYTE)(315 - SendMessage((HWND)l, TBM_GETPOS, 0, 0));
                if (self->settings.preset != PRESET_CUSTOM || self->settings.customAlpha != alpha) {
                    self->settings.customAlpha = alpha;
                    self->settings.preset = PRESET_CUSTOM;
                    self->settings.save(&self->settings);
                    self->applyExplorerAutoAll(self);
                    updateTrayAlpha(hwnd);
                }
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR* header = (NMHDR*)l;
            if (header->idFrom == ID_ALPHA_SLIDER && header->code == NM_CUSTOMDRAW) {
                NMCUSTOMDRAW* draw = (NMCUSTOMDRAW*)l;
                if (draw->dwDrawStage == CDDS_PREPAINT) {
                    RECT rect;
                    GetClientRect(header->hwndFrom, &rect);
                    FillRect(draw->hdc, &rect, trayPanelBrush);
                    return CDRF_NOTIFYITEMDRAW;
                }
                if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                    RECT rect = draw->rc;
                    if (draw->dwItemSpec == TBCD_CHANNEL) {
                        RECT thumb;
                        int center = (rect.left + rect.right) / 2;
                        rect.left = center - 3;
                        rect.right = center + 3;
                        paintTrayCard(draw->hdc, rect, UI_BORDER, UI_BORDER);
                        SendMessage(header->hwndFrom, TBM_GETTHUMBRECT, 0, (LPARAM)&thumb);
                        rect.top = (thumb.top + thumb.bottom) / 2;
                        paintTrayCard(draw->hdc, rect, UI_ACCENT, UI_ACCENT);
                        return CDRF_SKIPDEFAULT;
                    }
                    if (draw->dwItemSpec == TBCD_THUMB) {
                        int x = (rect.left + rect.right) / 2;
                        int y = (rect.top + rect.bottom) / 2;
                        RECT ring = {x - 11, y - 11, x + 11, y + 11};
                        RECT knob = {x - 7, y - 7, x + 7, y + 7};
                        paintTrayCard(draw->hdc, ring, UI_PANEL, GetFocus() == header->hwndFrom ? UI_ACCENT : UI_BORDER);
                        paintTrayCard(draw->hdc, knob, UI_TEXT, UI_TEXT);
                        return CDRF_SKIPDEFAULT;
                    }
                }
            }
            break;
        }
        case WM_TRAY_RECORD: {
            DWORD modifiers = (DWORD)w;
            int action = hotkeyRecordingAction;
            if (action == HOTKEY_ACTION_NONE || action != (int)l || modifiers == 0)
                return 0;
            if ((action == HOTKEY_ACTION_APPLY && modifiers == self->settings.restoreModifiers) ||
                (action == HOTKEY_ACTION_RESTORE && modifiers == self->settings.applyModifiers)) {
                /* Keep recording active so the user can immediately try another combination. */
                SetWindowTextW(GetDlgItem(hwnd, action == HOTKEY_ACTION_APPLY ?
                    ID_HOTKEY_APPLY_LABEL : ID_HOTKEY_RESTORE_LABEL),
                    effectiveLanguage(self) == LANGUAGE_KOREAN ? L"이미 사용 중인 조합입니다. 다시 입력하세요." : L"Already assigned. Try another combination.");
                return 0;
            }
            finishHotkeyRecording(modifiers);
            self->settings.applyModifiers = hotkeyDialogApplyModifiers;
            self->settings.restoreModifiers = hotkeyDialogRestoreModifiers;
            self->settings.adjustModifiers = hotkeyDialogAdjustModifiers;
            self->settings.save(&self->settings);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(w);
            int action = id == ID_HOTKEY_APPLY_LABEL ? HOTKEY_ACTION_APPLY :
                id == ID_HOTKEY_RESTORE_LABEL ? HOTKEY_ACTION_RESTORE :
                id == ID_HOTKEY_ADJUST_LABEL ? HOTKEY_ACTION_ADJUST : HOTKEY_ACTION_NONE;
            if (id == ID_TRAY_ALPHA_MENU || id == ID_TRAY_KEYS_MENU || id == ID_TRAY_BACK) {
                RECT rect;
                POINT anchor;
                int page = id == ID_TRAY_ALPHA_MENU ? TRAY_PAGE_ALPHA :
                    id == ID_TRAY_KEYS_MENU ? TRAY_PAGE_KEYS : TRAY_PAGE_HOME;
                GetWindowRect(hwnd, &rect);
                anchor.x = rect.left - 8;
                anchor.y = rect.top - 8;
                DestroyWindow(hwnd);
                showTrayPopupPage(self, page, &anchor);
                return 0;
            }
            if (action != HOTKEY_ACTION_NONE) {
                if (hotkeyRecordingAction == action) {
                    hotkeyRecordingAction = HOTKEY_ACTION_NONE;
                    updateHotkeyDialogLabels();
                    invalidateHotkeyButtons(hwnd);
                } else {
                    updateHotkeyDialogLabels();
                    setHotkeyRecording(hwnd, action);
                }
                return 0;
            }
            if (id == IDCANCEL) {
                if (hotkeyRecordingAction != HOTKEY_ACTION_NONE) {
                    hotkeyRecordingAction = HOTKEY_ACTION_NONE;
                    updateHotkeyDialogLabels();
                    invalidateHotkeyButtons(hwnd);
                } else {
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            if (id == ID_TRAY_MORE) {
                DestroyWindow(hwnd);
                PostMessage(self->trayWindow, WM_TRAY_MORE, 0, 0);
                return 0;
            }
            if (id == 3) {
                DestroyWindow(hwnd);
                self->shuttingDown = true;
                PostQuitMessage(0);
                return 0;
            }
            break;
        }
        case WM_ACTIVATE:
            if (LOWORD(w) == WA_INACTIVE && hotkeyRecordingAction == HOTKEY_ACTION_NONE)
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case WM_ERASEBKGND: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect((HDC)w, &rect, alphaDarkBrush);
            return 1;
        }
        case WM_PRINTCLIENT:
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = msg == WM_PRINTCLIENT ? (HDC)w : BeginPaint(hwnd, &ps);
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect(dc, &client, alphaDarkBrush);
            RECT left = {22, 64, 198, 336};
            if (trayPopupPage == TRAY_PAGE_ALPHA)
                paintTrayCard(dc, left, UI_PANEL, UI_BORDER);
            for (int i = 0; trayPopupPage == TRAY_PAGE_KEYS && i < 3; i++) {
                RECT row = {22, 78 + i * 84, 410, 156 + i * 84};
                paintTrayCard(dc, row, UI_PANEL, hotkeyRecordingAction == i + 1 ? UI_ACCENT : UI_BORDER);
            }
            if (msg == WM_PAINT)
                EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            int id = GetDlgCtrlID((HWND)l);
            boolean card = id == ID_ALPHA_EDIT || id == ID_ALPHA_SLIDER || id == 0 ||
                (id >= ID_TRAY_ROW_TITLE && id < ID_TRAY_ROW_TITLE + 3) ||
                (id >= ID_HOTKEY_APPLY_LABEL && id <= ID_HOTKEY_ADJUST_LABEL);
            SetTextColor((HDC)w, id == ID_ALPHA_EDIT ? UI_ACCENT :
                (id == ID_TRAY_HINT || (id >= ID_HOTKEY_APPLY_LABEL && id <= ID_HOTKEY_ADJUST_LABEL) ? UI_MUTED : UI_TEXT));
            SetBkColor((HDC)w, card ? UI_PANEL : UI_BG);
            return (LRESULT)(card ? trayPanelBrush : alphaDarkBrush);
        }
        case WM_DRAWITEM: {
            int id = (int)w;
            WCHAR label[256];
            GetWindowTextW(((DRAWITEMSTRUCT*)l)->hwndItem, label, 256);
            boolean active = (id == ID_HOTKEY_APPLY_LABEL && hotkeyRecordingAction == HOTKEY_ACTION_APPLY) ||
                (id == ID_HOTKEY_RESTORE_LABEL && hotkeyRecordingAction == HOTKEY_ACTION_RESTORE) ||
                (id == ID_HOTKEY_ADJUST_LABEL && hotkeyRecordingAction == HOTKEY_ACTION_ADJUST);
            drawDarkButton((DRAWITEMSTRUCT*)l, label, active);
            if (((DRAWITEMSTRUCT*)l)->itemState & ODS_FOCUS) {
                RECT focus = ((DRAWITEMSTRUCT*)l)->rcItem;
                InflateRect(&focus, -4, -4);
                DrawFocusRect(((DRAWITEMSTRUCT*)l)->hDC, &focus);
            }
            return true;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (hotkeyDialogWindow == hwnd)
                hotkeyDialogWindow = null;
            trayPopupWindow = null;
            hotkeyRecordingAction = HOTKEY_ACTION_NONE;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, w, l);
}

static void showTrayPopupPage(App* self, int page, const POINT* position) {
    WNDCLASSW wc = {0};
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    POINT anchor;
    if (IsWindow(trayPopupWindow)) {
        SetForegroundWindow(trayPopupWindow);
        return;
    }
    InitCommonControlsEx(&icc);
    ensureUiResources();
    wc.lpfnWndProc = trayPopupProc;
    wc.hInstance = GetModuleHandle(null);
    wc.hCursor = LoadCursor(null, IDC_ARROW);
    wc.hbrBackground = alphaDarkBrush;
    wc.lpszClassName = L"TransparencyTrayPopup";
    RegisterClassW(&wc);
    if (position)
        anchor = *position;
    else
        GetCursorPos(&anchor);
    trayPopupWindow = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, tr(STR_APP_NAME),
        WS_POPUP | WS_BORDER | WS_CLIPCHILDREN, 0, 0, page == TRAY_PAGE_ALPHA ? 224 : 436,
        page == TRAY_PAGE_HOME ? 248 : 432, self->trayWindow, null, wc.hInstance, (LPVOID)(INT_PTR)page);
    if (trayPopupWindow) {
        positionPopupWindow(trayPopupWindow, anchor);
        ShowWindow(trayPopupWindow, SW_SHOWNORMAL);
        SetForegroundWindow(trayPopupWindow);
        SetFocus(GetDlgItem(trayPopupWindow, page == TRAY_PAGE_ALPHA ? ID_ALPHA_SLIDER :
            page == TRAY_PAGE_KEYS ? ID_HOTKEY_APPLY_LABEL : ID_TRAY_ALPHA_MENU));
    }
}

static void showTrayPopup(App* self) {
    showTrayPopupPage(self, TRAY_PAGE_HOME, null);
}

static LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    App* self = appContext;
    MEASUREITEMSTRUCT* measure;
    DRAWITEMSTRUCT* draw;
    MenuItemData* item;
    SIZE size;

    if (!self)
        return DefWindowProc(hwnd, msg, w, l);

    if (msg == WM_MEASUREITEM) {
        measure = (MEASUREITEMSTRUCT*)l;
        item = (MenuItemData*)measure->itemData;

        if (measure->CtlType == ODT_MENU && item) {
            HDC dc = GetDC(hwnd);
            HGDIOBJ oldFont = null;

            ensureUiResources();
            if (uiFont)
                oldFont = SelectObject(dc, uiFont);
            GetTextExtentPoint32W(dc, item->text, (int)wcslen(item->text), &size);
            if (oldFont)
                SelectObject(dc, oldFont);
            ReleaseDC(hwnd, dc);

            measure->itemWidth = item->separator ? 180 : size.cx + 72;
            measure->itemHeight = item->separator ? 11 : 32;
            return true;
        }
    }

    if (msg == WM_DRAWITEM) {
        draw = (DRAWITEMSTRUCT*)l;
        item = (MenuItemData*)draw->itemData;

        if (draw->CtlType == ODT_MENU && item) {
            drawTrayMenuItem(draw, item);
            return true;
        }
    }

    if (msg == self->taskbarCreatedMessage) {
        self->trayIconAdded = false;
        addTrayIcon(self);
        return 0;
    }

    if (msg == WM_TIMER && w == TRAY_RETRY_TIMER_ID) {
        addTrayIcon(self);
        return 0;
    }

    if (msg == WM_TIMER && w == TRAY_STATUS_TIMER_ID) {
        if (statusMenuOpen)
            EndMenu();
        return 0;
    }

    if (msg == WM_TRAY && isTrayContextMenu(l)) {
        showTrayPopup(self);
        return 0;
    }

    if (msg == WM_TRAY_MORE) {
        HMENU root = CreatePopupMenu();
        HMENU setting = CreatePopupMenu();
        HMENU preset  = CreatePopupMenu();
        HMENU language = CreatePopupMenu();
        HBRUSH menuBrush = CreateSolidBrush(UI_MENU_BG);
        MENUINFO menuInfo = {0};
        LanguageMode displayLanguage = effectiveLanguage(self);
        ensureUiResources();
        resetMenuItems();

        menuInfo.cbSize = sizeof(menuInfo);
        menuInfo.fMask = MIM_BACKGROUND;
        menuInfo.hbrBack = menuBrush;
        SetMenuInfo(root, &menuInfo);
        SetMenuInfo(setting, &menuInfo);
        SetMenuInfo(preset, &menuInfo);
        SetMenuInfo(language, &menuInfo);

        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, tr(STR_APP_NAME), false, false);
        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, tr(STR_LICENSE), false, false);
        appendDarkSeparator(root);

        appendDarkMenu(setting, MF_STRING, ID_SETTING_EXPLORER, tr(STR_EXPLORER_AUTO), self->settings.explorerAuto, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_POPUPS, tr(STR_POPUP_TRANSPARENCY), self->settings.popupTransparency, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_STARTUP, tr(STR_RUN_AT_STARTUP), self->settings.startupEnabled, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_HOTKEYS, tr(STR_HOTKEYS), false, false);
        (void)displayLanguage;
        appendDarkMenu(language, MF_STRING, ID_LANG_SYSTEM, tr(STR_LANGUAGE_SYSTEM), self->settings.language == LANGUAGE_SYSTEM, false);
        appendDarkMenu(language, MF_STRING, ID_LANG_ENGLISH, tr(STR_LANGUAGE_ENGLISH), self->settings.language == LANGUAGE_ENGLISH, false);
        appendDarkMenu(language, MF_STRING, ID_LANG_KOREAN, tr(STR_LANGUAGE_KOREAN), self->settings.language == LANGUAGE_KOREAN, false);
        appendDarkMenu(setting, MF_POPUP, (UINT_PTR)language, tr(STR_LANGUAGE), false, true);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_FOLDER, tr(STR_OPEN_INSTALL_FOLDER), false, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_RESET, tr(STR_RESET_SETTINGS), false, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_SOLID, tr(STR_PRESET_SOLID), self->settings.preset == PRESET_SOLID, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_SOFT, tr(STR_PRESET_SOFT), self->settings.preset == PRESET_SOFT, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_GLASS, tr(STR_PRESET_GLASS), self->settings.preset == PRESET_GLASS, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_GHOST, tr(STR_PRESET_GHOST), self->settings.preset == PRESET_GHOST, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_CUSTOM, tr(STR_CUSTOM_ALPHA), self->settings.preset == PRESET_CUSTOM, false);

        appendDarkMenu(setting, MF_POPUP, (UINT_PTR)preset, tr(STR_PRESET), false, true);
        appendDarkSeparator(setting);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_UNINSTALL, tr(STR_UNINSTALL), false, false);
        appendDarkMenu(root, MF_POPUP, (UINT_PTR)setting, tr(STR_SETTING), false, true);

        appendDarkSeparator(root);
        appendDarkMenu(root, MF_STRING, ID_UPDATE_CHECK, tr(STR_CHECK_FOR_UPDATES), false, false);
        appendDarkMenu(root, MF_STRING, ID_LOG_OPEN, tr(STR_OPEN_LOG), false, false);
        appendDarkSeparator(root);
        appendDarkMenu(root, MF_STRING | MF_DISABLED, 1, tr(STR_DEVELOPED_BY), false, false);
        appendDarkMenu(root, MF_STRING, 2, L"GitHub", false, false);
        appendDarkSeparator(root);
        appendDarkMenu(root, MF_STRING, 3, tr(STR_EXIT), false, false);

        POINT p;
        POINT statusPoint;
        GetCursorPos(&p);
        SetForegroundWindow(hwnd);

        UINT cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, null);
        PostMessage(hwnd, WM_NULL, 0, 0);
        GetCursorPos(&statusPoint);

        DestroyMenu(root);
        DeleteObject(menuBrush);

        switch (cmd) {
            case 1:
                ShellExecuteA(null, "open", "https://github.com/sunwookim05", null, null, SW_SHOWNORMAL);
                break;

            case 2:
                ShellExecuteA(null, "open", "https://github.com/sunwookim05/Transparent-window", null, null, SW_SHOWNORMAL);
                break;

            case 3:
                self->shuttingDown = true;
                PostQuitMessage(0);
                break;

            case ID_SETTING_EXPLORER:
                self->settings.explorerAuto = !self->settings.explorerAuto;
                self->settings.save(&self->settings);
                self->applyExplorerAutoAll(self);
                break;

            case ID_SETTING_POPUPS:
                self->settings.popupTransparency = !self->settings.popupTransparency;
                self->settings.save(&self->settings);
                break;

            case ID_SETTING_STARTUP:
                self->settings.startupEnabled = !self->settings.startupEnabled;
                if (self->settings.startupEnabled)
                    registerStartupTask();
                else
                    unregisterStartupTask();
                self->settings.save(&self->settings);
                break;

            case ID_SETTING_HOTKEYS:
                showTrayPopupPage(self, TRAY_PAGE_KEYS, null);
                break;

            case ID_LANG_SYSTEM:
            case ID_LANG_ENGLISH:
            case ID_LANG_KOREAN:
                self->settings.language = (LanguageMode)(cmd - ID_LANG_SYSTEM);
                self->settings.save(&self->settings);
                break;

            case ID_SETTING_FOLDER:
                openInstallFolder();
                break;

            case ID_SETTING_RESET:
                self->settings.reset(&self->settings);
                self->settings.save(&self->settings);
                registerStartupTask();
                self->applyExplorerAutoAll(self);
                break;

            case ID_SETTING_UNINSTALL:
                uninstallApp(self, hwnd);
                break;

            case ID_PRESET_SOLID:
            case ID_PRESET_SOFT:
            case ID_PRESET_GLASS:
            case ID_PRESET_GHOST:
                self->settings.preset = (TransparencyPreset)(cmd - ID_PRESET_SOLID);
                self->settings.save(&self->settings);
                self->applyExplorerAutoAll(self);
                break;

            case ID_PRESET_CUSTOM:
                if (askAlpha(hwnd, &self->settings.customAlpha)) {
                    self->settings.preset = PRESET_CUSTOM;
                    self->settings.save(&self->settings);
                    self->applyExplorerAutoAll(self);
                }
                break;

            case ID_UPDATE_CHECK: {
                Installer installer = new_Installer();
                Updater updater = new_Updater(installer);
                UpdateResult result = updater.checkNow(&updater);

                if (result == UPDATE_CURRENT)
                    showStatusMenu(hwnd, statusPoint, tr(STR_ALREADY_CURRENT));
                else if (result == UPDATE_FAILED)
                    showStatusMenu(hwnd, statusPoint, tr(STR_UPDATE_FAILED));
                else if (result == UPDATE_SKIPPED)
                    showStatusMenu(hwnd, statusPoint, tr(STR_UPDATE_SKIPPED));
                break;
            }

            case ID_LOG_OPEN:
                openLog();
                break;

        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, w, l);
}

static LRESULT CALLBACK keyboardHook(int code, WPARAM w, LPARAM l) {
    App* self = appContext;

    if (!self || code != HC_ACTION || self->shuttingDown)
        return CallNextHookEx(null, code, w, l);

    KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)l;

    const boolean down = (w == WM_KEYDOWN || w == WM_SYSKEYDOWN);
    const boolean up   = (w == WM_KEYUP   || w == WM_SYSKEYUP);

    if (down && k->vkCode == VK_ESCAPE && trayPopupWindow && hotkeyRecordingAction != HOTKEY_ACTION_NONE) {
        PostMessage(trayPopupWindow, WM_COMMAND, IDCANCEL, 0);
        return 1;
    }

    if (k->vkCode == VK_CONTROL || k->vkCode == VK_LCONTROL || k->vkCode == VK_RCONTROL) {
        if (down) self->ctrlDown = true;
        else if (up) self->ctrlDown = false;

        return CallNextHookEx(null, code, w, l);
    }

    if (k->vkCode == VK_MENU || k->vkCode == VK_LMENU || k->vkCode == VK_RMENU) {
        if (down) self->altDown = true;
        else if (up) self->altDown = false;

        return CallNextHookEx(null, code, w, l);
    }

    if (k->vkCode == VK_SHIFT || k->vkCode == VK_LSHIFT || k->vkCode == VK_RSHIFT) {
        if (down) self->shiftDown = true;
        else if (up) self->shiftDown = false;

        return CallNextHookEx(null, code, w, l);
    }

    if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN) {
        if (down) {
            self->winDown = true;
            return CallNextHookEx(null, code, w, l);
        }

        if (up) {
            self->winDown = false;
            if (self->winUsed) {
                self->winUsed = false;
                INPUT in[3] = {0};

                in[0].type = INPUT_KEYBOARD;
                in[0].ki.wVk = VK_CONTROL;

                in[1].type = INPUT_KEYBOARD;
                in[1].ki.wVk = k->vkCode;
                in[1].ki.dwFlags = KEYEVENTF_KEYUP;

                in[2].type = INPUT_KEYBOARD;
                in[2].ki.wVk = VK_CONTROL;
                in[2].ki.dwFlags = KEYEVENTF_KEYUP;

                SendInput(3, in, sizeof(INPUT));

                return 1;
            }

            return CallNextHookEx(null, code, w, l);
        }

        return CallNextHookEx(null, code, w, l);
    }

    return CallNextHookEx(null, code, w, l);
}

static LRESULT CALLBACK mouseHook(int code, WPARAM w, LPARAM l) {
    App* self = appContext;

    if (!self || code != HC_ACTION || self->shuttingDown)
        return CallNextHookEx(null, code, w, l);

    DWORD modifiers = getCurrentModifiers(self);

    if (hotkeyRecordingAction != HOTKEY_ACTION_NONE) {
        if ((w == WM_MBUTTONDOWN && hotkeyRecordingAction != HOTKEY_ACTION_ADJUST) ||
            (w == WM_MOUSEWHEEL && hotkeyRecordingAction == HOTKEY_ACTION_ADJUST)) {
            if (trayPopupWindow && hotkeyDialogWindow == trayPopupWindow) {
                if (modifiers & HOTKEY_MOD_WIN)
                    self->winUsed = true;
                PostMessage(trayPopupWindow, WM_TRAY_RECORD, modifiers, hotkeyRecordingAction);
            } else {
                finishHotkeyRecording(modifiers);
            }
            return 1;
        }

        return CallNextHookEx(null, code, w, l);
    }

    if (modifiers == 0)
        return CallNextHookEx(null, code, w, l);

    MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l;
    HWND target = GetAncestor(WindowFromPoint(m->pt), GA_ROOT);
    if (!target)
        return CallNextHookEx(null, code, w, l);
    if (target == trayPopupWindow)
        return CallNextHookEx(null, code, w, l);

    if (w == WM_MBUTTONDOWN) {
        if (modifiersMatch(modifiers, self->settings.applyModifiers) && self->transparency.apply(&self->transparency, target,
                getCurrentAlpha(self))) {
            self->transparency.refresh(&self->transparency, target);
            if (self->settings.applyModifiers & HOTKEY_MOD_WIN)
                self->winUsed = true;
            return 1;
        }

        if (modifiersMatch(modifiers, self->settings.restoreModifiers) && self->transparency.apply(&self->transparency, target, ALPHA_OPAQUE)) {
            self->transparency.refresh(&self->transparency, target);
            if (self->settings.restoreModifiers & HOTKEY_MOD_WIN) {
                self->winUsed = true;
                self->winDown = false;
            }

            return 1;
        }
    }

    if (w == WM_MOUSEWHEEL && modifiersMatch(modifiers, self->settings.adjustModifiers)) {
        int delta = GET_WHEEL_DELTA_WPARAM(m->mouseData);
        BYTE alpha = self->transparency.getWindowAlpha(&self->transparency, target);

        alpha = (delta > 0) ? min(255, alpha + 15) : max(60, alpha - 15);

        if (self->transparency.apply(&self->transparency, target, alpha))
            self->transparency.refresh(&self->transparency, target);

        if (self->settings.adjustModifiers & HOTKEY_MOD_WIN)
            self->winUsed = true;

        return 1;
    }

    return CallNextHookEx(null, code, w, l);
}

static void load(App* self) {
    self->settings.load(&self->settings);
    EnumWindows(enumExplorerWindows, (LPARAM)self);
}

static void run(App* self) {
    WNDCLASSA wc = {0};
    appContext = self;
    self->taskbarCreatedMessage = RegisterWindowMessageA("TaskbarCreated");

    wc.lpfnWndProc = trayWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "TransparencyTray";
    RegisterClassA(&wc);

    self->trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPED | WS_SYSMENU, 0, 0, 0, 0, null, null, wc.hInstance, null);

    addTrayIcon(self);

    self->winEventHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, null, winEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT);
    self->keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHook, null, 0);
    self->mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHook, null, 0);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {
        if (trayPopupWindow && IsDialogMessage(trayPopupWindow, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (trayPopupWindow)
        DestroyWindow(trayPopupWindow);
    KillTimer(self->trayWindow, TRAY_RETRY_TIMER_ID);
    removeTrayIcon(self);

    if (self->winEventHook) UnhookWinEvent(self->winEventHook);
    if (self->keyHook) UnhookWindowsHookEx(self->keyHook);
    if (self->mouseHook) UnhookWindowsHookEx(self->mouseHook);

    appContext = null;
}

App new_App(void) {
    return (App) {
        .settings = new_Settings(),
        .transparency = new_Transparency(),
        .tracker = new_Tracker(),
        .ctrlDown = false,
        .altDown = false,
        .shiftDown = false,
        .winDown = false,
        .winUsed = false,
        .shuttingDown = false,
        .keyHook = null,
        .mouseHook = null,
        .winEventHook = null,
        .trayWindow = null,
        .trayIconAdded = false,
        .taskbarCreatedMessage = 0,
        .load = load,
        .run = run,
        .applyExplorerAutoAll = applyExplorerAutoAll
    };
}
