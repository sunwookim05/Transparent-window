#include "main.h"

#ifndef __SETTINGS_H
#define __SETTINGS_H

typedef enum {
    PRESET_SOLID,
    PRESET_SOFT,
    PRESET_GLASS,
    PRESET_GHOST,
    PRESET_CUSTOM
} TransparencyPreset;

typedef struct SETTINGS {
    boolean explorerAuto;
    boolean startupEnabled;
    TransparencyPreset preset;
    BYTE customAlpha;

    void (*load)(struct SETTINGS*);
    void (*save)(struct SETTINGS*);
    void (*reset)(struct SETTINGS*);
} Settings;

Settings new_Settings(void);

#endif
