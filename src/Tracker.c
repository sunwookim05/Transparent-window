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

    self->windows[self->count++] = (WindowAlpha){ hwnd, transparency->getWindowAlpha(transparency, hwnd) };
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
        .remove = removeWindow
    };
}
