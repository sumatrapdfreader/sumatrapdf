/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "Favorites.h"
#include "SumatraPDF.h"
#include "translations.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "resource.h"
#include "WindowInfo.h"
#include "FileHistory.h"
#include "AppTools.h"
#include "SumatraDialogs.h"
#include "Menu.h"

int FileFavs::FindByPage(int pageNo) const
{
    for (size_t i = 0; i < favNames.Count(); i++)
        if (favNames.At(i)->pageNo == pageNo)
            return (int)i;
    return -1;
}

int FileFavs::SortByPageNo(const void *a, const void *b)
{
    FavName *na = *(FavName **)a;
    FavName *nb = *(FavName **)b;
    // sort lower page numbers first
    return na->pageNo - nb->pageNo;
}

void FileFavs::ResetMenuIds()
{
    for (size_t i =0; i < favNames.Count(); i++) {
        FavName *fn = favNames.At(i);
        fn->menuId = 0;
    }
}

bool FileFavs::GetByMenuId(int menuId, size_t& idx)
{
    for (size_t i = 0; i < favNames.Count(); i++) {
        FavName *fn = favNames.At(i);
        if (fn->menuId == menuId) {
            idx = i;
            return true;
        }
    }
    return false;
}

bool FileFavs::HasFavName(FavName *fn)
{
    for (size_t i = 0; i < favNames.Count(); i++)
        if (fn == favNames.At(i))
            return true;
    return false;

}

bool FileFavs::Remove(int pageNo)
{
    int idx = FindByPage(pageNo);
    if (-1 == idx)
        return false;

    delete favNames.At(idx);
    favNames.RemoveAt(idx);
    return true;
}

void FileFavs::AddOrReplace(int pageNo, const TCHAR *name)
{
    int idx = FindByPage(pageNo);
    if (idx != -1) {
        favNames.At(idx)->SetName(name);
        return;
    }
    FavName *fn = new FavName(pageNo, name);
    favNames.Append(fn);
    favNames.Sort(SortByPageNo);
}

void Favorites::RemoveFav(FileFavs *fav, size_t idx)
{
    favs.RemoveAt(idx);
    delete fav;
    filePathCache = NULL;
    idxCache = (size_t)-1;
}

FileFavs *Favorites::GetByMenuId(int menuId, size_t& idx)
{
    for (size_t i = 0; i < favs.Count(); i++) {
        FileFavs *fav = favs.At(i);
        if (fav->GetByMenuId(menuId, idx))
            return fav;
    }
    return NULL;
}

FileFavs *Favorites::GetByFavName(FavName *fn)
{
    for (size_t i = 0; i < favs.Count(); i++) {
        FileFavs *fav = favs.At(i);
        if (fav->HasFavName(fn))
            return fav;
    }
    return NULL;
}

void Favorites::ResetMenuIds()
{
    for (size_t i = 0; i < favs.Count(); i++) {
        FileFavs *fav = favs.At(i);
        fav->ResetMenuIds();
    }
}

