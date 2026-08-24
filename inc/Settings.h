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

typedef enum {
    LANGUAGE_SYSTEM,
    LANGUAGE_ENGLISH,
    LANGUAGE_KOREAN
} LanguageMode;

#define HOTKEY_MOD_CTRL  0x01
#define HOTKEY_MOD_ALT   0x02
#define HOTKEY_MOD_SHIFT 0x04
#define HOTKEY_MOD_WIN   0x08
#define HOTKEY_MAX_MODIFIERS 4

typedef struct SETTINGS {
    boolean explorerAuto;
    boolean startupEnabled;
    TransparencyPreset preset;
    BYTE customAlpha;
    DWORD applyModifiers;
    DWORD restoreModifiers;
    DWORD adjustModifiers;
    LanguageMode language;

    void (*load)(struct SETTINGS*);
    void (*save)(struct SETTINGS*);
    void (*reset)(struct SETTINGS*);
} Settings;

Settings new_Settings(void);

#endif
