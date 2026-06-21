/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindBarWnd;

// Chrome-style floating search bar. Created hidden together with the toolbar;
// owns win->hwndFindEdit. Shown via Ctrl+F or the toolbar search icon.
FindBarWnd* CreateFindBar(MainWindow* win);
void DeleteFindBar(MainWindow* win);
void ShowFindBar(MainWindow* win);
void HideFindBar(MainWindow* win);
bool IsFindBarVisible(MainWindow* win);
// reposition over the search toolbar icon (no-op if not visible)
void FindBarReposition(MainWindow* win);
// show n/m or "No matches" style status in the bar
void FindBarSetStatus(MainWindow* win, const char* s);
// reflect match-case toggle state on the bar's button
void FindBarSetMatchCaseChecked(MainWindow* win, bool checked);
