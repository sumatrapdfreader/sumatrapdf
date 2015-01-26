/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(WindowInfo *win);
void ClearTocBox(WindowInfo *win);
void ToggleTocBox(WindowInfo *win);
void LoadTocTree(WindowInfo *win);
void UpdateTocColors(WindowInfo *win);
void UpdateTocSelection(WindowInfo *win, int currPageNo);
void UpdateTocExpansionState(TabInfo *tab, HWND hwndTocTree, HTREEITEM hItem);
