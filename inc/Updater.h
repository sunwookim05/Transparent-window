#include "main.h"
#include "Installer.h"

#ifndef __UPDATER_H
#define __UPDATER_H

typedef struct UPDATER {
    Installer installer;

    void (*checkAsync)(struct UPDATER*);
    void (*checkNow)(struct UPDATER*);
} Updater;

Updater new_Updater(Installer installer);

#endif
