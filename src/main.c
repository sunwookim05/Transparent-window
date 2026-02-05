#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <string.h>
#include <commctrl.h>

#include "main.h"
#include "thread.h"

#define WM_TRAY (WM_USER + 1)
#define TRAY_ID 1

#define ALPHA_TRANSPARENT 150
#define ALPHA_OPAQUE 255
#define MAX_TRACKED_WINDOWS 64

typedef struct {
    HWND hwnd;
    BYTE originalAlpha;
} WindowAlpha;

static volatile boolean ctrlDown = false;
static volatile boolean winDown  = false;
static volatile boolean winUsed  = false;
static volatile boolean shuttingDown = false;
static volatile boolean suppressWin = false;

static HHOOK keyHook = null;
static HHOOK mouseHook = null;
static HWINEVENTHOOK winEventHook = null;
static HWND trayWindow = null;
static WindowAlpha trackedWindows[MAX_TRACKED_WINDOWS];
static int trackedCount = 0;

static boolean IsAlreadyTracked(HWND hwnd) {
    for (int i = 0; i < trackedCount; i++)
        if (trackedWindows[i].hwnd == hwnd) return true;
    return false;
}

static boolean IsAutoTransparentTarget(HWND hwnd) {
    char cls[128];
    GetClassNameA(hwnd, cls, sizeof(cls));
    return !strcmp(cls, "CabinetWClass") || !strcmp(cls, "ExploreWClass");
}

static BYTE GetWindowAlpha(HWND hwnd) {
    if (!IsWindow(hwnd)) return ALPHA_OPAQUE;

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) return ALPHA_OPAQUE;

    BYTE alpha = ALPHA_OPAQUE;
    DWORD flags = 0;

    if (!GetLayeredWindowAttributes(hwnd, NULL, &alpha, &flags))
        return ALPHA_OPAQUE;

    return alpha;
}

static boolean ApplyTransparency(HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED))
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);

    if (GetWindowAlpha(hwnd) == alpha)
        return true;

    return SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

static void RefreshDwm(HWND hwnd) {
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static void TrackWindow(HWND hwnd) {
    if (trackedCount >= MAX_TRACKED_WINDOWS) return;
    if (IsAlreadyTracked(hwnd)) return;
    trackedWindows[trackedCount++] = (WindowAlpha){ hwnd, GetWindowAlpha(hwnd) };
}

static BOOL CALLBACK EnumExplorerWindows(HWND hwnd, LPARAM l) {
    if (!IsWindowVisible(hwnd)) return true;
    if (!IsAutoTransparentTarget(hwnd)) return true;
    ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
    TrackWindow(hwnd);
    return true;
}

static LRESULT CALLBACK MenuSubclassProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)  {
    switch (msg) {
        case WM_INITMENUPOPUP:
            // ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
            break;  

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, MenuSubclassProc, id);
            break;
    }
    return DefSubclassProc(hwnd, msg, w, l);
}

// static void SubclassMenu(HWND hwnd) {
//     LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
//     if (!(ex & WS_EX_LAYERED))
//         SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);

//     SetWindowSubclass(hwnd, MenuSubclassProc, 1, 0);
// }

static void RemoveTracked(HWND hwnd) {
    for (int i = 0; i < trackedCount; i++) {
        if (trackedWindows[i].hwnd == hwnd) {
            for (int j = i; j < trackedCount - 1; j++)
                trackedWindows[j] = trackedWindows[j + 1];

            trackedCount--;
            return;
        }
    }
}

static void CALLBACK WinEventCallback(HWINEVENTHOOK h, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time){
    if (!IsWindow(hwnd)) return;

    char cls[128];
    GetClassNameA(hwnd, cls, sizeof(cls));

    // if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW){
    //     LONG style = GetWindowLong(hwnd, GWL_STYLE);
    //     if (!strcmp(cls, "#32768") || obj == OBJID_MENU || (style & WS_POPUP)){
            // SubclassMenu(hwnd);
    //         return;
    //     }
    // }

    if (obj == OBJID_WINDOW && IsWindowVisible(hwnd) && IsAutoTransparentTarget(hwnd)) {
        if (!strcmp(cls, "TaskSwitcherWnd") || !strcmp(cls, "MultitaskingViewFrame"))
            return;

        ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
        TrackWindow(hwnd);
    }

    if (event == EVENT_OBJECT_DESTROY && obj == OBJID_WINDOW)
        RemoveTracked(hwnd);
}

