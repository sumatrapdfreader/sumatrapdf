/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;
struct FindBarWnd;
struct PlatformFont;

FindBarWnd* CreateFindBar(MainWindow* win);
void DeleteFindBar(MainWindow* win);
void RecreateFindBar(MainWindow* win);
void FindBarUpdateDpi(MainWindow* win);
int FindBarFontHeight(MainWindow* win);
int FindBarWindowHeight(MainWindow* win);
void ShowFindBar(MainWindow* win);
void HideFindBar(MainWindow* win);
bool IsFindBarVisible(MainWindow* win);
bool IsFindUIVisible(MainWindow* win);
void FindBarReposition(MainWindow* win);
void FindBarSetStatus(MainWindow* win, Str s, int totalHits = -1);
int FindStatusDx(PlatformFont* font, int totalHits, bool capped);
void StartPickedFindTerm(MainWindow* win, Str term);
void FindBarSetMatchCaseChecked(MainWindow* win, bool checked);
void FindBarSetMatchWholeWordChecked(MainWindow* win, bool checked);

void ToggleFloatingFindUI(MainWindow* win);
void FocusFindEditSelectAll(MainWindow* win);
void FindBarSyncHistory(MainWindow* win);
TempStr FindUiStateResultTemp(Str action, int* exitCodeOut = nullptr);
