/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "Dpi.h"
#include "FileUtil.h"
#include "GdiPlusUtil.h"
#include "LabelWithCloseWnd.h"
#include "UITask.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "EngineManager.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "resource.h"
#include "AppPrefs.h"
#include "Favorites.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Tabs.h"
#include "Translations.h"

Favorite *Favorites::GetByMenuId(int menuId, DisplayState **dsOut)
{
    DisplayState *ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != NULL; i++) {
        for (size_t j = 0; j < ds->favorites->Count(); j++) {
            if (menuId == ds->favorites->At(j)->menuId) {
                if (dsOut)
                    *dsOut = ds;
                return ds->favorites->At(j);
            }
        }
    }
    return NULL;
}

DisplayState *Favorites::GetByFavorite(Favorite *fn)
{
    DisplayState *ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != NULL; i++) {
        if (ds->favorites->Contains(fn))
            return ds;
    }
    return NULL;
}

void Favorites::ResetMenuIds()
{
    DisplayState *ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != NULL; i++) {
        for (size_t j = 0; j < ds->favorites->Count(); j++) {
            ds->favorites->At(j)->menuId = 0;
        }
    }
}

DisplayState *Favorites::GetFavByFilePath(const WCHAR *filePath)
{
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    DisplayState *ds = gFileHistory.Get(idxCache);
    if (!ds || !str::Eq(ds->filePath, filePath))
        ds = gFileHistory.Find(filePath, &idxCache);
    return ds;
}

bool Favorites::IsPageInFavorites(const WCHAR *filePath, int pageNo)
{
    DisplayState *fav = GetFavByFilePath(filePath);
    if (!fav)
        return false;
    for (size_t i = 0; i < fav->favorites->Count(); i++) {
        if (pageNo == fav->favorites->At(i)->pageNo)
            return true;
    }
    return false;
}

static Favorite *FindByPage(DisplayState *ds, int pageNo, const WCHAR *pageLabel=NULL)
{
    if (pageLabel) {
        for (size_t i = 0; i < ds->favorites->Count(); i++) {
            if (str::Eq(ds->favorites->At(i)->pageLabel, pageLabel))
                return ds->favorites->At(i);
        }
    }
    for (size_t i = 0; i < ds->favorites->Count(); i++) {
        if (pageNo == ds->favorites->At(i)->pageNo)
            return ds->favorites->At(i);
    }
    return NULL;
}

static int SortByPageNo(const void *a, const void *b)
{
    Favorite *na = *(Favorite **)a;
    Favorite *nb = *(Favorite **)b;
    // sort lower page numbers first
    return na->pageNo - nb->pageNo;
}

void Favorites::AddOrReplace(const WCHAR *filePath, int pageNo, const WCHAR *name, const WCHAR *pageLabel)
{
    DisplayState *fav = GetFavByFilePath(filePath);
    if (!fav) {
        CrashIf(gGlobalPrefs->rememberOpenedFiles);
        fav = NewDisplayState(filePath);
        gFileHistory.Append(fav);
    }

    Favorite *fn = FindByPage(fav, pageNo, pageLabel);
    if (fn) {
        str::ReplacePtr(&fn->name, name);
        CrashIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
    }
    else {
        fn = NewFavorite(pageNo, name, pageLabel);
        fav->favorites->Append(fn);
        fav->favorites->Sort(SortByPageNo);
    }
}

void Favorites::Remove(const WCHAR *filePath, int pageNo)
{
    DisplayState *fav = GetFavByFilePath(filePath);
    if (!fav)
        return;
    Favorite *fn = FindByPage(fav, pageNo);
    if (!fn)
        return;

    fav->favorites->Remove(fn);
    DeleteFavorite(fn);

    if (!gGlobalPrefs->rememberOpenedFiles && 0 == fav->favorites->Count()) {
        gFileHistory.Remove(fav);
        DeleteDisplayState(fav);
    }
}