static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_TRAY && l == WM_RBUTTONUP) {
        HMENU m = CreatePopupMenu();

        AppendMenuA(m, MF_STRING, 100, "System Transparency");
        AppendMenuA(m, MF_STRING, 101, "Licensed under MIT");
        AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        AppendMenuA(m, MF_STRING, 1, "Developed by sunwookim05");
        AppendMenuA(m, MF_STRING, 2, "GitHub");
        AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        AppendMenuA(m, MF_STRING, 3, "Exit");

        POINT p;
        GetCursorPos(&p);
        SetForegroundWindow(hwnd);

        int cmd = TrackPopupMenu( m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);

        switch (cmd) {
            case 1:
                ShellExecuteA(null, "open", "https://github.com/sunwookim05", null, null, SW_SHOWNORMAL); 
                break;

            case 2:
                ShellExecuteA(null, "open", "https://github.com/sunwookim05/Transparent-window", null, null, SW_SHOWNORMAL);
                break;

            case 3:
                shuttingDown = true;
                PostQuitMessage(0);
                break;
        }

        DestroyMenu(m);
        return 0;
    }

    return DefWindowProc(hwnd, msg, w, l);
}

static LRESULT CALLBACK KeyboardHook(int code, WPARAM w, LPARAM l){
    if (code != HC_ACTION || shuttingDown)
        return CallNextHookEx(NULL, code, w, l);

    KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)l;

    const boolean down = (w == WM_KEYDOWN || w == WM_SYSKEYDOWN);
    const boolean up   = (w == WM_KEYUP   || w == WM_SYSKEYUP);

    if (k->vkCode == VK_CONTROL) {
        if (down) ctrlDown = true;
        else if (up) ctrlDown = false;

        return CallNextHookEx(NULL, code, w, l);
    }

    if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN) {

        if (down) {
            winDown = true;
            return CallNextHookEx(NULL, code, w, l);
        }

        if (up) {
            winDown = false;
            if (winUsed) {
                winUsed = false;
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

            return CallNextHookEx(NULL, code, w, l);
        }

        return CallNextHookEx(NULL, code, w, l);
    }

    return CallNextHookEx(NULL, code, w, l);
}

static LRESULT CALLBACK MouseHook(int code, WPARAM w, LPARAM l){
    if (code != HC_ACTION || shuttingDown)
        return CallNextHookEx(NULL, code, w, l);

    boolean ctrl = ctrlDown || (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    boolean win  = winDown  || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

    if (!ctrl && !win)
        return CallNextHookEx(NULL, code, w, l);

    MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l;
    HWND hwnd = GetAncestor(WindowFromPoint(m->pt), GA_ROOT);
    if (!hwnd)
        return CallNextHookEx(NULL, code, w, l);

    if (w == WM_MBUTTONDOWN) {

        if (ctrl && ApplyTransparency(hwnd, ALPHA_TRANSPARENT)){
            RefreshDwm(hwnd);
            return 1;
        }

        if (win && ApplyTransparency(hwnd, ALPHA_OPAQUE)) {
            RefreshDwm(hwnd);
            winUsed = true; 
            winDown = false;

            return 1;
        }
    }

    if (w == WM_MOUSEWHEEL && ctrl && win) {

        int delta = GET_WHEEL_DELTA_WPARAM(m->mouseData);
        BYTE a = GetWindowAlpha(hwnd);

        a = (delta > 0) ? min(255, a + 15) : max(60,  a - 15);

        if (ApplyTransparency(hwnd, a))
            RefreshDwm(hwnd);
        winUsed = true; 

        return 1;
    }

    return CallNextHookEx(NULL, code, w, l);
}

void* appCoreThread(void* arg) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "TransparencyTray";
    RegisterClassA(&wc);

    trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPED | WS_SYSMENU, 0, 0, 0, 0, null, null, wc.hInstance, null);

    HICON icon = (HICON)LoadImage(GetModuleHandle(null), MAKEINTRESOURCE(102), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED);

    if (!icon)
        icon = LoadIcon(null, IDI_APPLICATION);

    NOTIFYICONDATAA nid = {
        sizeof(nid),
        trayWindow,
        TRAY_ID,
        NIF_MESSAGE | NIF_ICON | NIF_TIP,
        WM_TRAY
    };

    nid.hIcon = icon;
    strcpy(nid.szTip, "System Transparency");

    Shell_NotifyIconA(NIM_ADD, &nid);

    winEventHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, null, WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT);

    keyHook   = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, null, 0);
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHook, null, 0);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIconA(NIM_DELETE, &nid);

    if (winEventHook) UnhookWinEvent(winEventHook);
    if (keyHook) UnhookWindowsHookEx(keyHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);

    return null;
}

int main(void) {
    EnumWindows(EnumExplorerWindows, 0);

    Thread core = new_Thread(appCoreThread);
    core.start(&core);
    core.join(&core);
    core.delete(&core);

    ExitProcess(0);

    return 0;
}
