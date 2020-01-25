/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* enum from windowState */
enum {
    WIN_STATE_NORMAL = 1, /* use remembered position and size */
    WIN_STATE_MAXIMIZED,  /* ignore position and size, maximize the window */
    WIN_STATE_FULLSCREEN,
    WIN_STATE_MINIMIZED,
};

namespace prefs {

WCHAR* GetSettingsFileNameNoFree();
WCHAR* GetSettingsPath();

bool Load();
bool Save();
bool Reload();
void CleanUp();

void RegisterForFileChanges();
void UnregisterForFileChanges();

}; // namespace prefs
