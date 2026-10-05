/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the favorites model (the per-file list in FileHistory, sorting, add /
// remove, the readable name) and the tree model are orig's. What orig does
// with a win32 TreeView, custom draw and an HMENU is here: the tree model is
// handed to the gpui favorites pane (src/gui/Sidebar.cpp) and the Favorites
// menu fills a MenuModel. The full-window Favorites tab is step 10.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "FilterUtil.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppSettings.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Tabs.h"
#include "Theme.h"
#include "PagePosition.h"
#include "SumatraLog.h"
#include "gui/AppShell.h"
#include "gui/Sidebar.h"
#include "Favorites.h"

static void RememberFavTreeExpansionStateForAllWindows();

FavTreeItem::~FavTreeItem() {
    str::Free(text);
    DeleteVecMembers(children);
}

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

void FavTreeModel::SetUserData(TreeItem ti, uintptr_t userData) {
    ReportIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    treeItem->userData = userData;
}

uintptr_t FavTreeModel::GetUserData(TreeItem ti) {
    ReportIf(ti < 0);
    FavTreeItem* treeItem = (FavTreeItem*)ti;
    return treeItem->userData;
}

static FileState* GetByFavorite(Favorite* fn) {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        if (VecContains(*ds->favorites, fn)) {
            return ds;
        }
    }
    return nullptr;
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

static PointF CurrentFavoriteScrollPos(MainWindow* win, int pageNo) {
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return PointF(-1, -1);
    }
    ScrollState ss = dm->GetScrollState();
    if (ss.page != pageNo) {
        return PointF(-1, -1);
    }
    return PointF((float)ss.x, (float)ss.y);
}

// Restore the favorite's page and the stored position on it. addNavPt so
// Navigate Back returns to wherever we jumped from.
static void ApplyFavoriteView(MainWindow* win, Str pageNoStr, PointF scrollPos, bool addNavPt) {
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return;
    }
    StoredPagePos pos = ParseStoredPagePos(pageNoStr);
    if (pos.bookmark) {
        Location loc = win->ctrl->LookupBookmark(pos.bookmark);
        if (loc.IsValid()) {
            win->ctrl->GoToLocation(loc, addNavPt);
            return;
        }
    }
    if (win->ctrl->HasChapters()) {
        Location loc = LocationFromFlatPageNo(win->ctrl, pos.pageNo);
        if (loc.IsValid()) {
            win->ctrl->GoToLocation(loc, addNavPt);
            return;
        }
    }
    int pageNo = pos.pageNo;
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        if (addNavPt) {
            dm->AddNavPoint();
        }
        dm->SetScrollState(ScrollState(pageNo, scrollPos.x, scrollPos.y));
        return;
    }
    win->ctrl->GoToPage(pageNo, addNavPt);
}

void JumpToFavorite(MainWindow* win, Favorite* fav) {
    if (!win || !fav) {
        return;
    }
    ApplyFavoriteView(win, fav->pageNo, fav->scrollPos, true);
    win->Focus();
}

// identity for a chaptered doc's favorite: a flat pageNo shifts as chapters
// lay out, so a bookmarked favorite is matched by its (chapter, page) hint
// instead, when the caller has a Location to compare against
bool IsPageInFavorites(Str filePath, int pageNo, DocController* ctrl) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav || !fav->favorites) {
        return false;
    }
    Location loc = (ctrl && ctrl->HasChapters() && pageNo >= 1) ? ctrl->LocationFromPageNo(pageNo) : kInvalidLocation;
    for (int i = 0; i < len(*fav->favorites); i++) {
        Favorite* fn = (*fav->favorites)[i];
        StoredPagePos pos = ParseStoredPagePos(fn->pageNo);
        if (pos.bookmark && loc.IsValid()) {
            if (BookmarkLocationHint(pos.bookmark) == loc) {
                return true;
            }
            continue;
        }
        if (pageNo == pos.pageNo) {
            return true;
        }
    }
    return false;
}

