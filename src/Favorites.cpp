/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"

#include "wingui/Wingui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/TreeCtrl.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Flags.h"
#include "AppPrefs.h"
#include "Favorites.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Tabs.h"
#include "Translations.h"

struct FavTreeItem : public TreeItem {
    ~FavTreeItem() override;

    // TODO: convert to char*
    WCHAR* Text() override;
    TreeItem* Parent() override;
    int ChildCount() override;
    TreeItem* ChildAt(int index) override;
    // true if this tree item should be expanded i.e. showing children
    bool IsExpanded() override;
    // when showing checkboxes
    bool IsChecked() override;

    FavTreeItem* parent = nullptr;
    WCHAR* text = nullptr;
    bool isExpanded = false;

    // not owned by us
    Favorite* favorite = nullptr;

    Vec<FavTreeItem*> children;
};

FavTreeItem::~FavTreeItem() {
    free(text);
    DeleteVecMembers(children);
}

WCHAR* FavTreeItem::Text() {
    return text;
}

TreeItem* FavTreeItem::Parent() {
    return nullptr;
}

int FavTreeItem::ChildCount() {
    size_t n = children.size();
    return (int)n;
}

TreeItem* FavTreeItem::ChildAt(int index) {
    return children[index];
}

bool FavTreeItem::IsExpanded() {
    return isExpanded;
}

bool FavTreeItem::IsChecked() {
    return false;
}

struct FavTreeModel : public TreeModel {
    ~FavTreeModel() override;

    int RootCount() override;
    TreeItem* RootAt(int) override;

    Vec<FavTreeItem*> children;
};

FavTreeModel::~FavTreeModel() {
    DeleteVecMembers(children);
}

int FavTreeModel::RootCount() {
    size_t n = children.size();
    return (int)n;
}

TreeItem* FavTreeModel::RootAt(int n) {
    return children[n];
}

Favorite* Favorites::GetByMenuId(int menuId, DisplayState** dsOut) {
    DisplayState* ds;
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

DisplayState* Favorites::GetByFavorite(Favorite* fn) {
    DisplayState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->Contains(fn)) {
            return ds;
        }
    }
    return nullptr;
}

void Favorites::ResetMenuIds() {
    DisplayState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        for (size_t j = 0; j < ds->favorites->size(); j++) {
            ds->favorites->at(j)->menuId = 0;
        }
    }
}

DisplayState* Favorites::GetFavByFilePath(const WCHAR* filePath) {
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    DisplayState* ds = gFileHistory.Get(idxCache);
    if (!ds || !str::Eq(ds->filePath, filePath)) {
        ds = gFileHistory.Find(filePath, &idxCache);
    }
    return ds;
}

bool Favorites::IsPageInFavorites(const WCHAR* filePath, int pageNo) {
    DisplayState* fav = GetFavByFilePath(filePath);
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

static Favorite* FindByPage(DisplayState* ds, int pageNo, const WCHAR* pageLabel = nullptr) {
    if (pageLabel) {
        for (size_t i = 0; i < ds->favorites->size(); i++) {
            if (str::Eq(ds->favorites->at(i)->pageLabel, pageLabel)) {
                return ds->favorites->at(i);
            }
        }
    }
    for (size_t i = 0; i < ds->favorites->size(); i++) {
        if (pageNo == ds->favorites->at(i)->pageNo) {
            return ds->favorites->at(i);
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

void Favorites::AddOrReplace(const WCHAR* filePath, int pageNo, const WCHAR* name, const WCHAR* pageLabel) {
    DisplayState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        CrashIf(gGlobalPrefs->rememberOpenedFiles);
        fav = NewDisplayState(filePath);
        gFileHistory.Append(fav);
    }

    Favorite* fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        str::ReplacePtr(&fn->name, name);
        CrashIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
    } else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        fav->favorites->Sort(SortByPageNo);
    }
}

void Favorites::Remove(const WCHAR* filePath, int pageNo) {
    DisplayState* fav = GetFavByFilePath(filePath);
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
    DisplayState* fav = GetFavByFilePath(filePath);
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

MenuDef menuDefFavContext[] = {{_TRN("Remove from favorites"), IDM_FAV_DEL, 0}};

bool HasFavorites() {
    DisplayState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->size() > 0) {
            return true;
        }
    }
    return false;
}

