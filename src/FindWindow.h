/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindWindowWnd;

// The floating, movable/resizable variant of the find UI (see SearchUIFloating).
// Phase 1: search controls only; a results list is added in a later phase.
FindWindowWnd* CreateFindWindow(MainWindow* win);
void DeleteFindWindow(MainWindow* win);
void ShowFindWindow(MainWindow* win);
void HideFindWindow(MainWindow* win);
bool IsFindWindowVisible(MainWindow* win);
void FindWindowSetStatus(MainWindow* win, const char* s);
void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked);
void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked);
// repopulate the results list from win->findMatches (no-op if not visible)
void FindWindowRefreshResults(MainWindow* win);
// re-apply theme colors/icons to the floating window after a theme change
void UpdateFindWindowTheme(MainWindow* win);
