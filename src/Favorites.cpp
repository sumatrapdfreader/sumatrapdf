/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "AppSettings.h"
#include "Menu.h"
#include "AddFavoriteDialog.h"
#include "Translations.h"
#include "Accelerators.h"
#include "Tabs.h"
#include "Theme.h"
#include "FilterHighlightDraw.h"

static void RememberFavTreeExpansionStateForAllWindows();
void LayoutFavoritesContainer(MainWindow* win);
void PopulateFavTreeIfNeeded(MainWindow* win);
void UpdateFavoritesTreeForAllWindows();

struct FavTreeItem {
    ~FavTreeItem();

    HTREEITEM hItem = nullptr;
    FavTreeItem* parent = nullptr;
    Str text;
    bool isExpanded = false;

    // not owned by us
    Favorite* favorite = nullptr;

    Vec<FavTreeItem*> children;
};

FavTreeItem::~FavTreeItem() {
    str::Free(text);
    DeleteVecMembers(children);
}

struct FavTreeModel : TreeModel {
    ~FavTreeModel() override;

    TreeItem Root() override;

    Str Text(TreeItem ti) override;
    TreeItem Parent(TreeItem ti) override;
    int ChildCount(TreeItem ti) override;
    TreeItem ChildAt(TreeItem ti, int idx) override;
    bool IsExpanded(TreeItem ti) override;
    bool IsChecked(TreeItem ti) override;
    void SetHandle(TreeItem ti, HTREEITEM hItem) override;
    HTREEITEM GetHandle(TreeItem ti) override;

    FavTreeItem* root = nullptr;
};

FavTreeModel::~FavTreeModel() {
    delete root;
}

TreeItem FavTreeModel::Root() {
    return (TreeItem)root;
}

Str FavTreeModel::Text(TreeItem ti) {
    auto* fti = (FavTreeItem*)ti;
    return fti->text;
}

TreeItem FavTreeModel::Parent(TreeItem ti) {
    auto* fti = (FavTreeItem*)ti;
    return (TreeItem)fti->parent;
}

int FavTreeModel::ChildCount(TreeItem ti) {
    auto* fti = (FavTreeItem*)ti;
    if (!fti) {
        return 0;
    }
    int n = len(fti->children);
    return n;
}

TreeItem FavTreeModel::ChildAt(TreeItem ti, int idx) {
    auto* fti = (FavTreeItem*)ti;
    auto* res = fti->children[idx];
    return (TreeItem)res;
}

bool FavTreeModel::IsExpanded(TreeItem ti) {
    auto* fti = (FavTreeItem*)ti;
    return fti->isExpanded;
}

bool FavTreeModel::IsChecked(TreeItem /*ti*/) {
    return false;
}

void FavTreeModel::SetHandle(TreeItem ti, HTREEITEM hItem) {
    ReportIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    treeItem->hItem = hItem;
}

HTREEITEM FavTreeModel::GetHandle(TreeItem ti) {
    ReportIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    return treeItem->hItem;
}

static Favorite* GetFavByMenuId(int menuId, FileState** dsOut) {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        for (int j = 0; j < len(*ds->favorites); j++) {
            if (menuId == (*ds->favorites)[j]->menuId) {
                if (dsOut) {
                    *dsOut = ds;
                }
                return (*ds->favorites)[j];
            }
        }
    }
    return nullptr;
}

static FileState* GetByFavorite(Favorite* fn) {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        if (ds->favorites->Contains(fn)) {
            return ds;
        }
    }
    return nullptr;
}

static void ResetFavMenuIds() {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        for (int j = 0; j < len(*ds->favorites); j++) {
            (*ds->favorites)[j]->menuId = 0;
        }
    }
}

static int idxCache = -1;

static FileState* GetFavByFilePath(Str filePath) {
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    FileState* fs = FileHistoryGet(idxCache);
    if (fs && str::Eq(fs->filePath, filePath)) {
        return fs;
    }
    // Full paths only: FindByPath avoids basename collisions (two files named
    // the same in different folders must not share favorites).
    fs = FileHistoryFindByPath(filePath);
    idxCache = -1;
    if (fs && FileHistoryStates()) {
        int n = len(*FileHistoryStates());
        for (int i = 0; i < n; i++) {
            if ((*FileHistoryStates())[i] == fs) {
                idxCache = i;
                break;
            }
        }
    }
    return fs;
}

bool IsPageInFavorites(Str filePath, int pageNo) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return false;
    }
    for (int i = 0; i < len(*fav->favorites); i++) {
        if (pageNo == (*fav->favorites)[i]->pageNo) {
            return true;
        }
    }
    return false;
}

// navigate to the nearest favorite (bookmark) page after / before the current
// page in the open document (issue #3744)
void GoToNextFavorite(MainWindow* win, bool forward) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    FileState* fs = GetFavByFilePath(win->ctrl->GetFilePath());
    if (!fs || len(*fs->favorites) == 0) {
        return;
    }
    int cur = win->currPageNo;
    // pick the favorite page closest to the current page in the requested
    // direction (no wrap-around)
    int best = -1;
    for (int i = 0; i < len(*fs->favorites); i++) {
        int p = (*fs->favorites)[i]->pageNo;
        if (forward) {
            if (p > cur && (best == -1 || p < best)) {
                best = p;
            }
        } else {
            if (p < cur && (best == -1 || p > best)) {
                best = p;
            }
        }
    }
    if (best != -1 && win->ctrl->ValidPageNo(best)) {
        win->ctrl->GoToPage(best, true);
        win->Focus();
    }
}