// caller has to free() the result
static WCHAR* FavReadableName(Favorite* fn) {
    AutoFreeWstr plainLabel(str::Format(L"%d", fn->pageNo));
    const WCHAR* label = fn->pageLabel ? fn->pageLabel : plainLabel;
    if (fn->name) {
        AutoFreeWstr pageNo(str::Format(_TR("(page %s)"), label));
        return str::Join(fn->name, L" ", pageNo);
    }
    return str::Format(_TR("Page %s"), label);
}

// caller has to free() the result
static WCHAR* FavCompactReadableName(DisplayState* fav, Favorite* fn, bool isCurrent = false) {
    AutoFreeWstr rn(FavReadableName(fn));
    if (isCurrent) {
        return str::Format(L"%s : %s", _TR("Current file"), rn.Get());
    }
    const WCHAR* fp = path::GetBaseNameNoFree(fav->filePath);
    return str::Format(L"%s : %s", fp, rn.Get());
}

static void AppendFavMenuItems(HMENU m, DisplayState* f, UINT& idx, bool combined, bool isCurrent) {
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
    const WCHAR* baseA = path::GetBaseNameNoFree(filePathA);
    const WCHAR* baseB = path::GetBaseNameNoFree(filePathB);
    return str::CmpNatural(baseA, baseB);
}

