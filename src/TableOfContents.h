/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(MainWindow*);
void ClearTocBox(MainWindow*);
void ToggleTocBox(MainWindow*);
void LoadTocTree(MainWindow*);
void UpdateTocSelection(MainWindow*, int currPageNo);
void UpdateTocExpansionState(Vec<int>& tocState, TreeView*, TocTree*);
void UnsubclassToc(MainWindow*);

// shared with Favorites.cpp
// void TocCustomizeTooltip(TreeItemGetTooltipEvent*);
// LRESULT TocTreeKeyDown2(TreeKeyDownEvent*);

// void TocTreeCharHandler(CharEvent* ev);
// void TocTreeMouseWheelHandler(MouseWheelEvent* ev);
