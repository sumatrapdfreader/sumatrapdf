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
#include "Accelerators.h"
#include "Theme.h"

void RememberFavTreeExpansionStateForAllWindows();

// -------------------------------------------------------------------------
// Custom draw mejorado: soporta hot-tracking y post-paint para selección
// -------------------------------------------------------------------------
static void OnFavCustomDraw(TreeView::CustomDrawEvent* ev) {
    ev->result = CDRF_DODEFAULT;

    NMTVCUSTOMDRAW* tvcd = ev->nm;
    NMCUSTOMDRAW* cd = &tvcd->nmcd;

    if (cd->dwDrawStage == CDDS_PREPAINT) {
        ev->result = CDRF_NOTIFYITEMDRAW;
        return;
    }
    if (cd->dwDrawStage != CDDS_ITEMPREPAINT) {
        return;
    }
    if (!PrettyStyleEnabled()) {
        return;
    }

    bool isSelected = (cd->uItemState & CDIS_SELECTED) != 0;
    bool hasFocus   = (GetFocus() == ev->treeView->hwnd);
    bool isHot      = (cd->uItemState & CDIS_HOT) != 0;

    // Fondo: acento en selección activa, surface en hover/selección sin foco
    if (isSelected && hasFocus) {
        tvcd->clrTextBk = PrettyAccentColor();
    } else if (isSelected || isHot) {
        tvcd->clrTextBk = PrettySurfaceColor();
    } else {
        tvcd->clrTextBk = PrettySurfaceAltColor();
    }

    // Texto: blanco sobre acento, color normal en el resto
    tvcd->clrText = (isSelected && hasFocus)
        ? GetSysColor(COLOR_HIGHLIGHTTEXT)
        : ThemeWindowTextColor();

    // Solicitar notificación post-paint para poder dibujar encima si hace falta
    ev->result = CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT;
}

// -------------------------------------------------------------------------
// Modelo de datos del árbol de favoritos
// -------------------------------------------------------------------------
struct FavTreeItem {
    ~FavTreeItem();

    HTREEITEM hItem   = nullptr;
    FavTreeItem* parent = nullptr;
    char* text        = nullptr;
    bool isExpanded   = false;

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
    return (int)fti->children.size();
}

TreeItem FavTreeModel::ChildAt(TreeItem ti, int idx) {
    auto fti = (FavTreeItem*)ti;
    return (TreeItem)fti->children[idx];
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

// -------------------------------------------------------------------------
// Lógica de favoritos (sin cambios funcionales)
// -------------------------------------------------------------------------
static Favorite* GetFavByMenuId(int menuId, FileState** dsOut) {
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

static FileState* GetByFavorite(Favorite* fn) {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        if (ds->favorites->Contains(fn)) {
            return ds;
        }
    }
    return nullptr;
}

static void ResetFavMenuIds() {
    FileState* ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != nullptr; i++) {
        for (size_t j = 0; j < ds->favorites->size(); j++) {
            ds->favorites->at(j)->menuId = 0;
        }
    }
}

static size_t idxCache = (size_t)-1;

static FileState* GetFavByFilePath(const char* filePath) {
    FileState* fs = gFileHistory.Get(idxCache);
    if (!fs || !str::Eq(fs->filePath, filePath)) {
        fs = gFileHistory.FindByName(filePath, &idxCache);
    }
    return fs;
}

bool IsPageInFavorites(const char* filePath, int pageNo) {
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
    int n = favs->Size();
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
    return na->pageNo - nb->pageNo;
}

static void AddOrReplaceFav(const char* filePath, int pageNo, const char* name, const char* pageLabel) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
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

static void RemoveFav(const char* filePath, int pageNo) {
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

    if (!SettingsRememberOpenedFiles() && 0 == fav->favorites->size()) {
        gFileHistory.Remove(fav);
        DeleteFileState(fav);
    }
}