static void GetSortedFilePaths(Vec<const WCHAR*>& filePathsSortedOut, DisplayState* toIgnore = nullptr) {
    DisplayState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->size() > 0 && ds != toIgnore) {
            filePathsSortedOut.Append(ds->filePath);
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
    DisplayState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = gFavorites.GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    Vec<const WCHAR*> filePathsSorted;
    if (HasPermission(Perm_DiskAccess)) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && currFileFav->favorites->size() > 0) {
        filePathsSorted.InsertAt(0, currFileFav->filePath);
    }

    if (filePathsSorted.size() == 0) {
        return;
    }

    AppendMenu(m, MF_SEPARATOR, 0, nullptr);

    gFavorites.ResetMenuIds();
    UINT menuId = IDM_FAV_FIRST;

    size_t menusCount = filePathsSorted.size();
    if (menusCount > MAX_FAV_MENUS) {
        menusCount = MAX_FAV_MENUS;
    }

    for (size_t i = 0; i < menusCount; i++) {
        const WCHAR* filePath = filePathsSorted.at(i);
        DisplayState* f = gFavorites.GetFavByFilePath(filePath);
        CrashIf(!f);
        HMENU sub = m;
        bool combined = (f->favorites->size() == 1);
        if (!combined) {
            sub = CreateMenu();
        }
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            if (f == currFileFav) {
                AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, _TR("Current file"));
            } else {
                AutoFreeWstr tmp;
                tmp.SetCopy(path::GetBaseNameNoFree(filePath));
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
        win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
        win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
        AppendFavMenus(menu, nullptr);
    } else {
        AutoFreeWstr label(win->ctrl->GetPageLabel(win->currPageNo));
        bool isBookmarked = gFavorites.IsPageInFavorites(win->ctrl->FilePath(), win->currPageNo);
        if (isBookmarked) {
            win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
            AutoFreeWstr s(str::Format(_TR("Remove page %s from favorites"), label.Get()));
            win::menu::SetText(menu, IDM_FAV_DEL, s);
        } else {
            win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
            AutoFreeWstr s(str::Format(_TR("Add page %s to favorites\tCtrl+B"), label.Get()));
            win::menu::SetText(menu, IDM_FAV_ADD, s);
        }
        AppendFavMenus(menu, win->ctrl->FilePath());
    }
    win::menu::SetEnabled(menu, IDM_FAV_TOGGLE, HasFavorites());
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
static void GoToFavorite(WindowInfo* win, DisplayState* f, Favorite* fn) {
    CrashIf(!f || !fn);
    if (!f || !fn) {
        return;
    }

    WindowInfo* existingWin = FindWindowInfoByFile(f->filePath, true);
    if (existingWin) {
        int pageNo = fn->pageNo;
        uitask::Post([=] { GoToFavorite(existingWin, pageNo); });
        return;
    }

    if (!HasPermission(Perm_DiskAccess)) {
        return;
    }

    // When loading a new document, go directly to selected page instead of
    // first showing last seen page stored in file history
    // A hacky solution because I don't want to add even more parameters to
    // LoadDocument() and LoadDocumentInto()
    int pageNo = fn->pageNo;
    DisplayState* ds = gFileHistory.Find(f->filePath, nullptr);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fn->pageNo;
        ds->scrollPos = PointI(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    LoadArgs args(f->filePath, win);
    win = LoadDocument(args);
    if (win) {
        uitask::Post([=] { (win, pageNo); });
    }
}

void GoToFavoriteByMenuId(WindowInfo* win, int wmId) {
    DisplayState* f;
    Favorite* fn = gFavorites.GetByMenuId(wmId, &f);
    if (fn) {
        GoToFavorite(win, f, fn);
    }
}

static void GoToFavForTreeItem(WindowInfo* win, TreeItem* ti) {
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
    DisplayState* f = gFavorites.GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

#if 0
static void GoToFavForTVItem(WindowInfo* win, TreeCtrl* treeCtrl, HTREEITEM hItem = nullptr) {
    TreeItem* ti = nullptr;
    if (nullptr == hItem) {
        ti = treeCtrl->GetSelection();
    } else {
        ti = treeCtrl->GetTreeItemByHandle(hItem);
    }
    GoToFavForTreeItem(win, ti);
}
#endif

static FavTreeItem* MakeFavTopLevelItem(DisplayState* fav, bool isExpanded) {
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
        res->text = str::Dup(path::GetBaseNameNoFree(fav->filePath));
    }
    return res;
}

static void MakeFavSecondLevel(FavTreeItem* parent, DisplayState* f) {
    size_t n = f->favorites->size();
    for (size_t i = 0; i < n; i++) {
        Favorite* fn = f->favorites->at(i);
        auto* ti = new FavTreeItem();
        ti->text = FavReadableName(fn);
        ti->parent = parent;
        ti->favorite = fn;
        parent->children.push_back(ti);
    }
}

static FavTreeModel* BuildFavTreeModel(WindowInfo* win) {
    auto* res = new FavTreeModel();
    Vec<const WCHAR*> filePathsSorted;
    GetSortedFilePaths(filePathsSorted);
    for (size_t i = 0; i < filePathsSorted.size(); i++) {
        DisplayState* f = gFavorites.GetFavByFilePath(filePathsSorted.at(i));
        bool isExpanded = win->expandedFavorites.Contains(f);
        FavTreeItem* ti = MakeFavTopLevelItem(f, isExpanded);
        res->children.push_back(ti);
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
    if (0 == newModel->RootCount()) {
        SetSidebarVisibility(win, win->tocVisible, false);
    }
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
    DisplayState* fav = gFavorites.GetFavByFilePath(tab->filePath);
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
    win->expandedFavorites.clear();
    TreeCtrl* treeCtrl = win->favTreeCtrl;
    TreeModel* tm = treeCtrl ? treeCtrl->treeModel : nullptr;
    if (!tm) {
        // TODO: remember all favorites as expanded
        return;
    }
    int n = tm->RootCount();
    for (int i = 0; i < n; i++) {
        TreeItem* ti = tm->RootAt(i);
        bool isExpanded = treeCtrl->IsExpanded(ti);
        if (isExpanded) {
            FavTreeItem* fti = (FavTreeItem*)ti;
            Favorite* fn = fti->favorite;
            DisplayState* f = gFavorites.GetByFavorite(fn);
            win->expandedFavorites.push_back(f);
        }
    }
}

void RememberFavTreeExpansionStateForAllWindows() {
    for (size_t i = 0; i < gWindows.size(); i++) {
        RememberFavTreeExpansionState(gWindows.at(i));
    }
}

static void FavTreeSelectionChanged(TreeSelectionChangedArgs* args) {
    WindowInfo* win = FindWindowInfoByHwnd(args->w->hwnd);
    CrashIf(!win);

    // When the focus is set to the toc window the first item in the treeview is automatically
    // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action ==
    // 0x00001000. We have to ignore this message to prevent the current page to be changed.
    // The case pnmtv->action==TVC_UNKNOWN is ignored because
    // it corresponds to a notification sent by
    // the function TreeView_DeleteAllItems after deletion of the item.
    bool shouldHandle = args->byKeyboard || args->byMouse;
    if (!shouldHandle) {
        return;
    }
    bool allowExternal = args->byMouse;
    GoToFavForTreeItem(win, args->selectedItem);
    args->didHandle = true;
}

// if context menu invoked via keyboard, get selected item
// if via right-click, selects the item under the cursor
// in both cases can return null
// sets pt to screen position (for context menu coordinates)
TreeItem* GetOrSelectTreeItemAtPos(ContextMenuArgs* args, POINT& pt) {
    TreeCtrl* treeCtrl = (TreeCtrl*)args->w;
    HWND hwnd = treeCtrl->hwnd;

    TreeItem* ti = nullptr;
    pt = {args->mouseWindow.x, args->mouseWindow.y};
    if (pt.x == -1 || pt.y == -1) {
        // no mouse position when launched via keyboard shortcut
        // use position of selected item to show menu
        ti = treeCtrl->GetSelection();
        if (!ti) {
            return nullptr;
        }
        RECT rcItem;
        if (treeCtrl->GetItemRect(ti, true, rcItem)) {
            // rcItem is local to window, map to global screen position
            MapWindowPoints(hwnd, HWND_DESKTOP, (POINT*)&rcItem, 2);
            pt.x = rcItem.left;
            pt.y = rcItem.bottom;
        }
    } else {
        ti = treeCtrl->HitTest(pt.x, pt.y);
        if (!ti) {
            // only show context menu if over a node in tree
            return nullptr;
        }
        // context menu acts on this item so select it
        // for better visual feedback to the user
        treeCtrl->SelectItem(ti);
        pt.x = args->mouseGlobal.x;
        pt.y = args->mouseGlobal.y;
    }
    return ti;
}

static void FavTreeContextMenu(ContextMenuArgs* args) {
    args->didHandle = true;

    TreeCtrl* treeCtrl = (TreeCtrl*)args->w;
    CrashIf(!IsTree(treeCtrl->kind));
    HWND hwnd = treeCtrl->hwnd;
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);

    POINT pt{};
    TreeItem* ti = GetOrSelectTreeItemAtPos(args, pt);
    if (!ti) {
        return;
    }
    HMENU popup = BuildMenuFromMenuDef(menuDefFavContext, dimof(menuDefFavContext), CreatePopupMenu());
    MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, hwnd, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);

    // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
    // so that we can do destructive operations without asking for permission via
    // invasive model dialog boxes but also allow reverting them if were done
    // by mistake
    if (IDM_FAV_DEL == cmd) {
        RememberFavTreeExpansionStateForAllWindows();
        FavTreeItem* fti = (FavTreeItem*)ti;
        Favorite* toDelete = fti->favorite;
        if (fti->parent) {
            DisplayState* f = gFavorites.GetByFavorite(toDelete);
            gFavorites.Remove(f->filePath, toDelete->pageNo);
        } else {
            // this is a top-level node which represents all bookmarks for a given file
            DisplayState* f = gFavorites.GetByFavorite(toDelete);
            gFavorites.RemoveAllForFile(f->filePath);
        }
        UpdateFavoritesTreeForAllWindows();
        prefs::Save();
    }
}