// tuple order: chapter first, then page within chapter
static bool LocLess(Location a, Location b) {
    if (a.chapter != b.chapter) {
        return a.chapter < b.chapter;
    }
    return a.page < b.page;
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
    DocController* ctrl = win->ctrl;
    int cur = win->currPageNo;
    // chaptered docs: compare by (chapter, page) hint, not the flat pageNo,
    // which shifts as chapters lay out
    if (ctrl->HasChapters()) {
        Location curLoc = ctrl->CurrentLocation();
        Favorite* bestFav = nullptr;
        Location bestLoc{};
        for (Favorite* fav : *fs->favorites) {
            StoredPagePos pos = ParseStoredPagePos(fav->pageNo);
            Location favLoc = pos.bookmark ? BookmarkLocationHint(pos.bookmark) : ctrl->LocationFromPageNo(pos.pageNo);
            if (!favLoc.IsValid()) {
                continue;
            }
            if (forward) {
                if (LocLess(curLoc, favLoc) && (!bestFav || LocLess(favLoc, bestLoc))) {
                    bestFav = fav;
                    bestLoc = favLoc;
                }
            } else {
                if (LocLess(favLoc, curLoc) && (!bestFav || LocLess(bestLoc, favLoc))) {
                    bestFav = fav;
                    bestLoc = favLoc;
                }
            }
        }
        if (bestFav) {
            JumpToFavorite(win, bestFav);
        }
        return;
    }
    // pick the favorite page closest to the current page in the requested
    // direction (no wrap-around)
    Favorite* bestFav = nullptr;
    int bestPage = 0;
    for (int i = 0; i < len(*fs->favorites); i++) {
        Favorite* fav = (*fs->favorites)[i];
        int p = ParseStoredPagePos(fav->pageNo).pageNo;
        if (forward) {
            if (p > cur && (!bestFav || p < bestPage)) {
                bestFav = fav;
                bestPage = p;
            }
        } else {
            if (p < cur && (!bestFav || p > bestPage)) {
                bestFav = fav;
                bestPage = p;
            }
        }
    }
    if (bestFav) {
        JumpToFavorite(win, bestFav);
    }
}

static Favorite* FindByPage(FileState* ds, Str storedPagePos, Location loc = kInvalidLocation, Str pageLabel = {}) {
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
    StoredPagePos pos = ParseStoredPagePos(storedPagePos);
    for (int i = 0; i < n; i++) {
        auto* fav = (*favs)[i];
        StoredPagePos favPos = ParseStoredPagePos(fav->pageNo);
        if (favPos.bookmark && loc.IsValid()) {
            if (BookmarkLocationHint(favPos.bookmark) == loc) {
                return fav;
            }
            continue;
        }
        if (pos.bookmark && favPos.bookmark) {
            if (str::Eq(pos.bookmark, favPos.bookmark)) {
                return fav;
            }
            continue;
        }
        if (pos.pageNo == favPos.pageNo) {
            return fav;
        }
    }
    return nullptr;
}

static int SortByPageNo(Favorite* const* a, Favorite* const* b) {
    Favorite* na = *a;
    Favorite* nb = *b;
    StoredPagePos pa = ParseStoredPagePos(na->pageNo);
    StoredPagePos pb = ParseStoredPagePos(nb->pageNo);
    if (pa.bookmark && pb.bookmark) {
        Location la = BookmarkLocationHint(pa.bookmark);
        Location lb = BookmarkLocationHint(pb.bookmark);
        if (la != lb) {
            return LocLess(la, lb) ? -1 : 1;
        }
    }
    return pa.pageNo - pb.pageNo;
}

// Sort by user name if set, else page label; page number breaks ties and is
// the only key when neither favorite has a name/label (issue #2277).
static int SortByName(Favorite* const* a, Favorite* const* b) {
    Favorite* na = *a;
    Favorite* nb = *b;
    Str sa = na->name;
    if (len(sa) == 0) {
        sa = na->pageLabel;
    }
    Str sb = nb->name;
    if (len(sb) == 0) {
        sb = nb->pageLabel;
    }
    if (sa || sb) {
        if (len(sa) == 0) {
            return 1;
        }
        if (len(sb) == 0) {
            return -1;
        }
        int n = str::CmpNatural(sa, sb);
        if (n != 0) {
            return n;
        }
    }
    return SortByPageNo(a, b);
}