void Favorites::RemoveAllForFile(const WCHAR *filePath)
{
    DisplayState *fav = GetFavByFilePath(filePath);
    if (!fav)
        return;

    for (size_t i = 0; i < fav->favorites->Count(); i++) {
        DeleteFavorite(fav->favorites->At(i));
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

MenuDef menuDefFavContext[] = {
    { _TRN("Remove from favorites"),        IDM_FAV_DEL,                0 }
};

static bool HasFavorites()
{
    DisplayState *ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != NULL; i++) {
        if (ds->favorites->Count() > 0)
            return true;
    }
    return false;
}

// caller has to free() the result
static WCHAR *FavReadableName(Favorite *fn)
{
    ScopedMem<WCHAR> plainLabel(str::Format(L"%d", fn->pageNo));
    const WCHAR *label = fn->pageLabel ? fn->pageLabel : plainLabel;
    if (fn->name) {
        ScopedMem<WCHAR> pageNo(str::Format(_TR("(page %s)"), label));
        return str::Join(fn->name, L" ", pageNo);
    }
    return str::Format(_TR("Page %s"), label);
}

// caller has to free() the result
static WCHAR *FavCompactReadableName(DisplayState *fav, Favorite *fn, bool isCurrent=false)
{
    ScopedMem<WCHAR> rn(FavReadableName(fn));
    if (isCurrent)
        return str::Format(L"%s : %s", _TR("Current file"), rn.Get());
    const WCHAR *fp = path::GetBaseName(fav->filePath);
    return str::Format(L"%s : %s", fp, rn.Get());
}

static void AppendFavMenuItems(HMENU m, DisplayState *f, UINT& idx, bool combined, bool isCurrent)
{
    for (size_t i = 0; i < f->favorites->Count(); i++) {
        if (i >= MAX_FAV_MENUS)
            return;
        Favorite *fn = f->favorites->At(i);
        fn->menuId = idx++;
        ScopedMem<WCHAR> s;
        if (combined)
            s.Set(FavCompactReadableName(f, fn, isCurrent));
        else
            s.Set(FavReadableName(fn));
        s.Set(win::menu::ToSafeString(s));
        AppendMenu(m, MF_STRING, (UINT_PTR)fn->menuId, s);
    }
}

static int SortByBaseFileName(const void *a, const void *b)
{
    const WCHAR *filePathA = *(const WCHAR **)a;
    const WCHAR *filePathB = *(const WCHAR **)b;
    return str::CmpNatural(path::GetBaseName(filePathA), path::GetBaseName(filePathB));
}

static void GetSortedFilePaths(Vec<const WCHAR *>& filePathsSortedOut, DisplayState *toIgnore=NULL)
{
    DisplayState *ds;
    for (size_t i = 0; (ds = gFileHistory.Get(i)) != NULL; i++) {
        if (ds->favorites->Count() > 0 && ds != toIgnore)
            filePathsSortedOut.Append(ds->filePath);
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
static void AppendFavMenus(HMENU m, const WCHAR *currFilePath)
{
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    DisplayState *currFileFav = NULL;
    if (currFilePath) {
        currFileFav = gFavorites.GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    Vec<const WCHAR *> filePathsSorted;
    if (HasPermission(Perm_DiskAccess)) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && currFileFav->favorites->Count() > 0)
        filePathsSorted.InsertAt(0, currFileFav->filePath);

    if (filePathsSorted.Count() == 0)
        return;

    AppendMenu(m, MF_SEPARATOR, 0, NULL);

    gFavorites.ResetMenuIds();
    UINT menuId = IDM_FAV_FIRST;

    size_t menusCount = filePathsSorted.Count();
    if (menusCount > MAX_FAV_MENUS)
        menusCount = MAX_FAV_MENUS;
    for (size_t i = 0; i < menusCount; i++) {
        const WCHAR *filePath = filePathsSorted.At(i);
        DisplayState *f = gFavorites.GetFavByFilePath(filePath);
        CrashIf(!f);
        HMENU sub = m;
        bool combined = (f->favorites->Count() == 1);
        if (!combined)
            sub = CreateMenu();
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            if (f == currFileFav) {
                AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, _TR("Current file"));
            } else {
                const WCHAR *fileName = path::GetBaseName(filePath);
                ScopedMem<WCHAR> s(win::menu::ToSafeString(fileName));
                AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, s);
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
void RebuildFavMenu(WindowInfo *win, HMENU menu)
{
    if (!win->IsDocLoaded()) {
        win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
        win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
        AppendFavMenus(menu, NULL);
    } else {
        ScopedMem<WCHAR> label(win->ctrl->GetPageLabel(win->currPageNo));
        bool isBookmarked = gFavorites.IsPageInFavorites(win->ctrl->FilePath(), win->currPageNo);
        if (isBookmarked) {
            win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
            ScopedMem<WCHAR> s(str::Format(_TR("Remove page %s from favorites"), label.Get()));
            win::menu::SetText(menu, IDM_FAV_DEL, s);
        } else {
            win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
            ScopedMem<WCHAR> s(str::Format(_TR("Add page %s to favorites"), label.Get()));
            win::menu::SetText(menu, IDM_FAV_ADD, s);
        }
        AppendFavMenus(menu, win->ctrl->FilePath());
    }
    win::menu::SetEnabled(menu, IDM_FAV_TOGGLE, HasFavorites());
}

void ToggleFavorites(WindowInfo *win)
{
    if (gGlobalPrefs->showFavorites) {
        SetSidebarVisibility(win, win->tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->tocVisible, true);
        SetFocus(win->hwndFavTree);
    }
}

static void GoToFavorite(WindowInfo *win, int pageNo) {
    if (!WindowInfoStillValid(win))
        return;
    if (win->IsDocLoaded() && win->ctrl->ValidPageNo(pageNo))
        win->ctrl->GoToPage(pageNo, true);
    // we might have been invoked by clicking on a tree view
    // switch focus so that keyboard navigation works, which enables
    // a fluid experience
    win->Focus();
}

// Going to a bookmark within current file scrolls to a given page.
// Going to a bookmark in another file, loads the file and scrolls to a page
// (similar to how invoking one of the recently opened files works)
static void GoToFavorite(WindowInfo *win, DisplayState *f, Favorite *fn)
{
    assert(f && fn);
    if (!f || !fn) return;

    WindowInfo *existingWin = FindWindowInfoByFile(f->filePath, true);
    if (existingWin) {
        int pageNo = fn->pageNo;
        uitask::Post([=] { GoToFavorite(existingWin, pageNo); });
        return;
    }

    if (!HasPermission(Perm_DiskAccess))
        return;

    // When loading a new document, go directly to selected page instead of
    // first showing last seen page stored in file history
    // A hacky solution because I don't want to add even more parameters to
    // LoadDocument() and LoadDocumentInto()
    int pageNo = fn->pageNo;
    DisplayState *ds = gFileHistory.Find(f->filePath);
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

void GoToFavoriteByMenuId(WindowInfo *win, int wmId)
{
    DisplayState *f;
    Favorite *fn = gFavorites.GetByMenuId(wmId, &f);
    if (fn)
        GoToFavorite(win, f, fn);
}

static void GoToFavForTVItem(WindowInfo* win, HWND hTV, HTREEITEM hItem=NULL)
{
    if (NULL == hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);

    Favorite *fn = (Favorite *)item.lParam;
    if (!fn) {
        // can happen for top-level node which is not associated with a favorite
        // but only serves a parent node for favorites for a given file
        return;
    }
    DisplayState *f = gFavorites.GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

static HTREEITEM InsertFavSecondLevelNode(HWND hwnd, HTREEITEM parent, Favorite *fn)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    tvinsert.itemex.state = 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)fn;
    ScopedMem<WCHAR> s(FavReadableName(fn));
    tvinsert.itemex.pszText = s;
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void InsertFavSecondLevelNodes(HWND hwnd, HTREEITEM parent, DisplayState *f)
{
    for (size_t i = 0; i < f->favorites->Count(); i++) {
        InsertFavSecondLevelNode(hwnd, parent, f->favorites->At(i));
    }
}

static HTREEITEM InsertFavTopLevelNode(HWND hwnd, DisplayState *fav, bool isExpanded)
{
    WCHAR *s = NULL;
    bool collapsed = fav->favorites->Count() == 1;
    if (collapsed)
        isExpanded = false;
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = NULL;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    tvinsert.itemex.state = isExpanded ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = 0;
    if (collapsed) {
        Favorite *fn = fav->favorites->At(0);
        tvinsert.itemex.lParam = (LPARAM)fn;
        s = FavCompactReadableName(fav, fn);
        tvinsert.itemex.pszText = s;
    } else {
        tvinsert.itemex.pszText = (WCHAR*)path::GetBaseName(fav->filePath);
    }
    HTREEITEM ret = TreeView_InsertItem(hwnd, &tvinsert);
    free(s);
    return ret;
}

void PopulateFavTreeIfNeeded(WindowInfo *win)
{
    HWND hwndTree = win->hwndFavTree;
    if (TreeView_GetCount(hwndTree) > 0)
        return;

    Vec<const WCHAR *> filePathsSorted;
    GetSortedFilePaths(filePathsSorted);

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    for (size_t i = 0; i < filePathsSorted.Count(); i++) {
        DisplayState *f = gFavorites.GetFavByFilePath(filePathsSorted.At(i));
        bool isExpanded = win->expandedFavorites.Contains(f);
        HTREEITEM node = InsertFavTopLevelNode(hwndTree, f, isExpanded);
        if (f->favorites->Count() > 1)
            InsertFavSecondLevelNodes(hwndTree, node, f);
    }

    SendMessage(hwndTree, WM_SETREDRAW, TRUE, 0);
    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwndTree, NULL, NULL, fl);
}

static void UpdateFavoritesTreeIfNecessary(WindowInfo *win)
{
    HWND hwndTree = win->hwndFavTree;
    if (0 == TreeView_GetCount(hwndTree))
        return;

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwndTree);
    PopulateFavTreeIfNeeded(win);
}

void UpdateFavoritesTreeForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateFavoritesTreeIfNecessary(gWindows.At(i));
    }

    // hide the favorites tree if we removed the last favorite
    if (!HasFavorites()) {
        gGlobalPrefs->showFavorites = false;
        for (size_t i = 0; i < gWindows.Count(); i++) {
            WindowInfo *win = gWindows.At(i);
            SetSidebarVisibility(win, win->tocVisible, gGlobalPrefs->showFavorites);
        }
    }
}

static DocTocItem *TocItemForPageNo(DocTocItem *item, int pageNo)
{
    DocTocItem *currItem = NULL;

    for (; item; item = item->next) {
        if (1 <= item->pageNo && item->pageNo <= pageNo)
            currItem = item;
        if (item->pageNo >= pageNo)
            break;

        // find any child item closer to the specified page
        DocTocItem *subItem = TocItemForPageNo(item->child, pageNo);
        if (subItem)
            currItem = subItem;
    }

    return currItem;
}

void AddFavorite(WindowInfo *win)
{
    int pageNo = win->currPageNo;
    ScopedMem<WCHAR> name;
    if (win->ctrl->HasTocTree()) {
        // use the current ToC heading as default name
        DocTocItem *root = win->ctrl->GetTocTree();
        DocTocItem *item = TocItemForPageNo(root, pageNo);
        if (item)
            name.Set(str::Dup(item->title));
        delete root;
    }
    ScopedMem<WCHAR> pageLabel(win->ctrl->GetPageLabel(pageNo));

    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageLabel, name);
    if (!shouldAdd)
        return;

    ScopedMem<WCHAR> plainLabel(str::Format(L"%d", pageNo));
    bool needsLabel = !str::Eq(plainLabel, pageLabel);

    RememberFavTreeExpansionStateForAllWindows();
    gFavorites.AddOrReplace(win->loadedFilePath, pageNo, name, needsLabel ? pageLabel.Get() : NULL);
    // expand newly added favorites by default
    DisplayState *fav = gFavorites.GetFavByFilePath(win->loadedFilePath);
    if (fav && fav->favorites->Count() == 2)
        win->expandedFavorites.Append(fav);
    UpdateFavoritesTreeForAllWindows();
    prefs::Save();
}

