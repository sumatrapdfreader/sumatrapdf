/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MainWindow;

void CreateToolbar(MainWindow*);
void ReCreateToolbar(MainWindow* win);
void DestroyToolbar(MainWindow* win);
void ToolbarUpdateStateForWindow(MainWindow*, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow*);
void UpdateToolbarFindText(MainWindow*);
void UpdateToolbarPageText(MainWindow*, int pageCount, bool updateOnly = false);
void UpdateFindbox(MainWindow*);
void SetToolbarButtonEnableState(MainWindow*, int cmdId, bool isEnabled);
void SetToolbarButtonCheckedState(MainWindow*, int cmdId, bool isChecked);
bool ShouldShowToolbar(MainWindow*);
bool ShouldOverlayToolbar(MainWindow*);
void ShowOrHideToolbar(MainWindow*);
void PositionOverlayToolbar(MainWindow*);
void UpdateOverlayToolbarForMouse(MainWindow*);
void RevealOverlayToolbar(MainWindow*);

// delay before the overlay toolbar hides after the mouse moves away
constexpr int kDelayToolbarHide = 500;
void UpdateToolbarState(MainWindow*);
void UpdateToolbarAfterThemeChange(MainWindow*);
Rect GetToolbarButtonScreenRect(MainWindow*, int cmdId);
void ToolbarNoteDropdownClosed();

TempStr ToolbarButtonsResultTemp(int* exitCodeOut);