static WNDPROC DefWndProcFavBox = nullptr;
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
    }

    TreeCtrl* treeCtrl = win->favTreeCtrl;
    switch (message) {
        case WM_SIZE:
            LayoutTreeContainer(win->favLabelWithClose, nullptr, treeCtrl->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_FAV_LABEL_WITH_CLOSE) {
                ToggleFavorites(win);
            }
            break;
    }
    return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
}

// in TableOfContents.cpp
extern void TocTreeCharHandler(CharArgs* args);
extern void TocTreeMouseWheelHandler(MouseWheelArgs* args);
extern void TocTreeKeyDown(TreeKeyDownArgs* args);

void CreateFavorites(WindowInfo* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU)0, h, nullptr);

    auto* l = new LabelWithCloseWnd();
    l->Create(win->hwndFavBox, IDC_FAV_LABEL_WITH_CLOSE);
    win->favLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    l->SetFont(GetDefaultGuiFont());
    // label is set in UpdateToolbarSidebarText()

    TreeCtrl* treeCtrl = new TreeCtrl(win->hwndFavBox);

    treeCtrl->onContextMenu = FavTreeContextMenu;
    treeCtrl->onChar = TocTreeCharHandler;
    treeCtrl->onMouseWheel = TocTreeMouseWheelHandler;
    treeCtrl->onTreeSelectionChanged = FavTreeSelectionChanged;
    treeCtrl->onTreeKeyDown = TocTreeKeyDown;

    bool ok = treeCtrl->Create(L"Fav");
    CrashIf(!ok);

    win->favTreeCtrl = treeCtrl;

    if (nullptr == DefWndProcFavBox) {
        DefWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    UpdateTreeCtrlColors(win);
}
