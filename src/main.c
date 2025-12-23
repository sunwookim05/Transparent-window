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

static volatile boolean ctrlDown = false;
static volatile boolean winDown = false;
static volatile boolean winUsed = false;

static HHOOK keyHook = null;
static HHOOK mouseHook = null;
static HWINEVENTHOOK winEventHook = null;
static HWND trayWindow = null;

typedef struct { HWND hwnd; BYTE originalAlpha; } WindowAlpha;

#define MAX_TRACKED_WINDOWS 256
static WindowAlpha trackedWindows[MAX_TRACKED_WINDOWS];
static int trackedCount = 0;

/* ---------------- 공용 ---------------- */
static boolean IsAlreadyTracked(HWND hwnd) {
    for (int i = 0; i < trackedCount; i++) if (trackedWindows[i].hwnd == hwnd) return true;
    return false;
}

static boolean IsAutoTransparentTarget(HWND hwnd) {
    char className[128];
    GetClassNameA(hwnd, className, sizeof(className));
    if (!strcmp(className, "CabinetWClass")) return true;
    if (!strcmp(className, "ExploreWClass")) return true;
    return false;
}
/* ---------------- 투명화 ---------------- */
static BYTE GetWindowAlpha(HWND hwnd) {
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(style & WS_EX_LAYERED)) return ALPHA_OPAQUE;
    BYTE alpha = 0; DWORD flags = 0;
    GetLayeredWindowAttributes(hwnd, null, &alpha, &flags);
    return alpha;
}

static boolean ApplyTransparency(HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
    return true;
}

static void TrackWindow(HWND hwnd) {
    if (trackedCount >= MAX_TRACKED_WINDOWS) return;
    if (IsAlreadyTracked(hwnd)) return;
    trackedWindows[trackedCount].hwnd = hwnd;
    trackedWindows[trackedCount].originalAlpha = GetWindowAlpha(hwnd);
    trackedCount++;
}

/* ---------------- 초기 열거 ---------------- */

static BOOL CALLBACK EnumExplorerWindows(HWND hwnd, LPARAM lParam) {
    if (!IsWindow(hwnd)) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (!IsAutoTransparentTarget(hwnd)) return TRUE;

    ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
    TrackWindow(hwnd);
    return TRUE;
}

/* ---------------- WinEvent ---------------- */



static LRESULT CALLBACK MenuSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR idSubclass, DWORD_PTR refData) {
    switch (msg) {
        case WM_WINDOWPOSCHANGING:
        case WM_STYLECHANGING:
        case WM_SHOWWINDOW:
        case WM_NCPAINT:
        case WM_PAINT:  // 추가
        case WM_ERASEBKGND: // 추가
            ApplyTransparency(hwnd, ALPHA_MENU);
            break;

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, MenuSubclassProc, idSubclass);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void SubclassMenu(HWND hwnd) {
    static UINT_PTR subclassId = 1;

    if (!IsWindow(hwnd)) return;

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) {
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    }

    SetLayeredWindowAttributes(hwnd, 0, ALPHA_MENU, LWA_ALPHA);
    SetWindowSubclass(hwnd, MenuSubclassProc, subclassId, 0);
}

static void CALLBACK WinEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwThread, DWORD dwmsEventTime) {
    if (!IsWindow(hwnd)) return;

    // 1️⃣ OBJID_MENU 이벤트 처리: Windows 11 추가 메뉴 포함
    if (idObject == OBJID_MENU) {
        ApplyTransparency(hwnd, ALPHA_MENU);
        SubclassMenu(hwnd); // 계속 Alpha 유지
        return;
    }

    // 2️⃣ 기존 Explorer 자동 투명화
    if (idObject != OBJID_WINDOW) return;
    if (!IsWindowVisible(hwnd)) return;
    if (!IsAutoTransparentTarget(hwnd)) return;

    ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
    TrackWindow(hwnd);
}


