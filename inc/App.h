#include "main.h"
#include "Settings.h"
#include "Transparency.h"
#include "Tracker.h"

#ifndef __APP_H
#define __APP_H

typedef struct APP {
    Settings settings;
    Transparency transparency;
    Tracker tracker;

    volatile boolean ctrlDown;
    volatile boolean winDown;
    volatile boolean winUsed;
    volatile boolean shuttingDown;

    HHOOK keyHook;
    HHOOK mouseHook;
    HWINEVENTHOOK winEventHook;
    HWND trayWindow;
    boolean trayIconAdded;
    UINT taskbarCreatedMessage;

    void (*load)(struct APP*);
    void (*run)(struct APP*);
    void (*applyExplorerAutoAll)(struct APP*);
} App;

App new_App(void);

#endif