static void SortFileFavorites(FileState* fs) {
    if (!fs || !fs->favorites || len(*fs->favorites) < 2) {
        return;
    }
    if (gSettings->sortFavoritesByName) {
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
    gSettings->sortFavoritesByName = !gSettings->sortFavoritesByName;
    SortAllFavorites();
    RememberFavTreeExpansionStateForAllWindows();
    UpdateFavoritesTreeForAllWindows();
    ScheduleSaveSettings();
}

static void AddOrReplaceFav(Str filePath, Str storedPagePos, Str name, Str pageLabel, PointF scrollPos,
                            Location loc = kInvalidLocation) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        // we were asked to add a favorite for current file but couldn't find
        // history for this file
        fav = NewFileState(filePath);
        FileHistoryAppend(fav);
    }

    Favorite* fn = FindByPage(fav, storedPagePos, loc, pageLabel);
    if (fn) {
        str::ReplaceWithCopy(&fn->name, name);
        ReportIf(fn->pageLabel && !str::Eq(fn->pageLabel, pageLabel));
        fn->scrollPos = scrollPos;
        str::ReplaceWithCopy(&fn->pageNo, storedPagePos);
        SortFileFavorites(fav);
    } else {
        fn = NewFavorite(storedPagePos, name, pageLabel);
        fn->scrollPos = scrollPos;
        VecAppend(*fav->favorites, fn);
        SortFileFavorites(fav);
    }
}

// Name used for the Sioyek-style "search start" mark (issue #5726).
static Str SearchStartFavName() {
    return StrL("/");
}

static Favorite* FindByName(FileState* ds, Str name) {
    if (!ds || !ds->favorites || len(name) == 0) {
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
    if (!tab || len(tab->filePath) == 0) {
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
    bool needsLabel = pageLabel && !str::Eq(plainLabel, pageLabel) && !win->ctrl->HasChapters();
    Str pl = needsLabel ? pageLabel : Str{};

    FileState* fs = GetFavByFilePath(path);
    if (!fs) {
        fs = NewFileState(path);
        FileHistoryAppend(fs);
    }

    Str markName = SearchStartFavName();
    Favorite* fn = FindByName(fs, markName);
    PointF scrollPos = CurrentFavoriteScrollPos(win, pageNo);
    TempStr storedPos = StoredPagePosForPageTemp(win->ctrl, pageNo);
    if (fn) {
        if (fn->isTemporary && str::Eq(fn->pageNo, storedPos) && str::Eq(fn->pageLabel, pl) &&
            fn->scrollPos.x == scrollPos.x && fn->scrollPos.y == scrollPos.y) {
            return; // already marks this view
        }
        str::ReplaceWithCopy(&fn->pageNo, storedPos);
        str::ReplaceWithCopy(&fn->pageLabel, pl);
        fn->scrollPos = scrollPos;
        // mark as session-only even if a prior build persisted a "/" entry
        fn->isTemporary = true;
        SortFileFavorites(fs);
    } else {
        fn = NewFavorite(storedPos, markName, pl);
        fn->isTemporary = true;
        fn->scrollPos = scrollPos;
        VecAppend(*fs->favorites, fn);
        SortFileFavorites(fs);
    }
    UpdateFavoritesTreeForAllWindows();
}

static void RemoveFav(Str filePath, int pageNo, Location loc = kInvalidLocation) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav || !fav->favorites) {
        return;
    }
    for (int i = 0; i < len(*fav->favorites); i++) {
        Favorite* fn = (*fav->favorites)[i];
        StoredPagePos pos = ParseStoredPagePos(fn->pageNo);
        if (pos.bookmark && loc.IsValid()) {
            if (BookmarkLocationHint(pos.bookmark) == loc) {
                VecRemove(*fav->favorites, fn);
                DeleteFavorite(fn);
                break;
            }
            continue;
        }
        if (pageNo == pos.pageNo) {
            VecRemove(*fav->favorites, fn);
            DeleteFavorite(fn);
            break;
        }
    }

    if (!SettingsRememberOpenedFiles() && 0 == len(*fav->favorites)) {
        FileHistoryRemove(fav);
        DeleteFileState(fav);
    }
}

void DelFavorite(FileState* fs, Favorite* fav) {
    if (!fs || !fs->favorites || !fav) {
        return;
    }
    RememberFavTreeExpansionStateForAllWindows();
    VecRemove(*fs->favorites, fav);
    DeleteFavorite(fav);
    if (!SettingsRememberOpenedFiles() && 0 == len(*fs->favorites)) {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    }
    UpdateFavoritesTreeForAllWindows();
    ScheduleSaveSettings();
}

