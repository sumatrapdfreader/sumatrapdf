/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToolbar(WindowInfo *win);
void ToolbarUpdateStateForWindow(WindowInfo *win, bool showHide);
void UpdateToolbarButtonsToolTipsForWindow(WindowInfo *win);
void UpdateToolbarFindText(WindowInfo *win);
void UpdateToolbarPageText(WindowInfo *win, int pageCount, bool updateOnly=false);
void UpdateFindbox(WindowInfo* win);
void ShowOrHideToolbarGlobally();
void UpdateToolbarState(WindowInfo *win);
