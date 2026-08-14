/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindWindowWnd;

FindWindowWnd* CreateFindWindow(MainWindow* win);
void DeleteFindWindow(MainWindow* win);
void ShowFindWindow(MainWindow* win);
void HideFindWindow(MainWindow* win);
bool IsFindWindowVisible(MainWindow* win);
void FindWindowSetStatus(MainWindow* win, Str s);
void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked);
void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked);
void FindWindowRefreshResults(MainWindow* win, bool allowNavigation = true);
void FindWindowUpdatePagesLabel(MainWindow* win);
void FindWindowSaveSelectedMatch(MainWindow* win);
void UpdateFindWindowTheme(MainWindow* win);

TempStr FindResultPageColumnClipResultTemp(int* exitCodeOut = nullptr);
TempStr FindResultsOrderResultTemp(Str term, int startPage, int* exitCodeOut = nullptr);
