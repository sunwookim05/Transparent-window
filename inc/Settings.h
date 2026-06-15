#include "main.h"

#ifndef __SETTINGS_H
#define __SETTINGS_H

typedef enum {
    PRESET_SOLID,
    PRESET_SOFT,
    PRESET_GLASS,
    PRESET_GHOST
} TransparencyPreset;

typedef struct SETTINGS {
    boolean explorerAuto;
    TransparencyPreset preset;

    void (*load)(struct SETTINGS*);
    void (*save)(struct SETTINGS*);
} Settings;

Settings new_Settings(void);

#endif
