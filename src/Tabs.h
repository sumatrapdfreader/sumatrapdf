/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define kTabBarDy 24

int GetTabbarHeight(HWND, float factor = 1.f);

void SaveCurrentWindowTab(MainWindow*);
void LoadModelIntoTab(WindowTab*);

void CreateTabbar(MainWindow*);
WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab, bool deferUpdate = false);
void TabsOnCloseWindow(MainWindow*);
void TabsOnChangedDoc(MainWindow*);
void TabsSelect(MainWindow* win, int tabIndex);
void TabsOnCtrlTab(MainWindow* win, bool reverse);
void UpdateTabWidth(MainWindow*);
void SetTabsInTitlebar(MainWindow* win, bool inTitlebar);
void RemoveTab(WindowTab*);
void SetTabInfoColor(WindowTab*);
void UpdateTabIsError(WindowTab*);
TempStr MakeTabTooltipTemp(Str path, bool dirty = false);
void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft);
void CloseAllTabs(MainWindow*);
void MoveTab(MainWindow* win, int dir);
