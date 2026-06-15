#include <windows.h>

#include "Settings.h"

static void load(Settings* self) {
    HKEY key;
    DWORD size, type, val;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\SystemTransparency", 0, KEY_READ, &key ) != ERROR_SUCCESS)
        return;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "ExplorerAuto", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->explorerAuto = (boolean)val;

    size = sizeof(DWORD);
    if (RegQueryValueExA(key, "Preset", NULL, &type, (BYTE*)&val, &size) == ERROR_SUCCESS)
        self->preset = (TransparencyPreset)val;

    RegCloseKey(key);
}

static void save(Settings* self) {
    HKEY key;

    if (RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\SystemTransparency", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;

    DWORD explorer = self->explorerAuto;
    DWORD preset = self->preset;

    RegSetValueExA(key, "ExplorerAuto", 0, REG_DWORD, (BYTE*)&explorer, sizeof(DWORD));
    RegSetValueExA(key, "Preset", 0, REG_DWORD, (BYTE*)&preset, sizeof(DWORD));

    RegCloseKey(key);
}

Settings new_Settings(void) {
    return (Settings) {
        .explorerAuto = true,
        .preset = PRESET_GLASS,
        .load = load,
        .save = save
    };
}
