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
void FindWindowSetStatus(MainWindow* win, Str s);
void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked);
void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked);
// repopulate the results list from win->findMatches (no-op if not visible).
// allowNavigation=false for streamed partial updates: don't navigate the
// document (navigation would cancel the in-flight count scan)
void FindWindowRefreshResults(MainWindow* win, bool allowNavigation = true);
// re-apply theme colors/icons to the floating window after a theme change
void UpdateFindWindowTheme(MainWindow* win);

// Headless draw test for issue #5736: match highlights must not bleed into the page column.
TempStr FindResultPageColumnClipResultTemp(int* exitCodeOut = nullptr);
