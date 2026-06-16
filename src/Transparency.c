#include <windows.h>
#include <string.h>

#include "Transparency.h"

static BYTE presetToAlpha(Transparency* self, TransparencyPreset preset) {
    (void)self;

    switch (preset) {
        case PRESET_SOLID: return 255;
        case PRESET_SOFT:  return 200;
        case PRESET_GLASS: return 150;
        case PRESET_GHOST: return 80;
        case PRESET_CUSTOM: return ALPHA_TRANSPARENT;
    }

    return 150;
}

static BYTE getWindowAlpha(Transparency* self, HWND hwnd) {
    (void)self;

    if (!IsWindow(hwnd)) return ALPHA_OPAQUE;

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) return ALPHA_OPAQUE;

    BYTE alpha = ALPHA_OPAQUE;
    DWORD flags = 0;

    if (!GetLayeredWindowAttributes(hwnd, NULL, &alpha, &flags))
        return ALPHA_OPAQUE;

    return alpha;
}

static boolean apply(Transparency* self, HWND hwnd, BYTE alpha) {
    if (!IsWindow(hwnd)) return false;

    LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED))
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);

    if (self->getWindowAlpha(self, hwnd) == alpha)
        return true;

    return SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

static void refresh(Transparency* self, HWND hwnd) {
    (void)self;
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

static boolean isTarget(Transparency* self, HWND hwnd) {
    char cls[128];
    (void)self;

    GetClassNameA(hwnd, cls, sizeof(cls));
    return !strcmp(cls, "CabinetWClass") || !strcmp(cls, "ExploreWClass");
}

Transparency new_Transparency(void) {
    return (Transparency) {
        .presetToAlpha = presetToAlpha,
        .getWindowAlpha = getWindowAlpha,
        .apply = apply,
        .refresh = refresh,
        .isTarget = isTarget
    };
}
