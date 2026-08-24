#include <windows.h>
#include <string.h>

#include "Settings.h"

static void reset(Settings* self) {
    self->explorerAuto = true;
    self->startupEnabled = true;
    self->preset = PRESET_GLASS;
    self->customAlpha = 150;
    self->applyModifiers = HOTKEY_MOD_CTRL;
    self->restoreModifiers = HOTKEY_MOD_WIN;
    self->adjustModifiers = HOTKEY_MOD_CTRL | HOTKEY_MOD_WIN;
}

static boolean validModifiers(DWORD modifiers) {
    return modifiers > 0 && (modifiers & ~(HOTKEY_MOD_CTRL | HOTKEY_MOD_ALT | HOTKEY_MOD_SHIFT | HOTKEY_MOD_WIN)) == 0;
}

static void load(Settings* self) {
    HKEY key;
    DWORD size, type, val;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SystemTransparency", 0, KEY_READ, &key ) != ERROR_SUCCESS)
        return;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "ExplorerAuto", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->explorerAuto = (boolean)val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "StartupEnabled", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->startupEnabled = (boolean)val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "Preset", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->preset = (TransparencyPreset)val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "CustomAlpha", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->customAlpha = (BYTE)val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "ApplyModifiers", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS && validModifiers(val))
        self->applyModifiers = val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "RestoreModifiers", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS && validModifiers(val))
        self->restoreModifiers = val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "AdjustModifiers", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS && validModifiers(val))
        self->adjustModifiers = val;

    RegCloseKey(key);
}

static void save(Settings* self) {
    HKEY key;

    if (RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\SystemTransparency", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    DWORD explorer = self->explorerAuto;
    DWORD startup = self->startupEnabled;
    DWORD preset = self->preset;
    DWORD customAlpha = self->customAlpha;
    DWORD applyModifiers = self->applyModifiers;
    DWORD restoreModifiers = self->restoreModifiers;
    DWORD adjustModifiers = self->adjustModifiers;

    RegSetValueExA(key, "ExplorerAuto", 0, REG_DWORD, (BYTE*)&explorer, sizeof(DWORD));
    RegSetValueExA(key, "StartupEnabled", 0, REG_DWORD, (BYTE*)&startup, sizeof(DWORD));
    RegSetValueExA(key, "Preset", 0, REG_DWORD, (BYTE*)&preset, sizeof(DWORD));
    RegSetValueExA(key, "CustomAlpha", 0, REG_DWORD, (BYTE*)&customAlpha, sizeof(DWORD));
    RegSetValueExA(key, "ApplyModifiers", 0, REG_DWORD, (BYTE*)&applyModifiers, sizeof(DWORD));
    RegSetValueExA(key, "RestoreModifiers", 0, REG_DWORD, (BYTE*)&restoreModifiers, sizeof(DWORD));
    RegSetValueExA(key, "AdjustModifiers", 0, REG_DWORD, (BYTE*)&adjustModifiers, sizeof(DWORD));
    RegCloseKey(key);
}

Settings new_Settings(void) {
    Settings settings;
    reset(&settings);
    settings.load = load;
    settings.save = save;
    settings.reset = reset;
    return settings;
}
