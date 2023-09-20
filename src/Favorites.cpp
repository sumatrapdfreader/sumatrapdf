/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "AppSettings.h"
#include "Favorites.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Tabs.h"
#include "Translations.h"

struct FavTreeItem {
    ~FavTreeItem();

    HTREEITEM hItem = nullptr;
    FavTreeItem* parent = nullptr;
    char* text = nullptr;
    bool isExpanded = false;

    // not owned by us
    Favorite* favorite = nullptr;

    Vec<FavTreeItem*> children;
};

FavTreeItem::~FavTreeItem() {
    str::Free(text);
    DeleteVecMembers(children);
}

struct FavTreeModel : public TreeModel {
    ~FavTreeModel() override;

    TreeItem Root() override;

    char* Text(TreeItem) override;
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

char* FavTreeModel::Text(TreeItem ti) {
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
    size_t n = fti->children.size();
    return (int)n;
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

bool FavTreeModel::IsChecked(TreeItem ti) {
    return false;
}

void FavTreeModel::SetHandle(TreeItem ti, HTREEITEM hItem) {
    CrashIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    treeItem->hItem = hItem;
}

HTREEITEM FavTreeModel::GetHandle(TreeItem ti) {
    CrashIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    return treeItem->hItem;
}

Favorite* Favorites::GetByMenuId(int menuId, FileState** dsOut) {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        for (size_t j = 0; j < ds->favorites->size(); j++) {
            if (menuId == ds->favorites->at(j)->menuId) {
                if (dsOut) {
                    *dsOut = ds;
                }
                return ds->favorites->at(j);
            }
        }
    }
    return nullptr;
}

FileState* Favorites::GetByFavorite(Favorite* fn) {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->Contains(fn)) {
            return ds;
        }
    }
    return nullptr;
}

void Favorites::ResetMenuIds() {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        for (size_t j = 0; j < ds->favorites->size(); j++) {
            ds->favorites->at(j)->menuId = 0;
        }
    }
}

FileState* Favorites::GetFavByFilePath(const char* filePath) {
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    FileState* fs = gFileHistory.Get(idxCache);
    if (!fs || !str::Eq(fs->filePath, filePath)) {
        fs = gFileHistory.FindByName(filePath, &idxCache);
    }
    return fs;
}

bool Favorites::IsPageInFavorites(const char* filePath, int pageNo) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return false;
    }
    for (size_t i = 0; i < fav->favorites->size(); i++) {
        if (pageNo == fav->favorites->at(i)->pageNo) {
            return true;
        }
    }
    return false;
}

static Favorite* FindByPage(FileState* ds, int pageNo, const char* pageLabel = nullptr) {
    if (!ds || !ds->favorites) {
        return nullptr;
    }
    auto favs = ds->favorites;
    int n = favs->isize();
    if (pageLabel) {
        for (int i = 0; i < n; i++) {
            auto fav = favs->at(i);
            if (str::Eq(fav->pageLabel, pageLabel)) {
                return fav;
            }
        }
    }
    for (int i = 0; i < n; i++) {
        auto fav = favs->at(i);
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

void Favorites::AddOrReplace(const char* filePath, int pageNo, const char* name, const char* pageLabel) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        CrashIf(gGlobalPrefs->rememberOpenedFiles);
        fav = NewDisplayState(filePath);
        gFileHistory.Append(fav);
    }

    Favorite* fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        str::ReplaceWithCopy(&fn->name, name);
        CrashIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
    } else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        fav->favorites->Sort(SortByPageNo);
    }
}

void Favorites::Remove(const char* filePath, int pageNo) {
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

    if (!gGlobalPrefs->rememberOpenedFiles && 0 == fav->favorites->size()) {
        gFileHistory.Remove(fav);
        DeleteDisplayState(fav);
    }
}

void Favorites::RemoveAllForFile(const char* filePath) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return;
    }

    for (size_t i = 0; i < fav->favorites->size(); i++) {
        DeleteFavorite(fav->favorites->at(i));
    }
    fav->favorites->Reset();

    if (!gGlobalPrefs->rememberOpenedFiles) {
        gFileHistory.Remove(fav);
        DeleteDisplayState(fav);
    }
}

// Note: those might be too big
#define MAX_FAV_SUBMENUS 10
#define MAX_FAV_MENUS 10

bool HasFavorites() {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->size() > 0) {
            return true;
        }
    }
    return false;
}

