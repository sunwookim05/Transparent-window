#include "main.h"
#include "Transparency.h"

#ifndef __TRACKER_H
#define __TRACKER_H

#define MAX_TRACKED_WINDOWS 64

typedef struct {
    HWND hwnd;
    BYTE originalAlpha;
} WindowAlpha;

typedef struct TRACKER {
    WindowAlpha windows[MAX_TRACKED_WINDOWS];
    int count;

    boolean (*isTracked)(struct TRACKER*, HWND);
    void (*track)(struct TRACKER*, Transparency*, HWND);
    void (*remove)(struct TRACKER*, HWND);
} Tracker;

Tracker new_Tracker(void);

#endif
