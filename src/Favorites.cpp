/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"

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
#include "SumatraDialogs.h"
#include "Translations.h"
#include "Accelerators.h"

void RememberFavTreeExpansionStateForAllWindows();

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

    Str Text(TreeItem) override;
    TreeItem Parent(TreeItem) override;
    int ChildCount(TreeItem) override;
    TreeItem ChildAt(TreeItem, int index) override;
    bool IsExpanded(TreeItem) override;
    bool IsChecked(TreeItem) override;
    void SetHandle(TreeItem, HTREEITEM) override;
    HTREEITEM GetHandle(TreeItem) override;

    FavTreeItem* root = nullptr;
};

FavTreeModel::~FavTreeModel() {
    delete root;
}

TreeItem FavTreeModel::Root() {
    return (TreeItem)root;
}

Str FavTreeModel::Text(TreeItem ti) {
    auto fti = (FavTreeItem*)ti;
    return fti->text;
}

TreeItem FavTreeModel::Parent(TreeItem ti) {
    auto fti = (FavTreeItem*)ti;
    return (TreeItem)fti->parent;
}

int FavTreeModel::ChildCount(TreeItem ti) {
    auto fti = (FavTreeItem*)ti;
    if (!fti) {
        return 0;
    }
    int n = len(fti->children);
    return n;
}

TreeItem FavTreeModel::ChildAt(TreeItem ti, int idx) {
    auto fti = (FavTreeItem*)ti;
    auto res = fti->children[idx];
    return (TreeItem)res;
}

bool FavTreeModel::IsExpanded(TreeItem ti) {
    auto fti = (FavTreeItem*)ti;
    return fti->isExpanded;
}

bool FavTreeModel::IsChecked(TreeItem) {
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
    for (int i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
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
    for (int i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->Contains(fn)) {
            return ds;
        }
    }
    return nullptr;
}

static void ResetFavMenuIds() {
    FileState* ds;
    for (int i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        for (int j = 0; j < len(*ds->favorites); j++) {
            (*ds->favorites)[j]->menuId = 0;
        }
    }
}

static int idxCache = -1;

static FileState* GetFavByFilePath(Str filePath) {
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    FileState* fs = gFileHistory.Get(idxCache);
    if (!fs || !str::Eq(fs->filePath, filePath)) {
        fs = gFileHistory.FindByName(filePath, &idxCache);
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
    auto favs = ds->favorites;
    int n = len(*favs);
    if (pageLabel) {
        for (int i = 0; i < n; i++) {
            auto fav = (*favs)[i];
            if (str::Eq(fav->pageLabel, pageLabel)) {
                return fav;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        auto fav = (*favs)[i];
        if (pageNo == fav->pageNo) {
            return fav;
        }
    }
    return nullptr;
}

static int SortByPageNo(const void* a, const void* b) {
    Favorite* na = *(Favorite**)a;
    Favorite* nb = *(Favorite**)b;
    // sort lower page numbers first
    return na->pageNo - nb->pageNo;
}

static void AddOrReplaceFav(Str filePath, int pageNo, Str name, Str pageLabel) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        // we were asked to add a favorite for current file but couldn't find
        // history for this file
        fav = NewFileState(filePath);
        gFileHistory.Append(fav);
    }

    Favorite* fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        str::ReplaceWithCopy(&fn->name, name);
        ReportIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
    } else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        fav->favorites->Sort(SortByPageNo);
    }
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
        gFileHistory.Remove(fav);
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
        gFileHistory.Remove(fav);
        DeleteFileState(fav);
    }
}

// Note: those might be too big
#define MAX_FAV_SUBMENUS 10
#define MAX_FAV_MENUS 10

bool HasFavorites() {
    FileState* ds;
    for (int i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (len(*ds->favorites) > 0) {
            return true;
        }
    }
    return false;
}

