#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <string.h>

#include "main.h"
#include "thread.h"

#define WM_TRAY (WM_USER + 1)
#define TRAY_ID 1

#define ALPHA_TRANSPARENT 150
#define ALPHA_OPAQUE 255

static volatile boolean ctrlDown = false;
static volatile boolean winDown = false;
static volatile boolean winUsed = false;

static HHOOK keyHook = null;
static HHOOK mouseHook = null;
static HWINEVENTHOOK winEventHook = null;
static HWND trayWindow = null;

typedef struct {
    HWND hwnd;
    BYTE originalAlpha;
} WindowAlpha;

#define MAX_TRACKED_WINDOWS 128
static WindowAlpha trackedWindows[MAX_TRACKED_WINDOWS];
static int trackedCount = 0;

/* ---------------- 투명화 ---------------- */
static BYTE GetWindowAlpha(HWND hwnd) {
    LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(style & WS_EX_LAYERED)) return ALPHA_OPAQUE;
    BYTE alpha = 0;
    DWORD flags = 0;
    GetLayeredWindowAttributes(hwnd, null, &alpha, &flags);
    return alpha;
}

static boolean ApplyTransparency(HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED))
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
    return true;
}

static void TrackWindow(HWND hwnd) {
    if (trackedCount >= MAX_TRACKED_WINDOWS) return;
    trackedWindows[trackedCount].hwnd = hwnd;
    trackedWindows[trackedCount].originalAlpha = GetWindowAlpha(hwnd);
    trackedCount++;
}

/* ---------------- 탐색기 자동 투명화 ---------------- */
static BOOL CALLBACK EnumExplorerWindows(HWND hwnd, LPARAM lParam) {
    char className[128];
    GetClassNameA(hwnd, className, sizeof(className));
    if (!strcmp(className, "CabinetWClass") || !strcmp(className, "ExploreWClass")) {
        ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
        TrackWindow(hwnd);
    }
    return true;
}

static void CALLBACK WinEventCallback(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                      LONG idObject, LONG idChild, DWORD dwThread, DWORD dwmsEventTime) {
    if (!IsWindow(hwnd)) return;
    char className[128];
    GetClassNameA(hwnd, className, sizeof(className));
    if (!strcmp(className, "CabinetWClass") || !strcmp(className, "ExploreWClass")) {
        ApplyTransparency(hwnd, ALPHA_TRANSPARENT);
        TrackWindow(hwnd);
    }
}

/* ---------------- 트레이 ---------------- */
static LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAY && lParam == WM_RBUTTONUP) {
        HMENU menu = CreatePopupMenu();
        AppendMenuA(menu, MF_STRING, 1, "Exit");
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(hwnd);
        if (TrackPopupMenu(menu, TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, null) == 1)
            PostQuitMessage(0);
        DestroyMenu(menu);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void* TrayThread(void* arg) {
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = GetModuleHandle(null);
    wc.lpszClassName = "TransparencyTray";
    RegisterClassA(&wc);

    trayWindow = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW,
                               0, 0, 0, 0, null, null, wc.hInstance, null);

    HICON icon = (HICON)LoadImageA(null, "res\\trayicon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    if (!icon) icon = LoadIcon(null, IDI_APPLICATION);

    NOTIFYICONDATAA nid = { sizeof(nid), trayWindow, TRAY_ID, NIF_MESSAGE | NIF_ICON | NIF_TIP, WM_TRAY, icon };
    strcpy(nid.szTip, "System Transparency");
    Shell_NotifyIconA(NIM_ADD, &nid);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIconA(NIM_DELETE, &nid);
    return null;
}

/* ---------------- 키보드/마우스 훅 ---------------- */
static LRESULT CALLBACK KeyboardHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT* key = (KBDLLHOOKSTRUCT*)lParam;
        boolean down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        boolean up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        if (key->vkCode == VK_CONTROL)
            ctrlDown = down ? true : (up ? false : ctrlDown);

        if (key->vkCode == VK_LWIN || key->vkCode == VK_RWIN) {
            if (down) {
                winDown = true;
                winUsed = false;
            } else if (up) {
                winDown = false;
                if (!winUsed) {
                    INPUT input = {0};
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = key->vkCode;
                    SendInput(1, &input, sizeof(input));
                    input.ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(1, &input, sizeof(input));
                }
            }
            return 1; // Win키 자체 메시지 차단
        }
    }
    return CallNextHookEx(null, code, wParam, lParam);
}

static LRESULT CALLBACK MouseHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        MSLLHOOKSTRUCT* mouse = (MSLLHOOKSTRUCT*)lParam;
        HWND window = WindowFromPoint(mouse->pt);
        window = GetAncestor(window, GA_ROOT);
        if (!window) return CallNextHookEx(null, code, wParam, lParam);

        boolean ctrl = ctrlDown || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        boolean win  = winDown  || ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);

        boolean applied = false;

        // Ctrl + MIDDLE 클릭
        if (wParam == WM_MBUTTONDOWN) {
            if (ctrl) {
                applied = ApplyTransparency(window, ALPHA_TRANSPARENT);
            } 
            if (win) {
                applied = ApplyTransparency(window, ALPHA_OPAQUE);
                winUsed = true;
            }
            if (applied) return 1; // 클릭 차단
        }

        // Ctrl+Win+휠 단계별 알파 조정
        if ((wParam == WM_MOUSEWHEEL) && ctrl && win) {
            int delta = GET_WHEEL_DELTA_WPARAM(mouse->mouseData);
            BYTE alpha = GetWindowAlpha(window);
            if (delta > 0) alpha = (alpha + 15 > 255) ? 255 : alpha + 15;
            else alpha = (alpha - 15 < 60) ? 60 : alpha - 15;
            ApplyTransparency(window, alpha);
            return 1; // 휠 이벤트 차단
        }
    }
    return CallNextHookEx(null, code, wParam, lParam);
}

static void* HookThread(void* arg) {
    keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHook, null, 0);
    mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHook, null, 0);

    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (keyHook) UnhookWindowsHookEx(keyHook);
    if (mouseHook) UnhookWindowsHookEx(mouseHook);
    return null;
}

static void* MsgLoopThread(void* arg) {
    MSG msg;
    while (GetMessage(&msg, null, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return null;
}


/* ---------------- main ---------------- */
int main(void) {
    EnumWindows(EnumExplorerWindows, 0);
    Thread msgLoopThread = new_Thread(MsgLoopThread);
    Thread trayThread = new_Thread(TrayThread);
    Thread hookThread = new_Thread(HookThread);

    trayThread.start(&trayThread);

    winEventHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, null, WinEventCallback, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    
    hookThread.start(&hookThread);
    
    msgLoopThread.start(&msgLoopThread);

    trayThread.join(&trayThread);

    trayThread.cancel(&trayThread);
    hookThread.cancel(&hookThread);
    msgLoopThread.cancel(&msgLoopThread);

    trayThread.delete(&trayThread);
    hookThread.delete(&hookThread);
    msgLoopThread.delete(&msgLoopThread);

    if (winEventHook) UnhookWinEvent(winEventHook);
    ExitProcess(0);
}