static void RemoveAllFavForFile(Str filePath) {
    FileState* fav = GetFavByFilePath(filePath);
    if (!fav) {
        return;
    }

    for (int i = 0; i < len(*fav->favorites); i++) {
        DeleteFavorite((*fav->favorites)[i]);
    }
    VecReset(*fav->favorites);

    if (!SettingsRememberOpenedFiles()) {
        FileHistoryRemove(fav);
        DeleteFileState(fav);
    }
}

// Note: those might be too big
constexpr int kMaxFavSubmenus = 10;
constexpr int kMaxFavMenus = 10;

bool HasFavorites() {
    FileState* ds;
    for (int i = 0; (ds = FileHistoryGet(i)) != nullptr; i++) {
        if (len(*ds->favorites) > 0) {
            return true;
        }
    }
    return false;
}

// shared with CommandPalette.cpp (favorites mode)
TempStr FavReadableNameTemp(Favorite* fn) {
    StoredPagePos pos = ParseStoredPagePos(fn->pageNo);
    bool isChaptered = len(pos.bookmark) > 0;
    bool showChapter = isChaptered && gSettings && gSettings->showChaptersInEbooks;
    int chapter = 0, page = 0;
    if (isChaptered) {
        Location loc = BookmarkLocationHint(pos.bookmark);
        chapter = loc.chapter;
        page = loc.page;
    }

    Str label = fn->pageLabel;
    if (len(label) == 0 && !isChaptered) {
        label = fmt("%d", pos.pageNo);
    }
    if (len(label) == 0 && isChaptered && !showChapter && page >= 1) {
        label = fmt("%d", page);
    }

    if (fn->name) {
        TempStr loc;
        if (showChapter) {
            loc = fmt(Tr("(chapter %d page %d)").s, chapter, page);
        } else {
            loc = fmt(Tr("(page %s)").s, label);
        }
        return str::JoinTemp(fn->name, StrL(" "), loc);
    }
    if (showChapter) {
        return fmt(Tr("Chapter %d Page %d").s, chapter, page);
    }
    return fmt(Tr("Page %s").s, label);
}

static TempStr FavCompactReadableNameTemp(FileState* fav, Favorite* fn, bool isCurrent = false) {
    TempStr rn = FavReadableNameTemp(fn);
    if (isCurrent) {
        return fmt("%s : %s", Tr("Current file"), rn);
    }
    TempStr fp = path::GetBaseNameTemp(fav->filePath);
    // Keep the favorite's name first in compact menu entries so a long file
    // name doesn't push the user's description out of view (fixes #829, #2236).
    return fmt("%s : %s", rn, fp);
}

// Compact rows in the Favorites tree lead with the bold source filename so
// single-favorite files line up with the file nodes used for grouped entries.
static TempStr FavTreeCompactReadableNameTemp(FileState* fav, Favorite* fn) {
    TempStr fp = path::GetBaseNameTemp(fav->filePath);
    TempStr rn = FavReadableNameTemp(fn);
    return fmt("%s : %s", fp, rn);
}

struct FavMenuEntry {
    Str filePath;
    Str pageNo;
    int cmdId;
};

// A favorite in the menu is a CmdFavorite command carrying the file path and the
// page as arguments. Custom commands live until the settings are re-read, so reuse
// the one already made for a favorite instead of making one per menu rebuild.
// One pass over the commands serves all the entries.
static void SetFavCmdIds(Vec<FavMenuEntry>& favs) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdFavorite);
    for (CustomCommand* cmd : cmds) {
        Str filePath = GetCommandStringArg(cmd, kCmdArgFilePath, {});
        Str pageNo = GetCommandStringArg(cmd, kCmdArgPage, {});
        for (FavMenuEntry& fe : favs) {
            if (fe.cmdId == 0 && str::EqI(filePath, fe.filePath) && str::Eq(pageNo, fe.pageNo)) {
                fe.cmdId = cmd->id;
                break;
            }
        }
    }

    for (FavMenuEntry& fe : favs) {
        if (fe.cmdId != 0) {
            continue;
        }
        CommandArg* args = NewStringArg(kCmdArgFilePath, fe.filePath);
        args->next = NewStringArg(kCmdArgPage, fe.pageNo);
        fe.cmdId = CreateCustomCommand(StrL("CmdFavorite"), CmdFavorite, args)->id;
    }
}

