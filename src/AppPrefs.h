/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#define PREFS_FILE_NAME         L"SumatraPDF-settings.txt"

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

#include "DisplayState.h"

extern GlobalPrefs *gGlobalPrefs;

void DeleteGlobalPrefs(GlobalPrefs *globalPrefs);

bool LoadPrefs();
bool SavePrefs();
bool ReloadPrefs();

namespace DisplayModeConv {

const WCHAR *   NameFromEnum(DisplayMode var);
DisplayMode     EnumFromName(const WCHAR *txt, DisplayMode default);

}

#endif
