/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

int GetTabbarHeight(HWND, float factor = 1.f);

void SaveCurrentWindowTab(MainWindow*);
void LoadModelIntoTab(WindowTab*);

void CreateTabbar(MainWindow*);
WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab);
void TabsOnCloseWindow(MainWindow*);
void TabsOnChangedDoc(MainWindow*);
void TabsSelect(MainWindow* win, int tabIndex);
void TabsOnCtrlTab(MainWindow* win, bool reverse);
// also shows/hides the tabbar when necessary
void UpdateTabWidth(MainWindow*);
void SetTabsInTitlebar(MainWindow* win, bool inTitlebar);
void RemoveTab(WindowTab*);
// create a new window if win==nullptr
void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft);
void CloseAllTabs(MainWindow*);
void MoveTab(MainWindow* win, int dir);
