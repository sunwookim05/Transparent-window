#include <windows.h>

#include "Tracker.h"

static boolean isTracked(Tracker* self, HWND hwnd) {
    for (int i = 0; i < self->count; i++)
        if (self->windows[i].hwnd == hwnd) return true;

    return false;
}

static void track(Tracker* self, Transparency* transparency, HWND hwnd) {
    if (self->count >= MAX_TRACKED_WINDOWS) return;
    if (self->isTracked(self, hwnd)) return;

    WindowAlpha original = {0};
    original.hwnd = hwnd;
    original.originalAlpha = transparency->getWindowAlpha(transparency, hwnd);
    original.originalExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    GetWindowThreadProcessId(hwnd, &original.processId);
    if (original.originalExStyle & WS_EX_LAYERED)
        GetLayeredWindowAttributes(hwnd, &original.originalColorKey, &original.originalAlpha, &original.originalFlags);
    self->windows[self->count++] = original;
}

static void restoreAll(Tracker* self, Transparency* transparency) {
    for (int i = 0; i < self->count; i++) {
        WindowAlpha* original = &self->windows[i];
        DWORD processId = 0;
        if (!IsWindow(original->hwnd)) continue;
        GetWindowThreadProcessId(original->hwnd, &processId);
        if (processId != original->processId) continue;
        if (!(original->originalExStyle & WS_EX_LAYERED)) {
            LONG current = GetWindowLong(original->hwnd, GWL_EXSTYLE);
            SetWindowLong(original->hwnd, GWL_EXSTYLE, current & ~WS_EX_LAYERED);
        } else if (original->originalFlags) {
            SetLayeredWindowAttributes(original->hwnd, original->originalColorKey,
                original->originalAlpha, original->originalFlags);
        } else {
            transparency->apply(transparency, original->hwnd, original->originalAlpha);
        }
        transparency->refresh(transparency, original->hwnd);
        RedrawWindow(original->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    self->count = 0;
}

static void removeWindow(Tracker* self, HWND hwnd) {
    for (int i = 0; i < self->count; i++) {
        if (self->windows[i].hwnd == hwnd) {
            for (int j = i; j < self->count - 1; j++)
                self->windows[j] = self->windows[j + 1];

            self->count--;
            return;
        }
    }
}

Tracker new_Tracker(void) {
    return (Tracker) {
        .count = 0,
        .isTracked = isTracked,
        .track = track,
        .remove = removeWindow,
        .restoreAll = restoreAll
    };
}
