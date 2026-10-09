/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the gpui side of orig's two sidebar child windows (hwndTocBox and
// hwndFavBox): the bookmarks pane over the favorites pane, each a header with
// a close button, a search box and a tree. The model lives in
// TableOfContents.cpp and Favorites.cpp; this only draws it and reports input.

namespace gpui {
struct Ctx;
struct El;
} // namespace gpui

struct MainWindow;
struct TocItem;
struct FavTreeModel;
struct FavTreeItem;
enum class SidebarContent;

SidebarContent SidebarContentFromStr(Str, SidebarContent fallback);
Str SidebarContentToStr(SidebarContent);
bool SidebarContentVisible(MainWindow*, SidebarContent);
void SidebarResolveContents(MainWindow*);

gpui::El* SidebarBuild(MainWindow*, gpui::Ctx*);
// the same favorites pane, filling the canvas (the Favorites tab)
gpui::El* SidebarBuildFavTab(MainWindow*, gpui::Ctx*);
void SidebarDelete(MainWindow*);
void SidebarRefreshThumbnailPage(MainWindow*, int pageNo);
TempStr SidebarThumbnailsResultTemp(int* exitCodeOut);

// bookmarks pane
void SidebarSetTocSelection(MainWindow*, TocItem*);
TocItem* SidebarTocSelection(MainWindow*);
// expand the item's ancestors and scroll it into view
void SidebarRevealTocItem(MainWindow*, TocItem*);
void SidebarSetTocFilterText(MainWindow*, Str);
TempStr SidebarTocFilterTextTemp(MainWindow*);
void SidebarSetTocRtl(MainWindow*, bool);

// favorites pane; the pane owns the model it is given
void SidebarSetFavModel(MainWindow*, FavTreeModel*);
FavTreeModel* SidebarFavModel(MainWindow*);
TempStr SidebarFavFilterTextTemp(MainWindow*);
void SidebarFocusFavorites(MainWindow*);
void SidebarFocusTop(MainWindow*);
void SidebarToggleThumbnails(MainWindow*);
void SidebarToggleBookmarks(MainWindow*);
void SidebarToggleFavorites(MainWindow*);
bool SidebarOnKeyDown(MainWindow*, int vkey, bool ctrl, bool shift, bool alt);
// a typed character while a tree has the keyboard: the native tree's type-ahead
bool SidebarOnChar(MainWindow*, u32 ch);
// orig's WM_CONTEXTMENU from the keyboard while a tree has the focus: the
// selected row's menu, under the row. False when no tree has the focus
bool SidebarContextMenuFromKey(MainWindow*, gpui::Ctx* cx);
// for -dbg-control's TestUiState
TempStr SidebarStateTemp(MainWindow*);
TempStr SidebarTestToc(MainWindow*, Str op, int arg);
// orig's sidebarTop / sidebarBottom panels: where the keyboard goes in one
// (its tree, or the panel for thumbnails) and whether it is there
bool SidebarPanelVisible(MainWindow*, bool top);
bool SidebarPanelHasFocus(MainWindow*, bool top);
void SidebarFocusPanel(MainWindow*, bool top);
