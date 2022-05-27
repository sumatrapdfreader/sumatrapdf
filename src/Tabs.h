/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

int GetTabbarHeight(HWND, float factor = 1.f);

void SaveCurrentTabInfo(MainWindow* win);
void LoadModelIntoTab(TabInfo* tab);

void CreateTabbar(MainWindow* win);
TabInfo* CreateNewTab(MainWindow* win, const char* filePath);
void TabsOnCloseDoc(MainWindow* win);
void TabsOnCloseWindow(MainWindow* win);
void TabsOnChangedDoc(MainWindow* win);
LRESULT TabsOnNotify(MainWindow* win, LPARAM lp, int tab1 = -1, int tab2 = -1);
void TabsSelect(MainWindow* win, int tabIndex);
void TabsOnCtrlTab(MainWindow* win, bool reverse);
// also shows/hides the tabbar when necessary
void UpdateTabWidth(MainWindow* win);
void SetTabsInTitlebar(MainWindow* win, bool inTitlebar);
void UpdateCurrentTabBgColor(MainWindow* win);