void DelFavorite(WindowInfo *win)
{
    int pageNo = win->currPageNo;
    const WCHAR *filePath = win->loadedFilePath;
    RememberFavTreeExpansionStateForAllWindows();
    gFavorites.Remove(filePath, pageNo);
    UpdateFavoritesTreeForAllWindows();
    prefs::Save();
}

void RememberFavTreeExpansionState(WindowInfo *win)
{
    win->expandedFavorites.Reset();
    HTREEITEM treeItem = TreeView_GetRoot(win->hwndFavTree);
    while (treeItem) {
        TVITEM item;
        item.hItem = treeItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(win->hwndFavTree, &item);
        if ((item.state & TVIS_EXPANDED) != 0) {
            item.hItem = TreeView_GetChild(win->hwndFavTree, treeItem);
            item.mask = TVIF_PARAM;
            TreeView_GetItem(win->hwndFavTree, &item);
            Favorite *fn = (Favorite *)item.lParam;
            DisplayState *f = gFavorites.GetByFavorite(fn);
            win->expandedFavorites.Append(f);
        }

        treeItem = TreeView_GetNextSibling(win->hwndFavTree, treeItem);
    }
}

void RememberFavTreeExpansionStateForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++) {
        RememberFavTreeExpansionState(gWindows.At(i));
    }
}