static void RemoveAllFavForFile(const char* filePath) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return;
    }

    for (size_t i = 0; i < fav->favorites->size(); i++) {
        DeleteFavorite(fav->favorites->at(i));
    }
    fav->favorites->Reset();

    if (!SettingsRememberOpenedFiles()) {
        gFileHistory.Remove(fav);
        DeleteFileState(fav);
    }
}

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

static TempStr FavReadableNameTemp(Favorite* fn) {
    const char* label = fn->pageLabel;
    if (!label) {
        label = str::FormatTemp("%d", fn->pageNo);
    }
    char* res = nullptr;
    if (fn->name) {
        TempStr pageNo = str::FormatTemp(_TRA("(page %s)"), label);
        res = str::JoinTemp(fn->name, " ", pageNo);
    } else {
        res = str::FormatTemp(_TRA("Page %s"), label);
    }
    return res;
}

static TempStr FavCompactReadableNameTemp(FileState* fav, Favorite* fn, bool isCurrent = false) {
    TempStr rn = FavReadableNameTemp(fn);
    if (isCurrent) {
        return str::FormatTemp("%s : %s", _TRA("Current file"), rn);
    }
    TempStr fp = path::GetBaseNameTemp(fav->filePath);
    return str::FormatTemp("%s : %s", fp, rn);
}

static void AppendFavMenuItems(HMENU m, FileState* f, int& idx, bool combined, bool isCurrent) {
    ReportIf(!f);
    if (!f) {
        return;
    }
    for (size_t i = 0; i < f->favorites->size(); i++) {
        if (i >= MAX_FAV_MENUS) {
            return;
        }
        Favorite* fn = f->favorites->at(i);
        fn->menuId = idx++;
        TempStr s;
        if (combined) {
            s = FavCompactReadableNameTemp(f, fn, isCurrent);
        } else {
            s = FavReadableNameTemp(fn);
        }
        auto safeStr = MenuToSafeStringTemp(s);
        TempWStr ws = ToWStrTemp(safeStr);
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
    TempStr base1 = path::GetBaseNameTemp(s1);
    TempStr base2 = path::GetBaseNameTemp(s2);
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
    Sort(&filePathsSortedOut, SortByBaseFileName);
}

static void AppendFavMenus(HMENU m, const char* currFilePath) {
    FileState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = GetFavByFilePath(currFilePath);
    }

    StrVec filePathsSorted;
    if (CanAccessDisk()) {
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && currFileFav->favorites->size() > 0) {
        filePathsSorted.InsertAt(0, currFileFav->filePath);
    }

    if (filePathsSorted.Size() == 0) {
        return;
    }

    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    ResetFavMenuIds();
    int menuId = CmdFavoriteFirst;

    int menusCount = filePathsSorted.Size();
    if (menusCount > MAX_FAV_MENUS) {
        menusCount = MAX_FAV_MENUS;
    }

    for (int i = 0; i < menusCount; i++) {
        const char* filePath = filePathsSorted.At(i);
        FileState* f = GetFavByFilePath(filePath);
        ReportIf(!f);
        if (!f) {
            continue;
        }
        HMENU sub = m;
        bool combined = (f->favorites->Size() == 1);
        if (!combined) {
            sub = CreateMenu();
        }
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            const char* s = _TRA("Current file");
            if (f != currFileFav) {
                s = MenuToSafeStringTemp(path::GetBaseNameTemp(filePath));
            }
            AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, ToWStrTemp(s));
        }
    }
}

