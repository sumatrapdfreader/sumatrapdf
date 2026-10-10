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
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "FileHistory.h"
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
#include "Tabs.h"
#include "Theme.h"
#include "FilterHighlightDraw.h"
#include "PagePosition.h"
#include "SidebarPanel.h"
#include "Favorites.h"

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

// Note: those might be too big
constexpr int kMaxFavSubmenus = 10;
constexpr int kMaxFavMenus = 10;

static void AppendFavMenuItems(HMENU m, FileState* f, Vec<FavMenuEntry>& favs, int& idx, bool combined,
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
        auto safeStr = MenuToSafeStringTemp(s);
        WCHAR* ws = CWStrTemp(safeStr);
        AppendMenuW(m, MF_STRING, (UINT_PTR)cmdId, ws);
    }
}

// For easy access, we try to show favorites in the menu, similar to a list of
// recently opened files.
// The first menu items are for currently opened file (up to kMaxFavMenus), based
// on the assumption that user is usually interested in navigating current file.
// Then we have a submenu for each file for which there are bookmarks (up to
// kMaxFavSubmenus), each having up to kMaxFavMenus menu items.
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

    int menusCount = len(filePathsSorted);
    menusCount = std::min(menusCount, kMaxFavMenus);

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
        HMENU sub = m;
        bool combined = (len(*f->favorites) == 1);
        if (!combined) {
            sub = CreateMenu();
        }
        AppendFavMenuItems(sub, f, favs, favIdx, combined, f == currFileFav);
        if (!combined) {
            Str s = Tr("Current file");
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
        DocController* ctrl = win->ctrl;
        int pageNo = win->currPageNo;
        bool isBookmarked = IsPageInFavorites(ctrl->GetFilePath(), pageNo, ctrl);

        TempStr addText;
        TempStr delText;
        if (ctrl->HasChapters()) {
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
            TempStr s = AppendAccelKeyToMenuStringTemp(addText, CmdFavoriteAdd);
            MenuSetText(menu, CmdFavoriteAdd, s);
        }
        AppendFavMenus(menu, ctrl->GetFilePath());
    }
    MenuSetEnabled(menu, CmdFavoriteToggle, HasFavorites());
}

void GoToFavoritePage(MainWindow* win, Str pageNo, PointF scrollPos) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ApplyFavoriteView(win, pageNo, scrollPos, true);
    // we might have been invoked by clicking on a tree view
    // switch focus so that keyboard navigation works, which enables
    // a fluid experience
    win->Focus();
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

    LoadArgs args(fs->filePath, win);
    win = LoadDocument(&args);
    if (win && pageNo) {
        auto* data = new GoToFavoritePageData;
        data->pageNo = str::Dup(pageNo);
        data->scrollPos = scrollPos;
        data->win = win;
        auto fn = MkFunc0<GoToFavoritePageData>(GoToFavoritePage, data);
        uitask::Post(fn, "TaskGoToFavorite2");
    }
}

void GoToFavForTreeItem(MainWindow* win, TreeItem ti) {
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
    if (!win) {
        return;
    }
    EditSetFocus(win->favFilterEdit);
    EditSetCursorPosAtEnd(win->favFilterEdit);
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
    LayoutSidebarPanel(win->favoritesTabPanel);
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
                    edit->SetText(StrL(""));
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

// in a sidebar panel (independent of the Favorites tab)
void ToggleFavorites(MainWindow* win) {
    if (IsSidebarViewShown(win, SidebarView::Favorites)) {
        HideSidebarView(win, SidebarView::Favorites);
        return;
    }
    ShowSidebarView(win, SidebarView::Favorites);
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
        if (IsSidebarViewShown(win, SidebarView::Favorites)) {
            HideSidebarView(win, SidebarView::Favorites);
        } else {
            ScheduleUiUpdate(win, kUiForceRelayout | kUiSidebarDirty);
        }
        return;
    }
    // refresh the sidebar only when a panel is supposed to show favorites
    if (IsSidebarViewShown(win, SidebarView::Favorites)) {
        ApplySidebarPanels(win);
    } else if (FindFavoritesTab(win)) {
        ScheduleUiUpdate(win, kUiForceRelayout | kUiSidebarDirty);
    }
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
    TempStr pageLabel;
    if (ShowChapterUi(ctrl)) {
        Location loc = ctrl->LocationFromPageNo(pageNo);
        pageLabel = fmt("%d/%d", loc.chapter, loc.page);
    } else {
        pageLabel = ctrl->GetPageLabeTemp(pageNo);
    }
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel, name);
}

