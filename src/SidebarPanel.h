/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class SidebarPanelKind {
    // per document: shown or not (WindowTab::showToc), and its view
    Top,
    // app-wide: ShowFavorites, SidebarBottomView
    Bottom,
    // the full-window Favorites tab: only favorites, no view icons
    FavoritesTab,
};

// A panel of the sidebar: a header with an icon per view and a ✕, above the
// view it shows. The views (bookmarks tree, page thumbnails, favorites tree)
// are the window's; each is in one panel at most, and moves between them.
//
//   +-----------------------+
//   | [B] [T] [F]        x  |  header: B selected
//   | [search bookmarks  ]  |
//   | tree ...              |  the view
//   +-----------------------+
struct SidebarPanel {
    MainWindow* win = nullptr;
    SidebarPanelKind kind = SidebarPanelKind::Top;
    SidebarView view = SidebarView::Bookmarks;
    HWND hwnd = nullptr;
    UINT_PTR subclassId = 0;
    // VBox(header, view); the view's layout isn't owned, it's the window's
    VBox* layout = nullptr;
    VirtRoot* root = nullptr;
    VirtIconButton* viewBtns[kSidebarViewCount]{};
    VirtCloseButton* closeBtn = nullptr;
    // stands in for the view while the panel shows none
    ILayout* noView = nullptr;
    // the view's layout now in the panel (layout's second child)
    ILayout* hosted = nullptr;
};

SidebarPanel* CreateSidebarPanel(MainWindow*, SidebarPanelKind);
void DeleteSidebarPanel(SidebarPanel*);
void LayoutSidebarPanel(SidebarPanel*);
void RelayoutSidebarPanel(SidebarPanel*);
bool IsSidebarViewAvailable(MainWindow*, SidebarView);
SidebarPanel* SidebarPanelShowing(MainWindow*, SidebarView);
bool IsSidebarViewShown(MainWindow*, SidebarView);
void ResolveSidebarViews(MainWindow*);
void AttachSidebarViews(MainWindow*);
void ApplySidebarPanels(MainWindow*);
void ShowSidebarView(MainWindow*, SidebarView);
void HideSidebarView(MainWindow*, SidebarView);
void FocusSidebarPanel(SidebarPanel*);
HWND SidebarPanelFocusHwnd(SidebarPanel*);
bool SidebarPanelHasFocus(SidebarPanel*);
void UpdateSidebarPanelsText(MainWindow*);
void UpdateSidebarPanelsDpi(MainWindow*, int dpi);
void UpdateSidebarPanelsIcons(MainWindow*);
