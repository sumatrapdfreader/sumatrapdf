/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindWindowWnd;
struct DropDown;

FindWindowWnd* CreateFindWindow(MainWindow* win);
void DeleteFindWindow(MainWindow* win);
void ShowFindWindow(MainWindow* win);
void HideFindWindow(MainWindow* win);
bool IsFindWindowVisible(MainWindow* win);
void FindWindowSetStatus(MainWindow* win, Str s, int totalHits = -1);
void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked);
void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked);
void FindWindowRefreshResults(MainWindow* win, bool allowNavigation = true);
void FindWindowUpdatePagesLabel(MainWindow* win);
void FindWindowSaveSelectedMatch(MainWindow* win);
void UpdateFindWindowTheme(MainWindow* win);
int FindWindowFontHeight(MainWindow* win);
void FindWindowSyncHistory(MainWindow* win);

TempStr FindResultPageColumnClipResultTemp(int* exitCodeOut = nullptr);
TempStr FindResultsOrderResultTemp(Str term, int startPage, int* exitCodeOut = nullptr);
TempStr FindWindowContentsResultTemp(int* exitCodeOut = nullptr);

TempStr AppendCmdAccel(Str base, int cmd);
void FindHistoryCommitted(MainWindow* win, DropDown* edit, bool suppressed);
void FindTextChanged(MainWindow* win, bool suppressed);