/* ---------------- 트레이 ---------------- */
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
        POINT p; GetCursorPos(&p); SetForegroundWindow(hwnd);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
        if (cmd == 1) ShellExecuteA(null, "open", "https://github.com/sunwookim05", null, null, SW_SHOWNORMAL);
        else if (cmd == 2) ShellExecuteA(null, "open", "https://github.com/sunwookim05/Transparent-window", null, null, SW_SHOWNORMAL);
        else if (cmd == 3) PostQuitMessage(0);
        DestroyMenu(m);
    }
    return DefWindowProc(hwnd, msg, w, l);
}

static void* TrayThread(void* arg) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "TransparencyTray";
    RegisterClassA(&wc);

    trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, null, null, wc.hInstance, null);

    HICON icon = (HICON)LoadImageA(null, "res\\trayicon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!icon) icon = LoadIcon(null, IDI_APPLICATION);

    NOTIFYICONDATAA nid = { sizeof(nid), trayWindow, TRAY_ID, NIF_MESSAGE | NIF_ICON | NIF_TIP, WM_TRAY, icon };
    strcpy(nid.szTip, "System Transparency");
    Shell_NotifyIconA(NIM_ADD, &nid);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }

    Shell_NotifyIconA(NIM_DELETE, &nid);
    return null;
}

/* ---------------- 키보드 ---------------- */
static LRESULT CALLBACK KeyboardHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT* key = (KBDLLHOOKSTRUCT*)lParam;
        boolean down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        boolean up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (key->vkCode == VK_CONTROL) ctrlDown = down ? true : (up ? false : ctrlDown);

        if (key->vkCode == VK_LWIN || key->vkCode == VK_RWIN) {
            if (down) { winDown = true; winUsed = false; }
            else if (up) {
                winDown = false;
                if (!winUsed) {
                    INPUT input = {0};
                    input.type = INPUT_KEYBOARD; input.ki.wVk = key->vkCode;
                    SendInput(1, &input, sizeof(input));
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &input, sizeof(input));
                }
            }
            return 1;
        }
    }
    return CallNextHookEx(null, code, wParam, lParam);
}

/* ---------------- 마우스 ---------------- */
static LRESULT CALLBACK MouseHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
        HWND window = GetAncestor(WindowFromPoint(mouse->pt), GA_ROOT);
        if (!window) return CallNextHookEx(null, code, wParam, lParam);

        boolean ctrl = ctrlDown || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        boolean win = winDown || ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

        if (wParam == WM_MBUTTONDOWN) {
            if (ctrl && ApplyTransparency(window, ALPHA_TRANSPARENT)) return 1;
            if (win && ApplyTransparency(window, ALPHA_OPAQUE)) { winUsed = true; return 1; }
        }

        if (wParam == WM_MOUSEWHEEL && ctrl && win) {
            int delta = GET_WHEEL_DELTA_WPARAM(mouse->mouseData);
            BYTE alpha = GetWindowAlpha(window);
            alpha = delta > 0 ? (alpha + 15 > 255 ? 255 : alpha + 15) : (alpha - 15 < 60 ? 60 : alpha - 15);
            ApplyTransparency(window, alpha);
            return 1;
        }
    }
    return CallNextHookEx(null, code, wParam, lParam);
}

static void* HookThread(void* arg) {
    keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, null, 0);
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHook, null, 0);
    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    if (keyHook) UnhookWindowsHookEx(keyHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
    return null;
}

/* ---------------- WinEvent + 메시지 루프 ---------------- */
static void* MsgLoopThread(void* arg) {
    winEventHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_SHOW, null, WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    }

    if (winEventHook) UnhookWinEvent(winEventHook);
    return null;
}

/* ---------------- main ---------------- */
int main(void) {
    EnumWindows(EnumExplorerWindows, 0);

    Thread trayThread = new_Thread(TrayThread);
    Thread hookThread = new_Thread(HookThread);
    Thread msgLoopThread = new_Thread(MsgLoopThread);

    trayThread.start(&trayThread);
    hookThread.start(&hookThread);
    msgLoopThread.start(&msgLoopThread);

    trayThread.join(&trayThread);

    trayThread.cancel(&trayThread);
    hookThread.cancel(&hookThread);
    msgLoopThread.cancel(&msgLoopThread);

    trayThread.delete(&trayThread);
    hookThread.delete(&hookThread);
    msgLoopThread.delete(&msgLoopThread);

    ExitProcess(0);
}
