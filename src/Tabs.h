/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

int GetTabbarHeight(HWND, float factor = 1.f);

void SaveCurrentTabInfo(WindowInfo* win);
void LoadModelIntoTab(TabInfo* tab);

void CreateTabbar(WindowInfo* win);
TabInfo* CreateNewTab(WindowInfo* win, const WCHAR* filePath);
void TabsOnCloseDoc(WindowInfo* win);
void TabsOnCloseWindow(WindowInfo* win);
void TabsOnChangedDoc(WindowInfo* win);
LRESULT TabsOnNotify(WindowInfo* win, LPARAM lp, int tab1 = -1, int tab2 = -1);
void TabsSelect(WindowInfo* win, int tabIndex);
void TabsOnCtrlTab(WindowInfo* win, bool reverse);
// also shows/hides the tabbar when necessary
void UpdateTabWidth(WindowInfo* win);
void SetTabsInTitlebar(WindowInfo* win, bool set);
void UpdateCurrentTabBgColor(WindowInfo* win);