// caller has to free() the result
static char* FavReadableName(Favorite* fn) {
    const char* toFree = nullptr;
    const char* label = fn->pageLabel;
    if (!label) {
        label = str::Format("%d", fn->pageNo);
        toFree = label;
    }
    char* res = nullptr;
    if (fn->name) {
        AutoFreeStr pageNo(str::Format(_TRA("(page %s)"), label));
        res = str::Join(fn->name, " ", pageNo);
    } else {
        res = str::Format(_TRA("Page %s"), label);
    }
    str::Free(toFree);
    return res;
}

// caller has to free() the result
static char* FavCompactReadableName(FileState* fav, Favorite* fn, bool isCurrent = false) {
    AutoFreeStr rn(FavReadableName(fn));
    if (isCurrent) {
        return str::Format("%s : %s", _TRA("Current file"), rn.Get());
    }
    const char* fp = path::GetBaseNameTemp(fav->filePath);
    return str::Format("%s : %s", fp, rn.Get());
}

static void AppendFavMenuItems(HMENU m, FileState* f, int& idx, bool combined, bool isCurrent) {
    CrashIf(!f);
    if (!f) {
        return;
    }
    for (size_t i = 0; i < f->favorites->size(); i++) {
        if (i >= MAX_FAV_MENUS) {
            return;
        }
        Favorite* fn = f->favorites->at(i);
        fn->menuId = idx++;
        AutoFreeStr s;
        if (combined) {
            s = FavCompactReadableName(f, fn, isCurrent);
        } else {
            s = FavReadableName(fn);
        }
        auto safeStr = MenuToSafeStringTemp(s);
        WCHAR* ws = ToWstrTemp(safeStr);
        AppendMenuW(m, MF_STRING, (UINT_PTR)fn->menuId, ws);
    }
}

static bool SortByBaseFileName(const char* s1, const char* s2) {
    if (str::IsEmpty(s1)) {
        if (str::IsEmpty(s2)) {
            return false;
        }
        return true;
    }
    if (str::IsEmpty(s2)) {
        return false;
    }
    const char* base1 = path::GetBaseNameTemp(s1);
    const char* base2 = path::GetBaseNameTemp(s2);
    int n = str::CmpNatural(base1, base2);
    return n < 0;
}

static void GetSortedFilePaths(StrVec& filePathsSortedOut, FileState* toIgnore = nullptr) {
    FileState* fs;
    for (size_t i = 0; (fs = gFileHistory.Get(i)) != nullptr; i++) {
        if (fs->favorites->size() > 0 && fs != toIgnore) {
            filePathsSortedOut.Append(fs->filePath);
        }
    }
    filePathsSortedOut.Sort(SortByBaseFileName);
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
static void AppendFavMenus(HMENU m, const char* currFilePath) {
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    FileState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = gFavorites.GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    StrVec filePathsSorted;
    if (HasPermission(Perm::DiskAccess)) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && currFileFav->favorites->size() > 0) {
        filePathsSorted.InsertAt(0, currFileFav->filePath);
    }

    if (filePathsSorted.size() == 0) {
        return;
    }

    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    gFavorites.ResetMenuIds();
    int menuId = CmdFavoriteFirst;

    size_t menusCount = filePathsSorted.size();
    if (menusCount > MAX_FAV_MENUS) {
        menusCount = MAX_FAV_MENUS;
    }

    for (size_t i = 0; i < menusCount; i++) {
        const char* filePath = filePathsSorted.at(i);
        FileState* f = gFavorites.GetFavByFilePath(filePath);
        CrashIf(!f);
        if (!f) {
            continue;
        }
        HMENU sub = m;
        bool combined = (f->favorites->size() == 1);
        if (!combined) {
            sub = CreateMenu();
        }
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            if (f == currFileFav) {
                AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, _TR("Current file"));
            } else {
                TempStr fileName = MenuToSafeStringTemp(path::GetBaseNameTemp(filePath));
                AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, ToWstrTemp(fileName));
            }
        }
    }
}

#include "Accelerators.h"

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
        AppendFavMenus(menu, (const char*)nullptr);
    } else {
        AutoFreeStr label(win->ctrl->GetPageLabel(win->currPageNo));
        bool isBookmarked = gFavorites.IsPageInFavorites(win->ctrl->GetFilePath(), win->currPageNo);
        if (isBookmarked) {
            MenuSetEnabled(menu, CmdFavoriteAdd, false);
            AutoFreeStr s(str::Format(_TRA("Remove page %s from favorites"), label.Get()));
            MenuSetText(menu, CmdFavoriteDel, s);
        } else {
            MenuSetEnabled(menu, CmdFavoriteDel, false);
            str::Str str = _TRA("Add page %s to favorites");
            ACCEL a;
            bool ok = GetAccelByCmd(CmdFavoriteAdd, a);
            if (ok) {
                AppendAccelKeyToMenuString(str, a);
            }
            AutoFreeStr s(str::Format(str.Get(), label.Get()));
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
        win->favTreeView->SetFocus();
    }
}

