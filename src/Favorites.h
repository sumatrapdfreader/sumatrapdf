/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/*
A favorite is a bookmark (we call it a favorite, like Internet Explorer, to
differentiate from bookmarks inside a PDF file (which really are
table of contents)).

We can have multiple favorites per file.

Favorites are accurate to a page - it's simple and should be good enough
for the user.

A favorite is identified by a (mandatory) page number and (optional) name
(provided by the user) and page label (from EngineBase::GetPageLabel).

Favorites do not remember presentation settings like zoom or viewing mode -
they are for navigation only. Presentation settings are remembered on a
per-file basis in FileHistory.
*/

// Favorites is a convenience interface into gFileHistory
struct Favorites {
    size_t idxCache = (size_t)-1;

    Favorites() = default;

    Favorite* GetByMenuId(int menuId, FileState** dsOut = nullptr);
    void ResetMenuIds();
    FileState* GetFavByFilePath(const char* filePath);
    FileState* GetByFavorite(Favorite* fn);
    bool IsPageInFavorites(const char* filePath, int pageNo);
    void AddOrReplace(const char* filePath, int pageNo, const char* name, const char* pageLabel = nullptr);
    void Remove(const char* filePath, int pageNo);
    void RemoveAllForFile(const char* filePath);
};

bool HasFavorites();
void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, const char* pageLabel, const char* nameIn);
void AddFavoriteForCurrentPage(MainWindow* win, int pageNo);
void AddFavoriteForCurrentPage(MainWindow* win);
void DelFavorite(const char* filePath, int pageNo);
void RebuildFavMenu(MainWindow* win, HMENU menu);
void CreateFavorites(MainWindow* win);
void ToggleFavorites(MainWindow* win);
void PopulateFavTreeIfNeeded(MainWindow* win);
void RememberFavTreeExpansionStateForAllWindows();
void GoToFavoriteByMenuId(MainWindow* win, int wmId);
void UpdateFavoritesTree(MainWindow* win);
void UpdateFavoritesTreeForAllWindows();
