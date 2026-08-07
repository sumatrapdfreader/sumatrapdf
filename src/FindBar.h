/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindBarWnd;

FindBarWnd* CreateFindBar(MainWindow* win);
void DeleteFindBar(MainWindow* win);
void RecreateFindBar(MainWindow* win);
void ShowFindBar(MainWindow* win);
void HideFindBar(MainWindow* win);
bool IsFindBarVisible(MainWindow* win);
bool IsFindUIVisible(MainWindow* win);
void FindBarReposition(MainWindow* win);
void FindBarSetStatus(MainWindow* win, Str s);
void FindBarSetMatchCaseChecked(MainWindow* win, bool checked);
void FindBarSetMatchWholeWordChecked(MainWindow* win, bool checked);

void ToggleFloatingFindUI(MainWindow* win);
void FocusFindEditSelectAll(MainWindow* win);
