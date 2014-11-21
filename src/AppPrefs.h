/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

extern GlobalPrefs *gGlobalPrefs;

Favorite *NewFavorite(int pageNo, const WCHAR *name, const WCHAR *pageLabel);
void DeleteFavorite(Favorite *fav);

DisplayState *NewDisplayState(const WCHAR *filePath);
void DeleteDisplayState(DisplayState *ds);

namespace prefs {

WCHAR *GetSettingsPath();

bool Load();
bool Save();
bool Reload();
void CleanUp();

void RegisterForFileChanges();
void UnregisterForFileChanges();

namespace conv {

const WCHAR *   FromDisplayMode(DisplayMode mode);
DisplayMode     ToDisplayMode(const WCHAR *s, DisplayMode defVal=DM_AUTOMATIC);
void            FromZoom(char **dst, float zoom, DisplayState *stateForIssue2140=NULL);
float           ToZoom(const char *s, float defVal=ZOOM_FIT_PAGE);

};

};