static LRESULT OnFavTreeNotify(WindowInfo *win, LPNMTREEVIEW pnmtv)
{
    switch (pnmtv->hdr.code)
    {
        // TVN_SELCHANGED intentionally not implemented (mouse clicks are handled
        // in NM_CLICK, and keyboard navigation in NM_RETURN and TVN_KEYDOWN)

        case TVN_KEYDOWN: {
            TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)pnmtv;
            if (VK_TAB == ptvkd->wVKey) {
                if (win->tabsVisible && IsCtrlPressed())
                    TabsOnCtrlTab(win, IsShiftPressed());
                else
                    AdvanceFocus(win);
                return 1;
            }
            break;
        }

        case NM_CLICK: {
            // Determine which item has been clicked (if any)
            TVHITTESTINFO ht = { 0 };
            DWORD pos = GetMessagePos();
            ht.pt.x = GET_X_LPARAM(pos);
            ht.pt.y = GET_Y_LPARAM(pos);
            MapWindowPoints(HWND_DESKTOP, pnmtv->hdr.hwndFrom, &ht.pt, 1);
            TreeView_HitTest(pnmtv->hdr.hwndFrom, &ht);

            if ((ht.flags & TVHT_ONITEM))
                GoToFavForTVItem(win, pnmtv->hdr.hwndFrom, ht.hItem);
            break;
        }

        case NM_RETURN:
            GoToFavForTVItem(win, pnmtv->hdr.hwndFrom);
            break;

        case NM_CUSTOMDRAW:
            return CDRF_DODEFAULT;
    }
    return -1;
}

