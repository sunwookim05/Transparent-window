#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <commctrl.h>

#include "App.h"
#include "Installer.h"
#include "Updater.h"

#define WM_TRAY (WM_USER + 1)
#define TRAY_ID 1
#define TRAY_RETRY_TIMER_ID 100

#define ID_SETTING_EXPLORER  10
#define ID_SETTING_STARTUP   11
#define ID_SETTING_FOLDER    13
#define ID_SETTING_RESET     14

#define ID_PRESET_SOLID     20
#define ID_PRESET_SOFT      21
#define ID_PRESET_GLASS     22
#define ID_PRESET_GHOST     23
#define ID_PRESET_CUSTOM    24

#define ID_UPDATE_CHECK     30
#define ID_LOG_OPEN         31

#define ID_ALPHA_EDIT       1001
#define ID_ALPHA_SLIDER     1002
#define ID_ALPHA_OK         1003
#define ID_ALPHA_CANCEL     1004

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19

static App* appContext = null;
static BYTE alphaDialogValue = 150;
static boolean alphaDialogOk = false;
static HWND alphaPreviewWindow = null;
static BYTE alphaPreviewOriginal = ALPHA_OPAQUE;
static boolean alphaControlsUpdating = false;
static HBRUSH alphaDarkBrush = null;
static HBRUSH alphaEditBrush = null;

typedef struct {
    char text[80];
    boolean checked;
    boolean submenu;
} MenuItemData;

static MenuItemData menuItems[64];
static int menuItemCount = 0;

static void resetMenuItems(void) {
    menuItemCount = 0;
}

static MenuItemData* newMenuItem(string text, boolean checked, boolean submenu) {
    MenuItemData* item;

    if (menuItemCount >= 64)
        return null;

    item = &menuItems[menuItemCount++];
    lstrcpynA(item->text, text, sizeof(item->text));
    item->checked = checked;
    item->submenu = submenu;
    return item;
}