static void AppendFavMenuItems(MenuModel* m, FileState* f, Vec<FavMenuEntry>& favs, int& idx, bool combined,
                               bool isCurrent) {
    ReportIf(!f);
    if (!f) {
        return;
    }
    for (int i = 0; i < len(*f->favorites); i++) {
        if (i >= kMaxFavMenus) {
            return;
        }
        Favorite* fn = (*f->favorites)[i];
        int cmdId = favs[idx++].cmdId;
        TempStr s;
        if (combined) {
            s = FavCompactReadableNameTemp(f, fn, isCurrent);
        } else {
            s = FavReadableNameTemp(fn);
        }
        MenuAppendString(m, s, cmdId);
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
// The first menu items are for currently opened file (up to kMaxFavMenus), based
// on the assumption that user is usually interested in navigating current file.
// Then we have a submenu for each file for which there are bookmarks (up to
// kMaxFavSubmenus), each having up to kMaxFavMenus menu items.
static void AppendFavMenus(MenuModel* m, Str currFilePath) {
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

    MenuAppendSeparator(m);

    int menusCount = len(filePathsSorted);
    // ng: orig caps this with kMaxFavMenus and never uses kMaxFavSubmenus; both
    // are 10, and /WX rejects the unused constant
    menusCount = std::min(menusCount, kMaxFavSubmenus);

    // collect the favorites that will be shown, so that a single pass over the
    // commands gives all of them their command id
    Vec<FavMenuEntry> favs;
    for (int i = 0; i < menusCount; i++) {
        Str filePath = filePathsSorted[i];
        FileState* f = GetFavByFilePath(filePath);
        if (!f) {
            continue;
        }
        for (int j = 0; j < len(*f->favorites) && j < kMaxFavMenus; j++) {
            Favorite* fn = (*f->favorites)[j];
            VecAppend(favs, FavMenuEntry{filePath, fn->pageNo, 0});
        }
    }
    SetFavCmdIds(favs);

    int favIdx = 0;
    for (int i = 0; i < menusCount; i++) {
        Str filePath = filePathsSorted[i];
        FileState* f = GetFavByFilePath(filePath);
        ReportIf(!f);
        if (!f) {
            continue;
        }
        bool combined = (len(*f->favorites) == 1);
        if (combined) {
            AppendFavMenuItems(m, f, favs, favIdx, true, f == currFileFav);
            continue;
        }
        Str s = Tr("Current file");
        if (f != currFileFav) {
            s = path::GetBaseNameTemp(filePath);
        }
        MenuModel* sub = MenuAppendSubmenu(m, s);
        AppendFavMenuItems(sub, f, favs, favIdx, false, f == currFileFav);
    }
}

// Called when a user opens "Favorites" top-level menu. We need to construct
// the menu:
// - disable add/remove menu items if no document is opened
// - if a document is opened and the page is already bookmarked,
//   disable "add" menu item and enable "remove" menu item
// - if a document is opened and the page is not bookmarked,
//   enable "add" menu item and disable "remove" menu item
void RebuildFavMenu(MainWindow* win, MenuModel* menu) {
    if (!win->IsDocLoaded()) {
        MenuSetEnabled(menu, CmdFavoriteAdd, false);
        MenuSetEnabled(menu, CmdFavoriteDel, false);
        AppendFavMenus(menu, {});
    } else {
        DocController* ctrl = win->ctrl;
        int pageNo = win->currPageNo;
        bool isBookmarked = IsPageInFavorites(ctrl->GetFilePath(), pageNo, ctrl);

        TempStr addText;
        TempStr delText;
        if (ShowChapterUi(ctrl)) {
            Location loc = ctrl->LocationFromPageNo(pageNo);
            addText = fmt(Tr("Add chapter %d page %d to favorites").s, loc.chapter, loc.page);
            delText = fmt(Tr("Remove chapter %d page %d from favorites").s, loc.chapter, loc.page);
        } else {
            TempStr label = ctrl->GetPageLabeTemp(pageNo);
            addText = fmt(Tr("Add page %s to favorites").s, label);
            delText = fmt(Tr("Remove page %s from favorites").s, label);
        }

        if (isBookmarked) {
            MenuSetEnabled(menu, CmdFavoriteAdd, false);
            MenuSetText(menu, CmdFavoriteDel, delText);
        } else {
            MenuSetEnabled(menu, CmdFavoriteDel, false);
            MenuSetText(menu, CmdFavoriteAdd, addText);
        }
        AppendFavMenus(menu, ctrl->GetFilePath());
    }
    MenuSetEnabled(menu, CmdFavoriteToggle, HasFavorites());
}

// ng: orig's FindMainWindowByFile for a favorite: the file in any window, its
// tab brought to the front
static MainWindow* FindWindowWithFile(Str filePath) {
    for (MainWindow* win : gWindows) {
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* tab = win->GetTab(i);
            if (str::EqI(tab->filePath, filePath)) {
                TabsSelect(win, i);
                return win;
            }
        }
    }
    return nullptr;
}

static void GoToFavoritePage(MainWindow* win, Str pageNo, PointF scrollPos) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ApplyFavoriteView(win, pageNo, scrollPos, true);
    // we might have been invoked by clicking on a tree view
    // switch focus so that keyboard navigation works, which enables
    // a fluid experience
    win->Focus();
    AppShellInvalidate(win);
}