static Favorite* FindByPage(FileState* ds, int pageNo, Str pageLabel = {}) {
    if (!ds || !ds->favorites) {
        return nullptr;
    }
    auto* favs = ds->favorites;
    int n = len(*favs);
    if (pageLabel) {
        for (int i = 0; i < n; i++) {
            auto* fav = (*favs)[i];
            if (str::Eq(fav->pageLabel, pageLabel)) {
                return fav;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        auto* fav = (*favs)[i];
        if (pageNo == fav->pageNo) {
            return fav;
        }
    }
    return nullptr;
}

static int SortByPageNo(Favorite* const* a, Favorite* const* b) {
    Favorite* na = *a;
    Favorite* nb = *b;
    // sort lower page numbers first
    return na->pageNo - nb->pageNo;
}

// Sort by user name if set, else page label; page number breaks ties and is
// the only key when neither favorite has a name/label (issue #2277).
static int SortByName(Favorite* const* a, Favorite* const* b) {
    Favorite* na = *a;
    Favorite* nb = *b;
    Str sa = na->name;
    if (!sa) {
        sa = na->pageLabel;
    }
    Str sb = nb->name;
    if (!sb) {
        sb = nb->pageLabel;
    }
    if (sa || sb) {
        if (!sa) {
            return 1;
        }
        if (!sb) {
            return -1;
        }
        int n = str::CmpNatural(sa, sb);
        if (n != 0) {
            return n;
        }
    }
    return na->pageNo - nb->pageNo;
}

static void SortFileFavorites(FileState* fs) {
    if (!fs || !fs->favorites || len(*fs->favorites) < 2) {
        return;
    }
    if (gGlobalPrefs->sortFavoritesByName) {
        VecSort(*fs->favorites, SortByName);
    } else {
        VecSort(*fs->favorites, SortByPageNo);
    }
}

static void SortAllFavorites() {
    FileState* fs;
    for (int i = 0; (fs = FileHistoryGet(i)) != nullptr; i++) {
        SortFileFavorites(fs);
    }
}

// toggle SortFavoritesByName, re-sort, refresh trees, and save settings
void ToggleSortFavoritesByName() {
    gGlobalPrefs->sortFavoritesByName = !gGlobalPrefs->sortFavoritesByName;
    SortAllFavorites();
    RememberFavTreeExpansionStateForAllWindows();
    UpdateFavoritesTreeForAllWindows();
    SaveSettings();
}

static void AddOrReplaceFav(Str filePath, int pageNo, Str name, Str pageLabel) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        // we were asked to add a favorite for current file but couldn't find
        // history for this file
        fav = NewFileState(filePath);
        FileHistoryAppend(fav);
    }

    Favorite* fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        str::ReplaceWithCopy(&fn->name, name);
        ReportIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
        SortFileFavorites(fav);
    } else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        SortFileFavorites(fav);
    }
}

// Name used for the Sioyek-style "search start" mark (issue #5726).
static Str SearchStartFavName() {
    return StrL("/");
}

static Favorite* FindByName(FileState* ds, Str name) {
    if (!ds || !ds->favorites || !name) {
        return nullptr;
    }
    for (Favorite* fav : *ds->favorites) {
        if (str::Eq(fav->name, name)) {
            return fav;
        }
    }
    return nullptr;
}

// Silently set/update favorite "/" to the current page so the user can jump
// back after searching (command palette $ Favorites, or the Favorites sidebar).
// Session-only: isTemporary is true so SerializeStruct skips the entry when
// writing settings (issue #5862).
void SetSearchStartFavorite(MainWindow* win) {
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->filePath) {
        return;
    }
    int pageNo = win->currPageNo;
    if (pageNo < 1) {
        pageNo = win->ctrl->CurrentPageNo();
    }
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }

    Str path = tab->filePath;
    TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
    TempStr plainLabel = fmt("%d", pageNo);
    bool needsLabel = pageLabel && !str::Eq(plainLabel, pageLabel);
    Str pl = needsLabel ? pageLabel : Str{};

    FileState* fs = GetFavByFilePath(path);
    if (!fs) {
        fs = NewFileState(path);
        FileHistoryAppend(fs);
    }

    Str markName = SearchStartFavName();
    Favorite* fn = FindByName(fs, markName);
    if (fn) {
        if (fn->isTemporary && fn->pageNo == pageNo && str::Eq(fn->pageLabel, pl)) {
            return; // already marks this page
        }
        fn->pageNo = pageNo;
        str::ReplaceWithCopy(&fn->pageLabel, pl);
        // mark as session-only even if a prior build persisted a "/" entry
        fn->isTemporary = true;
        SortFileFavorites(fs);
    } else {
        fn = NewFavorite(pageNo, markName, pl);
        fn->isTemporary = true;
        fs->favorites->Append(fn);
        SortFileFavorites(fs);
    }
    UpdateFavoritesTreeForAllWindows();
}

static void RemoveFav(Str filePath, int pageNo) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return;
    }
    Favorite* fn = FindByPage(fav, pageNo);
    if (!fn) {
        return;
    }

    fav->favorites->Remove(fn);
    DeleteFavorite(fn);

    if (!SettingsRememberOpenedFiles() && 0 == len(*fav->favorites)) {
        FileHistoryRemove(fav);
        DeleteFileState(fav);
    }
}

static void RemoveAllFavForFile(Str filePath) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return;
    }

    for (int i = 0; i < len(*fav->favorites); i++) {
        DeleteFavorite((*fav->favorites)[i]);
    }
    fav->favorites->Reset();

    if (!SettingsRememberOpenedFiles()) {
        FileHistoryRemove(fav);
        DeleteFileState(fav);
    }
}

// Note: those might be too big
#define MAX_FAV_SUBMENUS 10
#define MAX_FAV_MENUS 10

bool HasFavorites() {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        if (len(*ds->favorites) > 0) {
            return true;
        }
    }
    return false;
}

// caller has to free() the result
// shared with CommandPalette.cpp (favorites mode)
TempStr FavReadableNameTemp(Favorite* fn) {
    Str label = fn->pageLabel;
    if (!label) {
        label = fmt("%d", fn->pageNo);
    }
    if (fn->name) {
        TempStr pageNo = fmt(_TRA("(page %s)").s, label);
        return str::JoinTemp(fn->name, StrL(" "), pageNo);
    }
    return fmt(_TRA("Page %s").s, label);
}

