/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(WindowInfo*);
void ClearTocBox(WindowInfo*);
void ToggleTocBox(WindowInfo*);
void LoadTocTree(WindowInfo*);
void UpdateTreeCtrlColors(WindowInfo*);
void UpdateTocSelection(WindowInfo*, int currPageNo);
void UpdateTocExpansionState(Vec<int>& tocState, TreeView*, TocTree*);

// shared with Favorites.cpp
// void TocCustomizeTooltip(TreeItemGetTooltipEvent*);
// LRESULT TocTreeKeyDown2(TreeKeyDownEvent*);

// void TocTreeCharHandler(CharEvent* ev);
// void TocTreeMouseWheelHandler(MouseWheelEvent* ev);
