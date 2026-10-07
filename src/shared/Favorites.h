/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
A favorite is a bookmark (we call it a favorite, like Internet Explorer, to
differentiate from bookmarks inside a PDF file (which really are
table of contents)).

We can have multiple favorites per file.

A favorite is identified by a (mandatory) page number, an optional name
(provided by the user), an optional page label (from EngineBase::GetPageLabel),
and the scroll position on that page (so jumping back lands where you were,
not only at the top).

Favorites do not remember presentation settings like zoom or viewing mode -
they are for navigation only. Presentation settings are remembered on a
per-file basis in FileHistory.
*/

struct WindowTab;
struct DocController;
struct MainWindow;
struct MenuModel;
struct CustomCommand;

// one row of the favorites tree: a file node with its favorites under it, or a
// single favorite when the file has only one
struct FavTreeItem {
    ~FavTreeItem();

    uintptr_t userData = 0;
    FavTreeItem* parent = nullptr;
    Str text;
    bool isExpanded = false;
    int fileNameOffset = -1;
    int fileNameLen = 0;

    // not owned by us
    Favorite* favorite = nullptr;

    Vec<FavTreeItem*> children;
};

struct FavTreeModel : TreeModel {
    ~FavTreeModel() override;

    TreeItem Root() override;

    Str Text(TreeItem ti) override;
    TreeItem Parent(TreeItem ti) override;
    int ChildCount(TreeItem ti) override;
    TreeItem ChildAt(TreeItem ti, int idx) override;
    bool IsExpanded(TreeItem ti) override;
    void SetUserData(TreeItem ti, uintptr_t userData) override;
    uintptr_t GetUserData(TreeItem ti) override;

    FavTreeItem* root = nullptr;
};

bool HasFavorites();
void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, Str pageLabel, Str nameIn);
void ApplyAddFavorite(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name);
void AddFavoriteForPage(MainWindow* win, int pageNo);
void AddFavoriteForCurrentPage(MainWindow* win);
// ctrl, when it's the DocController for filePath, lets a chaptered doc's
// favorite be matched by (chapter, page) instead of the stale flat pageNo
void DelFavorite(Str filePath, int pageNo, DocController* ctrl = nullptr);
void DelFavorite(FileState* fs, Favorite* fav);
#if OS_WIN
void RebuildFavMenu(MainWindow* win, HMENU menu);
void CreateFavorites(MainWindow* win);
#endif
void RebuildFavMenu(MainWindow* win, MenuModel* menu);
void ToggleFavorites(MainWindow* win); // sidebar
// the full-window favorites list (CmdFavoriteShowInTab)
void ToggleFavoritesTab(MainWindow* win);
WindowTab* FindFavoritesTab(MainWindow* win);
void PopulateFavTreeIfNeeded(MainWindow* win);
void GoToFavoriteByCmd(MainWindow* win, CustomCommand* cmd);
void UpdateFavoritesTree(MainWindow* win);
void UpdateFavoritesTreeForAllWindows();
void RememberFavTreeExpansionState(MainWindow* win);
bool IsPageInFavorites(Str filePath, int pageNo, DocController* ctrl = nullptr);

TempStr FavReadableNameTemp(Favorite* fn);

// the favorites pane (src/gui/Sidebar.cpp) drives these
void GoToFavForTreeItem(MainWindow* win, FavTreeItem* fti);
MenuModel* BuildFavContextMenu(MainWindow* win, FavTreeItem* fti);
void FavContextMenuCommand(MainWindow* win, FavTreeItem* fti, int cmdId);
void FavFilterChanged(MainWindow* win);

void GoToNextFavorite(MainWindow* win, bool forward);
void GoToFavorite(MainWindow* win, FileState* fs, Favorite* fav);
void JumpToFavorite(MainWindow* win, Favorite* fav);

void SetSearchStartFavorite(MainWindow* win);

void ToggleSortFavoritesByName();