static void OnFavTreeContextMenu(WindowInfo *win, PointI pt)
{
    TVITEM item;
    if (pt.x != -1 || pt.y != -1) {
        TVHITTESTINFO ht = { 0 };
        ht.pt.x = pt.x;
        ht.pt.y = pt.y;

        MapWindowPoints(HWND_DESKTOP, win->hwndFavTree, &ht.pt, 1);
        TreeView_HitTest(win->hwndFavTree, &ht);
        if ((ht.flags & TVHT_ONITEM) == 0)
            return; // only display menu if over a node in tree

        TreeView_SelectItem(win->hwndFavTree, ht.hItem);
        item.hItem = ht.hItem;
    }
    else {
        item.hItem = TreeView_GetSelection(win->hwndFavTree);
        if (!item.hItem)
            return;
        RECT rcItem;
        if (TreeView_GetItemRect(win->hwndFavTree, item.hItem, &rcItem, TRUE)) {
            MapWindowPoints(win->hwndFavTree, HWND_DESKTOP, (POINT *)&rcItem, 2);
            pt.x = rcItem.left;
            pt.y = rcItem.bottom;
        }
        else {
            WindowRect rc(win->hwndFavTree);
            pt = rc.TL();
        }
    }

    item.mask = TVIF_PARAM;
    TreeView_GetItem(win->hwndFavTree, &item);
    Favorite *toDelete = (Favorite *)item.lParam;

    HMENU popup = BuildMenuFromMenuDef(menuDefFavContext, dimof(menuDefFavContext), CreatePopupMenu());

    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, win->hwndFavTree, NULL);
    DestroyMenu(popup);
    if (IDM_FAV_DEL == cmd) {
        RememberFavTreeExpansionStateForAllWindows();
        if (toDelete) {
            DisplayState *f = gFavorites.GetByFavorite(toDelete);
            gFavorites.Remove(f->filePath, toDelete->pageNo);
        } else {
            // toDelete == NULL => this is a parent node signifying all bookmarks in a file
            item.hItem = TreeView_GetChild(win->hwndFavTree, item.hItem);
            item.mask = TVIF_PARAM;
            TreeView_GetItem(win->hwndFavTree, &item);
            toDelete = (Favorite *)item.lParam;
            DisplayState *f = gFavorites.GetByFavorite(toDelete);
            gFavorites.RemoveAllForFile(f->filePath);
        }
        UpdateFavoritesTreeForAllWindows();
        prefs::Save();

        // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
        // so that we can do destructive operations without asking for permission via
        // invasive model dialog boxes but also allow reverting them if were done
        // by mistake
    }
}

