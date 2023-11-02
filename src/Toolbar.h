/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToolbar(MainWindow* win);
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility);
void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win);
void UpdateToolbarFindText(MainWindow* win);
void UpdateToolbarPageText(MainWindow* win, int pageCount, bool updateOnly = false);
void UpdateFindbox(MainWindow* win);
void ShowOrHideToolbar(MainWindow* win);
void UpdateToolbarState(MainWindow* win);
