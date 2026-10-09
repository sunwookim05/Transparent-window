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
#define WM_TRAY_RECORD (WM_USER + 3)
#define ID_TRAY_HINT 1302
#define ID_TRAY_ALPHA_TITLE 1303
#define ID_TRAY_HOTKEY_TITLE 1304
#define ID_TRAY_ROW_TITLE 1310
#define ID_TRAY_BACK 1322
#define TRAY_ID 1
#define TRAY_RETRY_TIMER_ID 100
#define TRAY_STATUS_TIMER_ID 101
#define TRAY_MENU_TIMER_ID 102
#define ID_INLINE_ALPHA 1400
#define ID_INLINE_APPLY 1401
#define ID_INLINE_RESTORE 1402
#define ID_INLINE_ADJUST 1403

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
static HBRUSH alphaDarkBrush = null;
static HBRUSH alphaEditBrush = null;
static HFONT uiFont = null;
static boolean confirmDialogOk = false;
static BYTE confirmDialogAlpha = ALPHA_OPAQUE;
static boolean statusMenuOpen = false;


static HMENU inlineAlphaMenu = null;
static HMENU inlineKeysMenu = null;
static HWND inlineMenuWindow = null;
static HMENU inlineActiveMenu = null;
static boolean inlinePointerPressed = false;
#define WM_INLINE_POINTER (WM_USER + 5)
static int inlineRecordingAction = HOTKEY_ACTION_NONE;
static boolean inlineDragging = false;
static HMENU selectedInlineMenu = null;
static UINT selectedInlineItem = 0;
static DWORD recordedModifierKeys = 0;
static HHOOK menuMessageHook = NULL;
static HHOOK menuPointerHook = NULL;
static HANDLE menuOpacityTimer = NULL;
static boolean inlineMiddlePressed = false;
#define WM_INLINE_START (WM_USER + 4)


typedef struct {
    WCHAR text[160];
    boolean checked;
    boolean submenu;
    boolean separator;
    UINT id;
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
    item->id = 0;
    return item;
}

static void appendDarkMenu(HMENU menu, UINT flags, UINT_PTR id, const WCHAR* text, boolean checked, boolean submenu) {
    MenuItemData* item = newMenuItem(text, checked, submenu, false);
    if (item) item->id = submenu ? 0 : (UINT)id;
    AppendMenuW(menu, flags | MF_OWNERDRAW, id, (LPCWSTR)item);
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
        case STR_HOTKEYS: return ko ? L"단축키" : L"Hotkeys";
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
        case STR_CUSTOM_ALPHA: return ko ? L"사용자 지정 투명도" : L"Custom Alpha";
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
        alphaDarkBrush);

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

static boolean isOwnTrayMenu(HWND hwnd) {
    char cls[64] = {0};
    GetClassNameA(hwnd, cls, sizeof(cls));
    return !strcmp(cls, "#32768") && GetWindowThreadProcessId(hwnd, NULL) == GetCurrentThreadId();
}