// caller has to free() the result
static TempStr FavCompactReadableNameTemp(FileState* fav, Favorite* fn, bool isCurrent = false) {
    TempStr rn = FavReadableNameTemp(fn);
    if (isCurrent) {
        return fmt("%s : %s", _TRA("Current file"), rn);
    }
    TempStr fp = path::GetBaseNameTemp(fav->filePath);
    // show the favorite's name first, then the file name, so that a long file
    // name doesn't push the user's description out of view in the favorites
    // pane / menu (fixes #829, #2236)
    return fmt("%s : %s", rn, fp);
}

static void AppendFavMenuItems(HMENU m, FileState* f, int& idx, bool combined, bool isCurrent) {
    ReportIf(!f);
    if (!f) {
        return;
    }
    for (int i = 0; i < len(*f->favorites); i++) {
        if (i >= MAX_FAV_MENUS) {
            return;
        }
        Favorite* fn = (*f->favorites)[i];
        fn->menuId = idx++;
        TempStr s;
        if (combined) {
            s = FavCompactReadableNameTemp(f, fn, isCurrent);
        } else {
            s = FavReadableNameTemp(fn);
        }
        auto safeStr = MenuToSafeStringTemp(s);
        WCHAR* ws = CWStrTemp(safeStr);
        AppendMenuW(m, MF_STRING, (UINT_PTR)fn->menuId, ws);
    }
}

static bool SortByBaseFileName(Str s1, Str s2) {
    if (len(s1) == 0) {
        if (len(s2) == 0) {
            return false;
        }
        return true;
    }
    if (len(s2) == 0) {
        return false;
    }
    TempStr base1 = path::GetBaseNameTemp(s1);
    TempStr base2 = path::GetBaseNameTemp(s2);
    int n = str::CmpNatural(base1, base2);
    return n < 0;
}

static void GetSortedFilePaths(StrVec& filePathsSortedOut, FileState* toIgnore = nullptr) {
    FileState* fs;
    for (int i = 0; (fs = FileHistoryGet(i)) != nullptr; i++) {
        if (len(*fs->favorites) > 0 && fs != toIgnore) {
            filePathsSortedOut.Append(fs->filePath);
        }
    }
    Sort(&filePathsSortedOut, SortByBaseFileName);
}

// For easy access, we try to show favorites in the menu, similar to a list of
// recently opened files.
// The first menu items are for currently opened file (up to MAX_FAV_MENUS), based
// on the assumption that user is usually interested in navigating current file.
// Then we have a submenu for each file for which there are bookmarks (up to
// MAX_FAV_SUBMENUS), each having up to MAX_FAV_MENUS menu items.
// If not all favorites can be shown, we also enable "Show all favorites" menu which
// will provide a way to see all favorites.
// Note: not sure if that's the best layout. Maybe we should always use submenu and
// put the submenu for current file as the first one (potentially named as "Current file"
// or some such, to make it stand out from other submenus)
static void AppendFavMenus(HMENU m, Str currFilePath) {
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    FileState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    StrVec filePathsSorted;
    if (CanAccessDisk()) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && len(*currFileFav->favorites) > 0) {
        filePathsSorted.InsertAt(0, currFileFav->filePath);
    }

    if (len(filePathsSorted) == 0) {
        return;
    }

    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    ResetFavMenuIds();
    int menuId = CmdFavoriteFirst;

    int menusCount = len(filePathsSorted);
    menusCount = std::min(menusCount, MAX_FAV_MENUS);

    for (int i = 0; i < menusCount; i++) {
        Str filePath = filePathsSorted[i];
        FileState* f = GetFavByFilePath(filePath);
        ReportIf(!f);
        if (!f) {
            continue;
        }
        HMENU sub = m;
        bool combined = (len(*f->favorites) == 1);
        if (!combined) {
            sub = CreateMenu();
        }
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            Str s = _TRA("Current file");
            if (f != currFileFav) {
                s = MenuToSafeStringTemp(path::GetBaseNameTemp(filePath));
            }
            AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, CWStrTemp(s));
        }
    }
}

// Called when a user opens "Favorites" top-level menu. We need to construct
// the menu:
// - disable add/remove menu items if no document is opened
// - if a document is opened and the page is already bookmarked,
//   disable "add" menu item and enable "remove" menu item
// - if a document is opened and the page is not bookmarked,
//   enable "add" menu item and disable "remove" menu item
void RebuildFavMenu(MainWindow* win, HMENU menu) {
    if (!win->IsDocLoaded()) {
        MenuSetEnabled(menu, CmdFavoriteAdd, false);
        MenuSetEnabled(menu, CmdFavoriteDel, false);
        AppendFavMenus(menu, {});
    } else {
        TempStr label = win->ctrl->GetPageLabeTemp(win->currPageNo);
        bool isBookmarked = IsPageInFavorites(win->ctrl->GetFilePath(), win->currPageNo);
        if (isBookmarked) {
            MenuSetEnabled(menu, CmdFavoriteAdd, false);
            TempStr s = fmt(_TRA("Remove page %s from favorites").s, label);
            MenuSetText(menu, CmdFavoriteDel, s);
        } else {
            MenuSetEnabled(menu, CmdFavoriteDel, false);
            TempStr s = fmt(_TRA("Add page %s to favorites").s, label);
            s = AppendAccelKeyToMenuStringTemp(s, CmdFavoriteAdd);
            MenuSetText(menu, CmdFavoriteAdd, s);
        }
        AppendFavMenus(menu, win->ctrl->GetFilePath());
    }
    MenuSetEnabled(menu, CmdFavoriteToggle, HasFavorites());
}