void RememberFavTreeExpansionState(MainWindow* win) {
    VecReset(win->expandedFavorites);
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
            VecAppend(win->expandedFavorites, f);
        }
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

// Repaint favorite labels that contain a source file name so only that span is
// bold. The same pass preserves multi-word search highlights when filtering.
static void DrawFavItemText(TreeView::CustomDrawEvent* ev, MainWindow* win) {
    FavTreeItem* fti = (FavTreeItem*)ev->treeItem;
    if (!fti || len(fti->text) == 0) {
        return;
    }
    StrVec words;
    GetFavFilterWords(win, words);
    bool hasFileName = fti->fileNameOffset >= 0 && fti->fileNameLen > 0;
    if (len(words) == 0 && !hasFileName) {
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
    int right = std::min(itemRect.x + itemRect.dx, (int)cd->rc.right);
    labelRect.dx = std::max(0, right - labelRect.x);
    if (labelRect.IsEmpty()) {
        return;
    }
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
    DrawTreeItemFilterHighlight(&gfx, labelRect, fti->text, words, bgCol, txtCol, tv->GetFont(), fti->fileNameOffset,
                                fti->fileNameLen);
    if ((cd->uItemState & CDIS_FOCUS) && isSelected && hasFocus) {
        gfx.DrawFocusRect(labelRect);
    }
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
        FavTreeItem* fti = (FavTreeItem*)ev->treeItem;
        bool hasFileName = fti->fileNameOffset >= 0 && fti->fileNameLen > 0;
        LRESULT res = 0;
        if (filterActive || hasFileName) {
            res |= CDRF_NOTIFYPOSTPAINT;
        }
        ev->result = res;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPOSTPAINT) {
        FavTreeItem* fti = (FavTreeItem*)ev->treeItem;
        bool hasFileName = fti->fileNameOffset >= 0 && fti->fileNameLen > 0;
        if ((filterActive || hasFileName) && win) {
            DrawFavItemText(ev, win);
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
            win->favFilterEdit->SetText(StrL(""));
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
    MenuSetChecked(popup, CmdToggleFavoritesSort, gSettings->sortFavoritesByName);
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
        FavTreeItem* fti = (FavTreeItem*)ti;
        Favorite* toDelete = fti->favorite;
        FileState* f = GetByFavorite(toDelete);
        if (fti->parent) {
            DelFavorite(f, toDelete);
        } else {
            RememberFavTreeExpansionStateForAllWindows();
            RemoveAllFavForFile(f->filePath);
            UpdateFavoritesTreeForAllWindows();
            ScheduleSaveSettings();
        }
    }
}

// The Favorites view; a sidebar panel or the Favorites tab shows it (SidebarPanel.cpp)
void CreateFavorites(MainWindow* win) {
    HWND parent = win->sidebarBottom->hwnd;
    auto* filterEdit = new Edit();
    {
        Edit::CreateArgs eargs;
        eargs.parent = parent;
        eargs.withBorder = true;
        eargs.cueText = Tr("Search Favorites");
        eargs.font = GetAppFont();
        filterEdit->Create(eargs);
    }
    win->favFilterEdit = filterEdit;
    filterEdit->onTextChanged = MkFunc0(OnFavFilterTextChanged, win);
    SetWindowSubclass(filterEdit->hwnd, WndProcFavFilterEdit, NextSubclassId(), (DWORD_PTR)win);

    auto* treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent = parent;
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

    // the filter edit over the tree, which takes the remaining height
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(filterEdit);
    vbox->AddChild(new Spacer(0, 2)); // gap under the search field
    vbox->AddChild(treeView, 1);
    win->favViewLayout = vbox;

    UpdateControlsColors(win);
}
