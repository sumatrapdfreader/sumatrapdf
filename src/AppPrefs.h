/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef AppPrefs_h
#define AppPrefs_h

#include "DisplayState.h"

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

extern GlobalPrefs *gGlobalPrefs;
// convenience pointer for &gGlobalPrefs->userPrefs
extern UserPrefs *gUserPrefs;

void DeleteGlobalPrefs(GlobalPrefs *globalPrefs);

namespace DisplayModeConv {

const WCHAR *   NameFromEnum(DisplayMode var);
DisplayMode     EnumFromName(const WCHAR *txt, DisplayMode default);

}

bool LoadPrefs();
bool SavePrefs();
bool ReloadPrefs();

#endif
