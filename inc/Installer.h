#include "main.h"

#ifndef __INSTALLER_H
#define __INSTALLER_H

#define APP_NAME "SystemTransparency"
#define APP_VERSION "1.0.3"
#define APP_REG_KEY "Software\\SystemTransparency"
#define APP_TASK_NAME "SystemTransparency"
#define APP_EXE_NAME "SystemTransparency.exe"

typedef struct INSTALLER {
    boolean (*ensure)(struct INSTALLER*, int, string*);
    boolean (*isInstalledPath)(struct INSTALLER*);
    boolean (*getInstalledExePath)(struct INSTALLER*, string, DWORD);
    void (*install)(struct INSTALLER*);
} Installer;

Installer new_Installer(void);

#endif
