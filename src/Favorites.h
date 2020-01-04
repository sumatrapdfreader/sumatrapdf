/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
class Favorites {
    size_t idxCache = (size_t)-1;

  public:
    Favorites() = default;

    Favorite* GetByMenuId(int menuId, DisplayState** dsOut = nullptr);
    void ResetMenuIds();
    DisplayState* GetFavByFilePath(const WCHAR* filePath);
    DisplayState* GetByFavorite(Favorite* fn);
    bool IsPageInFavorites(const WCHAR* filePath, int pageNo);
    void AddOrReplace(const WCHAR* filePath, int pageNo, const WCHAR* name, const WCHAR* pageLabel = nullptr);
    void Remove(const WCHAR* filePath, int pageNo);
    void RemoveAllForFile(const WCHAR* filePath);
};

bool HasFavorites();
void AddFavoriteWithLabelAndName(WindowInfo* win, int pageNo, const WCHAR* pageLabel, AutoFreeWstr& name);
void AddFavoriteForCurrentPage(WindowInfo* win, int pageNo);
void AddFavoriteForCurrentPage(WindowInfo* win);
void DelFavorite(const WCHAR* filePath, int pageNo);
void RebuildFavMenu(WindowInfo* win, HMENU menu);
void CreateFavorites(WindowInfo* win);
void ToggleFavorites(WindowInfo* win);
void PopulateFavTreeIfNeeded(WindowInfo* win);
void RememberFavTreeExpansionStateForAllWindows();
void GoToFavoriteByMenuId(WindowInfo* win, int wmId);
void UpdateFavoritesTree(WindowInfo* win);
void UpdateFavoritesTreeForAllWindows();
