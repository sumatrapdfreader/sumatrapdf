/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "AppPrefs.h"
#include "Favorites.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Tabs.h"
#include "Translations.h"

struct FavTreeItem {
    ~FavTreeItem();

    HTREEITEM hItem{nullptr};
    FavTreeItem* parent{nullptr};
    WCHAR* text{nullptr};
    bool isExpanded{false};

    // not owned by us
    Favorite* favorite{nullptr};

    Vec<FavTreeItem*> children;
};

FavTreeItem::~FavTreeItem() {
    free(text);
    DeleteVecMembers(children);
}

struct FavTreeModel : public TreeModel {
    ~FavTreeModel() override;

    TreeItem Root() override;

    WCHAR* Text(TreeItem) override;
    TreeItem Parent(TreeItem) override;
    int ChildCount(TreeItem) override;
    TreeItem ChildAt(TreeItem, int index) override;
    bool IsExpanded(TreeItem) override;
    bool IsChecked(TreeItem) override;
    void SetHandle(TreeItem, HTREEITEM) override;
    HTREEITEM GetHandle(TreeItem) override;

    FavTreeItem* root{nullptr};
};

FavTreeModel::~FavTreeModel() {
    delete root;
}

TreeItem FavTreeModel::Root() {
    return (TreeItem)root;
}