struct GoToFavoritePageData {
    MainWindow* win;
    Str pageNo; // owned
    PointF scrollPos;
};

static void GoToFavoritePage(GoToFavoritePageData* d) {
    GoToFavoritePage(d->win, d->pageNo, d->scrollPos);
    str::Free(d->pageNo);
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
    MainWindow* existingWin = FindWindowWithFile(fp);
    if (existingWin) {
        auto* data = new GoToFavoritePageData;
        data->pageNo = str::Dup(fav->pageNo);
        data->scrollPos = fav->scrollPos;
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
    Str pageNo = fav->pageNo;
    PointF scrollPos = fav->scrollPos;
    FileState* ds = FileHistoryFindByPath(fs->filePath);
    if (ds && !ds->useDefaultState && gSettings->rememberStatePerDocument) {
        str::ReplaceWithCopy(&ds->pageNo, fav->pageNo);
        ds->scrollPos = fav->scrollPos;
        pageNo = {};
    }

    win = LoadDocument(win, fs->filePath);
    if (win && pageNo) {
        auto* data = new GoToFavoritePageData;
        data->pageNo = str::Dup(pageNo);
        data->scrollPos = scrollPos;
        data->win = win;
        auto fn = MkFunc0<GoToFavoritePageData>(GoToFavoritePage, data);
        uitask::Post(fn, "TaskGoToFavorite2");
    }
}

// a favorite in the Favorites menu carries its file path and page as arguments
void GoToFavoriteByCmd(MainWindow* win, CustomCommand* cmd) {
    Str filePath = GetCommandStringArg(cmd, kCmdArgFilePath, {});
    Str pageNo = GetCommandStringArg(cmd, kCmdArgPage, {});
    FileState* fs = GetFavByFilePath(filePath);
    if (!fs) {
        return;
    }
    for (Favorite* fn : *fs->favorites) {
        if (str::Eq(fn->pageNo, pageNo)) {
            GoToFavorite(win, fs, fn);
            return;
        }
    }
}

void GoToFavForTreeItem(MainWindow* win, FavTreeItem* fti) {
    if (!fti) {
        return;
    }
    logf("GoToFavForTreeItem: '%s'\n", fti->text);
    Favorite* fn = fti->favorite;
    if (!fn) {
        // can happen for top-level node which is not associated with a favorite
        // but only serves a parent node for favorites for a given file
        return;
    }
    FileState* f = GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

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

    TempStr baseName = path::GetBaseNameTemp(fs->filePath);
    TempStr text;
    if (isCollapsed) {
        text = FavTreeCompactReadableNameTemp(fs, fn);
    } else {
        text = baseName;
    }
    res->text = str::Dup(text);
    res->fileNameOffset = 0;
    res->fileNameLen = len(baseName);
    return res;
}

// true if every filter word appears in this favorite's searchable text
// (file base name + readable label / optional user name - same idea as palette)
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
// match. Only rows that match are shown - no "include all children of this file".
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
                VecAppend(res->root->children, ti);
            }
            continue;
        }

        // multi-favorite file
        if (!filtering) {
            auto* parent = new FavTreeItem();
            parent->favorite = (*fs->favorites)[0];
            parent->text = str::Dup(baseName);
            parent->fileNameOffset = 0;
            parent->fileNameLen = len(baseName);
            parent->isExpanded = VecContains(win->expandedFavorites, fs);
            for (int j = 0; j < nFavs; j++) {
                Favorite* fn = (*fs->favorites)[j];
                auto* ti = new FavTreeItem();
                ti->text = str::Dup(FavReadableNameTemp(fn));
                ti->parent = parent;
                ti->favorite = fn;
                VecAppend(parent->children, ti);
            }
            VecAppend(res->root->children, parent);
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
            ti->text = str::Dup(FavTreeCompactReadableNameTemp(fs, fn));
            ti->fileNameOffset = 0;
            ti->fileNameLen = len(baseName);
            ti->isExpanded = false;
            VecAppend(res->root->children, ti);
        }
    }
    return res;
}