static void appendDarkMenu(HMENU menu, UINT flags, UINT_PTR id, string text, boolean checked, boolean submenu) {
    AppendMenuA(menu, flags | MF_OWNERDRAW, id, (LPCSTR)newMenuItem(text, checked, submenu));
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

static BYTE sliderPointToAlpha(HWND slider, int x) {
    RECT rect;
    int width;
    int value;
    int left;
    int right;

    GetClientRect(slider, &rect);
    left = rect.left + 12;
    right = rect.right - 12;
    width = right - left;

    if (width <= 1)
        return alphaDialogValue;

    if (x <= left) return 60;
    if (x >= right) return 255;

    value = 60 + ((255 - 60) * (x - left) + width / 2) / width;
    if (value < 60) value = 60;
    if (value > 255) value = 255;

    return (BYTE)value;
}

static int alphaToSliderX(HWND slider, BYTE alpha) {
    RECT rect;
    int left;
    int right;

    GetClientRect(slider, &rect);
    left = rect.left + 12;
    right = rect.right - 12;

    return left + ((int)(alpha - 60) * (right - left)) / (255 - 60);
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
    int x;

    (void)id;
    (void)ref;

    if (msg == WM_LBUTTONDOWN || (msg == WM_MOUSEMOVE && (w & MK_LBUTTON))) {
        SetCapture(hwnd);
        updateAlphaControls(dialog, sliderPointToAlpha(hwnd, (short)LOWORD(l)));
        return 0;
    }

    if (msg == WM_LBUTTONUP) {
        if (GetCapture() == hwnd)
            ReleaseCapture();

        updateAlphaControls(dialog, sliderPointToAlpha(hwnd, (short)LOWORD(l)));
        return 0;
    }

    if (msg == WM_ERASEBKGND)
        return 1;

    if (msg == WM_PAINT) {
        HDC dc = BeginPaint(hwnd, &ps);

        GetClientRect(hwnd, &rect);

        brush = CreateSolidBrush(RGB(24, 24, 27));
        FillRect(dc, &rect, brush);
        DeleteObject(brush);

        track.left = rect.left + 12;
        track.right = rect.right - 12;
        track.top = rect.top + ((rect.bottom - rect.top) / 2) - 3;
        track.bottom = track.top + 6;

        brush = CreateSolidBrush(RGB(68, 68, 76));
        FillRect(dc, &track, brush);
        DeleteObject(brush);

        x = alphaToSliderX(hwnd, alphaDialogValue);
        active = track;
        active.right = x;

        brush = CreateSolidBrush(RGB(88, 166, 255));
        FillRect(dc, &active, brush);
        DeleteObject(brush);

        brush = CreateSolidBrush(RGB(238, 244, 255));
        Ellipse(dc, x - 7, track.top - 5, x + 7, track.bottom + 5);
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

static void showTrayMessage(App* self, string title, string message) {
    NOTIFYICONDATAA data;
    makeTrayIconData(self, &data);

    data.uFlags |= NIF_INFO;
    lstrcpynA(data.szInfoTitle, title, sizeof(data.szInfoTitle));
    lstrcpynA(data.szInfo, message, sizeof(data.szInfo));
    data.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIconA(NIM_MODIFY, &data);
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

static boolean isAutoTarget(App* self, HWND hwnd) {
    return self->transparency.isTarget(&self->transparency, hwnd);
}

static BYTE getCurrentAlpha(App* self) {
    if (self->settings.preset == PRESET_CUSTOM)
        return self->settings.customAlpha;

    return self->transparency.presetToAlpha(&self->transparency, self->settings.preset);
}

static LRESULT CALLBACK alphaWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    HWND edit;
    HWND slider;
    char text[16];
    int value;

    switch (msg) {
        case WM_CREATE:
            {
                BOOL dark = true;
                DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
                DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &dark, sizeof(dark));
            }

            if (!alphaDarkBrush)
                alphaDarkBrush = CreateSolidBrush(RGB(24, 24, 27));
            if (!alphaEditBrush)
                alphaEditBrush = CreateSolidBrush(RGB(38, 38, 43));

            CreateWindowA("STATIC", "Custom Alpha", WS_VISIBLE | WS_CHILD, 18, 14, 120, 20, hwnd, null, null, null);

            slider = CreateWindowExA(0, "STATIC", "", WS_VISIBLE | WS_CHILD | SS_NOTIFY,
                20, 48, 300, 34, hwnd, (HMENU)ID_ALPHA_SLIDER, null, null);
            SetWindowSubclass(slider, alphaSliderProc, 1, 0);

            CreateWindowA("STATIC", "60", WS_VISIBLE | WS_CHILD, 22, 82, 32, 18, hwnd, null, null, null);
            CreateWindowA("STATIC", "255", WS_VISIBLE | WS_CHILD | SS_RIGHT, 286, 82, 32, 18, hwnd, null, null, null);
            CreateWindowA("STATIC", "Value", WS_VISIBLE | WS_CHILD, 94, 112, 42, 20, hwnd, null, null, null);
            edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | ES_NUMBER,
                144, 108, 70, 24, hwnd, (HMENU)ID_ALPHA_EDIT, null, null);
            snprintf(text, sizeof(text), "%u", alphaDialogValue);
            SetWindowTextA(edit, text);

            CreateWindowA("BUTTON", "OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_OWNERDRAW,
                82, 150, 82, 28, hwnd, (HMENU)ID_ALPHA_OK, null, null);
            CreateWindowA("BUTTON", "Cancel", WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
                178, 150, 82, 28, hwnd, (HMENU)ID_ALPHA_CANCEL, null, null);

            applyAlphaPreview(hwnd);

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
            SetTextColor((HDC)w, RGB(235, 235, 240));
            SetBkColor((HDC)w, RGB(24, 24, 27));
            return (LRESULT)alphaDarkBrush;

        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)w, RGB(245, 245, 248));
            SetBkColor((HDC)w, RGB(38, 38, 43));
            return (LRESULT)alphaEditBrush;

        case WM_DRAWITEM:
            if (w == ID_ALPHA_OK || w == ID_ALPHA_CANCEL) {
                DRAWITEMSTRUCT* draw = (DRAWITEMSTRUCT*)l;
                HBRUSH brush;
                COLORREF background = (draw->itemState & ODS_SELECTED) ? RGB(54, 54, 60) : RGB(38, 38, 43);
                COLORREF border = (w == ID_ALPHA_OK) ? RGB(88, 166, 255) : RGB(74, 74, 82);
                string label = (w == ID_ALPHA_OK) ? "OK" : "Cancel";

                brush = CreateSolidBrush(background);
                FillRect(draw->hDC, &draw->rcItem, brush);
                DeleteObject(brush);

                brush = CreateSolidBrush(border);
                FrameRect(draw->hDC, &draw->rcItem, brush);
                DeleteObject(brush);

                SetBkMode(draw->hDC, TRANSPARENT);
                SetTextColor(draw->hDC, RGB(245, 245, 248));
                DrawTextA(draw->hDC, label, -1, &draw->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
                    MessageBoxA(hwnd, "Enter a value between 60 and 255.", "System Transparency", MB_OK | MB_ICONWARNING);
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

    window = CreateWindowExA(WS_EX_DLGMODALFRAME, wc.lpszClassName, "Custom Alpha",
        WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 350, 230,
        owner, null, wc.hInstance, null);

    if (!window)
        return false;

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

static LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    App* self = appContext;
    MEASUREITEMSTRUCT* measure;
    DRAWITEMSTRUCT* draw;
    MenuItemData* item;
    HBRUSH brush;
    RECT textRect;
    SIZE size;

    if (!self)
        return DefWindowProc(hwnd, msg, w, l);

    if (msg == WM_MEASUREITEM) {
        measure = (MEASUREITEMSTRUCT*)l;
        item = (MenuItemData*)measure->itemData;

        if (measure->CtlType == ODT_MENU && item) {
            HDC dc = GetDC(hwnd);
            GetTextExtentPoint32A(dc, item->text, (int)strlen(item->text), &size);
            ReleaseDC(hwnd, dc);

            measure->itemWidth = size.cx + 52;
            measure->itemHeight = 26;
            return true;
        }
    }

    if (msg == WM_DRAWITEM) {
        draw = (DRAWITEMSTRUCT*)l;
        item = (MenuItemData*)draw->itemData;

        if (draw->CtlType == ODT_MENU && item) {
            boolean selected = (draw->itemState & ODS_SELECTED) ? true : false;
            brush = CreateSolidBrush(selected ? RGB(54, 54, 60) : RGB(32, 32, 36));
            FillRect(draw->hDC, &draw->rcItem, brush);
            DeleteObject(brush);

            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, RGB(238, 238, 242));

            textRect = draw->rcItem;
            textRect.left += 28;
            textRect.right -= 20;
            DrawTextA(draw->hDC, item->text, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

            if (item->checked) {
                RECT checkRect = draw->rcItem;
                checkRect.left += 8;
                checkRect.right = checkRect.left + 10;
                checkRect.top += 8;
                checkRect.bottom -= 8;
                brush = CreateSolidBrush(RGB(120, 190, 255));
                FillRect(draw->hDC, &checkRect, brush);
                DeleteObject(brush);
            }

            if (item->submenu) {
                RECT arrowRect = draw->rcItem;
                arrowRect.left = arrowRect.right - 18;
                DrawTextA(draw->hDC, ">", -1, &arrowRect, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
            }

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

    if (msg == WM_TRAY && isTrayContextMenu(l)) {
        HMENU root = CreatePopupMenu();
        HMENU setting = CreatePopupMenu();
        HMENU preset  = CreatePopupMenu();
        HBRUSH menuBrush = CreateSolidBrush(RGB(32, 32, 36));
        MENUINFO menuInfo = {0};
        resetMenuItems();

        menuInfo.cbSize = sizeof(menuInfo);
        menuInfo.fMask = MIM_BACKGROUND;
        menuInfo.hbrBack = menuBrush;
        SetMenuInfo(root, &menuInfo);
        SetMenuInfo(setting, &menuInfo);
        SetMenuInfo(preset, &menuInfo);

        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, "System Transparency", false, false);
        appendDarkMenu(root, MF_STRING | MF_DISABLED, 0, "Licensed under MIT", false, false);
        AppendMenuA(root, MF_SEPARATOR, 0, null);

        appendDarkMenu(setting, MF_STRING, ID_SETTING_EXPLORER, "Explorer Auto Transparency", self->settings.explorerAuto, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_STARTUP, "Run at Startup", self->settings.startupEnabled, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_FOLDER, "Open Install Folder", false, false);
        appendDarkMenu(setting, MF_STRING, ID_SETTING_RESET, "Reset Settings", false, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_SOLID, "Solid", self->settings.preset == PRESET_SOLID, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_SOFT, "Soft", self->settings.preset == PRESET_SOFT, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_GLASS, "Glass", self->settings.preset == PRESET_GLASS, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_GHOST, "Ghost", self->settings.preset == PRESET_GHOST, false);
        appendDarkMenu(preset, MF_STRING, ID_PRESET_CUSTOM, "Custom Alpha...", self->settings.preset == PRESET_CUSTOM, false);

        appendDarkMenu(setting, MF_POPUP, (UINT_PTR)preset, "Preset", false, true);
        appendDarkMenu(root, MF_POPUP, (UINT_PTR)setting, "Setting", false, true);

        AppendMenuA(root, MF_SEPARATOR, 0, null);
        appendDarkMenu(root, MF_STRING, ID_UPDATE_CHECK, "Check for Updates", false, false);
        appendDarkMenu(root, MF_STRING, ID_LOG_OPEN, "Open Log", false, false);
        AppendMenuA(root, MF_SEPARATOR, 0, null);
        appendDarkMenu(root, MF_STRING | MF_DISABLED, 1, "Developed by sunwookim05", false, false);
        appendDarkMenu(root, MF_STRING, 2, "GitHub", false, false);
        AppendMenuA(root, MF_SEPARATOR, 0, null);
        appendDarkMenu(root, MF_STRING, 3, "Exit", false, false);

        POINT p;
        GetCursorPos(&p);
        SetForegroundWindow(hwnd);

        UINT cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, null);

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

            case ID_SETTING_STARTUP:
                self->settings.startupEnabled = !self->settings.startupEnabled;
                if (self->settings.startupEnabled)
                    registerStartupTask();
                else
                    unregisterStartupTask();
                self->settings.save(&self->settings);
                showTrayMessage(self, "System Transparency", self->settings.startupEnabled ? "Startup enabled." : "Startup disabled.");
                break;

            case ID_SETTING_FOLDER:
                openInstallFolder();
                break;

            case ID_SETTING_RESET:
                self->settings.reset(&self->settings);
                self->settings.save(&self->settings);
                registerStartupTask();
                self->applyExplorerAutoAll(self);
                showTrayMessage(self, "System Transparency", "Settings reset.");
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
                    showTrayMessage(self, "System Transparency", "Already up to date.");
                else if (result == UPDATE_FAILED)
                    showTrayMessage(self, "System Transparency", "Update check failed. See log.");
                else if (result == UPDATE_SKIPPED)
                    showTrayMessage(self, "System Transparency", "Update skipped outside install path.");
                break;
            }

            case ID_LOG_OPEN:
                openLog();
                break;

        }

        DestroyMenu(root);
        DeleteObject(menuBrush);
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

    if (k->vkCode == VK_CONTROL) {
        if (down) self->ctrlDown = true;
        else if (up) self->ctrlDown = false;

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

    boolean ctrl = self->ctrlDown || (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    boolean win  = self->winDown  || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

    if (!ctrl && !win)
        return CallNextHookEx(null, code, w, l);

    MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l;
    HWND target = GetAncestor(WindowFromPoint(m->pt), GA_ROOT);
    if (!target)
        return CallNextHookEx(null, code, w, l);

    if (w == WM_MBUTTONDOWN) {
        if (ctrl && self->transparency.apply(&self->transparency, target,
                getCurrentAlpha(self))) {
            self->transparency.refresh(&self->transparency, target);
            return 1;
        }

        if (win && self->transparency.apply(&self->transparency, target, ALPHA_OPAQUE)) {
            self->transparency.refresh(&self->transparency, target);
            self->winUsed = true;
            self->winDown = false;

            return 1;
        }
    }

    if (w == WM_MOUSEWHEEL && ctrl && win) {
        int delta = GET_WHEEL_DELTA_WPARAM(m->mouseData);
        BYTE alpha = self->transparency.getWindowAlpha(&self->transparency, target);

        alpha = (delta > 0) ? min(255, alpha + 15) : max(60, alpha - 15);

        if (self->transparency.apply(&self->transparency, target, alpha))
            self->transparency.refresh(&self->transparency, target);

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
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

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