static boolean isPopupMenuWindow(HWND hwnd) {
    char cls[128];
    LONG exStyle;
    LONG style;

    if (!IsWindow(hwnd))
        return false;

    GetClassNameA(hwnd, cls, sizeof(cls));
    if (!strcmp(cls, "#32768"))
        return true;
    if (!strcmp(cls, "NotifyIconOverflowWindow") || !strcmp(cls, "TopLevelWindowForOverflowXamlIsland"))
        return true;

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

static boolean applyTrackedAlpha(App* self, HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;
    self->tracker.track(&self->tracker, &self->transparency, hwnd);
    if (!self->tracker.isTracked(&self->tracker, hwnd)) return false;
    return self->transparency.apply(&self->transparency, hwnd, alpha);
}

static void applyPopupTransparency(App* self, HWND hwnd) {
    BYTE alpha;

    if (!self || (!self->settings.popupTransparency && !isOwnTrayMenu(hwnd)) || !isPopupMenuWindow(hwnd))
        return;

    alpha = getCurrentAlpha(self);
    applyTrackedAlpha(self, hwnd, alpha);
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

static BOOL CALLBACK enumExplorerWindows(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return true;

    if (!self->settings.explorerAuto)
        return true;

    if (!isAutoTarget(self, hwnd))
        return true;

    BYTE alpha = getCurrentAlpha(self);
    applyTrackedAlpha(self, hwnd, alpha);
    return true;
}

static BOOL CALLBACK applyExplorerAutoWindow(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (isAutoTarget(self, hwnd)) {
        BYTE alpha = self->settings.explorerAuto ?
            getCurrentAlpha(self) :
            ALPHA_OPAQUE;

        applyTrackedAlpha(self, hwnd, alpha);
        self->transparency.refresh(&self->transparency, hwnd);

    }

    if (isPopupMenuWindow(hwnd)) {
        if (self->settings.popupTransparency || isOwnTrayMenu(hwnd))
            applyPopupTransparency(self, hwnd);
        else if (self->tracker.isTracked(&self->tracker, hwnd)) {
            for (int i = 0; i < self->tracker.count; i++) {
                if (self->tracker.windows[i].hwnd == hwnd)
                    self->transparency.apply(&self->transparency, hwnd, self->tracker.windows[i].originalAlpha);
            }
        }
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

    if (!self)
        return;

    if (event == EVENT_OBJECT_DESTROY && obj == OBJID_WINDOW) {
        self->tracker.remove(&self->tracker, hwnd);
        return;
    }
    if (!IsWindow(hwnd)) return;

    GetClassNameA(hwnd, cls, sizeof(cls));

    if (((event == EVENT_OBJECT_SHOW && obj == OBJID_WINDOW) ||
         event == EVENT_SYSTEM_MENUPOPUPSTART) && isPopupMenuWindow(hwnd)) {
        // Menu windows can be reused and Windows can reset their layered alpha.
        // Reapply on every opening, preserving the first snapshot in the tracker.
        if (!isOwnTrayMenu(hwnd)) applyPopupTransparency(self, hwnd);
        return;
    }

    if (obj == OBJID_WINDOW && IsWindowVisible(hwnd) && isAutoTarget(self, hwnd)) {
        if (!strcmp(cls, "TaskSwitcherWnd") || !strcmp(cls, "MultitaskingViewFrame"))
            return;

        if (!self->settings.explorerAuto)
            return;

        applyTrackedAlpha(self, hwnd, getCurrentAlpha(self));
    }

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

static int inlineAction(UINT id) {
    return id >= ID_INLINE_APPLY && id <= ID_INLINE_ADJUST ? (int)(id - ID_INLINE_APPLY + 1) : HOTKEY_ACTION_NONE;
}

static void changeInlineAlpha(HWND hwnd, int y) {
    RECT rect;
    if (!GetMenuItemRect(appContext->trayWindow, inlineAlphaMenu, 0, &rect)) return;
    POINT point = {0, y};
    ClientToScreen(hwnd, &point);
    int top = rect.top + 60;
    int bottom = rect.bottom - 24;
    int value = 255 - ((point.y - top) * 195 + (bottom - top) / 2) / (bottom - top);
    value = max(60, min(255, value));
    appContext->settings.preset = PRESET_CUSTOM;
    appContext->settings.customAlpha = (BYTE)value;
    appContext->settings.save(&appContext->settings);
    appContext->applyExplorerAutoAll(appContext);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

static void startInlineRecording(HWND hwnd, UINT id) {
    int action = inlineAction(id);
    if (!action) return;
    inlineRecordingAction = inlineRecordingAction == action ? HOTKEY_ACTION_NONE : action;
    inlineMenuWindow = hwnd;
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

static LRESULT CALLBACK inlineMenuProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    HMENU menu = (HMENU)ref;
    if (menu == inlineAlphaMenu) {
        if (msg == WM_LBUTTONDOWN || (msg == WM_MOUSEMOVE && inlineDragging)) {
            if (msg == WM_LBUTTONDOWN) inlineDragging = true;
            changeInlineAlpha(hwnd, (short)HIWORD(l));
            return 0;
        }
        if (msg == WM_LBUTTONUP) {
            if (inlineDragging) changeInlineAlpha(hwnd, (short)HIWORD(l));
            inlineDragging = false;
            return 0;
        }
        if (msg == WM_CAPTURECHANGED) inlineDragging = false;
        if (msg == WM_KEYDOWN && (w == VK_UP || w == VK_DOWN || w == VK_HOME || w == VK_END)) {
            int alpha = getCurrentAlpha(appContext);
            alpha = w == VK_HOME ? 255 : w == VK_END ? 60 : alpha + (w == VK_UP ? 1 : -1);
            appContext->settings.preset = PRESET_CUSTOM;
            appContext->settings.customAlpha = (BYTE)max(60, min(255, alpha));
            appContext->settings.save(&appContext->settings);
            appContext->applyExplorerAutoAll(appContext);
            InvalidateRect(hwnd, NULL, false);
            return 0;
        }
    } else if (menu == inlineKeysMenu && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)) {
        if (msg == WM_LBUTTONUP) {
            POINT point = {(short)LOWORD(l), (short)HIWORD(l)};
            ClientToScreen(hwnd, &point);
            for (UINT i = 0; i < 3; i++) {
                RECT rect;
                if (GetMenuItemRect(appContext->trayWindow, menu, i, &rect) && PtInRect(&rect, point))
                    startInlineRecording(hwnd, ID_INLINE_APPLY + i);
            }
        }
        return 0;
    }
    if (msg == WM_SHOWWINDOW && !w && menu == inlineKeysMenu)
        inlineRecordingAction = HOTKEY_ACTION_NONE;
    if (msg == WM_NCDESTROY) {
        appContext->tracker.remove(&appContext->tracker, hwnd);
        if (inlineMenuWindow == hwnd) { inlineMenuWindow = null; inlineRecordingAction = HOTKEY_ACTION_NONE; }
        RemoveWindowSubclass(hwnd, inlineMenuProc, id);
    }
    return DefSubclassProc(hwnd, msg, w, l);
}

static BOOL CALLBACK attachInlineMenus(HWND hwnd, LPARAM param) {
    App* self = (App*)param;
    char cls[64];
    RECT windowRect;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "#32768") || !IsWindowVisible(hwnd)) return true;
    GetWindowRect(hwnd, &windowRect);
    HMENU candidates[] = {inlineAlphaMenu, inlineKeysMenu};
    for (int i = 0; i < 2; i++) {
        RECT itemRect;
        if (candidates[i] && GetMenuItemRect(self->trayWindow, candidates[i], 0, &itemRect) &&
            itemRect.right > itemRect.left && itemRect.bottom > itemRect.top &&
            itemRect.left >= windowRect.left && itemRect.right <= windowRect.right &&
            itemRect.top >= windowRect.top && itemRect.bottom <= windowRect.bottom) {
            SetWindowSubclass(hwnd, inlineMenuProc, 77, (DWORD_PTR)candidates[i]);
            inlineMenuWindow = hwnd;
            inlineActiveMenu = candidates[i];
        }
    }
    if (self->transparency.getWindowAlpha(&self->transparency, hwnd) != getCurrentAlpha(self)) {
        applyPopupTransparency(self, hwnd);
        // Run from the menu loop, after native positioning/painting has returned.
        // A layered menu needs a fresh paint without waiting for mouse input.
        RedrawWindow(hwnd, NULL, NULL,
            RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
    return true;
}

static BOOL CALLBACK detachInlineMenus(HWND hwnd, LPARAM param) {
    (void)param;
    RemoveWindowSubclass(hwnd, inlineMenuProc, 77);
    return true;
}

static BOOL CALLBACK updateVisibleMenuOpacity(HWND hwnd, LPARAM param) {
    App* self = (App*)param;
    char cls[64] = {0};
    RECT rect;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "#32768") || !IsWindowVisible(hwnd) ||
        !GetWindowRect(hwnd, &rect) || rect.right <= rect.left || rect.bottom <= rect.top)
        return TRUE;
    BYTE alpha = getCurrentAlpha(self);
    if (self->transparency.getWindowAlpha(&self->transparency, hwnd) != alpha &&
        self->transparency.apply(&self->transparency, hwnd, alpha)) {
        // This worker runs even when the native menu loop is waiting for input.
        // Own menu windows are transient; keep the shared tracker on the UI thread.
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
        DWORD_PTR result;
        SendMessageTimeout(self->trayWindow, WM_NULL, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result);
    }
    return TRUE;
}

static VOID CALLBACK menuOpacityTick(PVOID context, BOOLEAN fired) {
    App* self = (App*)context;
    (void)fired;
    EnumThreadWindows(GetWindowThreadProcessId(self->trayWindow, NULL),
        updateVisibleMenuOpacity, (LPARAM)self);
}

static boolean handleInlinePointer(UINT message, POINT screenPoint) {
    if (!appContext || (message != WM_LBUTTONDOWN && message != WM_LBUTTONUP &&
        !(message == WM_MOUSEMOVE && inlinePointerPressed))) return false;
    EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)appContext);
    if (!inlineMenuWindow || !inlineActiveMenu || !IsWindowVisible(inlineMenuWindow)) return false;
    RECT windowRect;
    GetWindowRect(inlineMenuWindow, &windowRect);
    boolean hit = inlinePointerPressed;
    for (UINT i = 0; !hit && i < (inlineActiveMenu == inlineKeysMenu ? 3u : 1u); i++) {
        RECT row;
        if (GetMenuItemRect(appContext->trayWindow, inlineActiveMenu, i, &row) &&
            row.left >= windowRect.left && row.right <= windowRect.right &&
            PtInRect(&windowRect, screenPoint) && PtInRect(&row, screenPoint)) hit = true;
    }
    if (!hit) return false;
    if (message == WM_LBUTTONDOWN) inlinePointerPressed = true;
    if (message == WM_LBUTTONUP) inlinePointerPressed = false;
    ScreenToClient(inlineMenuWindow, &screenPoint);
    inlineMenuProc(inlineMenuWindow, message,
        message == WM_MOUSEMOVE ? MK_LBUTTON : 0, MAKELPARAM(screenPoint.x, screenPoint.y),
        77, (DWORD_PTR)inlineActiveMenu);
    return true;
}