static void ApplyFavFilter(MainWindow* win) {
    SidebarSetFavModel(win, BuildFavTreeModel(win, SidebarFavFilterTextTemp(win)));
}

void FavFilterChanged(MainWindow* win) {
    ApplyFavFilter(win);
    FavTreeModel* tm = SidebarFavModel(win);
    int n = tm ? tm->ChildCount(tm->Root()) : 0;
    logf("FavFilterChanged: '%s' -> %d rows\n", SidebarFavFilterTextTemp(win), n);
    AppShellInvalidate(win);
}

void PopulateFavTreeIfNeeded(MainWindow* win) {
    if (SidebarFavModel(win)) {
        return;
    }
    ApplyFavFilter(win);
}

void ToggleFavorites(MainWindow* win) {
    SidebarToggleFavorites(win);
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

// open/select the full-window Favorites tab (can be used with the sidebar's
// Favorites panel). Always switches to / creates the tab; it is closed with
// the tab's ✕. Requires tabs; falls back to the sidebar toggle when tabs are
// off.
void ToggleFavoritesTab(MainWindow* win) {
    if (!SettingsUseTabs()) {
        ToggleFavorites(win);
        return;
    }
    WindowTab* favTab = FindFavoritesTab(win);
    if (favTab) {
        int idx = win->GetTabIdx(favTab);
        if (idx >= 0 && win->CurrentTab() != favTab) {
            TabsSelect(win, idx);
        }
        PopulateFavTreeIfNeeded(win);
        AppShellInvalidate(win);
        return;
    }
    PopulateFavTreeIfNeeded(win);
    SaveCurrentWindowTab(win);
    auto* tab = new WindowTab(win);
    tab->type = WindowTab::Type::Favorites;
    AddTabToWindow(win, tab);
    LoadModelIntoTab(tab);
}

void UpdateFavoritesTree(MainWindow* win) {
    // rebuild (honors current search filter if any)
    ApplyFavFilter(win);
    FavTreeModel* newModel = SidebarFavModel(win);

    // hide favorites UI if we've removed the last favorite
    bool hasAny = false;
    if (newModel) {
        hasAny = newModel->ChildCount(newModel->Root()) > 0;
    }
    if (!hasAny) {
        if (WindowTab* favTab = FindFavoritesTab(win)) {
            CloseTab(favTab, false);
        }
        if (gSettings->showFavorites) {
            SetSidebarVisibility(win, win->uiState.tocVisible, false);
        }
        return;
    }
    // refresh sidebar visibility only when the sidebar panel is supposed to be open
    if (gSettings->showFavorites) {
        SetSidebarVisibility(win, win->uiState.tocVisible, true);
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
    if (len(filePath) == 0 || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    DocController* ctrl = win->ctrl;
    TempStr plainLabel = fmt("%d", pageNo);
    bool needsLabel = !str::Eq(plainLabel, pageLabel) && !(ctrl && ctrl->HasChapters());

    RememberFavTreeExpansionStateForAllWindows();
    Str pl = needsLabel ? pageLabel : Str{};
    TempStr storedPos = StoredPagePosForPageTemp(ctrl, pageNo);
    Location loc = (ctrl && ctrl->HasChapters()) ? ctrl->LocationFromPageNo(pageNo) : kInvalidLocation;

    logf("ApplyAddFavorite: '%s' page %d label '%s' name '%s'\n", filePath, pageNo, pageLabel, name);
    AddOrReplaceFav(filePath, storedPos, name, pl, CurrentFavoriteScrollPos(win, pageNo), loc);
    // expand newly added favorites by default
    FileState* fav = GetFavByFilePath(filePath);
    if (fav && len(*fav->favorites) == 2) {
        VecAppend(win->expandedFavorites, fav);
    }
    UpdateFavoritesTreeForAllWindows();
    ScheduleSaveSettings();
}

void AddFavoriteWithLabelAndName(MainWindow* win, int pageNo, Str pageLabel, Str nameIn) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->CurrentTab()) {
        return;
    }
    ShowAddFavoriteDialog(win, win->CurrentTab()->filePath, pageNo, pageLabel, nameIn);
}

void AddFavoriteForPage(MainWindow* win, int pageNo) {
    Str name;
    auto* tab = win->CurrentTab();
    auto* ctrl = tab ? tab->ctrl : nullptr;
    if (!ctrl) {
        return;
    }
    if (ctrl->HasToc()) {
        // use the current ToC heading as default name
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree ? docTree->root : nullptr;
        TocItem* item = TocItemForPageNo(root, pageNo);
        if (item) {
            name = item->title;
        }
    }
    TempStr pageLabel;
    if (ShowChapterUi(ctrl)) {
        Location loc = ctrl->LocationFromPageNo(pageNo);
        pageLabel = fmt("%d/%d", loc.chapter, loc.page);
    } else {
        pageLabel = ctrl->GetPageLabeTemp(pageNo);
    }
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel, name);
}

