/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr int kTabBarDy = 24;

struct MenuModel;

#if OS_WIN
int GetTabbarHeight(HWND, float factor = 1.f);
void CreateTabbar(MainWindow*);
void UpdateTabWidth(MainWindow*);
void SetTabsInTitlebar(MainWindow* win, bool inTitlebar);
void SetTabInfoColor(WindowTab*);
void UpdateTabIsError(WindowTab*);
#endif

void SaveCurrentWindowTab(MainWindow*);
void LoadModelIntoTab(WindowTab*);

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab, bool deferUpdate = false);
void TabsOnCloseWindow(MainWindow*);
void TabsOnChangedDoc(MainWindow*);
void TabsSelect(MainWindow* win, int tabIndex);
void GoToHomeTab(MainWindow*);
void TabsOnCtrlTab(MainWindow* win, bool reverse);
void RemoveTab(WindowTab*);
TempStr MakeTabTooltipTemp(Str path, bool dirty = false);
void CloseAllTabs(MainWindow*);
void MoveTab(MainWindow* win, int dir);

// the tab strip shows a suffix with the current page ("3/17") when
// ShowPageNumberInTabs is on
TempStr TabPageSuffixTemp(WindowTab* tab);
void UpdateTabPageText(WindowTab* tab);
bool TabIsDirty(WindowTab* tab);

// is the tab bar shown at all (orig's UpdateTabWidth)
bool TabsAreVisible(MainWindow* win);
// orig's UpdateAfterDrag: move the tab at `from` so it lands at `to`
void TabsMoveTab(MainWindow* win, int from, int to);
// a tab dropped outside the strip moves to `newWin`, or into a new window
void MaybeMigrateTab(WindowTab* tab, MainWindow* newWin);

void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft);
void CloseCollectedTabs(MainWindow* win, const Vec<WindowTab*>& toClose);
// the tab's right-click menu; the caller owns the model
MenuModel* BuildTabContextMenu(MainWindow* win, WindowTab* tabUnderMouse);
void TabContextMenuCommand(MainWindow* win, WindowTab* tabUnderMouse, int cmdId);
