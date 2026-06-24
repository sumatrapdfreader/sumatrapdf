/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(MainWindow*);
void ClearTocBox(MainWindow*);
void ToggleTocBox(MainWindow*);
void LoadTocTree(MainWindow*);
void UpdateTocSelection(MainWindow*, int currPageNo);
void ExpandTocToCurrentPage(MainWindow*);
void UpdateTocExpansionState(Vec<int>& tocState, TreeView*, TocTree*);
void UnsubclassToc(MainWindow*);
void TocFilterChanged(MainWindow*);

// navigate to a TocItem (used by the command palette's TOC mode)
void GoToTocItem(MainWindow*, TocItem*);

// shared with Favorites.cpp
// void TocCustomizeTooltip(TreeItem::GetTooltipEvent*);
// LRESULT TocTreeKeyDown2(TreeKeyDownEvent*);
