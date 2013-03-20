/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

bool ParseViewMode(DisplayMode *mode, const WCHAR *txt);

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

#include "AppPrefs2.h"

extern SerializableGlobalPrefs gGlobalPrefs;
extern AdvancedSettings gUserPrefs;

namespace DisplayModeConv {

const WCHAR *   NameFromEnum(DisplayMode var);
bool            EnumFromName(const WCHAR *txt, DisplayMode *resOut);

}

bool LoadPrefs();
bool SavePrefs();
bool ReloadPrefs();

#endif
