/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow* win);
void ToolbarUpdateStateForWindow(MainWindow*, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow*);
void UpdateToolbarFindText(MainWindow*);
void UpdateToolbarPageText(MainWindow*, int pageCount, bool updateOnly = false);
void UpdateFindbox(MainWindow*);
void SetToolbarButtonEnableState(MainWindow*, int cmdId, bool isEnabled);
void SetToolbarButtonCheckedState(MainWindow*, int cmdId, bool isChecked);
bool ShouldShowToolbar(MainWindow*);
void ShowOrHideToolbar(MainWindow*);
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);

void CreateMenuBarRebar(MainWindow*);
void DestroyMenuBarRebar(MainWindow*);
void ShowMenuBarRebar(MainWindow*);
void RebuildMenuBarButtons(MainWindow*);
bool IsShowingMenuBarRebar(MainWindow*);
bool HandleMenuBarCommand(MainWindow*, int cmdId);
bool ActivateMenuBarByAccel(MainWindow*, WCHAR accel);
void UpdateCustomMenuBarMenuSelect(MainWindow*, WPARAM, LPARAM);