static WNDPROC DefWndProcFavTree = NULL;
static LRESULT CALLBACK WndProcFavTree(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcFavTree, hwnd, msg, wParam, lParam);

    switch (msg) {

        case WM_ERASEBKGND:
            return FALSE;

        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs->escToExit && MayCloseWindow(win))
                CloseWindow(win, true);
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->hwndFavTree))
                return SendMessage(win->hwndCanvas, msg, wParam, lParam);
            break;
    }

    return CallWindowProc(DefWndProcFavTree, hwnd, msg, wParam, lParam);
}

static WNDPROC DefWndProcFavBox = NULL;
static LRESULT CALLBACK WndProcFavBox(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
    switch (message) {

        case WM_SIZE:
            LayoutTreeContainer(win->favLabelWithClose, win->hwndFavTree);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_FAV_LABEL_WITH_CLOSE)
                ToggleFavorites(win);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_FAV_TREE) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
                LRESULT res = OnFavTreeNotify(win, pnmtv);
                if (res != -1)
                    return res;
            }
            break;

        case WM_CONTEXTMENU:
            if (win->hwndFavTree == (HWND)wParam) {
                OnFavTreeContextMenu(win, PointI(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
                return 0;
            }
            break;

    }
    return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
}

void CreateFavorites(WindowInfo *win)
{
    win->hwndFavBox = CreateWindow(WC_STATIC, L"", WS_CHILD|WS_CLIPCHILDREN,
                                   0, 0, gGlobalPrefs->sidebarDx, 0,
                                   win->hwndFrame, (HMENU)0, GetModuleHandle(NULL), NULL);
    LabelWithCloseWnd *l =  CreateLabelWithCloseWnd(win->hwndFavBox, IDC_FAV_LABEL_WITH_CLOSE);
    win->favLabelWithClose = l;
    int padXY = DpiScaleX(win->hwndFrame, 2);
    SetPaddingXY(l, padXY, padXY);
    SetFont(l, GetDefaultGuiFont());
    // label is set in UpdateSidebarTitles()

    win->hwndFavTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, L"Fav",
                                      TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                                      TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                                      WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                      0, 0, 0, 0, win->hwndFavBox, (HMENU)IDC_FAV_TREE, GetModuleHandle(NULL), NULL);

    TreeView_SetUnicodeFormat(win->hwndFavTree, true);

    if (NULL == DefWndProcFavTree)
        DefWndProcFavTree = (WNDPROC)GetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC, (LONG_PTR)WndProcFavTree);

    if (NULL == DefWndProcFavBox)
        DefWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);
}