void AddFavoriteForCurrentPage(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    int pageNo = win->currPageNo;
    AddFavoriteForPage(win, pageNo);
}

void DelFavorite(Str filePath, int pageNo, DocController* ctrl) {
    if (len(filePath) == 0) {
        return;
    }
    RememberFavTreeExpansionStateForAllWindows();
    Location loc = (ctrl && ctrl->HasChapters() && pageNo >= 1) ? ctrl->LocationFromPageNo(pageNo) : kInvalidLocation;
    RemoveFav(filePath, pageNo, loc);
    UpdateFavoritesTreeForAllWindows();
    ScheduleSaveSettings();
}

void RememberFavTreeExpansionState(MainWindow* win) {
    VecReset(win->expandedFavorites);
    FavTreeModel* tm = SidebarFavModel(win);
    if (!tm) {
        return;
    }
    TreeItem root = tm->Root();
    int n = tm->ChildCount(root);
    for (int i = 0; i < n; i++) {
        TreeItem ti = tm->ChildAt(root, i);
        auto* fti = (FavTreeItem*)ti;
        if (!fti->isExpanded) {
            continue;
        }
        FileState* f = GetByFavorite(fti->favorite);
        if (f) {
            VecAppend(win->expandedFavorites, f);
        }
    }
}

static void RememberFavTreeExpansionStateForAllWindows() {
    for (int i = 0; i < len(gWindows); i++) {
        RememberFavTreeExpansionState(gWindows[i]);
    }
}

// clang-format off
static MenuDef menuDefContextFav[] = {
    {
        TrN("Sort By Name"),
        CmdToggleFavoritesSort,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        {},
        0,
    },
};
// clang-format on

MenuModel* BuildFavContextMenu(MainWindow*, FavTreeItem* fti) {
    MenuModel* popup = BuildMenuFromDef(menuDefContextFav, nullptr);
    MenuSetChecked(popup, CmdToggleFavoritesSort, gSettings->sortFavoritesByName);
    if (!fti) {
        // Sort By Name works with no selection; Remove needs a favorite row.
        MenuRemove(popup, CmdFavoriteDel);
    }
    RemoveBadMenuSeparators(popup);
    return popup;
}

void FavContextMenuCommand(MainWindow*, FavTreeItem* fti, int cmd) {
    logf("FavContextMenuCommand: %d (%s) on '%s'\n", cmd, GetCommandName(cmd), fti ? fti->text : Str{});
    // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
    // so that we can do destructive operations without asking for permission via
    // invasive model dialog boxes but also allow reverting them if were done
    // by mistake
    if (CmdToggleFavoritesSort == cmd) {
        ToggleSortFavoritesByName();
        return;
    }
    if (CmdFavoriteDel == cmd && fti) {
        Favorite* toDelete = fti->favorite;
        FileState* f = GetByFavorite(toDelete);
        if (!f) {
            return;
        }
        if (fti->parent) {
            DelFavorite(f, toDelete);
            return;
        }
        RememberFavTreeExpansionStateForAllWindows();
        RemoveAllFavForFile(f->filePath);
        UpdateFavoritesTreeForAllWindows();
        ScheduleSaveSettings();
    }
}