FileFavs *Favorites::GetFavByFilePath(const TCHAR *filePath, bool createIfNotExist, size_t *idx)
{
    // it's likely that we'll ask about the info for the same
    // file as in previous call, so use one element cache
    FileFavs *fav = NULL;
    bool found = false;
    if (str::Eq(filePath, filePathCache)) {
        fav = favs.At(idxCache);
        found = true;
    } else {
        for (size_t i = 0; i < favs.Count(); i++) {
            fav = favs.At(i);
            if (str::Eq(filePath, fav->filePath)) {
                idxCache = i;
                filePathCache = fav->filePath;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        if (!createIfNotExist)
            return NULL;
        fav = new FileFavs(filePath);
        favs.Append(fav);
        filePathCache = fav->filePath;
        idxCache = favs.Count() - 1;
    }
    if (idx)
        *idx = idxCache;
    assert(fav != NULL);
    return fav;
}

bool Favorites::IsPageInFavorites(const TCHAR *filePath, int pageNo) 
{
    FileFavs *fav = GetFavByFilePath(filePath);
    if (!fav)
        return false;
    return fav->Exists(pageNo);
}

void Favorites::AddOrReplace(const TCHAR *filePath, int pageNo, const TCHAR *name)
{
    FileFavs *fav = GetFavByFilePath(filePath, true);
    fav->AddOrReplace(pageNo, name);
}

void Favorites::Remove(const TCHAR *filePath, int pageNo)
{
    size_t idx;
    FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
    if (!fav)
        return;
    fav->Remove(pageNo);
    if (fav->IsEmpty())
        RemoveFav(fav, idx);
}

void Favorites::RemoveAllForFile(const TCHAR *filePath)
{
    size_t idx;
    FileFavs *fav = GetFavByFilePath(filePath, false, &idx);
    if (fav)
        RemoveFav(fav, idx);
}
// Note: those might be too big
#define MAX_FAV_SUBMENUS 10
#define MAX_FAV_MENUS 10

MenuDef menuDefFavContext[] = {
    { _TRN("Remove from favorites"),        IDM_FAV_DEL,                0 }
};

// caller has to free() the result
static TCHAR *FavReadableName(FavName *fn)
{
    // TODO: save non-default page labels (cf. BaseEngine::GetPageLabel)
    ScopedMem<TCHAR> label(str::Format(_T("%d"), fn->pageNo));
    if (fn->name) {
        ScopedMem<TCHAR> pageNo(str::Format(_TR("(page %s)"), label));
        return str::Join(fn->name, _T(" "), pageNo);
    }
    return str::Format(_TR("Page %s"), label);
}

// caller has to free() the result
static TCHAR *FavCompactReadableName(FileFavs *fav, FavName *fn, bool isCurrent=false)
{
    ScopedMem<TCHAR> rn(FavReadableName(fn));
    if (isCurrent)
        return str::Format(_T("%s : %s"), _TR("Current file"), rn);
    const TCHAR *fp = path::GetBaseName(fav->filePath);
    return str::Format(_T("%s : %s"), fp, rn);
}

static void AppendFavMenuItems(HMENU m, FileFavs *f, UINT& idx, bool combined, bool isCurrent)
{
    size_t items = f->favNames.Count();
    if (items > MAX_FAV_MENUS) {
        items = MAX_FAV_MENUS;
    }
    for (size_t i = 0; i < items; i++) {
        FavName *fn = f->favNames.At(i);
        fn->menuId = idx++;
        ScopedMem<TCHAR> s;
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
    TCHAR *filePathA = *(TCHAR**)a;
    TCHAR *filePathB = *(TCHAR**)b;
    return str::CmpNatural(path::GetBaseName(filePathA), path::GetBaseName(filePathB));
}

static void GetSortedFilePaths(Favorites *favorites, Vec<TCHAR*>& filePathsSortedOut, FileFavs *toIgnore)
{
    for (size_t i = 0; i < favorites->favs.Count(); i++) {
        FileFavs *f = favorites->favs.At(i);
        if (f != toIgnore)
            filePathsSortedOut.Append(f->filePath);
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
static void AppendFavMenus(HMENU m, const TCHAR *currFilePath)
{
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    FileFavs *currFileFav = NULL;
    if (NULL != currFilePath) {
        currFileFav = gFavorites->GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    Vec<TCHAR*> filePathsSorted;
    if (HasPermission(Perm_DiskAccess)) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(gFavorites, filePathsSorted, currFileFav);
    }
    if (currFileFav)
        filePathsSorted.InsertAt(0, currFileFav->filePath);

    if (filePathsSorted.Count() == 0)
        return;

    AppendMenu(m, MF_SEPARATOR, 0, NULL);

    gFavorites->ResetMenuIds();
    UINT menuId = IDM_FAV_FIRST;

    size_t menusCount = filePathsSorted.Count();
    if (menusCount > MAX_FAV_MENUS)
        menusCount = MAX_FAV_MENUS;
    for (size_t i = 0; i < menusCount; i++) {
        TCHAR *filePath = filePathsSorted.At(i);
        const TCHAR *fileName = path::GetBaseName(filePath);
        FileFavs *f = gFavorites->GetFavByFilePath(filePath);
        HMENU sub = m;
        bool combined = (f->favNames.Count() == 1);
        if (!combined)
            sub = CreateMenu();
        AppendFavMenuItems(sub, f, menuId, combined, f == currFileFav);
        if (!combined) {
            if (f == currFileFav) {
                AppendMenu(m, MF_POPUP | MF_STRING, (UINT_PTR)sub, _TR("Current file"));
            } else {
                ScopedMem<TCHAR> s(win::menu::ToSafeString(fileName));
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
static void RebuildFavMenu(TCHAR *filePath, int pageNo, HMENU menu, BaseEngine *engine=NULL)
{
    win::menu::Empty(menu);
    BuildMenuFromMenuDef(menuDefFavorites, 3, menu);
    if (!filePath) {
        win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
        win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
        AppendFavMenus(menu, NULL);
    } else {
        ScopedMem<TCHAR> label(engine ? engine->GetPageLabel(pageNo) : str::Format(_T("%d"), pageNo));
        bool isBookmarked = gFavorites->IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            win::menu::SetEnabled(menu, IDM_FAV_ADD, false);
            ScopedMem<TCHAR> s(str::Format(_TR("Remove page %s from favorites"), label));
            win::menu::SetText(menu, IDM_FAV_DEL, s);
        } else {
            win::menu::SetEnabled(menu, IDM_FAV_DEL, false);
            ScopedMem<TCHAR> s(str::Format(_TR("Add page %s to favorites"), label));
            win::menu::SetText(menu, IDM_FAV_ADD, s);
        }
        AppendFavMenus(menu, filePath);
    }
    win::menu::SetEnabled(menu, IDM_FAV_TOGGLE, HasFavorites());
}

void RebuildFavMenu(WindowInfo *win, HMENU menu)
{
    if (win->IsDocLoaded()) {
        RebuildFavMenu(win->loadedFilePath, win->currPageNo, menu, win->dm->engine);
    } else {
        RebuildFavMenu(NULL, 0, menu);
    }
}

void ToggleFavorites(WindowInfo *win)
{
    if (gGlobalPrefs.favVisible) {
        SetSidebarVisibility(win, win->tocVisible, false);
    } else {
        SetSidebarVisibility(win, win->tocVisible, true);
        SetFocus(win->hwndFavTree);
    }
}

class GoToFavoriteWorkItem : public UIThreadWorkItem
{
    int pageNo;

public:
    GoToFavoriteWorkItem(WindowInfo *win, int pageNo = -1) :
        UIThreadWorkItem(win), pageNo(pageNo) {}

    virtual void Execute() {
        if (!WindowInfoStillValid(win))
            return;
        SetForegroundWindow(win->hwndFrame);
        if (win->IsDocLoaded()) {
            if (-1 != pageNo)
                win->dm->GoToPage(pageNo, 0, true);
            // we might have been invoked by clicking on a tree view
            // switch focus so that keyboard navigation works, which enables
            // a fluid experience
            SetFocus(win->hwndFrame);
        }
    }
};

// Going to a bookmark within current file scrolls to a given page.
// Going to a bookmark in another file, loads the file and scrolls to a page
// (similar to how invoking one of the recently opened files works)
static void GoToFavorite(WindowInfo *win, FileFavs *f, FavName *fn)
{
    assert(f && fn);
    if (!f || !fn) return;

    WindowInfo *existingWin = FindWindowInfoByFile(f->filePath);
    if (existingWin) {
        QueueWorkItem(new GoToFavoriteWorkItem(existingWin, fn->pageNo));
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
    if (ds && !ds->useGlobalValues && !gGlobalPrefs.globalPrefsOnly) {
        ds->pageNo = fn->pageNo;
        ds->scrollPos = PointI(-1, -1); // don't scroll the page
        pageNo = -1;
    }

    win = LoadDocument(f->filePath, win);
    if (win)
        QueueWorkItem(new GoToFavoriteWorkItem(win, pageNo));
}

void GoToFavoriteByMenuId(WindowInfo *win, int wmId)
{
    size_t idx;
    FileFavs *f = gFavorites->GetByMenuId(wmId, idx);
    if (!f)
        return;
    FavName *fn = f->favNames.At(idx);
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

    FavName *fn = (FavName*)item.lParam;
    if (!fn) {
        // can happen for top-level node which is not associated with a favorite
        // but only serves a parent node for favorites for a given file
        return;
    }
    FileFavs *f = gFavorites->GetByFavName(fn);
    GoToFavorite(win, f, fn);
}

static HTREEITEM InsertFavSecondLevelNode(HWND hwnd, HTREEITEM parent, FavName *fn)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    tvinsert.itemex.state = 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)fn;
    ScopedMem<TCHAR> s(FavReadableName(fn));
    tvinsert.itemex.pszText = s;
    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void InsertFavSecondLevelNodes(HWND hwnd, HTREEITEM parent, FileFavs *f)
{
    for (size_t i = 0; i < f->favNames.Count(); i++)
    {
        InsertFavSecondLevelNode(hwnd, parent, f->favNames.At(i));
    }
}

static HTREEITEM InsertFavTopLevelNode(HWND hwnd, FileFavs *fav, bool isExpanded)
{
    TCHAR *s = NULL;
    bool collapsed = fav->favNames.Count() == 1;
    if (collapsed)
        isExpanded = false;
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = NULL;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
    tvinsert.itemex.state = isExpanded ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = NULL;
    if (collapsed) {
        FavName *fn = fav->favNames.At(0);
        tvinsert.itemex.lParam = (LPARAM)fn;
        s = FavCompactReadableName(fav, fn);
        tvinsert.itemex.pszText = s;
    } else {
        tvinsert.itemex.pszText = (TCHAR*)path::GetBaseName(fav->filePath);
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

    Vec<TCHAR*> filePathsSorted;
    GetSortedFilePaths(gFavorites, filePathsSorted, NULL);

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    for (size_t i = 0; i < filePathsSorted.Count(); i++) {
        FileFavs *f = gFavorites->GetFavByFilePath(filePathsSorted.At(i));
        bool isExpanded = (win->expandedFavorites.Find(f->filePath) != -1);
        HTREEITEM node = InsertFavTopLevelNode(hwndTree, f, isExpanded);
        if (f->favNames.Count() > 1)
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

bool HasFavorites()
{
    for (size_t i = 0; i < gFavorites->Count(); i++) {
        FileFavs *f = gFavorites->favs.At(i);
        if (f->favNames.Count() > 0)
            return true;
    }
    return false;
}

void UpdateFavoritesTreeForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++)
        UpdateFavoritesTreeIfNecessary(gWindows.At(i));

    // hide the favorites tree if we removed the last favorite
    if (!HasFavorites()) {
        gGlobalPrefs.favVisible = false;
        for (size_t i = 0; i < gWindows.Count(); i++) {
            WindowInfo *win = gWindows.At(i);
            SetSidebarVisibility(win, win->tocVisible, gGlobalPrefs.favVisible);
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
    ScopedMem<TCHAR> name;
    if (win->dm->HasTocTree()) {
        // use the current ToC heading as default name
        DocTocItem *root = win->dm->engine->GetTocTree();
        DocTocItem *item = TocItemForPageNo(root, pageNo);
        if (item)
            name.Set(str::Dup(item->title));
        delete root;
    }
    ScopedMem<TCHAR> pageLabel(win->dm->engine->GetPageLabel(pageNo));

    bool shouldAdd = Dialog_AddFavorite(win->hwndFrame, pageLabel, name);
    if (!shouldAdd)
        return;

    RememberFavTreeExpansionStateForAllWindows();
    gFavorites->AddOrReplace(win->loadedFilePath, pageNo, name);
    UpdateFavoritesTreeForAllWindows();
    SavePrefs();
}

void DelFavorite(WindowInfo *win)
{
    int pageNo = win->currPageNo;
    TCHAR *filePath = win->loadedFilePath;
    RememberFavTreeExpansionStateForAllWindows();
    gFavorites->Remove(filePath, pageNo);
    UpdateFavoritesTreeForAllWindows();
    SavePrefs();
}

void RememberFavTreeExpansionState(WindowInfo *win)
{
    FreeVecMembers(win->expandedFavorites);
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
            FavName *fn = (FavName*)item.lParam;
            FileFavs *f = gFavorites->GetByFavName(fn);
            win->expandedFavorites.Append(str::Dup(f->filePath));
        }

        treeItem = TreeView_GetNextSibling(win->hwndFavTree, treeItem);
    }
}

void RememberFavTreeExpansionStateForAllWindows()
{
    for (size_t i = 0; i < gWindows.Count(); i++)
        RememberFavTreeExpansionState(gWindows.At(i));
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

static void OnFavTreeContextMenu(WindowInfo *win, int xScreen, int yScreen)
{
    TVITEM item;
    if (xScreen != -1 || yScreen != -1) {
        TVHITTESTINFO ht = { 0 };
        ht.pt.x = xScreen;
        ht.pt.y = yScreen;

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
            xScreen = rcItem.left;
            yScreen = rcItem.bottom;
        }
        else {
            WindowRect rc(win->hwndFavTree);
            xScreen = rc.x;
            yScreen = rc.y;
        }
    }

    FavName *toDelete = NULL;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(win->hwndFavTree, &item);
    toDelete = (FavName*)item.lParam;

    HMENU popup = BuildMenuFromMenuDef(menuDefFavContext, dimof(menuDefFavContext), CreatePopupMenu());

    INT cmd = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             xScreen, yScreen, 0, win->hwndFavTree, NULL);
    DestroyMenu(popup);
    if (IDM_FAV_DEL == cmd) {
        RememberFavTreeExpansionStateForAllWindows();
        if (toDelete) {
            FileFavs *f = gFavorites->GetByFavName(toDelete);
            gFavorites->Remove(f->filePath, toDelete->pageNo);
        } else {
            // toDelete == NULL => this is a parent node signifying all bookmarks in a file
            item.hItem = TreeView_GetChild(win->hwndFavTree, item.hItem);
            item.mask = TVIF_PARAM;
            TreeView_GetItem(win->hwndFavTree, &item);
            toDelete = (FavName*)item.lParam;
            FileFavs *f = gFavorites->GetByFavName(toDelete);
            gFavorites->RemoveAllForFile(f->filePath);
        }
        UpdateFavoritesTreeForAllWindows();
        SavePrefs();

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

        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.escToExit)
                DestroyWindow(win->hwndFrame);
            break;

        case WM_MOUSEWHEEL:
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
            LayoutTreeContainer(hwnd, IDC_FAV_BOX);
            break;

        case WM_DRAWITEM:
            if (IDC_FAV_CLOSE == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawCloseButton(dis);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_FAV_CLOSE && HIWORD(wParam) == STN_CLICKED)
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
                OnFavTreeContextMenu(win, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            }
            break;

    }
    return CallWindowProc(DefWndProcFavBox, hwnd, message, wParam, lParam);
}

void CreateFavorites(WindowInfo *win)
{
    win->hwndFavBox = CreateWindow(WC_STATIC, _T(""), WS_CHILD,
                                   0, 0, gGlobalPrefs.sidebarDx, 0,
                                   win->hwndFrame, (HMENU)0, ghinst, NULL);
    HWND title = CreateWindow(WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                              0, 0, 0, 0, win->hwndFavBox, (HMENU)IDC_FAV_TITLE, ghinst, NULL);
    SetWindowFont(title, gDefaultGuiFont, FALSE);
    win::SetText(title, _TR("Favorites"));

    HWND hwndClose = CreateWindow(WC_STATIC, _T(""),
                                  SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                                  0, 0, 16, 16, win->hwndFavBox, (HMENU)IDC_FAV_CLOSE, ghinst, NULL);

    win->hwndFavTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("Fav"),
                                      TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                                      TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                                      WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                      0, 0, 0, 0, win->hwndFavBox, (HMENU)IDC_FAV_TREE, ghinst, NULL);

    // Note: those must be consecutive numbers and in title/close/tree order
    CASSERT(IDC_FAV_BOX + 1 == IDC_FAV_TITLE &&
            IDC_FAV_BOX + 2 == IDC_FAV_CLOSE &&
            IDC_FAV_BOX + 3 == IDC_FAV_TREE, consecutive_fav_ids);

#ifdef UNICODE
    TreeView_SetUnicodeFormat(win->hwndFavTree, true);
#endif

    if (NULL == DefWndProcFavTree)
        DefWndProcFavTree = (WNDPROC)GetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavTree, GWLP_WNDPROC, (LONG_PTR)WndProcFavTree);

    if (NULL == DefWndProcFavBox)
        DefWndProcFavBox = (WNDPROC)GetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndFavBox, GWLP_WNDPROC, (LONG_PTR)WndProcFavBox);

    if (NULL == DefWndProcCloseButton)
        DefWndProcCloseButton = (WNDPROC)GetWindowLongPtr(hwndClose, GWLP_WNDPROC);
    SetWindowLongPtr(hwndClose, GWLP_WNDPROC, (LONG_PTR)WndProcCloseButton);
}