WCHAR* FavTreeModel::Text(TreeItem ti) {
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

FileState* Favorites::GetFavByFilePath(const WCHAR* filePath) {
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    FileState* fs = gFileHistory.Get(idxCache);
    char* filePathA = ToUtf8Temp(filePath);
    if (!fs || !str::Eq(fs->filePath, filePathA)) {
        fs = gFileHistory.Find(filePathA, &idxCache);
    }
    return fs;
}

bool Favorites::IsPageInFavorites(const WCHAR* filePath, int pageNo) {
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

static Favorite* FindByPage(FileState* ds, int pageNo, const WCHAR* pageLabelW = nullptr) {
    if (!ds || !ds->favorites) {
        return nullptr;
    }
    auto favs = ds->favorites;
    int n = favs->isize();
    if (pageLabelW) {
        char* pageLabel = ToUtf8Temp(pageLabelW);
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

void Favorites::AddOrReplace(const WCHAR* filePathW, int pageNo, const WCHAR* name, const WCHAR* pageLabel) {
    FileState* fav = GetFavByFilePath(filePathW);
    if (!fav) {
        CrashIf(gGlobalPrefs->rememberOpenedFiles);
        char* filePath = ToUtf8Temp(filePathW);
        fav = NewDisplayState(filePath);
        gFileHistory.Append(fav);
    }

    Favorite* fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        char* nameA = strconv::WstrToUtf8(name);
        str::ReplacePtr(&fn->name, nameA);
        CrashIf(fn->pageLabel && !str::Eq(fn->pageLabel, ToUtf8Temp(pageLabel)));
    } else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        fav->favorites->Sort(SortByPageNo);
    }
}

void Favorites::Remove(const WCHAR* filePath, int pageNo) {
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

void Favorites::RemoveAllForFile(const WCHAR* filePath) {
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
static WCHAR* FavReadableName(Favorite* fn) {
    const WCHAR* toFree{nullptr};
    const WCHAR* label = ToWstrTemp(fn->pageLabel);
    if (!label) {
        label = str::Format(L"%d", fn->pageNo);
        toFree = label;
    }
    WCHAR* res{nullptr};
    if (fn->name) {
        AutoFreeWstr pageNo(str::Format(_TR("(page %s)"), label));
        res = str::Join(ToWstrTemp(fn->name), L" ", pageNo);
    } else {
        res = str::Format(_TR("Page %s"), label);
    }
    str::Free(toFree);
    return res;
}

// caller has to free() the result
static WCHAR* FavCompactReadableName(FileState* fav, Favorite* fn, bool isCurrent = false) {
    AutoFreeWstr rn(FavReadableName(fn));
    if (isCurrent) {
        return str::Format(L"%s : %s", _TR("Current file"), rn.Get());
    }
    const WCHAR* fp = path::GetBaseNameTemp(ToWstrTemp(fav->filePath));
    return str::Format(L"%s : %s", fp, rn.Get());
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
        AutoFreeWstr s;
        if (combined) {
            s.Set(FavCompactReadableName(f, fn, isCurrent));
        } else {
            s.Set(FavReadableName(fn));
        }
        auto str = win::menu::ToSafeString(s);
        AppendMenuW(m, MF_STRING, (UINT_PTR)fn->menuId, str);
    }
}

static int SortByBaseFileName(const void* a, const void* b) {
    const WCHAR* filePathA = *(const WCHAR**)a;
    const WCHAR* filePathB = *(const WCHAR**)b;
    const WCHAR* baseA = path::GetBaseNameTemp(filePathA);
    const WCHAR* baseB = path::GetBaseNameTemp(filePathB);
    return str::CmpNatural(baseA, baseB);
}

static void GetSortedFilePaths(Vec<const WCHAR*>& filePathsSortedOut, FileState* toIgnore = nullptr) {
    FileState* fs;
    for (size_t i = 0; (fs = gFileHistory.Get(i)) != nullptr; i++) {
        if (fs->favorites->size() > 0 && fs != toIgnore) {
            const WCHAR* s = strconv::Utf8ToWstr(fs->filePath);
            filePathsSortedOut.Append(s);
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
static void AppendFavMenus(HMENU m, const WCHAR* currFilePath) {
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    FileState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = gFavorites.GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    Vec<const WCHAR*> filePathsSorted;
    if (HasPermission(Perm::DiskAccess)) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && currFileFav->favorites->size() > 0) {
        filePathsSorted.InsertAt(0, ToWstrTemp(currFileFav->filePath));
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
        const WCHAR* filePath = filePathsSorted.at(i);
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
                AutoFreeWstr tmp;
                tmp.SetCopy(path::GetBaseNameTemp(filePath));
                auto fileName = win::menu::ToSafeString(tmp);
                AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, fileName);
            }
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
void RebuildFavMenu(WindowInfo* win, HMENU menu) {
    if (!win->IsDocLoaded()) {
        win::menu::SetEnabled(menu, CmdFavoriteAdd, false);
        win::menu::SetEnabled(menu, CmdFavoriteDel, false);
        AppendFavMenus(menu, nullptr);
    } else {
        AutoFreeWstr label(win->ctrl->GetPageLabel(win->currPageNo));
        bool isBookmarked = gFavorites.IsPageInFavorites(win->ctrl->GetFilePath(), win->currPageNo);
        if (isBookmarked) {
            win::menu::SetEnabled(menu, CmdFavoriteAdd, false);
            AutoFreeWstr s(str::Format(_TR("Remove page %s from favorites"), label.Get()));
            win::menu::SetText(menu, CmdFavoriteDel, s);
        } else {
            win::menu::SetEnabled(menu, CmdFavoriteDel, false);
            AutoFreeWstr s(str::Format(_TR("Add page %s to favorites\tCtrl+B"), label.Get()));
            win::menu::SetText(menu, CmdFavoriteAdd, s);
        }
        AppendFavMenus(menu, win->ctrl->GetFilePath());
    }
    win::menu::SetEnabled(menu, CmdFavoriteToggle, HasFavorites());
}

void ToggleFavorites(WindowInfo* win) {
    if (gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(win, win->tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->tocVisible, true);
        win->favTreeCtrl->SetFocus();
    }
}

static void GoToFavorite(WindowInfo* win, int pageNo) {
    if (!WindowInfoStillValid(win)) {
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
static void GoToFavorite(WindowInfo* win, FileState* fs, Favorite* fn) {
    CrashIf(!fs || !fn);
    if (!fs || !fn) {
        return;
    }

    WCHAR* fp = ToWstrTemp(fs->filePath);
    WindowInfo* existingWin = FindWindowInfoByFile(fp, true);
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
    FileState* ds = gFileHistory.Find(fs->filePath, nullptr);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fn->pageNo;
        ds->scrollPos = PointF(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    LoadArgs args(fs->filePath, win);
    win = LoadDocument(args);
    if (win) {
        uitask::Post([=] { GoToFavorite(win, pageNo); });
    }
}

void GoToFavoriteByMenuId(WindowInfo* win, int wmId) {
    FileState* f;
    Favorite* fn = gFavorites.GetByMenuId(wmId, &f);
    if (fn) {
        GoToFavorite(win, f, fn);
    }
}

static void GoToFavForTreeItem(WindowInfo* win, TreeItem ti) {
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
static void GoToFavForTVItem(WindowInfo* win, TreeCtrl* treeCtrl, HTREEITEM hItem = nullptr) {
    TreeItem ti = nullptr;
    if (nullptr == hItem) {
        ti = treeCtrl->GetSelection();
    } else {
        ti = treeCtrl->GetTreeItemByHandle(hItem);
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
        WCHAR* fp = ToWstrTemp(fav->filePath);
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

static FavTreeModel* BuildFavTreeModel(WindowInfo* win) {
    auto* res = new FavTreeModel();
    res->root = new FavTreeItem();
    Vec<const WCHAR*> filePathsSorted;
    GetSortedFilePaths(filePathsSorted);
    for (size_t i = 0; i < filePathsSorted.size(); i++) {
        FileState* f = gFavorites.GetFavByFilePath(filePathsSorted.at(i));
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

void PopulateFavTreeIfNeeded(WindowInfo* win) {
    TreeCtrl* treeCtrl = win->favTreeCtrl;
    if (treeCtrl->treeModel) {
        return;
    }
    TreeModel* tm = BuildFavTreeModel(win);
    treeCtrl->SetTreeModel(tm);
}

void UpdateFavoritesTree(WindowInfo* win) {
    TreeCtrl* treeCtrl = win->favTreeCtrl;
    auto* prevModel = treeCtrl->treeModel;
    TreeModel* newModel = BuildFavTreeModel(win);
    treeCtrl->SetTreeModel(newModel);
    delete prevModel;

    // hide the favorites tree if we've removed the last favorite
    TreeItem root = newModel->Root();
    bool show = newModel->ChildCount(root) > 0;
    SetSidebarVisibility(win, win->tocVisible, show);
}

void UpdateFavoritesTreeForAllWindows() {
    for (WindowInfo* win : gWindows) {
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

void AddFavoriteWithLabelAndName(WindowInfo* win, int pageNo, const WCHAR* pageLabel, AutoFreeWstr& name) {
    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageLabel, name);
    if (!shouldAdd) {
        return;
    }

    AutoFreeWstr plainLabel(str::Format(L"%d", pageNo));
    bool needsLabel = !str::Eq(plainLabel, pageLabel);

    RememberFavTreeExpansionStateForAllWindows();
    const WCHAR* pl = nullptr;
    if (needsLabel) {
        pl = pageLabel;
    }
    TabInfo* tab = win->currentTab;
    gFavorites.AddOrReplace(tab->filePath, pageNo, name, pl);
    // expand newly added favorites by default
    FileState* fav = gFavorites.GetFavByFilePath(tab->filePath);
    if (fav && fav->favorites->size() == 2) {
        win->expandedFavorites.Append(fav);
    }
    UpdateFavoritesTreeForAllWindows();
    prefs::Save();
}

void AddFavoriteForCurrentPage(WindowInfo* win, int pageNo) {
    AutoFreeWstr name;
    auto tab = win->currentTab;
    auto* ctrl = tab->ctrl;
    if (ctrl->HacToc()) {
        // use the current ToC heading as default name
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        TocItem* item = TocItemForPageNo(root, pageNo);
        if (item) {
            name.SetCopy(item->title);
        }
    }
    AutoFreeWstr pageLabel = ctrl->GetPageLabel(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel.Get(), name);
}

void AddFavoriteForCurrentPage(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    int pageNo = win->currPageNo;
    AddFavoriteForCurrentPage(win, pageNo);
}

void DelFavorite(const WCHAR* filePath, int pageNo) {
    if (!filePath) {
        return;
    }
    RememberFavTreeExpansionStateForAllWindows();
    gFavorites.Remove(filePath, pageNo);
    UpdateFavoritesTreeForAllWindows();
    prefs::Save();
}

void RememberFavTreeExpansionState(WindowInfo* win) {
    win->expandedFavorites.Reset();
    TreeCtrl* treeCtrl = win->favTreeCtrl;
    TreeModel* tm = treeCtrl ? treeCtrl->treeModel : nullptr;
    if (!tm) {
        // TODO: remember all favorites as expanded
        return;
    }
    TreeItem root = tm->Root();
    int n = tm->ChildCount(root);
    for (int i = 0; i < n; i++) {
        TreeItem ti = tm->ChildAt(root, i);
        bool isExpanded = treeCtrl->IsExpanded(ti);
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

static void FavTreeItemClicked(TreeClickEvent* ev) {
    ev->didHandle = true;
    WindowInfo* win = FindWindowInfoByHwnd(ev->w->hwnd);
    CrashIf(!win);
    GoToFavForTreeItem(win, ev->treeItem);
}

static void FavTreeSelectionChanged(TreeSelectionChangedEvent* ev) {
    ev->didHandle = true;
    WindowInfo* win = FindWindowInfoByHwnd(ev->w->hwnd);
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

static void FavTreeContextMenu(ContextMenuEvent* ev) {
    ev->didHandle = true;

    TreeCtrl* treeCtrl = (TreeCtrl*)ev->w;
    CrashIf(!IsTreeKind(treeCtrl->kind));
    HWND hwnd = treeCtrl->hwnd;
    // WindowInfo* win = FindWindowInfoByHwnd(hwnd);

    POINT pt{};
    TreeItem ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (!ti) {
        return;
    }
    HMENU popup = BuildMenuFromMenuDef(menuDefContextFav, CreatePopupMenu(), nullptr);
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, hwnd, nullptr);
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
        if (fti->parent) {
            FileState* f = gFavorites.GetByFavorite(toDelete);
            WCHAR* fp = ToWstrTemp(f->filePath);
            gFavorites.Remove(fp, toDelete->pageNo);
        } else {
            // this is a top-level node which represents all bookmarks for a given file
            FileState* f = gFavorites.GetByFavorite(toDelete);
            WCHAR* fp = ToWstrTemp(f->filePath);
            gFavorites.RemoveAllForFile(fp);
        }
        UpdateFavoritesTreeForAllWindows();
        prefs::Save();
    }
}

static WNDPROC DefWndProcFavBox = nullptr;
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(DefWndProcFavBox, hwnd, msg, wp, lp);
    }

    LRESULT res = 0;
    if (HandleRegisteredMessages(hwnd, msg, wp, lp, res)) {
        return res;
    }

    TreeCtrl* treeCtrl = win->favTreeCtrl;
    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->favLabelWithClose, treeCtrl->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_FAV_LABEL_WITH_CLOSE) {
                ToggleFavorites(win);
            }
            break;
    }
    return CallWindowProc(DefWndProcFavBox, hwnd, msg, wp, lp);
}

// in TableOfContents.cpp
extern void TocTreeCharHandler(CharEvent* ev);
extern void TocTreeMouseWheelHandler(MouseWheelEvent* ev);
extern void TocTreeKeyDown(TreeKeyDownEvent* ev);

HFONT GetTreeFont() {
    int fntSize = GetSizeOfDefaultGuiFont();
    int fntSizeUser = gGlobalPrefs->treeFontSize;
    if (fntSizeUser > 5) {
        fntSize = fntSizeUser;
    }
    HFONT fnt = GetDefaultGuiFontOfSize(fntSize);
    CrashIf(!fnt);
    return fnt;
}

void CreateFavorites(WindowInfo* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU) nullptr, h, nullptr);

    auto* l = new LabelWithCloseWnd();
    l->Create(win->hwndFavBox, IDC_FAV_LABEL_WITH_CLOSE);
    win->favLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    l->SetFont(GetDefaultGuiFont(true, false));
    // label is set in UpdateToolbarSidebarText()

    TreeCtrl* treeCtrl = new TreeCtrl(win->hwndFavBox);

    treeCtrl->fullRowSelect = true;
    treeCtrl->onContextMenu = FavTreeContextMenu;
    treeCtrl->onChar = TocTreeCharHandler;
    treeCtrl->onMouseWheel = TocTreeMouseWheelHandler;
    treeCtrl->onTreeSelectionChanged = FavTreeSelectionChanged;
    treeCtrl->onTreeClick = FavTreeItemClicked;
    treeCtrl->onTreeKeyDown = TocTreeKeyDown;

    // TODO: leaks font?
    HFONT fnt = GetTreeFont();
    treeCtrl->SetFont(fnt);

    bool ok = treeCtrl->Create();
    CrashIf(!ok);

    win->favTreeCtrl = treeCtrl;

    if (nullptr == DefWndProcFavBox) {
        DefWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    UpdateTreeCtrlColors(win);
}