static LRESULT CALLBACK menuPointerCallback(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        MOUSEHOOKSTRUCT* pointer = (MOUSEHOOKSTRUCT*)l;
        UINT message = (UINT)w;
        if (message == WM_NCLBUTTONDOWN) message = WM_LBUTTONDOWN;
        if (message == WM_NCLBUTTONUP) message = WM_LBUTTONUP;
        if (message == WM_NCMOUSEMOVE) message = WM_MOUSEMOVE;
        if (handleInlinePointer(message, pointer->pt)) return 1;
    }
    return CallNextHookEx(menuPointerHook, code, w, l);
}

static LRESULT CALLBACK menuMessageCallback(int code, WPARAM w, LPARAM l) {
    if (code == MSGF_MENU) {
        MSG* message = (MSG*)l;
        if (handleInlinePointer(message->message, message->pt)) return 1;
    }
    return CallNextHookEx(menuMessageHook, code, w, l);
}

static void CALLBACK inlineMenuTimer(HWND hwnd, UINT msg, UINT_PTR timer, DWORD time) {
    (void)hwnd; (void)msg; (void)timer; (void)time;
    if (appContext)
        EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)appContext);
}

static void drawInlineMenu(DRAWITEMSTRUCT* draw, MenuItemData* item) {
    RECT rect = draw->rcItem;
    HGDIOBJ font = SelectObject(draw->hDC, uiFont);
    SetBkMode(draw->hDC, TRANSPARENT);
    FillRect(draw->hDC, &rect, alphaDarkBrush);
    if (inlineAction(item->id) && (draw->itemState & ODS_SELECTED))
        paintTrayCard(draw->hDC, rect, UI_MENU_HOVER, UI_MENU_HOVER);
    if (item->id == ID_INLINE_ALPHA) {
        WCHAR value[80];
        swprintf(value, 80, L"%s  %u", effectiveLanguage(appContext) == LANGUAGE_KOREAN ? L"불투명도" : L"Opacity",
            (unsigned)getCurrentAlpha(appContext));
        RECT title = rect; title.top += 12; title.bottom = title.top + 24;
        SetTextColor(draw->hDC, UI_TEXT);
        DrawTextW(draw->hDC, value, -1, &title, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        RECT track = {(rect.left + rect.right) / 2 - 3, rect.top + 60,
            (rect.left + rect.right) / 2 + 3, rect.bottom - 24};
        paintTrayCard(draw->hDC, track, UI_BORDER, UI_BORDER);
        int y = track.top + ((255 - getCurrentAlpha(appContext)) * (track.bottom - track.top)) / 195;
        RECT active = track; active.top = y;
        paintTrayCard(draw->hDC, active, UI_ACCENT, UI_ACCENT);
        RECT knob = {track.left - 5, y - 8, track.right + 5, y + 8};
        paintTrayCard(draw->hDC, knob, UI_TEXT, UI_TEXT);
        RECT limit = rect;
        limit.top += 38; limit.bottom = limit.top + 18;
        SetTextColor(draw->hDC, UI_MUTED);
        DrawTextW(draw->hDC, L"255", -1, &limit, DT_CENTER | DT_SINGLELINE);
        limit.top = rect.bottom - 18; limit.bottom = rect.bottom;
        DrawTextW(draw->hDC, L"60", -1, &limit, DT_CENTER | DT_SINGLELINE);
    } else {
        int action = inlineAction(item->id);
        WCHAR shortcut[128];
        DWORD modifiers = action == HOTKEY_ACTION_APPLY ? appContext->settings.applyModifiers :
            action == HOTKEY_ACTION_RESTORE ? appContext->settings.restoreModifiers : appContext->settings.adjustModifiers;
        RECT title = rect; title.left += 14; title.top += 8; title.bottom = title.top + 20;
        RECT value = title; value.top += 24; value.bottom += 24;
        SetTextColor(draw->hDC, UI_TEXT);
        DrawTextW(draw->hDC, item->text, -1, &title, DT_SINGLELINE | DT_VCENTER);
        shortcutToText(modifiers, tr(action == HOTKEY_ACTION_ADJUST ? STR_MOUSE_WHEEL : STR_MIDDLE_CLICK), shortcut, 128);
        SetTextColor(draw->hDC, inlineRecordingAction == action ? UI_ACCENT : UI_MUTED);
        DrawTextW(draw->hDC, inlineRecordingAction == action ? tr(action == HOTKEY_ACTION_ADJUST ? STR_HOLD_WHEEL : STR_HOLD_MIDDLE) : shortcut,
            -1, &value, DT_SINGLELINE | DT_VCENTER);
    }
    SelectObject(draw->hDC, font);
}

static LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    App* self = appContext;
    MEASUREITEMSTRUCT* measure;
    DRAWITEMSTRUCT* draw;
    MenuItemData* item;
    SIZE size;

    if (!self)
        return DefWindowProc(hwnd, msg, w, l);

    if (msg == WM_INLINE_POINTER) {
        if (!inlineMenuWindow || !IsWindowVisible(inlineMenuWindow) || !inlineActiveMenu) return 0;
        POINT point = {(short)LOWORD(l), (short)HIWORD(l)};
        ScreenToClient(inlineMenuWindow, &point);
        inlineMenuProc(inlineMenuWindow, (UINT)w, w == WM_MOUSEMOVE ? MK_LBUTTON : 0,
            MAKELPARAM(point.x, point.y), 77, (DWORD_PTR)inlineActiveMenu);
        return 0;
    }

    if (msg == WM_TIMER && w == TRAY_MENU_TIMER_ID) {
        EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)self);
        return 0;
    }
    if (msg == WM_INITMENUPOPUP) {
        EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)self);
        return 0;
    }
    if (msg == WM_MENUSELECT) {
        selectedInlineMenu = (HMENU)l;
        selectedInlineItem = LOWORD(w);
        return 0;
    }
    if (msg == WM_UNINITMENUPOPUP && (HMENU)w == inlineKeysMenu) {
        inlineRecordingAction = HOTKEY_ACTION_NONE;
        return 0;
    }
    if (msg == WM_INLINE_START) {
        startInlineRecording(inlineMenuWindow, (UINT)w);
        return 0;
    }
    if (msg == WM_TRAY_RECORD && inlineRecordingAction) {
        DWORD modifiers = (DWORD)w;
        int action = inlineRecordingAction;
        if (!modifiers || action != (int)l || countHotkeyModifiers(modifiers) > HOTKEY_MAX_MODIFIERS) return 0;
        if ((action == HOTKEY_ACTION_APPLY && modifiers == self->settings.restoreModifiers) ||
            (action == HOTKEY_ACTION_RESTORE && modifiers == self->settings.applyModifiers)) {
            MessageBeep(MB_ICONWARNING);
            return 0;
        }
        if (action == HOTKEY_ACTION_APPLY) self->settings.applyModifiers = modifiers;
        if (action == HOTKEY_ACTION_RESTORE) self->settings.restoreModifiers = modifiers;
        if (action == HOTKEY_ACTION_ADJUST) self->settings.adjustModifiers = modifiers;
        self->settings.save(&self->settings);
        inlineRecordingAction = HOTKEY_ACTION_NONE;
        RedrawWindow(inlineMenuWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
        return 0;
    }

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
            if (item->id == ID_INLINE_ALPHA) { measure->itemWidth = 180; measure->itemHeight = 240; }
            if (inlineAction(item->id)) { measure->itemWidth = 320; measure->itemHeight = 64; }
            return true;
        }
    }

    if (msg == WM_DRAWITEM) {
        draw = (DRAWITEMSTRUCT*)l;
        item = (MenuItemData*)draw->itemData;

        if (draw->CtlType == ODT_MENU && item) {
            if (item->id == ID_INLINE_ALPHA || inlineAction(item->id))
                drawInlineMenu(draw, item);
            else
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
        HMENU root = CreatePopupMenu();
        HMENU setting = CreatePopupMenu();
        HMENU preset  = CreatePopupMenu();
        HMENU language = CreatePopupMenu();
        inlineAlphaMenu = CreatePopupMenu();
        inlineKeysMenu = CreatePopupMenu();
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
        SetMenuInfo(inlineAlphaMenu, &menuInfo);
        SetMenuInfo(inlineKeysMenu, &menuInfo);
        appendDarkMenu(inlineAlphaMenu, MF_STRING, ID_INLINE_ALPHA, L"", false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_APPLY, tr(STR_HOTKEY_APPLY), false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_RESTORE, tr(STR_HOTKEY_RESTORE), false, false);
        appendDarkMenu(inlineKeysMenu, MF_STRING, ID_INLINE_ADJUST, tr(STR_HOTKEY_ADJUST), false, false);

        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, tr(STR_APP_NAME), false, false);
        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, tr(STR_LICENSE), false, false);
        appendDarkSeparator(root);

        appendDarkMenu(setting, MF_STRING, ID_SETTING_EXPLORER, tr(STR_EXPLORER_AUTO), self->settings.explorerAuto, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_POPUPS, tr(STR_POPUP_TRANSPARENCY), self->settings.popupTransparency, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_STARTUP, tr(STR_RUN_AT_STARTUP), self->settings.startupEnabled, false);
        appendDarkMenu(setting, MF_POPUP, (UINT_PTR)inlineKeysMenu, tr(STR_HOTKEYS), false, true);
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
        appendDarkMenu(preset, MF_POPUP, (UINT_PTR)inlineAlphaMenu, tr(STR_CUSTOM_ALPHA), self->settings.preset == PRESET_CUSTOM, true);

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

        menuPointerHook = SetWindowsHookEx(WH_MOUSE, menuPointerCallback, NULL, GetCurrentThreadId());
        menuMessageHook = SetWindowsHookEx(WH_MSGFILTER, menuMessageCallback, NULL, GetCurrentThreadId());
        CreateTimerQueueTimer(&menuOpacityTimer, NULL, menuOpacityTick, self,
            0, 16, WT_EXECUTEDEFAULT);
        SetTimer(hwnd, TRAY_MENU_TIMER_ID, 15, inlineMenuTimer);
        UINT cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NOANIMATION, p.x, p.y, 0, hwnd, null);
        KillTimer(hwnd, TRAY_MENU_TIMER_ID);
        if (menuOpacityTimer) DeleteTimerQueueTimer(NULL, menuOpacityTimer, INVALID_HANDLE_VALUE);
        menuOpacityTimer = NULL;
        if (menuPointerHook) UnhookWindowsHookEx(menuPointerHook);
        menuPointerHook = NULL;
        if (menuMessageHook) UnhookWindowsHookEx(menuMessageHook);
        menuMessageHook = NULL;
        EnumThreadWindows(GetCurrentThreadId(), detachInlineMenus, 0);
        inlineRecordingAction = HOTKEY_ACTION_NONE;
        inlineDragging = false;
        inlineMenuWindow = null;
        inlineActiveMenu = null;
        inlinePointerPressed = false;
        selectedInlineMenu = null;
        selectedInlineItem = 0;
        PostMessage(hwnd, WM_NULL, 0, 0);
        GetCursorPos(&statusPoint);

        DestroyMenu(root);
        inlineAlphaMenu = inlineKeysMenu = null;
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
                self->applyExplorerAutoAll(self);
                break;

            case ID_SETTING_STARTUP:
                self->settings.startupEnabled = !self->settings.startupEnabled;
                if (self->settings.startupEnabled)
                    registerStartupTask();
                else
                    unregisterStartupTask();
                self->settings.save(&self->settings);
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

    if (down && inlineRecordingAction && k->vkCode == VK_ESCAPE) {
        inlineRecordingAction = HOTKEY_ACTION_NONE;
        if (inlineMenuWindow) InvalidateRect(inlineMenuWindow, NULL, false);
        return 1;
    }
    if (down && selectedInlineMenu && selectedInlineMenu == inlineKeysMenu && k->vkCode == VK_RETURN && inlineAction(selectedInlineItem)) {
        SendMessage(self->trayWindow, WM_INLINE_START, selectedInlineItem, 0);
        return 1;
    }
    if (down && selectedInlineMenu && selectedInlineMenu == inlineAlphaMenu && inlineMenuWindow &&
        (k->vkCode == VK_UP || k->vkCode == VK_DOWN || k->vkCode == VK_HOME || k->vkCode == VK_END)) {
        PostMessage(inlineMenuWindow, WM_KEYDOWN, k->vkCode, 0);
        return 1;
    }

    DWORD modifierKey = (k->vkCode == VK_CONTROL || k->vkCode == VK_LCONTROL || k->vkCode == VK_RCONTROL) ? HOTKEY_MOD_CTRL :
        (k->vkCode == VK_MENU || k->vkCode == VK_LMENU || k->vkCode == VK_RMENU) ? HOTKEY_MOD_ALT :
        (k->vkCode == VK_SHIFT || k->vkCode == VK_LSHIFT || k->vkCode == VK_RSHIFT) ? HOTKEY_MOD_SHIFT :
        (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN) ? HOTKEY_MOD_WIN : 0;
    if (modifierKey && ((down && inlineRecordingAction) || (recordedModifierKeys & modifierKey))) {
        if (down) recordedModifierKeys |= modifierKey;
        if (up) recordedModifierKeys &= ~modifierKey;
        if (modifierKey == HOTKEY_MOD_CTRL) self->ctrlDown = down;
        if (modifierKey == HOTKEY_MOD_ALT) self->altDown = down;
        if (modifierKey == HOTKEY_MOD_SHIFT) self->shiftDown = down;
        if (modifierKey == HOTKEY_MOD_WIN) {
            self->winDown = down;
            if (up) self->winUsed = false;
        }
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

    if (w == WM_MBUTTONUP && inlineMiddlePressed) {
        inlineMiddlePressed = false;
        return 1;
    }

    if (menuMessageHook && (w == WM_LBUTTONDOWN || w == WM_LBUTTONUP))
        EnumThreadWindows(GetCurrentThreadId(), attachInlineMenus, (LPARAM)self);

    /* TrackPopupMenu consumes input before window dispatch: intercept interactive rows here. */
    if (inlineActiveMenu && inlineMenuWindow && IsWindowVisible(inlineMenuWindow) &&
        (w == WM_LBUTTONDOWN || w == WM_LBUTTONUP || (w == WM_MOUSEMOVE && inlinePointerPressed))) {
        MSLLHOOKSTRUCT* pointer = (MSLLHOOKSTRUCT*)l;
        boolean hit = inlinePointerPressed;
        RECT windowRect;
        GetWindowRect(inlineMenuWindow, &windowRect);
        for (UINT i = 0; !hit && i < (inlineActiveMenu == inlineKeysMenu ? 3u : 1u); i++) {
            RECT row;
            if (GetMenuItemRect(self->trayWindow, inlineActiveMenu, i, &row) &&
                row.left >= windowRect.left && row.right <= windowRect.right &&
                PtInRect(&windowRect, pointer->pt) && PtInRect(&row, pointer->pt)) hit = true;
        }
        if (hit) {
            if (w == WM_LBUTTONDOWN) inlinePointerPressed = true;
            if (w == WM_LBUTTONUP) inlinePointerPressed = false;
            // Handle before returning to the native menu loop. Posted owner
            // messages can remain queued while TrackPopupMenu waits for input.
            SendMessage(self->trayWindow, WM_INLINE_POINTER, w, MAKELPARAM(pointer->pt.x, pointer->pt.y));
            return 1;
        }
    }

    if (inlineRecordingAction) {
        if ((w == WM_MBUTTONDOWN && inlineRecordingAction != HOTKEY_ACTION_ADJUST) ||
            (w == WM_MOUSEWHEEL && inlineRecordingAction == HOTKEY_ACTION_ADJUST)) {
            if (w == WM_MBUTTONDOWN) inlineMiddlePressed = true;
            if (modifiers & HOTKEY_MOD_WIN) self->winUsed = true;
            SendMessage(self->trayWindow, WM_TRAY_RECORD, modifiers, inlineRecordingAction);
            return 1;
        }
        return CallNextHookEx(null, code, w, l);
    }
    if (inlineDragging) return CallNextHookEx(null, code, w, l);

    if (modifiers == 0)
        return CallNextHookEx(null, code, w, l);

    MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l;
    HWND target = GetAncestor(WindowFromPoint(m->pt), GA_ROOT);
    if (!target)
        return CallNextHookEx(null, code, w, l);


    if (w == WM_MBUTTONDOWN) {
        if (modifiersMatch(modifiers, self->settings.applyModifiers) && applyTrackedAlpha(self, target,
                getCurrentAlpha(self))) {
            if (!isPopupMenuWindow(target)) self->transparency.refresh(&self->transparency, target);
            if (self->settings.applyModifiers & HOTKEY_MOD_WIN)
                self->winUsed = true;
            return 1;
        }

        if (modifiersMatch(modifiers, self->settings.restoreModifiers) && applyTrackedAlpha(self, target, ALPHA_OPAQUE)) {
            if (!isPopupMenuWindow(target)) self->transparency.refresh(&self->transparency, target);
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

        if (applyTrackedAlpha(self, target, alpha) && !isPopupMenuWindow(target))
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
    self->popupEventHook = SetWinEventHook(EVENT_SYSTEM_MENUPOPUPSTART, EVENT_SYSTEM_MENUPOPUPSTART,
        null, winEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT);
    self->keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHook, GetModuleHandle(NULL), 0);
    self->mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHook, GetModuleHandle(NULL), 0);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    KillTimer(self->trayWindow, TRAY_RETRY_TIMER_ID);
    removeTrayIcon(self);

    if (self->winEventHook) UnhookWinEvent(self->winEventHook);
    if (self->popupEventHook) UnhookWinEvent(self->popupEventHook);
    if (self->keyHook) UnhookWindowsHookEx(self->keyHook);
    if (self->mouseHook) UnhookWindowsHookEx(self->mouseHook);

    self->tracker.restoreAll(&self->tracker, &self->transparency);

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
