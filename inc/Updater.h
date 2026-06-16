#include "main.h"
#include "Installer.h"

#ifndef __UPDATER_H
#define __UPDATER_H

typedef enum {
    UPDATE_FAILED,
    UPDATE_CURRENT,
    UPDATE_STARTED,
    UPDATE_SKIPPED
} UpdateResult;

typedef struct UPDATER {
    Installer installer;

    void (*checkAsync)(struct UPDATER*);
    UpdateResult (*checkNow)(struct UPDATER*);
} Updater;

Updater new_Updater(Installer installer);

#endif
