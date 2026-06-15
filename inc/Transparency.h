#include "main.h"
#include "Settings.h"

#ifndef __TRANSPARENCY_H
#define __TRANSPARENCY_H

#define ALPHA_TRANSPARENT 150
#define ALPHA_OPAQUE 255

typedef struct TRANSPARENCY {
    BYTE (*presetToAlpha)(struct TRANSPARENCY*, TransparencyPreset);
    BYTE (*getWindowAlpha)(struct TRANSPARENCY*, HWND);
    boolean (*apply)(struct TRANSPARENCY*, HWND, BYTE);
    void (*refresh)(struct TRANSPARENCY*, HWND);
    boolean (*isTarget)(struct TRANSPARENCY*, HWND);
} Transparency;

Transparency new_Transparency(void);

#endif
