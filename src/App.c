#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include <commctrl.h>

#include "App.h"

#define WM_TRAY (WM_USER + 1)
#define TRAY_ID 1
#define TRAY_RETRY_TIMER_ID 100

#define ID_SETTING_EXPLORER  10

#define ID_PRESET_SOLID     20
#define ID_PRESET_SOFT      21
#define ID_PRESET_GLASS     22
#define ID_PRESET_GHOST     23

static App* appContext = null;

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

static BOOL CALLBACK enumExplorerWindows(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd))
        return true;

    if (!self->settings.explorerAuto)
        return true;

    if (!self->transparency.isTarget(&self->transparency, hwnd))
        return true;

    BYTE alpha = self->transparency.presetToAlpha(&self->transparency, self->settings.preset);
    self->transparency.apply(&self->transparency, hwnd, alpha);
    self->tracker.track(&self->tracker, &self->transparency, hwnd);

    return true;
}

static BOOL CALLBACK applyExplorerAutoWindow(HWND hwnd, LPARAM lParam) {
    App* self = (App*)lParam;

    if (!IsWindowVisible(hwnd))
        return true;

    if (self->transparency.isTarget(&self->transparency, hwnd)) {
        BYTE alpha = self->settings.explorerAuto ?
            self->transparency.presetToAlpha(&self->transparency, self->settings.preset) :
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

    if (obj == OBJID_WINDOW && IsWindowVisible(hwnd) && self->transparency.isTarget(&self->transparency, hwnd)) {
        if (!strcmp(cls, "TaskSwitcherWnd") || !strcmp(cls, "MultitaskingViewFrame"))
            return;

        if (!self->settings.explorerAuto)
            return;

        self->transparency.apply(&self->transparency, hwnd,
            self->transparency.presetToAlpha(&self->transparency, self->settings.preset));
        self->tracker.track(&self->tracker, &self->transparency, hwnd);
    }

    if (event == EVENT_OBJECT_DESTROY && obj == OBJID_WINDOW)
        self->tracker.remove(&self->tracker, hwnd);
}

static LRESULT CALLBACK trayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    App* self = appContext;

    if (!self)
        return DefWindowProc(hwnd, msg, w, l);

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

        AppendMenuA(root, MF_STRING, 0, "System Transparency");
        AppendMenuA(root, MF_STRING, 0, "Licensed under MIT");
        AppendMenuA(root, MF_SEPARATOR, 0, null);

        AppendMenuA(setting, MF_STRING | (self->settings.explorerAuto ? MF_CHECKED : 0), ID_SETTING_EXPLORER, "Explorer Auto Transparency");
        AppendMenuA(preset, MF_STRING | (self->settings.preset == PRESET_SOLID ? MF_CHECKED : 0), ID_PRESET_SOLID, "Solid");
        AppendMenuA(preset, MF_STRING | (self->settings.preset == PRESET_SOFT  ? MF_CHECKED : 0), ID_PRESET_SOFT,  "Soft");
        AppendMenuA(preset, MF_STRING | (self->settings.preset == PRESET_GLASS ? MF_CHECKED : 0), ID_PRESET_GLASS, "Glass");
        AppendMenuA(preset, MF_STRING | (self->settings.preset == PRESET_GHOST ? MF_CHECKED : 0), ID_PRESET_GHOST, "Ghost");

        AppendMenuA(setting, MF_POPUP, (UINT_PTR)preset, "Preset");
        AppendMenuA(root, MF_POPUP, (UINT_PTR)setting, "Setting");

        AppendMenuA(root, MF_SEPARATOR, 0, null);
        AppendMenuA(root, MF_STRING, 1, "Developed by sunwookim05");
        AppendMenuA(root, MF_STRING, 2, "GitHub");
        AppendMenuA(root, MF_SEPARATOR, 0, null);
        AppendMenuA(root, MF_STRING, 3, "Exit");

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

            case ID_PRESET_SOLID:
            case ID_PRESET_SOFT:
            case ID_PRESET_GLASS:
            case ID_PRESET_GHOST:
                self->settings.preset = (TransparencyPreset)(cmd - ID_PRESET_SOLID);
                self->settings.save(&self->settings);
                self->applyExplorerAutoAll(self);
                break;
        }

        DestroyMenu(root);
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
                self->transparency.presetToAlpha(&self->transparency, self->settings.preset))) {
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
