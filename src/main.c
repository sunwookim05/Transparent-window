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
#define ALPHA_MENU 175
#define ALPHA_OPAQUE 255
#define MAX_TRACKED_WINDOWS 64

static volatile boolean ctrlDown = false;
static volatile boolean winDown  = false;
static volatile boolean winUsed  = false;
static volatile boolean shuttingDown = false;

static HHOOK keyHook = null;
static HHOOK mouseHook = null;
static HWINEVENTHOOK winEventHook = null;
static HWND trayWindow = null;

/* ---------------- Window Tracking ---------------- */

typedef struct {
    HWND hwnd;
    BYTE originalAlpha;
} WindowAlpha;


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
    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) return ALPHA_OPAQUE;
    BYTE alpha = 0; DWORD flags = 0;
    GetLayeredWindowAttributes(hwnd, null, &alpha, &flags);
    return alpha;
}

static boolean ApplyTransparency(HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;
    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED))
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    return SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

static void TrackWindow(HWND hwnd) {
    if (trackedCount >= MAX_TRACKED_WINDOWS) return;
    if (IsAlreadyTracked(hwnd)) return;
    trackedWindows[trackedCount++] = (WindowAlpha){ hwnd, GetWindowAlpha(hwnd) };
}

static BOOL CALLBACK EnumExplorerWindows(HWND hwnd, LPARAM l) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (!IsAutoTransparentTarget(hwnd)) return TRUE;
    ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
    TrackWindow(hwnd);
    return TRUE;
}

/* ---------------- Menu Transparency ---------------- */

static LRESULT CALLBACK MenuSubclassProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref) {
    if (msg == WM_NCDESTROY)
        RemoveWindowSubclass(hwnd, MenuSubclassProc, id);
    else
        ApplyTransparency(hwnd, ALPHA_MENU);

    return DefSubclassProc(hwnd, msg, w, l);
}

static void SubclassMenu(HWND hwnd) {
    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED))
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);

    SetLayeredWindowAttributes(hwnd, 0, ALPHA_MENU, LWA_ALPHA);
    SetWindowSubclass(hwnd, MenuSubclassProc, 1, 0);
}

static void RemoveTracked(HWND hwnd) {
    for (int i = 0; i < trackedCount; i++) {
        if (trackedWindows[i].hwnd == hwnd) {

            // 뒤에 있는 요소들을 앞으로 당김
            for (int j = i; j < trackedCount - 1; j++)
                trackedWindows[j] = trackedWindows[j + 1];

            trackedCount--;
            return;
        }
    }
}


/* ---------------- WinEvent ---------------- */

static void CALLBACK WinEventCallback(HWINEVENTHOOK h, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time) {
    if (!IsWindow(hwnd)) return;

    if (obj == OBJID_MENU) {
        SubclassMenu(hwnd);
        return;
    }

    if (obj == OBJID_WINDOW && IsWindowVisible(hwnd) && IsAutoTransparentTarget(hwnd)) {
        ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
        TrackWindow(hwnd);
    }

    if (event == EVENT_OBJECT_DESTROY && obj == OBJID_WINDOW) {
        RemoveTracked(hwnd);
    }
}

/* ---------------- Tray ---------------- */

static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_TRAY && l == WM_RBUTTONUP) {
        HMENU m = CreatePopupMenu();

        /* 정보성 메뉴 (동작 없음) */
        AppendMenuA(m, MF_STRING, 100, "System Transparency");
        AppendMenuA(m, MF_STRING, 101, "Licensed under MIT");
        AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        /* 개발자 / 링크 */
        AppendMenuA(m, MF_STRING, 1, "Developed by sunwookim05");
        AppendMenuA(m, MF_STRING, 2, "GitHub");
        AppendMenuA(m, MF_SEPARATOR, 0, NULL);
        /* 종료 */
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
                /* 🔒 안전 종료 */
                shuttingDown = true;
                PostQuitMessage(0);
                break;
        }

        DestroyMenu(m);
        return 0;
    }

    return DefWindowProc(hwnd, msg, w, l);
}


/* ---------------- Hooks ---------------- */

static LRESULT CALLBACK KeyboardHook(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION && !shuttingDown) {
        KBDLLHOOKSTRUCT* k = (KBDLLHOOKSTRUCT*)l;
        boolean down = (w == WM_KEYDOWN || w == WM_SYSKEYDOWN);
        boolean up   = (w == WM_KEYUP   || w == WM_SYSKEYUP);

        if (k->vkCode == VK_CONTROL)
            ctrlDown = down ? true : (up ? false : ctrlDown);

        if (k->vkCode == VK_LWIN || k->vkCode == VK_RWIN) {
            if (down) {
                winDown = true;
                winUsed = false;
            } else if (up) {
                winDown = false;
                if (winUsed) {
                    winUsed = false;
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(null, code, w, l);
}

static LRESULT CALLBACK MouseHook(int code, WPARAM w, LPARAM l) {
    if (code != HC_ACTION || shuttingDown)
        return CallNextHookEx(null, code, w, l);

    boolean ctrl = ctrlDown || (GetAsyncKeyState(VK_CONTROL) & 0x8000);
    boolean win  = winDown  || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);

    /* 🔒 Ctrl / Win 안 눌리면 훅 비활성 */
    if (!ctrl && !win)
        return CallNextHookEx(null, code, w, l);

    MSLLHOOKSTRUCT* m = (MSLLHOOKSTRUCT*)l;
    HWND hwnd = GetAncestor(WindowFromPoint(m->pt), GA_ROOT);
    if (!hwnd) return CallNextHookEx(null, code, w, l);

    if (w == WM_MBUTTONDOWN) {
        if (ctrl && ApplyTransparency(hwnd, ALPHA_TRANSPARENT)) return 1;
        if (win  && ApplyTransparency(hwnd, ALPHA_OPAQUE)) {
            winUsed = true;
            return 1;
        }
    }

    if (w == WM_MOUSEWHEEL && ctrl && win) {
        int delta = GET_WHEEL_DELTA_WPARAM(m->mouseData);
        BYTE a = GetWindowAlpha(hwnd);
        a = delta > 0 ? min(255, a + 15) : max(60, a - 15);
        ApplyTransparency(hwnd, a);
        return 1;
    }

    return CallNextHookEx(null, code, w, l);
}

/* ---------------- Core Thread ---------------- */

void* appCoreThread(void* arg) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "TransparencyTray";
    RegisterClassA(&wc);

    trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, null, null, wc.hInstance, null);

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

    winEventHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, null, WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

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


/* ---------------- main ---------------- */

int main(void) {
    EnumWindows(EnumExplorerWindows, 0);

    Thread core = new_Thread(appCoreThread);
    core.start(&core);
    core.join(&core);
    core.delete(&core);

    ExitProcess(0);

    return 0;
}
