/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

void CreateToc(MainWindow*);
void ClearTocBox(MainWindow*);
void ToggleTocBox(MainWindow*);
void LoadTocTree(MainWindow*);
// rebuild the tree view after the controller replaced its TocTree
void ReloadTocTree(WindowTab*);
void UpdateTocSelection(MainWindow*, int currPageNo);
void ExpandTocToCurrentPage(MainWindow*);
void UpdateTocExpansionState(Vec<int>& tocState, TreeView*, TocTree*);
void UnsubclassToc(MainWindow*);
void TocFilterChanged(MainWindow*);

// When true (default), the bookmarks pane highlights every TOC entry that
// matches the current page (same page number as the best match, plus the
// ancestor chain), not only the single TreeView selection (issue #4642).
// Flip to false to restore single-highlight-only behavior.
extern bool gShowAllMatchingTOC;

void GoToTocItem(MainWindow*, TocItem*);

// shared with Favorites.cpp
// void TocCustomizeTooltip(TreeItem::GetTooltipEvent*);
// LRESULT TocTreeKeyDown2(TreeKeyDownEvent*);