// find the Favorites tab in this window, or nullptr
WindowTab* FindFavoritesTab(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsFavoritesTab()) {
            return tab;
        }
    }
    return nullptr;
}

static void GoToFavoritePage(MainWindow* win, int pageNo) {
    if (!IsMainWindowValid(win)) {
        return;
    }
    if (win->IsDocLoaded() && win->ctrl->ValidPageNo(pageNo)) {
        win->ctrl->GoToPage(pageNo, true);
    }
    // we might have been invoked by clicking on a tree view
    // switch focus so that keyboard navigation works, which enables
    // a fluid experience
    win->Focus();
}

struct GoToFavoritePageData {
    MainWindow* win;
    int pageNo;
};

static void GoToFavoritePage(GoToFavoritePageData* d) {
    GoToFavoritePage(d->win, d->pageNo);
    delete d;
}

// Going to a bookmark within current file scrolls to a given page.
// Going to a bookmark in another file, loads the file and scrolls to a page
// (similar to how invoking one of the recently opened files works)
void GoToFavorite(MainWindow* win, FileState* fs, Favorite* fav) {
    ReportIf(!fs || !fav);
    if (!fs || !fav) {
        return;
    }

    Str fp = fs->filePath;
    MainWindow* existingWin = FindMainWindowByFile(fp, true);
    if (existingWin) {
        auto* data = new GoToFavoritePageData;
        data->pageNo = fav->pageNo;
        data->win = existingWin;
        auto fn = MkFunc0<GoToFavoritePageData>(GoToFavoritePage, data);
        uitask::Post(fn, "TaskGoToFavorite");
        return;
    }

    if (!CanAccessDisk()) {
        return;
    }

    // When loading a new document, go directly to selected page instead of
    // first showing last seen page stored in file history
    // A hacky solution because I don't want to add even more parameters to
    // LoadDocument() and LoadDocumentInto()
    int pageNo = fav->pageNo;
    FileState* ds = FileHistoryFindByPath(fs->filePath);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fav->pageNo;
        ds->scrollPos = PointF(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    LoadArgs args(fs->filePath, win);
    win = LoadDocument(&args);
    if (win) {
        auto* data = new GoToFavoritePageData;
        data->pageNo = pageNo;
        data->win = win;
        auto fn = MkFunc0<GoToFavoritePageData>(GoToFavoritePage, data);
        uitask::Post(fn, "TaskGoToFavorite2");
    }
}

void GoToFavoriteByMenuId(MainWindow* win, int cmdId) {
    FileState* f;
    Favorite* fn = GetFavByMenuId(cmdId, &f);
    if (fn) {
        GoToFavorite(win, f, fn);
    }
}

static void GoToFavForTreeItem(MainWindow* win, TreeItem ti) {
    if (!ti) {
        return;
    }

    FavTreeItem* fti = (FavTreeItem*)ti;
    Favorite* fn = fti->favorite;
    if (!fn) {
        // can happen for top-level node which is not associated with a favorite
        // but only serves a parent node for favorites for a given file
        return;
    }
    FileState* f = GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

#if 0
static void GoToFavForTVItem(MainWindow* win, TreeCtrl* treeView, HTREEITEM hItem = nullptr) {
    TreeItem ti = nullptr;
    if (nullptr == hItem) {
        ti = treeView->GetSelection();
    } else {
        ti = treeView->GetTreeItemByHandle(hItem);
    }
    GoToFavForTreeItem(win, ti);
}
#endif

static FavTreeItem* MakeFavTopLevelItem(FileState* fs, bool isExpanded) {
    if (!fs->favorites || len(*fs->favorites) == 0) {
        return nullptr;
    }
    auto* res = new FavTreeItem();
    Favorite* fn = (*fs->favorites)[0];
    res->favorite = fn;

    bool isCollapsed = len(*fs->favorites) == 1;
    if (isCollapsed) {
        isExpanded = false;
    }
    res->isExpanded = isExpanded;

    TempStr text = nullptr;
    if (isCollapsed) {
        text = FavCompactReadableNameTemp(fs, fn);
    } else {
        text = path::GetBaseNameTemp(fs->filePath);
    }
    res->text = str::Dup(text);
    return res;
}

// true if every filter word appears in this favorite's searchable text
// (file base name + readable label / optional user name — same idea as palette)
static bool FavMatchesFilter(FileState* fs, Favorite* fn, const StrVec& words) {
    if (len(words) == 0) {
        return true;
    }
    TempStr baseName = path::GetBaseNameTemp(fs->filePath);
    TempStr rn = FavReadableNameTemp(fn);
    TempStr compact = FavCompactReadableNameTemp(fs, fn);
    TempStr hay = fmt("%s : %s", baseName, rn);
    if (FilterMatches(compact, words) || FilterMatches(hay, words) || FilterMatches(rn, words) ||
        FilterMatches(baseName, words) || (fn->name && FilterMatches(fn->name, words))) {
        return true;
    }
    return false;
}

// filter empty => full tree; multi-word (command palette style): every word must
// match. Only rows that match are shown — no "include all children of this file".
static FavTreeModel* BuildFavTreeModel(MainWindow* win, Str filter) {
    StrVec words;
    if (filter) {
        SplitFilterToWords(filter, words);
    }
    bool filtering = len(words) > 0;
    auto* res = new FavTreeModel();
    res->root = new FavTreeItem();
    StrVec filePathsSorted;
    GetSortedFilePaths(filePathsSorted);
    for (int i = 0; i < len(filePathsSorted); i++) {
        Str path = filePathsSorted[i];
        FileState* fs = GetFavByFilePath(path);
        ReportIf(!fs);
        if (!fs || !fs->favorites || len(*fs->favorites) == 0) {
            continue;
        }
        // keep in-file order aligned with SortFavoritesByName (issue #2277)
        SortFileFavorites(fs);
        TempStr baseName = path::GetBaseNameTemp(fs->filePath);
        int nFavs = len(*fs->favorites);

        if (nFavs == 1) {
            Favorite* fn = (*fs->favorites)[0];
            if (filtering && !FavMatchesFilter(fs, fn, words)) {
                continue;
            }
            FavTreeItem* ti = MakeFavTopLevelItem(fs, false);
            if (ti) {
                res->root->children.Append(ti);
            }
            continue;
        }

        // multi-favorite file
        if (!filtering) {
            auto* parent = new FavTreeItem();
            parent->favorite = (*fs->favorites)[0];
            parent->text = str::Dup(baseName);
            parent->isExpanded = win->expandedFavorites.Contains(fs);
            for (int j = 0; j < nFavs; j++) {
                Favorite* fn = (*fs->favorites)[j];
                auto* ti = new FavTreeItem();
                ti->text = str::Dup(FavReadableNameTemp(fn));
                ti->parent = parent;
                ti->favorite = fn;
                parent->children.Append(ti);
            }
            res->root->children.Append(parent);
            continue;
        }

        // filtering: only favorites that match every word, as top-level compact
        // rows so each visible label itself contains all matched terms (nesting
        // under a file parent would show child labels that often lack them).
        for (int j = 0; j < nFavs; j++) {
            Favorite* fn = (*fs->favorites)[j];
            if (!FavMatchesFilter(fs, fn, words)) {
                continue;
            }
            auto* ti = new FavTreeItem();
            ti->favorite = fn;
            ti->text = str::Dup(FavCompactReadableNameTemp(fs, fn));
            ti->isExpanded = false;
            res->root->children.Append(ti);
        }
    }
    return res;
}

static TempStr GetFavFilterTemp(MainWindow* win) {
    if (!win || !win->favFilterEdit) {
        return {};
    }
    return win->favFilterEdit->GetTextTemp();
}

static bool IsFavoritesTabActive(MainWindow* win) {
    return win && win->CurrentTab() && win->CurrentTab()->IsFavoritesTab();
}

// Expand every branch (used when the full-window Favorites tab is shown).
static void ExpandAllFavTree(MainWindow* win) {
    if (win && win->favTreeView && win->favTreeView->hwnd) {
        win->favTreeView->ExpandAll();
    }
}

static void FocusFavFilterEdit(MainWindow* win) {
    if (!win || !win->favFilterEdit || !win->favFilterEdit->hwnd) {
        return;
    }
    HwndSetFocus(win->favFilterEdit->hwnd);
    win->favFilterEdit->SetCursorPositionAtEnd();
}

// Select first top-level item's first child when it has children; otherwise the
// first top-level item (Down from the search box).
static void SelectFirstFavTreeItem(MainWindow* win) {
    TreeView* tv = win ? win->favTreeView : nullptr;
    if (!tv || !tv->treeModel || !tv->hwnd) {
        return;
    }
    TreeModel* tm = tv->treeModel;
    TreeItem root = tm->Root();
    if (tm->ChildCount(root) == 0) {
        return;
    }
    TreeItem first = tm->ChildAt(root, 0);
    TreeItem sel = first;
    if (tm->ChildCount(first) > 0) {
        // ensure the first child is visible
        HTREEITEM hFirst = tv->GetHandleByTreeItem(first);
        if (hFirst) {
            TreeView_Expand(tv->hwnd, hFirst, TVE_EXPAND);
        }
        sel = tm->ChildAt(first, 0);
    }
    tv->SelectItem(sel);
    TreeView_EnsureVisible(tv->hwnd, tv->GetHandleByTreeItem(sel));
}

static void ApplyFavFilter(MainWindow* win) {
    if (!win || !win->favTreeView) {
        return;
    }
    TreeView* treeView = win->favTreeView;
    auto* prevModel = treeView->treeModel;
    TreeModel* newModel = BuildFavTreeModel(win, GetFavFilterTemp(win));
    treeView->SetTreeModel(newModel);
    delete prevModel;
    if (IsFavoritesTabActive(win)) {
        ExpandAllFavTree(win);
    }
}

static void OnFavFilterTextChanged(MainWindow* win) {
    ApplyFavFilter(win);
}

// Favorites-tab chrome: expand all, layout, focus the search box.
static void PrepareFavoritesTabUi(MainWindow* win) {
    if (!win) {
        return;
    }
    PopulateFavTreeIfNeeded(win);
    ExpandAllFavTree(win);
    LayoutFavoritesContainer(win);
    FocusFavFilterEdit(win);
    if (win->favTreeView) {
        RedrawWindow(win->favTreeView->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
}

static LRESULT CALLBACK WndProcFavFilterEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                             DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN) {
        if (wp == VK_DOWN) {
            // move into the tree: first child of the first file node (or first row)
            if (win && win->favTreeView) {
                SelectFirstFavTreeItem(win);
                HwndSetFocus(win->favTreeView->hwnd);
            }
            return 0;
        }
        if (wp == VK_ESCAPE) {
            Edit* edit = win ? win->favFilterEdit : nullptr;
            if (edit) {
                TempStr txt = edit->GetTextTemp();
                if (txt && len(txt) > 0) {
                    edit->SetText("");
                    // onTextChanged restores the full tree
                    return 0;
                }
                // empty: stay in the edit (Favorites tab) or fall through to tree in sidebar
                if (IsFavoritesTabActive(win)) {
                    return 0;
                }
                if (win->favTreeView) {
                    SetFocus(win->favTreeView->hwnd);
                }
                return 0;
            }
        }
        if (wp == VK_RETURN) {
            // prevent ding; navigation is done from the tree
            return 0;
        }
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == '\r' || wp == '\n')) {
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void PopulateFavTreeIfNeeded(MainWindow* win) {
    TreeView* treeView = win->favTreeView;
    if (treeView->treeModel) {
        return;
    }
    TreeModel* tm = BuildFavTreeModel(win, GetFavFilterTemp(win));
    treeView->SetTreeModel(tm);
}

void ToggleFavorites(MainWindow* win) {
    // Sidebar Favorites panel (independent of the Favorites tab)
    if (gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(win, win->uiState.tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->uiState.tocVisible, true);
        HwndSetFocus(win->favTreeView->hwnd);
    }
}

// open/select full-window Favorites tab (can use with sidebar Favorites)
void ToggleFavoritesTab(MainWindow* win) {
    // Full-window Favorites tab (independent of the sidebar Favorites panel).
    // Always switches to / creates the tab (close with the tab's ✕).
    // Requires tabs; falls back to sidebar toggle when tabs are off.
    if (!SettingsUseTabs()) {
        ToggleFavorites(win);
        return;
    }
    WindowTab* favTab = FindFavoritesTab(win);
    if (favTab) {
        int idx = win->GetTabIdx(favTab);
        if (idx >= 0 && win->CurrentTab() != favTab) {
            TabsSelect(win, idx); // LoadModelIntoTab does layout + focus
        } else {
            // already on Favorites: re-layout and focus search
            PrepareFavoritesTabUi(win);
        }
        return;
    }

    // Save the document tab first: AddTabToWindow selects via TabCtrl_SetCurSel,
    // which does not send TCN_SELCHANGE, so we must LoadModelIntoTab ourselves.
    SaveCurrentWindowTab(win);
    auto* tab = new WindowTab(win);
    tab->type = WindowTab::Type::Favorites;
    AddTabToWindow(win, tab);
    LoadModelIntoTab(tab);
}

void UpdateFavoritesTree(MainWindow* win) {
    TreeView* treeView = win->favTreeView;
    // rebuild (honors current search filter if any)
    ApplyFavFilter(win);
    TreeModel* newModel = treeView->treeModel;

    // hide favorites UI if we've removed the last favorite
    bool hasAny = false;
    if (newModel) {
        hasAny = newModel->ChildCount(newModel->Root()) > 0;
    }
    if (!hasAny) {
        if (WindowTab* favTab = FindFavoritesTab(win)) {
            CloseTab(favTab, false);
        }
        if (gGlobalPrefs->showFavorites) {
            SetSidebarVisibility(win, win->uiState.tocVisible, false);
        } else {
            ScheduleUiUpdate(win, kUiForceRelayout | kUiSidebarDirty);
        }
        return;
    }
    // refresh sidebar visibility only when the sidebar panel is supposed to be open
    if (gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(win, win->uiState.tocVisible, true);
    } else if (FindFavoritesTab(win)) {
        ScheduleUiUpdate(win, kUiForceRelayout | kUiSidebarDirty);
    }
}

void UpdateFavoritesTreeForAllWindows() {
    for (MainWindow* win : gWindows) {
        UpdateFavoritesTree(win);
    }
}

static TocItem* TocItemForPageNo(TocItem* item, int pageNo) {
    TocItem* currItem = nullptr;

    for (; item; item = item->next) {
        if (1 <= item->pageNo && item->pageNo <= pageNo) {
            currItem = item;
        }
        if (item->pageNo >= pageNo) {
            break;
        }

        // find any child item closer to the specified page
        TocItem* subItem = TocItemForPageNo(item->child, pageNo);
        if (subItem) {
            currItem = subItem;
        }
    }

    return currItem;
}

// Persist a favorite after the Add Favorite dialog's OK (name may be empty).
void ApplyAddFavorite(MainWindow* win, Str filePath, int pageNo, Str pageLabel, Str name) {
    if (!filePath || !IsMainWindowValid(win)) {
        return;
    }
    TempStr plainLabel = fmt("%d", pageNo);
    bool needsLabel = !str::Eq(plainLabel, pageLabel);

    RememberFavTreeExpansionStateForAllWindows();
    Str pl = needsLabel ? pageLabel : Str{};
    AddOrReplaceFav(filePath, pageNo, name, pl);
    // expand newly added favorites by default
    FileState* fav = GetFavByFilePath(filePath);
    if (fav && len(*fav->favorites) == 2) {
        win->expandedFavorites.Append(fav);
    }
    UpdateFavoritesTreeForAllWindows();
    SaveSettings();
}

void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, Str pageLabel, Str nameIn) {
    if (!IsMainWindowValid(win) || !win->CurrentTab()) {
        return;
    }
    ShowAddFavoriteDialog(win, win->CurrentTab()->filePath, pageNo, pageLabel, nameIn);
}

void AddFavoriteForPage(MainWindow* win, int pageNo) {
    Str name;
    auto* tab = win->CurrentTab();
    auto* ctrl = tab->ctrl;
    if (ctrl->HasToc()) {
        // use the current ToC heading as default name
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        TocItem* item = TocItemForPageNo(root, pageNo);
        if (item) {
            name = item->title;
        }
    }
    TempStr pageLabel = ctrl->GetPageLabeTemp(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel, name);
}

void AddFavoriteForCurrentPage(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    int pageNo = win->currPageNo;
    AddFavoriteForPage(win, pageNo);
}

void DelFavorite(Str filePath, int pageNo) {
    if (!filePath) {
        return;
    }
    RememberFavTreeExpansionStateForAllWindows();
    RemoveFav(filePath, pageNo);
    UpdateFavoritesTreeForAllWindows();
    SaveSettings();
}

void RememberFavTreeExpansionState(MainWindow* win) {
    win->expandedFavorites.Reset();
    TreeView* treeView = win->favTreeView;
    TreeModel* tm = treeView ? treeView->treeModel : nullptr;
    if (!tm) {
        // TODO: remember all favorites as expanded
        return;
    }
    TreeItem root = tm->Root();
    int n = tm->ChildCount(root);
    for (int i = 0; i < n; i++) {
        TreeItem ti = tm->ChildAt(root, i);
        bool isExpanded = treeView->IsExpanded(ti);
        if (isExpanded) {
            FavTreeItem* fti = (FavTreeItem*)ti;
            Favorite* fn = fti->favorite;
            FileState* f = GetByFavorite(fn);
            win->expandedFavorites.Append(f);
        }
    }
}

void RememberFavTreeExpansionStateForAllWindows() {
    for (int i = 0; i < len(gWindows); i++) {
        RememberFavTreeExpansionState(gWindows[i]);
    }
}

static void GetFavFilterWords(MainWindow* win, StrVec& wordsOut) {
    wordsOut.Reset();
    TempStr filter = GetFavFilterTemp(win);
    if (filter) {
        SplitFilterToWords(filter, wordsOut);
    }
}

static bool HasFavFilter(MainWindow* win) {
    StrVec words;
    GetFavFilterWords(win, words);
    return len(words) > 0;
}

// multi-word yellow/accent highlights via shared command-palette helpers
static void DrawFavItemHighlight(TreeView::CustomDrawEvent* ev, MainWindow* win) {
    FavTreeItem* fti = (FavTreeItem*)ev->treeItem;
    if (!fti || !fti->text) {
        return;
    }
    StrVec words;
    GetFavFilterWords(win, words);
    if (len(words) == 0) {
        return;
    }

    Rect labelRect;
    TreeView* tv = ev->treeView;
    if (!tv->GetItemRect(ev->treeItem, true, labelRect)) {
        return;
    }
    Rect itemRect{};
    tv->GetItemRect(ev->treeItem, false, itemRect);

    NMTVCUSTOMDRAW* tvcd = ev->nm;
    HDC hdc = tvcd->nmcd.hdc;
    NMCUSTOMDRAW* cd = &tvcd->nmcd;
    // POSTPAINT often omits CDIS_SELECTED; also check the control selection.
    bool isSelected = (cd->uItemState & CDIS_SELECTED) != 0;
    if (!isSelected) {
        HTREEITEM hSel = TreeView_GetSelection(tv->hwnd);
        HTREEITEM hItem = tv->GetHandleByTreeItem(ev->treeItem);
        isSelected = hSel && hItem && hSel == hItem;
    }
    bool hasFocus = (GetFocus() == tv->hwnd);
    Color bgCol, txtCol;
    ResolveTreeFilterItemColors(hdc, itemRect, tv->bgColor, tv->textColor, isSelected, hasFocus, &bgCol, &txtCol);
    GfxHdc gfx(hdc);
    DrawTreeItemFilterHighlight(&gfx, labelRect, fti->text, words, bgCol, txtCol, tv->GetFont());
}

static void OnFavCustomDraw(TreeView::CustomDrawEvent* ev) {
    ev->result = CDRF_DODEFAULT;
    NMTVCUSTOMDRAW* tvcd = ev->nm;
    NMCUSTOMDRAW* cd = &(tvcd->nmcd);

    if (cd->dwDrawStage == CDDS_PREPAINT) {
        ev->result = CDRF_NOTIFYITEMDRAW;
        return;
    }

    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    bool filterActive = HasFavFilter(win);

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        if (!ev->treeItem) {
            return;
        }
        LRESULT res = 0;
        if (filterActive) {
            res |= CDRF_NOTIFYPOSTPAINT;
        }
        ev->result = res;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPOSTPAINT) {
        if (filterActive && win) {
            DrawFavItemHighlight(ev, win);
        }
        ev->result = CDRF_DODEFAULT;
        return;
    }
}

static void FavTreeItemClicked(TreeView::ClickEvent* ev) {
    if (ev->treeItem != ev->treeView->GetSelection()) {
        return;
    }
    // Parent rows with children: leave expand/collapse to the tree; only
    // navigate when the click is a leaf (or a single-favorite file row).
    FavTreeItem* fti = (FavTreeItem*)ev->treeItem;
    if (fti && len(fti->children) > 0) {
        return;
    }
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    ReportIf(!win);
    GoToFavForTreeItem(win, ev->treeItem);
}

static void FavTreeSelectionChanged(TreeView::SelectionChangedEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    ReportIf(!win);

    // Navigate only on a mouse click, not on keyboard selection changes:
    // arrow keys / type-ahead should move the selection so the user can browse
    // favorites without each move jumping the document (and stealing focus to
    // the canvas). Enter navigates, handled in FavTreeKeyDown (#1936).
    if (!ev->byMouse) {
        return;
    }
    FavTreeItem* fti = (FavTreeItem*)ev->selectedItem;
    if (fti && len(fti->children) > 0) {
        // selecting a parent to expand/collapse must not navigate away
        return;
    }
    GoToFavForTreeItem(win, ev->selectedItem);
}

// in TableOfContents.cpp
extern void TocTreeKeyDown2(TreeView::KeyDownEvent*);

static void FavTreeKeyDown(TreeView::KeyDownEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    // Enter opens the selected favorite (sidebar panel and Favorites tab).
    // Must set result so TreeView skips its default Enter = expand/collapse.
    if (ev->keyCode == VK_RETURN) {
        if (win) {
            GoToFavForTreeItem(win, ev->treeView->GetSelection());
            ev->result = 1; // also prevents the default Windows ding
            return;
        }
    }
    // Esc: clear search and focus the filter (Favorites tab and sidebar)
    if (ev->keyCode == VK_ESCAPE) {
        if (win && win->favFilterEdit) {
            win->favFilterEdit->SetText("");
            FocusFavFilterEdit(win);
            ev->result = 1;
            return;
        }
    }
    // Up on the first top-level node: return focus to the search box
    if (ev->keyCode == VK_UP && win && win->favFilterEdit) {
        TreeItem sel = ev->treeView->GetSelection();
        HTREEITEM hSel = sel ? ev->treeView->GetHandleByTreeItem(sel) : nullptr;
        HTREEITEM hFirst = TreeView_GetRoot(ev->treeView->hwnd);
        if (hSel && hFirst && hSel == hFirst) {
            FocusFavFilterEdit(win);
            ev->result = 1;
            return;
        }
    }
    // reuse the toc tree handler for Tab/focus handling
    TocTreeKeyDown2(ev);
}

// clang-format off
static MenuDef menuDefContextFav[] = {
    {
        _TRN("Sort By Name"),
        CmdToggleFavoritesSort,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        nullptr,
        0,
    },
};
// clang-format on

static void FavTreeContextMenu(ContextMenuEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    if (!win) {
        return;
    }

    Point pt{};
    TreeItem ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (!ti) {
        pt = {ev->mouseScreen.x, ev->mouseScreen.y};
    }
    HMENU popup = BuildMenuFromDef(menuDefContextFav, CreatePopupMenu(), nullptr);
    MenuSetChecked(popup, CmdToggleFavoritesSort, gGlobalPrefs->sortFavoritesByName);
    if (!ti) {
        // Sort By Name works with no selection; Remove needs a favorite row.
        MenuRemove(popup, CmdFavoriteDel);
    }
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
    // so that we can do destructive operations without asking for permission via
    // invasive model dialog boxes but also allow reverting them if were done
    // by mistake
    if (CmdToggleFavoritesSort == cmd) {
        ToggleSortFavoritesByName();
        return;
    }
    if (CmdFavoriteDel == cmd && ti) {
        RememberFavTreeExpansionStateForAllWindows();
        FavTreeItem* fti = (FavTreeItem*)ti;
        Favorite* toDelete = fti->favorite;
        FileState* f = GetByFavorite(toDelete);
        Str fp = f->filePath;
        if (fti->parent) {
            RemoveFav(fp, toDelete->pageNo);
        } else {
            // this is a top-level node which represents all bookmarks for a given file
            RemoveAllFavForFile(fp);
        }
        UpdateFavoritesTreeForAllWindows();
        SaveSettings();
    }
}

static WNDPROC gWndProcFavBox = nullptr;
// Position label, filter edit and tree within favorites container using the
// wingui layout engine (VBox built in CreateFavorites).
// layout label + tree inside hwndFavBox (call after resizing the box)
void LayoutFavoritesContainer(MainWindow* win) {
    if (!win || !win->favLayout || !win->hwndFavBox) {
        return;
    }
    // HwndClientRect: layout is in parent client coordinates
    Rect rc = HwndClientRect(win->hwndFavBox);
    if (rc.IsEmpty()) {
        return;
    }
    LayoutTreeToSize(win->hwndFavBox, win->favLayout, {rc.dx, rc.dy}, &win->favRoot);
}

static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(gWndProcFavBox, hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    // the panel header (label + close button) is a virtual control tree, so
    // this window paints it and hands it its input
    if (VirtHostOnMessage(hwnd, win->favRoot, msg, wp, lp, res, ThemeControlBackgroundColor())) {
        return res;
    }

    switch (msg) {
        case WM_SIZE:
            LayoutFavoritesContainer(win);
            break;
    }
    return CallWindowProc(gWndProcFavBox, hwnd, msg, wp, lp);
}

// Full-window Favorites tab: close the tab. Sidebar panel: hide it.
static void FavCloseClicked(MainWindow* win, VirtMouseEvent*) {
    if (WindowTab* favTab = FindFavoritesTab(win); favTab && win->CurrentTab() == favTab) {
        CloseTab(favTab, false);
    } else {
        ToggleFavorites(win);
    }
}

void CreateFavorites(MainWindow* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATICW, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU) nullptr, h, nullptr);

    PlatformFont* labelFont = GetAppSidebarLabelFont();
    auto header = NewLabelWithClose(win->hwndFavBox, labelFont, MkFunc1(FavCloseClicked, win));
    win->favLabel = header.label;
    // label text is set in UpdateToolbarSidebarText()

    auto* filterEdit = new Edit();
    {
        Edit::CreateArgs eargs;
        eargs.parent = win->hwndFavBox;
        eargs.withBorder = false;
        // underline so the filter field is visible on flat sidebar/tab backgrounds
        eargs.withBottomBorder = true;
        eargs.cueText = _TRA("Search Favorites");
        eargs.font = GetAppFont();
        filterEdit->Create(eargs);
    }
    win->favFilterEdit = filterEdit;
    filterEdit->onTextChanged = MkFunc0(OnFavFilterTextChanged, win);
    SetWindowSubclass(filterEdit->hwnd, WndProcFavFilterEdit, NextSubclassId(), (DWORD_PTR)win);

    auto* treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent = win->hwndFavBox;
    args.font = GetAppTreeFont();
    args.fullRowSelect = true;
    args.exStyle = 0;
    args.isRtl = IsUIRtl();

    auto fn = MkFunc1Void(FavTreeContextMenu);
    treeView->onContextMenu = fn;
    treeView->onSelectionChanged = MkFunc1Void(FavTreeSelectionChanged);
    treeView->onKeyDown = MkFunc1Void(FavTreeKeyDown);
    treeView->onClick = MkFunc1Void(FavTreeItemClicked);
    treeView->onCustomDraw = MkFunc1Void(OnFavCustomDraw);

    treeView->Create(args);
    ReportIf(!treeView->hwnd);

    win->favTreeView = treeView;

    // stack label, filter edit and tree vertically; the tree flexes to fill
    // the remaining height. The VBox owns these controls/spacer (freed in ~MainWindow).
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(header.box);
    vbox->AddChild(filterEdit);
    vbox->AddChild(new Spacer(0, 2)); // gap under the search field
    vbox->AddChild(treeView, 1);
    win->favLayout = vbox;

    if (nullptr == gWndProcFavBox) {
        gWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    UpdateControlsColors(win);
}