void RebuildFavMenu(MainWindow* win, HMENU menu) {
    if (!win->IsDocLoaded()) {
        MenuSetEnabled(menu, CmdFavoriteAdd, false);
        MenuSetEnabled(menu, CmdFavoriteDel, false);
        AppendFavMenus(menu, (const char*)nullptr);
    } else {
        TempStr label = win->ctrl->GetPageLabeTemp(win->currPageNo);
        bool isBookmarked = IsPageInFavorites(win->ctrl->GetFilePath(), win->currPageNo);
        if (isBookmarked) {
            MenuSetEnabled(menu, CmdFavoriteAdd, false);
            TempStr s = str::FormatTemp(_TRA("Remove page %s from favorites"), label);
            MenuSetText(menu, CmdFavoriteDel, s);
        } else {
            MenuSetEnabled(menu, CmdFavoriteDel, false);
            TempStr s = str::FormatTemp(_TRA("Add page %s to favorites"), label);
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

static void GoToFavorite(MainWindow* win, FileState* fs, Favorite* fav) {
    ReportIf(!fs || !fav);
    if (!fs || !fav) {
        return;
    }

    char* fp = fs->filePath;
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

    int pageNo = fav->pageNo;
    FileState* ds = gFileHistory.FindByPath(fs->filePath);
    if (ds && !ds->useDefaultState && gGlobalPrefs->rememberStatePerDocument) {
        ds->pageNo = fav->pageNo;
        ds->scrollPos = PointF(-1, -1);
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
        return;
    }
    FileState* f = GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

static FavTreeItem* MakeFavTopLevelItem(FileState* fs, bool isExpanded) {
    if (!fs->favorites || fs->favorites->Size() == 0) {
        return nullptr;
    }
    auto* res = new FavTreeItem();
    Favorite* fn = fs->favorites->at(0);
    res->favorite = fn;

    bool isCollapsed = fs->favorites->size() == 1;
    if (isCollapsed) {
        isExpanded = false;
    }
    res->isExpanded = isExpanded;

    TempStr text = nullptr;
    if (isCollapsed) {
        text = FavCompactReadableNameTemp(fs, fn);
    } else {
        char* fp = fs->filePath;
        text = path::GetBaseNameTemp(fp);
    }
    res->text = str::Dup(text);
    return res;
}

static void MakeFavSecondLevel(FavTreeItem* parent, FileState* f) {
    size_t n = f->favorites->size();
    for (size_t i = 0; i < n; i++) {
        Favorite* fn = f->favorites->at(i);
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
    for (char* path : filePathsSorted) {
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
        if (fs->favorites->size() > 1) {
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

    TempStr plainLabel = str::FormatTemp("%d", pageNo);
    bool needsLabel = !str::Eq(plainLabel, pageLabel);

    RememberFavTreeExpansionStateForAllWindows();
    const char* pl = nullptr;
    if (needsLabel) {
        pl = pageLabel;
    }
    WindowTab* tab = win->CurrentTab();
    const char* path = tab->filePath;
    AddOrReplaceFav(path, pageNo, name, pl);
    FileState* fav = GetFavByFilePath(path);
    if (fav && fav->favorites->size() == 2) {
        win->expandedFavorites.Append(fav);
    }
    UpdateFavoritesTreeForAllWindows();
    SaveSettings();
}

static void AddFavoriteForPage(MainWindow* win, int pageNo) {
    char* name = nullptr;
    auto tab = win->CurrentTab();
    auto* ctrl = tab->ctrl;
    if (ctrl->HasToc()) {
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

void DelFavorite(const char* filePath, int pageNo) {
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
    for (size_t i = 0; i < gWindows.size(); i++) {
        RememberFavTreeExpansionState(gWindows.at(i));
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

    if (CmdFavoriteDel == cmd) {
        RememberFavTreeExpansionStateForAllWindows();
        FavTreeItem* fti = (FavTreeItem*)ti;
        Favorite* toDelete = fti->favorite;
        FileState* f = GetByFavorite(toDelete);
        char* fp = f->filePath;
        if (fti->parent) {
            RemoveFav(fp, toDelete->pageNo);
        } else {
            RemoveAllFavForFile(fp);
        }
        UpdateFavoritesTreeForAllWindows();
        SaveSettings();
    }
}

// -------------------------------------------------------------------------
// FavBox: encapsula el panel lateral sin subclasificación global
// -------------------------------------------------------------------------

// Eliminamos gWndProcFavBox global. Usamos una clase que almacena el
// WNDPROC original en su instancia para mayor seguridad y encapsulación.
struct FavBox {
    HWND hwnd      = nullptr;
    WNDPROC origProc = nullptr;
    MainWindow* win  = nullptr;

    static FavBox* FromHwnd(HWND hwnd) {
        return (FavBox*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        FavBox* self = FromHwnd(hwnd);
        if (!self) {
            return DefWindowProc(hwnd, msg, wp, lp);
        }

        MainWindow* win = self->win;

        LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
        if (res) {
            return res;
        }

        TreeView* treeView = win->favTreeView;

        switch (msg) {
            case WM_ERASEBKGND:
                return TRUE;

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                COLORREF bgCol = PrettyStyleEnabled()
                    ? PrettySurfaceAltColor()
                    : ThemeControlBackgroundColor();
                AutoDeleteBrush br = CreateSolidBrush(bgCol);
                FillRect(hdc, &ps.rcPaint, br);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_SIZE:
                LayoutTreeContainer(win->favLabelWithClose, treeView->hwnd);
                break;

            case WM_COMMAND:
                if (LOWORD(wp) == IDC_FAV_LABEL_WITH_CLOSE) {
                    ToggleFavorites(win);
                }
                break;

            case WM_DESTROY:
                // Restaurar el WNDPROC original antes de destruir
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)self->origProc);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
                delete self;
                return 0;
        }

        return CallWindowProc(self->origProc, hwnd, msg, wp, lp);
    }

    static FavBox* Install(HWND hwnd, MainWindow* win) {
        auto* self  = new FavBox();
        self->hwnd  = hwnd;
        self->win   = win;
        self->origProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC,  (LONG_PTR)FavBox::WndProc);
        return self;
    }
};

// in TableOfContents.cpp
extern void TocTreeKeyDown2(TreeView::KeyDownEvent*);

// -------------------------------------------------------------------------
// CreateFavorites: creación del panel con estilos modernos del TreeView
// -------------------------------------------------------------------------
void CreateFavorites(MainWindow* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN;
    win->hwndFavBox = CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0,
                                    win->hwndFrame, (HMENU)nullptr, h, nullptr);

    // Header con label+close
    auto l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndFavBox;
        args.cmdId  = IDC_FAV_LABEL_WITH_CLOSE;
        args.font   = GetDefaultGuiFont(true, false);
        args.isRtl  = IsUIRtl();
        l->Create(args);
    }
    win->favLabelWithClose = l;
    l->SetPaddingXY(6, 4);

    // TreeView con estilos modernos
    auto treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent       = win->hwndFavBox;
    args.font         = GetAppTreeFont();
    args.fullRowSelect = true;
    args.exStyle      = 0;
    args.isRtl        = IsUIRtl();

    auto fn = MkFunc1Void(FavTreeContextMenu);
    treeView->onContextMenu      = fn;
    treeView->onSelectionChanged = MkFunc1Void(FavTreeSelectionChanged);
    treeView->onCustomDraw       = MkFunc1Void(OnFavCustomDraw);
    treeView->onKeyDown          = MkFunc1Void(TocTreeKeyDown2);
    treeView->onClick            = MkFunc1Void(FavTreeItemClicked);

    treeView->Create(args);
    ReportIf(!treeView->hwnd);

    // --- Estilos extendidos modernos del TreeView ---
    HWND hTree = treeView->hwnd;

    // Double-buffer elimina parpadeo; fade anima los botones expand/collapse
    DWORD tvExStyle = TVS_EX_FADEINOUTEXPANDOS | TVS_EX_AUTOHSCROLL | TVS_EX_DOUBLEBUFFER;
    TreeView_SetExtendedStyle(hTree, tvExStyle, tvExStyle);

    // Mayor altura de ítem para "breathing room" visual (~28 px escalado por DPI)
    TreeView_SetItemHeight(hTree, DpiScale(hTree, 28));

    // Indentación generosa para jerarquía clara
    TreeView_SetIndent(hTree, DpiScale(hTree, 16));
    // -----------------------------------------------

    win->favTreeView = treeView;

    // Instala FavBox (reemplaza la subclasificación con global gWndProcFavBox)
    FavBox::Install(win->hwndFavBox, win);

    UpdateControlsColors(win);
}