// caller has to free() the result
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
    for (int i = 0; (fs = gFileHistory.Get(i)) != nullptr; i++) {
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
    if (menusCount > MAX_FAV_MENUS) {
        menusCount = MAX_FAV_MENUS;
    }

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

void ToggleFavorites(MainWindow* win) {
    if (gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(win, win->tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->tocVisible, true);
        HwndSetFocus(win->favTreeView->hwnd);
    }
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
        auto data = new GoToFavoritePageData;
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
    FileState* ds = gFileHistory.FindByPath(fs->filePath);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fav->pageNo;
        ds->scrollPos = PointF(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    LoadArgs args(fs->filePath, win);
    win = LoadDocument(&args);
    if (win) {
        auto data = new GoToFavoritePageData;
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

static void MakeFavSecondLevel(FavTreeItem* parent, FileState* f) {
    int n = len(*f->favorites);
    for (int i = 0; i < n; i++) {
        Favorite* fn = (*f->favorites)[i];
        auto* ti = new FavTreeItem();
        ti->text = str::Dup(FavReadableNameTemp(fn));
        ti->parent = parent;
        ti->favorite = fn;
        parent->children.Append(ti);
    }
}

static FavTreeModel* BuildFavTreeModel(MainWindow* win) {
    auto* res = new FavTreeModel();
    res->root = new FavTreeItem();
    StrVec filePathsSorted;
    GetSortedFilePaths(filePathsSorted);
    for (int i = 0; i < len(filePathsSorted); i++) {
        Str path = filePathsSorted[i];
        FileState* fs = GetFavByFilePath(path);
        ReportIf(!fs);
        if (!fs) {
            continue;
        }
        bool isExpanded = win->expandedFavorites.Contains(fs);
        FavTreeItem* ti = MakeFavTopLevelItem(fs, isExpanded);
        if (!ti) {
            continue;
        }
        res->root->children.Append(ti);
        if (len(*fs->favorites) > 1) {
            MakeFavSecondLevel(ti, fs);
        }
    }
    return res;
}

void PopulateFavTreeIfNeeded(MainWindow* win) {
    TreeView* treeView = win->favTreeView;
    if (treeView->treeModel) {
        return;
    }
    TreeModel* tm = BuildFavTreeModel(win);
    treeView->SetTreeModel(tm);
}

void UpdateFavoritesTree(MainWindow* win) {
    TreeView* treeView = win->favTreeView;
    auto* prevModel = treeView->treeModel;
    TreeModel* newModel = BuildFavTreeModel(win);
    treeView->SetTreeModel(newModel);
    delete prevModel;

    // hide the favorites tree if we've removed the last favorite
    TreeItem root = newModel->Root();
    bool show = gGlobalPrefs->showFavorites;
    if (newModel->ChildCount(root) == 0) {
        show = false;
    }
    SetSidebarVisibility(win, win->tocVisible, show);
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

void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, Str pageLabel, Str nameIn) {
    Str name = str::Dup(nameIn);
    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageLabel, name);
    if (shouldAdd) {
        TempStr plainLabel = fmt("%d", pageNo);
        bool needsLabel = !str::Eq(plainLabel, pageLabel);

        RememberFavTreeExpansionStateForAllWindows();
        Str pl = needsLabel ? pageLabel : Str{};
        WindowTab* tab = win->CurrentTab();
        Str path = tab->filePath;
        AddOrReplaceFav(path, pageNo, name, pl);
        // expand newly added favorites by default
        FileState* fav = GetFavByFilePath(path);
        if (fav && len(*fav->favorites) == 2) {
            win->expandedFavorites.Append(fav);
        }
        UpdateFavoritesTreeForAllWindows();
        SaveSettings();
    }
    str::Free(name);
}

void AddFavoriteForPage(MainWindow* win, int pageNo) {
    Str name;
    auto tab = win->CurrentTab();
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

static void FavTreeItemClicked(TreeView::ClickEvent* ev) {
    if (ev->treeItem == ev->treeView->GetSelection()) {
        MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
        ReportIf(!win);
        GoToFavForTreeItem(win, ev->treeItem);
    }
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
    GoToFavForTreeItem(win, ev->selectedItem);
}

// in TableOfContents.cpp
extern void TocTreeKeyDown2(TreeView::KeyDownEvent*);

static void FavTreeKeyDown(TreeView::KeyDownEvent* ev) {
    if (ev->keyCode == VK_RETURN) {
        MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
        if (win) {
            GoToFavForTreeItem(win, ev->treeView->GetSelection());
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
        _TRN("Remove from favorites"),
        CmdFavoriteDel
    },
    {
        nullptr,
        0,
    }
};
// clang-format on

static void FavTreeContextMenu(ContextMenuEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    // TreeView* treeView = (TreeView*)ev->w;
    // HWND hwnd = treeView->hwnd;
    // MainWindow* win = FindMainWindowByHwnd(hwnd);

    POINT pt{};
    TreeItem ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (!ti) {
        return;
    }
    HMENU popup = BuildMenuFromDef(menuDefContextFav, CreatePopupMenu(), nullptr);
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
    // so that we can do destructive operations without asking for permission via
    // invasive model dialog boxes but also allow reverting them if were done
    // by mistake
    if (CmdFavoriteDel == cmd) {
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
// Position label and tree within favorites container using the wingui layout
// engine (VBox built in CreateFavorites).
static void LayoutFavContainer(MainWindow* win) {
    if (!win->favLayout) {
        return;
    }
    Rect rc = WindowRect(win->hwndFavBox);
    win->favLayout->Layout(Tight(Size{rc.dx, rc.dy}));
    win->favLayout->SetBounds(Rect{0, 0, rc.dx, rc.dy});
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

    switch (msg) {
        case WM_SIZE:
            LayoutFavContainer(win);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_FAV_LABEL_WITH_CLOSE) {
                ToggleFavorites(win);
            }
            break;
    }
    return CallWindowProc(gWndProcFavBox, hwnd, msg, wp, lp);
}

void CreateFavorites(MainWindow* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU) nullptr, h, nullptr);

    auto l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndFavBox;
        args.cmdId = IDC_FAV_LABEL_WITH_CLOSE;
        args.font = GetAppSidebarLabelFont();
        args.isRtl = IsUIRtl();
        l->Create(args);
    }

    win->favLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    // label is set in UpdateToolbarSidebarText()

    auto treeView = new TreeView();
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

    treeView->Create(args);
    ReportIf(!treeView->hwnd);

    win->favTreeView = treeView;

    // stack label and tree vertically; the tree flexes to fill the remaining
    // height. The VBox owns these two controls (freed in ~MainWindow).
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(l);
    vbox->AddChild(treeView, 1);
    win->favLayout = vbox;

    if (nullptr == gWndProcFavBox) {
        gWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    UpdateControlsColors(win);
}
