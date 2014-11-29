/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

int GetTabbarHeight(WindowInfo *win, float factor=1.f);

void SaveCurrentTabInfo(WindowInfo *win);
void LoadModelIntoTab(WindowInfo *win, TabInfo *tdata);

void CreateTabbar(WindowInfo *win);
void TabsOnLoadedDoc(WindowInfo *win);
void TabsOnCloseDoc(WindowInfo *win);
void TabsOnCloseWindow(WindowInfo *win);
void TabsOnChangedDoc(WindowInfo *win);
LRESULT TabsOnNotify(WindowInfo *win, LPARAM lparam, int tab1=-1, int tab2=-1);
void TabsSelect(WindowInfo *win, int tabIndex);
void TabsOnCtrlTab(WindowInfo *win, bool reverse);
// also shows/hides the tabbar when necessary
void UpdateTabWidth(WindowInfo *win);
void SetTabsInTitlebar(WindowInfo *win, bool set);
void SetCurrentTabBgCol(WindowInfo *win, COLORREF bgCol);

COLORREF AdjustLightness2(COLORREF c, float units);
