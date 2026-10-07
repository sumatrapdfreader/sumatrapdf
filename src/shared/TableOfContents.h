/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MenuModel;
struct TreeView;

void ClearTocBox(MainWindow*);
void ToggleTocBox(MainWindow*);
void LoadTocTree(MainWindow*);
// rebuild the tree view after the controller replaced its TocTree
void ReloadTocTree(WindowTab*);
void UpdateTocSelection(MainWindow*, int currPageNo);
void ExpandTocToCurrentPage(MainWindow*);
void UpdateTocExpansionState(Vec<int>& tocState, MainWindow*, TocTree*);
void TocFilterChanged(MainWindow*);

// When true (default), the bookmarks pane highlights every TOC entry that
// matches the current page (same page number as the best match, plus the
// ancestor chain), not only the single tree selection (issue #4642).
// Flip to false to restore single-highlight-only behavior.
extern bool gShowAllMatchingTOC;

void GoToTocItem(MainWindow*, TocItem*);
// the tree the bookmarks pane shows: the filtered one if a filter is on
TocTree* CurrentTocTree(MainWindow*);
bool TocItemIsMultiHighlight(MainWindow*, TocItem*);
// clicking a row in the bookmarks pane (src/gui/Sidebar.cpp)
void TocTreeItemClicked(MainWindow*, TocItem*);
void TocTreeItemSelectedByKey(MainWindow*, TocItem*);
// the bookmarks pane's context menu, built and executed from the pane
MenuModel* BuildTocContextMenu(MainWindow*, TocItem*);
void TocContextMenuCommand(MainWindow*, TocItem*, int cmdId);
void TocExpandAll(MainWindow*);
void TocCollapseAll(MainWindow*);
void TocExpandToLevel(MainWindow*, int level);
void TocCollapseSameLevel(MainWindow*, TocItem*);

#if OS_WIN
void CreateToc(MainWindow*);
void RefreshTocTreeIfNeeded(MainWindow*);
void UpdateTocExpansionState(Vec<int>& tocState, TreeView*, TocTree*);
bool CanShowThumbnails(WindowTab*);
void UpdateSidebarThumbnails(MainWindow*);
void SidebarPagesChanged(MainWindow*);
void ClearSidebarThumbnails(MainWindow*);
void UpdateSidebarColors(MainWindow*);
bool ThumbnailsTakeKey(MainWindow*, HWND, WPARAM key);
#endif