static void GoToFavorite(MainWindow* win, int pageNo) {
    if (!MainWindowStillValid(win)) {
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

// Going to a bookmark within current file scrolls to a given page.
// Going to a bookmark in another file, loads the file and scrolls to a page
// (similar to how invoking one of the recently opened files works)
static void GoToFavorite(MainWindow* win, FileState* fs, Favorite* fn) {
    CrashIf(!fs || !fn);
    if (!fs || !fn) {
        return;
    }

    char* fp = fs->filePath;
    MainWindow* existingWin = FindMainWindowByFile(fp, true);
    if (existingWin) {
        int pageNo = fn->pageNo;
        uitask::Post([=] { GoToFavorite(existingWin, pageNo); });
        return;
    }

    if (!HasPermission(Perm::DiskAccess)) {
        return;
    }

    // When loading a new document, go directly to selected page instead of
    // first showing last seen page stored in file history
    // A hacky solution because I don't want to add even more parameters to
    // LoadDocument() and LoadDocumentInto()
    int pageNo = fn->pageNo;
    FileState* ds = gFileHistory.FindByPath(fs->filePath);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fn->pageNo;
        ds->scrollPos = PointF(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    LoadArgs args(fs->filePath, win);
    win = LoadDocument(&args, false, false);
    if (win) {
        uitask::Post([=] { GoToFavorite(win, pageNo); });
    }
}

void GoToFavoriteByMenuId(MainWindow* win, int wmId) {
    FileState* f;
    Favorite* fn = gFavorites.GetByMenuId(wmId, &f);
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
    FileState* f = gFavorites.GetByFavorite(fn);
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

static FavTreeItem* MakeFavTopLevelItem(FileState* fav, bool isExpanded) {
    auto* res = new FavTreeItem();
    Favorite* fn = fav->favorites->at(0);
    res->favorite = fn;

    bool isCollapsed = fav->favorites->size() == 1;
    if (isCollapsed) {
        isExpanded = false;
    }
    res->isExpanded = isExpanded;

    if (isCollapsed) {
        res->text = FavCompactReadableName(fav, fn);
    } else {
        char* fp = fav->filePath;
        res->text = str::Dup(path::GetBaseNameTemp(fp));
    }
    return res;
}

static void MakeFavSecondLevel(FavTreeItem* parent, FileState* f) {
    size_t n = f->favorites->size();
    for (size_t i = 0; i < n; i++) {
        Favorite* fn = f->favorites->at(i);
        auto* ti = new FavTreeItem();
        ti->text = FavReadableName(fn);
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
    for (char* path : filePathsSorted) {
        FileState* f = gFavorites.GetFavByFilePath(path);
        CrashIf(!f);
        if (!f) {
            continue;
        }
        bool isExpanded = win->expandedFavorites.Contains(f);
        FavTreeItem* ti = MakeFavTopLevelItem(f, isExpanded);
        res->root->children.Append(ti);
        if (f->favorites->size() > 1) {
            MakeFavSecondLevel(ti, f);
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

void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, const char* pageLabel, const char* nameIn) {
    AutoFreeStr name = str::Dup(nameIn);
    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageLabel, name);
    if (!shouldAdd) {
        return;
    }

    AutoFreeStr plainLabel(str::Format("%d", pageNo));
    bool needsLabel = !str::Eq(plainLabel, pageLabel);

    RememberFavTreeExpansionStateForAllWindows();
    const char* pl = nullptr;
    if (needsLabel) {
        pl = pageLabel;
    }
    WindowTab* tab = win->CurrentTab();
    char* path = tab->filePath;
    gFavorites.AddOrReplace(path, pageNo, name, pl);
    // expand newly added favorites by default
    FileState* fav = gFavorites.GetFavByFilePath(path);
    if (fav && fav->favorites->size() == 2) {
        win->expandedFavorites.Append(fav);
    }
    UpdateFavoritesTreeForAllWindows();
    SaveSettings();
}

void AddFavoriteForCurrentPage(MainWindow* win, int pageNo) {
    char* name = nullptr;
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
    AutoFreeStr pageLabel = ctrl->GetPageLabel(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel.Get(), name);
}

void AddFavoriteForCurrentPage(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    int pageNo = win->currPageNo;
    AddFavoriteForCurrentPage(win, pageNo);
}

void DelFavorite(const char* filePath, int pageNo) {
    if (!filePath) {
        return;
    }
    RememberFavTreeExpansionStateForAllWindows();
    gFavorites.Remove(filePath, pageNo);
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
            FileState* f = gFavorites.GetByFavorite(fn);
            win->expandedFavorites.Append(f);
        }
    }
}

void RememberFavTreeExpansionStateForAllWindows() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        RememberFavTreeExpansionState(gWindows.at(i));
    }
}

#if 0
static void FavTreeItemClicked(TreeClickEvent* ev) {
    ev->didHandle = true;
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    CrashIf(!win);
    GoToFavForTreeItem(win, ev->treeItem);
}
#endif

static void FavTreeSelectionChanged(TreeSelectionChangedEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    CrashIf(!win);

    // When the focus is set to the toc window the first item in the treeview is automatically
    // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action ==
    // 0x00001000. We have to ignore this message to prevent the current page to be changed.
    // The case pnmtv->action==TVC_UNKNOWN is ignored because
    // it corresponds to a notification sent by
    // the function TreeView_DeleteAllItems after deletion of the item.
    bool shouldHandle = ev->byKeyboard || ev->byMouse;
    if (!shouldHandle) {
        return;
    }
    GoToFavForTreeItem(win, ev->selectedItem);
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
    TreeView* treeView = (TreeView*)ev->w;
    HWND hwnd = treeView->hwnd;
    // MainWindow* win = FindMainWindowByHwnd(hwnd);

    POINT pt{};
    TreeItem ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (!ti) {
        return;
    }
    HMENU popup = BuildMenuFromMenuDef(menuDefContextFav, CreatePopupMenu(), nullptr);
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
        FileState* f = gFavorites.GetByFavorite(toDelete);
        char* fp = f->filePath;
        if (fti->parent) {
            gFavorites.Remove(fp, toDelete->pageNo);
        } else {
            // this is a top-level node which represents all bookmarks for a given file
            gFavorites.RemoveAllForFile(fp);
        }
        UpdateFavoritesTreeForAllWindows();
        SaveSettings();
    }
}

static WNDPROC gWndProcFavBox = nullptr;
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(gWndProcFavBox, hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    TreeView* treeView = win->favTreeView;
    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->favLabelWithClose, treeView->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_FAV_LABEL_WITH_CLOSE) {
                ToggleFavorites(win);
            }
            break;
    }
    return CallWindowProc(gWndProcFavBox, hwnd, msg, wp, lp);
}

static HFONT gTreeFont = nullptr;

HFONT GetTreeFont() {
    if (gTreeFont) {
        return gTreeFont;
    }

    int fntSize = GetSizeOfDefaultGuiFont();
    int fntSizeUser = gGlobalPrefs->treeFontSize;
    int fntWeightOffsetUser = gGlobalPrefs->treeFontWeightOffset;
    char* fntNameUser_utf8 = gGlobalPrefs->treeFontName;
    if (fntSizeUser > 5) {
        fntSize = fntSizeUser;
    }
    gTreeFont = GetUserGuiFont(fntSize, fntWeightOffsetUser, fntNameUser_utf8);
    CrashIf(!gTreeFont);
    return gTreeFont;
}

// in TableOfContents.cpp
extern LRESULT TocTreeKeyDown2(TreeKeyDownEvent*);

void CreateFavorites(MainWindow* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU) nullptr, h, nullptr);

    auto l = new LabelWithCloseWnd();
    {
        LabelWithCloseCreateArgs args;
        args.parent = win->hwndFavBox;
        args.cmdId = IDC_FAV_LABEL_WITH_CLOSE;
        // TODO: use the same font size as in GetTreeFont()?
        args.font = GetDefaultGuiFont(true, false);
        l->Create(args);
    }

    win->favLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    // label is set in UpdateToolbarSidebarText()

    auto treeView = new TreeView();
    TreeViewCreateArgs args;
    args.parent = win->hwndFavBox;
    args.font = GetTreeFont();
    args.fullRowSelect = true;
    args.exStyle = WS_EX_STATICEDGE;

    treeView->onContextMenu = FavTreeContextMenu;
    treeView->onTreeSelectionChanged = FavTreeSelectionChanged;
    treeView->onTreeKeyDown = TocTreeKeyDown2;
    // treeView->onTreeClick = FavTreeItemClicked;
    // treeView->onChar = TocTreeCharHandler;
    // treeView->onMouseWheel = TocTreeMouseWheelHandler;

    treeView->Create(args);
    CrashIf(!treeView->hwnd);

    win->favTreeView = treeView;

    if (nullptr == gWndProcFavBox) {
        gWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    UpdateControlsColors(win);
}
