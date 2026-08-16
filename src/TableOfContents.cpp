/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/BitManip.h"
#include "gui/Dpi.h"
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
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "DisplayModel.h"
#include "Favorites.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Translations.h"
#include "Tabs.h"
#include "Menu.h"
#include "Accelerators.h"
#include "Theme.h"
#include "FilterHighlightDraw.h"

static void LayoutTocContainer(MainWindow* win);

// When true, multi-highlight every TOC item that matches the current page
// (issue #4642). Easy to flip for comparison with single-selection behavior.
bool gShowAllMatchingTOC = true;

// set tooltip for this item but only if the text isn't fully shown
// TODO: I might have lost something in translation
static void TocCustomizeTooltip(TreeView::GetTooltipEvent* ev) {
    auto* treeView = ev->treeView;
    auto ti = ev->treeItem;
    auto* nm = ev->info;
    TocItem* tocItem = (TocItem*)ti;
    IPageDestination* link = tocItem->GetPageDestination();
    if (!link) {
        return;
    }
    Str path = PageDestGetValue(link);
    if (!path) {
        path = tocItem->title;
    }
    if (!path) {
        return;
    }
    const auto* k = link->GetKind();
    // TODO: TocItem from Chm contain other types
    // we probably shouldn't set TocItem::dest there
    if (k == kindDestinationScrollTo) {
        return;
    }
    if (k == kindDestinationNone) {
        return;
    }

    bool isOk = (k == kindDestinationLaunchURL) || (k == kindDestinationLaunchFile) ||
                (k == kindDestinationLaunchEmbedded) || (k == kindDestinationMupdf) || (k == kindDestinationDjVu) ||
                (k == kindDestinationAttachment);
    ReportIf(!isOk);

    str::Builder infotip;

    // Display the item's full label, if it's overlong
    Rect rcLine, rcLabel;
    treeView->GetItemRect(ev->treeItem, false, rcLine);
    treeView->GetItemRect(ev->treeItem, true, rcLabel);

    // TODO: this causes a duplicate. Not sure what changed
    if (false && rcLine.x + rcLine.dx + 2 < rcLabel.x + rcLabel.dx) {
        Str currInfoTip = treeView->treeModel->Text(ti);
        infotip.Append(currInfoTip);
        infotip.Append("\r\n");
    }

    if (kindDestinationLaunchEmbedded == k || kindDestinationAttachment == k) {
        TempStr tmp = fmt(_TRA("Attachment: %s").s, path);
        infotip.Append(tmp);
    } else {
        infotip.Append(path);
    }

    str::BufSet(nm->pszText, nm->cchTextMax, ToStr(infotip));
}

// Deferred TOC navigation must not hold raw TocItem* / IPageDestination*
// pointers: the TOC tree can be rebuilt or freed before the uitask runs
// (while tab->ctrl still matches), which caused UAF in HandleLink
// (crash 8bfe7adb1000001: EngineMupdf::HandleLink / dest->GetKind).

// Own a stable copy of the destination at post time. Engine-private kinds
// that hold fz_outline/fz_link (mupdf) are converted to scrollTo with
// page/rect/zoom already resolved on the TocItem/dest.
static IPageDestination* SnapshotDestForDeferredNav(IPageDestination* dest, int tocPageNo) {
    if (!dest) {
        return nullptr;
    }
    Kind k = dest->GetKind();
    if (k == kindDestinationLaunchURL) {
        Str url = ((PageDestinationURL*)dest)->url;
        if (!url) {
            url = PageDestGetValue(dest);
        }
        return url ? new PageDestinationURL(url) : nullptr;
    }
    if (k == kindDestinationLaunchFile) {
        auto* f = (PageDestinationFile*)dest;
        auto* copy = new PageDestinationFile(f->path, f->dest);
        copy->openInNewWindow = f->openInNewWindow;
        copy->rect = f->rect;
        return copy;
    }
    if (k == kindDestinationLaunchEmbedded || k == kindDestinationAttachment) {
        auto* p = (PageDestination*)dest;
        auto* copy = new PageDestination();
        copy->kind = k;
        copy->pageNo = p->pageNo;
        copy->rect = p->rect;
        copy->zoom = p->zoom;
        copy->value = str::Dup(p->value);
        copy->name = str::Dup(p->name);
        copy->embedObjNum = p->embedObjNum;
        return copy;
    }
    if (k == kindDestinationScrollTo) {
        int pageNo = PageDestGetPageNo(dest);
        if (pageNo <= 0) {
            pageNo = tocPageNo;
        }
        if (pageNo < 1) {
            logf("SnapshotDestForDeferredNav: skip scrollTo pageNo=%d (tocPageNo=%d)\n", PageDestGetPageNo(dest),
                 tocPageNo);
            return nullptr;
        }
        auto* copy = new PageDestination();
        copy->kind = k;
        copy->pageNo = pageNo;
        copy->rect = PageDestGetRect(dest);
        copy->zoom = PageDestGetZoom(dest);
        copy->value = str::Dup(PageDestGetValue(dest));
        copy->name = str::Dup(PageDestGetName(dest));
        return copy;
    }
    // mupdf, djvu, none → page navigation snapshot
    int pageNo = PageDestGetPageNo(dest);
    if (pageNo <= 0) {
        pageNo = tocPageNo;
    }
    if (pageNo < 1) {
        Str val = PageDestGetValue(dest);
        if (val && IsExternalUrl(val)) {
            return new PageDestinationURL(val);
        }
        logf("SnapshotDestForDeferredNav: skip dest kind pageNo=%d (tocPageNo=%d)\n", PageDestGetPageNo(dest),
             tocPageNo);
        return nullptr;
    }
    RectF r = PageDestGetRect(dest);
    float zoom = PageDestGetZoom(dest);
    if (k == kindDestinationMupdf) {
        // Prefer resolved anchor; outline x/y can be 0 and scroll to the wrong place
        RectF pt = PageDestGetDestPoint(dest);
        if ((r.dx == 0 && r.dy == 0) || (r.dx == kDestUseDefault && r.dy == kDestUseDefault)) {
            if (pt.x != 0 || pt.y != 0 || r.IsEmpty()) {
                r = RectF{pt.x, pt.y, kDestUseDefault, kDestUseDefault};
            }
        }
        zoom = dest->GetZoom2();
    }
    return NewSimpleDest(pageNo, r, zoom);
}

#if defined(DEBUG)
bool TableOfContents_UnitTestSnapshotNamedDest() {
    PageDestination source;
    source.kind = kindDestinationScrollTo;
    source.pageNo = 1;
    source.rect = RectF(2, 3, 4, 5);
    source.zoom = 125;
    source.value = str::Dup(StrL("value"));
    source.name = str::Dup(StrL("https://sumatrapdf.md/issue-5842.html#target-heading"));

    IPageDestination* snapshot = SnapshotDestForDeferredNav(&source, 7);
    bool ok = snapshot && snapshot->GetKind() == kindDestinationScrollTo && PageDestGetPageNo(snapshot) == 1 &&
              PageDestGetRect(snapshot) == source.rect && PageDestGetZoom(snapshot) == source.zoom &&
              str::Eq(PageDestGetValue(snapshot), source.value) && str::Eq(PageDestGetName(snapshot), source.name);
    delete snapshot;
    return ok;
}
#endif

static TocItem* FindTocItemByTitlePage(TocItem* item, Str title, int pageNo) {
    for (; item; item = item->next) {
        if (pageNo > 0 && item->pageNo != pageNo) {
            // keep searching children; same page can nest under different titles
        } else if (title && item->title && str::Eq(title, item->title)) {
            if (pageNo <= 0 || item->pageNo == pageNo) {
                return item;
            }
        } else if (!title && pageNo > 0 && item->pageNo == pageNo) {
            return item;
        }
        TocItem* found = FindTocItemByTitlePage(item->child, title, pageNo);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

struct GoToTocLinkData {
    WindowTab* tab = nullptr;
    DocController* ctrl = nullptr;
    // owned snapshot; may be null (then pageNo alone is used)
    IPageDestination* dest = nullptr;
    int pageNo = 0;
    // owned; used to re-select in tree after palette-driven nav
    Str title;
    // true when the navigation was driven from outside the tree (e.g. the
    // command palette), so afterwards we must move the tree's selection to the
    // item ourselves. For tree-driven navigation the tree is already selected.
    bool selectInTree = false;

    ~GoToTocLinkData() {
        delete dest;
        str::Free(title);
    }
};

// URL / file / embedded targets keep pageNo = -1 by design; only page-nav dests need pageNo >= 1.
static bool DestNeedsValidPageNo(IPageDestination* dest) {
    if (!dest) {
        return false;
    }
    Kind k = dest->GetKind();
    return k != kindDestinationLaunchURL && k != kindDestinationLaunchFile && k != kindDestinationLaunchEmbedded &&
           k != kindDestinationAttachment;
}

static GoToTocLinkData* NewGoToTocLinkData(MainWindow* win, TocItem* tocItem, bool selectInTree) {
    int pageNo = tocItem->pageNo;
    IPageDestination* dest = SnapshotDestForDeferredNav(tocItem->GetPageDestination(), pageNo);

    // drop page-navigation destinations that still have no valid page
    if (dest && DestNeedsValidPageNo(dest) && PageDestGetPageNo(dest) < 1) {
        logf("NewGoToTocLinkData: skip dest with pageNo=%d\n", PageDestGetPageNo(dest));
        delete dest;
        dest = nullptr;
    }

    // nothing to navigate to: no dest and no valid page number
    if (!dest && pageNo < 1) {
        logf("NewGoToTocLinkData: skip toc item pageNo=%d title='%s'\n", pageNo, tocItem->title);
        return nullptr;
    }

    auto* data = new GoToTocLinkData;
    data->ctrl = win->ctrl;
    data->tab = win->CurrentTab();
    data->pageNo = pageNo;
    data->dest = dest;
    data->selectInTree = selectInTree;
    if (selectInTree && tocItem->title) {
        data->title = str::Dup(tocItem->title);
    }
    return data;
}

static void GoToTocLink(GoToTocLinkData* d) {
    AutoDelete delData(d);

    auto* tab = d->tab;
    auto* ctrl = d->ctrl;

    // validate tab before dereferencing — it may have been freed
    // while this task was queued (e.g. user closed the tab/window)
    if (!IsWindowTabValid(tab)) {
        return;
    }
    MainWindow* win = tab->win;
    // destination snapshot is invalid if the DocController has been replaced
    if (!IsMainWindowValid(win) || win->CurrentTab() != tab || tab->ctrl != ctrl) {
        return;
    }

    // make sure that the tree item that the user selected
    // isn't unselected in UpdateTocSelection right again
    win->tocKeepSelection = true;
    if (d->dest) {
        ctrl->HandleLink(d->dest, win->linkHandler);
    } else if (d->pageNo > 0) {
        ctrl->GoToPage(d->pageNo, true);
    }
    win->tocKeepSelection = false;

    // when driven from the command palette the tree wasn't the source of the
    // navigation, so the page-based UpdateTocSelection was suppressed above and
    // the tree still shows the old item. Move the selection to this item now
    // (programmatic SelectItem doesn't re-navigate -- see TocTreeSelectionChanged).
    if (d->selectInTree && win->tocLoaded && win->tocTreeView) {
        TocTree* tree = tab->currToc;
        TocItem* tocItem = nullptr;
        if (tree && tree->root) {
            tocItem = FindTocItemByTitlePage(tree->root, d->title, d->pageNo);
        }
        if (tocItem) {
            TreeView* treeView = win->tocTreeView;
            HTREEITEM hi = treeView->GetHandleByTreeItem((TreeItem)tocItem);
            if (hi) {
                TreeView_EnsureVisible(treeView->hwnd, hi);
            }
            treeView->SelectItem((TreeItem)tocItem);
        }
    }
}

// navigate to a TocItem regardless of whether it points to a page in this
// document or to an external destination (used by the command palette, where
// the user explicitly picked the item so we always honor it)
// navigate to a TocItem (used by the command palette's TOC mode)
void GoToTocItem(MainWindow* win, TocItem* tocItem) {
    if (!win || !tocItem) {
        return;
    }
    auto* data = NewGoToTocLinkData(win, tocItem, true);
    if (!data) {
        return;
    }
    auto fn = MkFunc0<GoToTocLinkData>(GoToTocLink, data);
    uitask::Post(fn, "TaskGoToTocFromPalette");
}

static bool IsScrollToLink(IPageDestination* link) {
    if (!link) {
        return false;
    }
    const auto* kind = link->GetKind();
    return kind == kindDestinationScrollTo;
}

static void GoToTocTreeItem(MainWindow* win, TreeItem ti, bool allowExternal) {
    if (!ti) {
        return;
    }
    TocItem* tocItem = (TocItem*)ti;
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->GetPageDestination());
    if (validPage || (allowExternal || isScroll)) {
        // delay changing the page until the tree messages have been handled
        auto* data = NewGoToTocLinkData(win, tocItem, false);
        if (!data) {
            return;
        }
        auto fn = MkFunc0<GoToTocLinkData>(GoToTocLink, data);
        uitask::Post(fn, "TaskGoToTocTreeItem");
    }
}

void ClearTocBox(MainWindow* win) {
    if (!win->tocLoaded) {
        return;
    }

    // set tocLoaded to false before SetText("") because SetText triggers
    // EN_CHANGE synchronously which calls ApplyTocFilter() re-entrantly
    // and we need it to bail out early
    win->tocLoaded = false;

    win->tocTreeView->Clear();
    win->tocMatchingItems.Reset();

    // clear filter state
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;
    if (win->tocFilterEdit) {
        win->tocFilterEdit->SetText("");
    }

    win->currPageNo = 0;
}

void ToggleTocBox(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    if (win->uiState.tocVisible) {
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
        return;
    }
    SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    if (win->uiState.tocVisible) {
        HwndSetFocus(win->tocTreeView->hwnd);
    }
}

struct VistorForPageNoData {
    int pageNo = -1;

    TocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;
    int nItems = 0;
};

static void visitTree(VistorForPageNoData* d, TreeItemVisitorData* vd) {
    auto* tocItem = (TocItem*)vd->item;
    if (!tocItem) {
        return;
    }
    if (!d->bestMatch) {
        // if nothing else matches, match the root node
        d->bestMatch = tocItem;
    }
    ++d->nItems;
    int page = tocItem->pageNo;
    if ((page <= d->pageNo) && (page >= d->bestMatchPageNo) && (page >= 1)) {
        d->bestMatch = tocItem;
        d->bestMatchPageNo = page;
        if (d->pageNo == d->bestMatchPageNo) {
            // we can stop earlier if we found the exact match
            vd->stopTraversal = true;
            return;
        }
    }
}

// find the closest item in tree view to a given page number
static TocItem* TreeItemForPageNo(TreeView* treeView, int pageNo) {
    TreeModel* tm = treeView->treeModel;
    if (!tm) {
        return nullptr;
    }
    VistorForPageNoData d;
    d.pageNo = pageNo;
    auto fn = MkFunc1<VistorForPageNoData, TreeItemVisitorData*>(visitTree, &d);
    VisitTreeModelItems(tm, fn);
    // if there's only one item, we want to unselect it so that it can
    // be selected by the user
    if (d.nItems < 2) {
        return nullptr;
    }
    return d.bestMatch;
}

struct CollectSamePageData {
    int pageNo = 0;
    Vec<TocItem*>* out = nullptr;
};

static void visitCollectSamePage(CollectSamePageData* d, TreeItemVisitorData* vd) {
    auto* tocItem = (TocItem*)vd->item;
    if (!tocItem || tocItem->pageNo < 1) {
        return;
    }
    if (tocItem->pageNo == d->pageNo) {
        d->out->Append(tocItem);
    }
}

static bool TocMatchingItemsContains(const Vec<TocItem*>& items, TocItem* item) {
    for (TocItem* t : items) {
        if (t == item) {
            return true;
        }
    }
    return false;
}

// Fill win->tocMatchingItems with every entry that should look "current" for
// bestMatch: all TOC items on the same page, plus the ancestor chain (so a
// nested 6 / 6.1 / 6.1.1 path all highlight together). TreeView still has only
// one selection; extras are painted in OnTocCustomDraw when gShowAllMatchingTOC.
static void SetTocMultiHighlight(MainWindow* win, TreeView* treeView, TocItem* bestMatch) {
    win->tocMatchingItems.Reset();
    if (!gShowAllMatchingTOC || !bestMatch || !treeView) {
        return;
    }

    // All bookmarks that point at the same page as the best match (the issue's
    // "subsequent" same-page entries that TreeView single-select cannot show).
    if (bestMatch->pageNo >= 1 && treeView->treeModel) {
        CollectSamePageData d;
        d.pageNo = bestMatch->pageNo;
        d.out = &win->tocMatchingItems;
        auto fn = MkFunc1<CollectSamePageData, TreeItemVisitorData*>(visitCollectSamePage, &d);
        VisitTreeModelItems(treeView->treeModel, fn);
    }

    // Ancestor chain (chapter → section → subsection), including bestMatch.
    for (TocItem* p = bestMatch; p; p = p->parent) {
        if (!TocMatchingItemsContains(win->tocMatchingItems, p)) {
            win->tocMatchingItems.Append(p);
        }
    }

    // TreeView selection paint won't cover the extra matches; repaint so
    // OnTocCustomDraw can draw them.
    if (treeView->hwnd) {
        HwndInvalidate(treeView->hwnd, true);
    }
}

static bool TocItemIsMultiHighlight(MainWindow* win, TocItem* item) {
    if (!gShowAllMatchingTOC || !win || !item) {
        return false;
    }
    return TocMatchingItemsContains(win->tocMatchingItems, item);
}

// TODO: I can't use TreeItem->IsExpanded() because it's not in sync with
// the changes user makes to TreeCtrl
static TocItem* FindVisibleParentTreeItem(TreeView* treeView, TocItem* ti) {
    if (!ti) {
        return nullptr;
    }
    while (true) {
        auto* parent = ti->parent;
        if (parent == nullptr) {
            // ti is a root node
            return ti;
        }
        if (treeView->IsExpanded((TreeItem)parent)) {
            return ti;
        }
        ti = parent;
    }
    return nullptr;
}

void UpdateTocSelection(MainWindow* win, int currPageNo) {
    auto* treeView = win->tocTreeView;
    if (!win->tocLoaded || !win->uiState.tocVisible || !treeView) {
        return;
    }

    // Browser (WebView2) markdown/HTML docs render a whole file as a single
    // "page" and we can't detect which heading is scrolled into view, so a
    // page-based update would select/highlight every heading in the file. Skip
    // it and leave the TOC selection wherever the user's last click put it.
    if (win->ctrl && win->ctrl->AsMarkdown()) {
        return;
    }

    auto* item = TreeItemForPageNo(treeView, currPageNo);
    if (win->tocKeepSelection) {
        // the tree selection is deliberately left alone: the user clicked a
        // bookmark and GoToTocLink set tocKeepSelection so the page change
        // doesn't move the selection off it. The multi-match "current page"
        // highlight is a different thing though and must still follow the page,
        // otherwise the previous page's highlight stays until the sidebar is
        // rebuilt.
        SetTocMultiHighlight(win, treeView, item);
        return;
    }

    // only select the items that are visible i.e. are top nodes or
    // children of expanded node
    TreeItem toSelect = (TreeItem)FindVisibleParentTreeItem(treeView, item);
    treeView->SelectItem(toSelect);
    SetTocMultiHighlight(win, treeView, item);
}

// expand the table of contents tree down to the entry matching the current
// page, then select and scroll to it (issue #1998, like Explorer's
// "Expand to current folder")
void ExpandTocToCurrentPage(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    // make sure the bookmarks (table of contents) sidebar is visible
    if (!win->uiState.tocVisible) {
        SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    }
    if (!win->tocLoaded || !win->uiState.tocVisible) {
        return;
    }
    TreeView* treeView = win->tocTreeView;
    int currPageNo = win->ctrl->CurrentPageNo();
    TocItem* item = TreeItemForPageNo(treeView, currPageNo);
    if (!item) {
        return;
    }
    HTREEITEM hi = treeView->GetHandleByTreeItem((TreeItem)item);
    if (!hi) {
        return;
    }
    // TreeView_EnsureVisible expands any collapsed ancestors and scrolls the
    // item into view, which is exactly the "expand to current page" behavior
    TreeView_EnsureVisible(treeView->hwnd, hi);
    treeView->SelectItem((TreeItem)item);
    SetTocMultiHighlight(win, treeView, item);
    HwndSetFocus(treeView->hwnd);
}

static void UpdateDocTocExpansionStateRecur(TreeView* treeView, Vec<int>& tocState, TocItem* tocItem) {
    while (tocItem) {
        // items without children cannot be toggled
        if (tocItem->child) {
            // we have to query the state of the tree view item because
            // isOpenToggled is not kept in sync
            // TODO: keep toggle state on TocItem in sync
            // by subscribing to the right notifications
            bool isExpanded = treeView->IsExpanded((TreeItem)tocItem);
            bool wasToggled = isExpanded != tocItem->isOpenDefault;
            if (wasToggled) {
                tocState.Append(tocItem->id);
            }
            UpdateDocTocExpansionStateRecur(treeView, tocState, tocItem->child);
        }
        tocItem = tocItem->next;
    }
}

void UpdateTocExpansionState(Vec<int>& tocState, TreeView* treeView, TocTree* docTree) {
    if (treeView->treeModel != docTree) {
        // CrashMe();
        return;
    }
    tocState.Reset();
    TocItem* tocItem = docTree->root->child;
    UpdateDocTocExpansionStateRecur(treeView, tocState, tocItem);
}

static bool inRange(WCHAR c, WCHAR low, WCHAR hi) {
    return (low <= c) && (c <= hi);
}

// copied from mupdf/fitz/dev_text.c
// clang-format off
static bool isLeftToRightChar(WCHAR c) {
    return (
        inRange(c, 0x0041, 0x005A) ||
        inRange(c, 0x0061, 0x007A) ||
        inRange(c, 0xFB00, 0xFB06)
    );
}

static bool isRightToLeftChar(WCHAR c) {
    return (
        inRange(c, 0x0590, 0x05FF) ||
        inRange(c, 0x0600, 0x06FF) ||
        inRange(c, 0x0750, 0x077F) ||
        inRange(c, 0xFB50, 0xFDFF) ||
        inRange(c, 0xFE70, 0xFEFE)
    );
}
// clang-format off

static void GetLeftRightCounts(TocItem* node, int& l2r, int& r2l) {
next:
    if (!node) {
        return;
    }
    // short-circuit because this could overflow the stack due to recursion
    // (happened in doc from https://github.com/sumatrapdfreader/sumatrapdf/issues/1795)
    if (l2r + r2l > 1024) {
        return;
    }
    if (node->title) {
        TempWStr ws = ToWStrTemp(node->title);
        for (int i = 0; i < ws.len; i++) {
            WCHAR c = ws.s[i];
            if (isLeftToRightChar(c)) {
                l2r++;
            } else if (isRightToLeftChar(c)) {
                r2l++;
            }
        }
    }
    GetLeftRightCounts(node->child, l2r, r2l);
    // could be: GetLeftRightCounts(node->next, l2r, r2l);
    // but faster if not recursive
    node = node->next;
    goto next;
}

static void SetInitialExpandState(TocItem* item, Vec<int>& tocState) {
    while (item) {
        item->isOpenToggled = tocState.Contains(item->id);
        SetInitialExpandState(item->child, tocState);
        item = item->next;
    }
}

static void AddFavoriteFromToc(MainWindow* win, TocItem* dti) {
    int pageNo = 0;
    if (!dti) {
        return;
    }
    if (dti->dest) {
        pageNo = PageDestGetPageNo(dti->dest);
    }
    Str name = dti->title;
    TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel, name);
}

static void SaveAttachment(WindowTab* tab, Str fileName, int attachmentNo) {
    EngineBase* engine = tab->AsFixed()->GetEngine();
    Str data = EngineMupdfLoadAttachment(engine, attachmentNo);
    if (len(data) == 0) {
        return;
    }
    TempStr dir = path::GetDirTemp(tab->filePath);
    fileName = path::GetBaseNameTemp(fileName);
    TempStr dstPath = path::JoinTemp(dir, fileName);
    SaveDataToFile(tab->win->hwndFrame, dstPath, data);
    str::Free(data);
}

static void OpenAttachment(WindowTab* tab, Str fileName, int attachmentNo) {
    EngineBase* engine = tab->AsFixed()->GetEngine();
    Str data = EngineMupdfLoadAttachment(engine, attachmentNo);
    if (len(data) == 0) {
        return;
    }
    MainWindow* win = tab->win;
    EngineBase* newEngine = CreateEngineMupdfFromData(data, fileName, nullptr);
    DocController* ctrl = CreateControllerForEngineOrFile(newEngine, nullptr, nullptr, win);
    LoadArgs* args = new LoadArgs(tab->filePath, win);
    args->SetDisplayName(fileName);
    args->ctrl = ctrl;
    LoadDocumentFinish(args);
    str::Free(data);
}

static void OpenEmbeddedFile(WindowTab* tab, IPageDestination* dest) {
    ReportIf(!tab || !dest);
    if (!tab || !dest) {
        return;
    }
    MainWindow* win = tab->win;
    PageDestinationFile *destFile = (PageDestinationFile*)dest;
    Str path = destFile->path;
    Str tabPath = tab->filePath;
    if (!str::StartsWith(path, tabPath)) {
        return;
    }
    LoadArgs args(path, win);
    args.activateExisting = true;
    args.activateExistingInWindow = true;
    LoadDocument(&args);
}

static void SaveEmbeddedFile(WindowTab* tab, Str srcPath, Str fileName) {
    Str data = LoadEmbeddedPDFFile(srcPath);
    if (len(data) == 0) {
        // TODO: show an error message
        return;
    }
    TempStr dir = path::GetDirTemp(tab->filePath);
    fileName = path::GetBaseNameTemp(fileName);
    TempStr dstPath = path::JoinTemp(dir, fileName);
    SaveDataToFile(tab->win->hwndFrame, dstPath, data);
    str::Free(data);
}

// Expand outline nodes whose depth is < maxDepth (depth 1 = top-level rows).
// Call after a full collapse so the tree ends at exactly that level.
static void TocExpandItemsToDepth(HWND hwnd, HTREEITEM item, int depth, int maxDepth) {
    while (item) {
        if (depth < maxDepth) {
            TreeView_Expand(hwnd, item, TVE_EXPAND);
            HTREEITEM child = TreeView_GetChild(hwnd, item);
            if (child) {
                TocExpandItemsToDepth(hwnd, child, depth + 1, maxDepth);
            }
        }
        item = TreeView_GetNextSibling(hwnd, item);
    }
}

// Expand outline only through `level` (1 = top-level rows collapsed, 2 = expand
// top-level once, 3 = two levels deep). Issue #5239.
static void TocExpandToLevel(TreeView* tv, int level) {
    if (!tv || !tv->hwnd || level < 1) {
        return;
    }
    HWND hwnd = tv->hwnd;
    tv->SuspendRedraw();
    HTREEITEM root = TreeView_GetRoot(hwnd);
    TreeViewExpandRecursively(hwnd, root, TVE_COLLAPSE, false);
    if (level > 1) {
        TocExpandItemsToDepth(hwnd, root, 1, level);
    }
    tv->ResumeRedraw();
}

// Collapse all; if there is a single top-level entry with children (typical
// Word-export TOC), expand it one level so Collapse All is useful (#5239).
static void TocCollapseAll(TreeView* tv) {
    if (!tv || !tv->hwnd) {
        return;
    }
    TocExpandToLevel(tv, 1);
    HWND hwnd = tv->hwnd;
    HTREEITEM root = TreeView_GetRoot(hwnd);
    if (root && !TreeView_GetNextSibling(hwnd, root) && TreeView_GetChild(hwnd, root)) {
        TreeView_Expand(hwnd, root, TVE_EXPAND);
    }
}

// Collapse every outline row that shares the parent of `ti` (same nesting level
// / siblings). If `ti` is null, use the current selection; if still none, all
// top-level rows. Issue #1895.
static void TocCollapseSameLevel(TreeView* tv, TreeItem ti) {
    if (!tv || !tv->hwnd) {
        return;
    }
    HWND hwnd = tv->hwnd;
    HTREEITEM hItem = TreeModel::kNullItem != ti ? tv->GetHandleByTreeItem(ti) : nullptr;
    if (!hItem) {
        hItem = TreeView_GetSelection(hwnd);
    }
    HTREEITEM first = nullptr;
    if (hItem) {
        HTREEITEM parent = TreeView_GetParent(hwnd, hItem);
        first = parent ? TreeView_GetChild(hwnd, parent) : TreeView_GetRoot(hwnd);
    } else {
        first = TreeView_GetRoot(hwnd);
    }
    if (!first) {
        return;
    }
    tv->SuspendRedraw();
    for (HTREEITEM sibling = first; sibling; sibling = TreeView_GetNextSibling(hwnd, sibling)) {
        if (TreeView_GetChild(hwnd, sibling)) {
            TreeView_Expand(hwnd, sibling, TVE_COLLAPSE);
        }
    }
    tv->ResumeRedraw();
}

// clang-format off
static MenuDef menuDefContextToc[] = {
    {
        _TRN("Expand All"),
        CmdExpandAll,
    },
    {
        _TRN("Collapse All"),
        CmdCollapseAll,
    },
    {
        _TRN("Expand to Level 1"),
        CmdTocExpandToLevel1,
    },
    {
        _TRN("Expand to Level 2"),
        CmdTocExpandToLevel2,
    },
    {
        _TRN("Expand to Level 3"),
        CmdTocExpandToLevel3,
    },
    {
        _TRN("Collapse Same Level"),
        CmdTocCollapseSameLevel,
    },
    {
        _TRN("Expand to Current Page"),
        CmdExpandToCurrentPage,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Open Embedded PDF"),
        CmdOpenEmbeddedPDF,
    },
    {
        _TRN("Save Embedded File..."),
        CmdSaveEmbeddedFile,
    },
    {
        _TRN("Open Attachment"),
        CmdOpenAttachment,
    },
    {
        _TRN("Save Attachment..."),
        CmdSaveAttachment,
    },
    // note: strings cannot be "" or else items are not there
    {
        "Add to favorites",
        CmdFavoriteAdd,
    },
    {
        "Remove from favorites",
        CmdFavoriteDel,
    },
    {
        nullptr,
        0,
    },
};
// clang-format on

static void TocContextMenu(ContextMenuEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    Str filePath = win->ctrl->GetFilePath();

    Point pt{};

    TreeItem ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (ti == TreeModel::kNullItem) {
        pt = {ev->mouseScreen.x, ev->mouseScreen.y};
    }
    int pageNo = 0;
    TocItem* dti = (TocItem*)ti;
    IPageDestination* dest = dti ? dti->dest : nullptr;
    if (dest) {
        pageNo = PageDestGetPageNo(dti->dest);
    }

    WindowTab* tab = win->CurrentTab();
    HMENU popup = BuildMenuFromDef(menuDefContextToc, CreatePopupMenu(), nullptr);

    Str path;
    Str fileName;
    Kind destKind = dest ? dest->GetKind() : nullptr;

    // TODO: this is pontentially not used at all
    if (destKind == kindDestinationLaunchEmbedded) {
        auto* embeddedFile = (PageDestinationFile*)dest;
        // this is a path to a file on disk, e.g. a path to opened PDF
        // with the embedded stream number
        path = embeddedFile->path;
        // this is name of the file as set inside PDF file
        fileName = PageDestGetName(dest);
        bool canOpenEmbedded = str::EndsWithI(fileName, StrL(".pdf"));
        if (!canOpenEmbedded) {
            MenuRemove(popup, CmdOpenEmbeddedPDF);
        }
    } else {
        // TODO: maybe move this to BuildMenuFromMenuDef
        MenuRemove(popup, CmdSaveEmbeddedFile);
        MenuRemove(popup, CmdOpenEmbeddedPDF);
    }

    int attachmentNo = -1;
    if (destKind == kindDestinationAttachment) {
        auto* attachment = (PageDestinationFile*)dest;
        // this is a path to a file on disk, e.g. a path to opened PDF
        // with the embedded stream number
        path = attachment->path;
        // this is name of the file as set inside PDF file
        fileName = PageDestGetName(dest);
        // hack: attachmentNo is saved in pageNo see
        // PdfLoadAttachments and DestFromAttachment
        attachmentNo = pageNo;
        bool canOpenEmbedded = str::EndsWithI(fileName, StrL(".pdf"));
        if (!canOpenEmbedded) {
            MenuRemove(popup, CmdOpenAttachment);
        }
    } else {
        // TODO: maybe move this to BuildMenuFromMenuDef
        MenuRemove(popup, CmdSaveAttachment);
        MenuRemove(popup, CmdOpenAttachment);
    }

    if (pageNo > 0) {
        TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
        bool isBookmarked = IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            MenuRemove(popup, CmdFavoriteAdd);

            // %s and not %d because re-using translation from RebuildFavMenu()
            Str tr = _TRA("Remove page %s from favorites");
            TempStr s = fmt(tr.s, pageLabel);
            MenuSetText(popup, CmdFavoriteDel, s);
        } else {
            MenuRemove(popup, CmdFavoriteDel);
            // %s and not %d because re-using translation from RebuildFavMenu()
            TempStr s = fmt(_TRA("Add page %s to favorites").s, pageLabel);
            s = AppendAccelKeyToMenuStringTemp(s, CmdFavoriteAdd);
            MenuSetText(popup, CmdFavoriteAdd, s);
        }
    } else {
        MenuRemove(popup, CmdFavoriteAdd);
        MenuRemove(popup, CmdFavoriteDel);
    }
    RemoveBadMenuSeparators(popup);
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case CmdExpandAll:
            win->tocTreeView->ExpandAll();
            break;
        case CmdCollapseAll:
            TocCollapseAll(win->tocTreeView);
            break;
        case CmdTocExpandToLevel1:
            TocExpandToLevel(win->tocTreeView, 1);
            break;
        case CmdTocExpandToLevel2:
            TocExpandToLevel(win->tocTreeView, 2);
            break;
        case CmdTocExpandToLevel3:
            TocExpandToLevel(win->tocTreeView, 3);
            break;
        case CmdTocCollapseSameLevel:
            TocCollapseSameLevel(win->tocTreeView, ti);
            break;
        case CmdExpandToCurrentPage:
            ExpandTocToCurrentPage(win);
            break;
        case CmdFavoriteAdd:
            AddFavoriteFromToc(win, dti);
            break;
        case CmdFavoriteDel:
            DelFavorite(filePath, pageNo);
            break;
        case CmdSaveEmbeddedFile: {
            SaveEmbeddedFile(tab, path, fileName);
        } break;
        case CmdOpenEmbeddedPDF:
            // TODO: maybe also allow for a fileName hint
            OpenEmbeddedFile(tab, dest);
            break;
        case CmdSaveAttachment: {
            SaveAttachment(tab, fileName, attachmentNo);
            break;
        }
        case CmdOpenAttachment: {
            OpenAttachment(tab, fileName, attachmentNo);
        }
    }
}

static void OnTocCustomDraw(TreeView::CustomDrawEvent* /*ev*/);

// auto-expand root level ToC nodes if there are at most two
static void AutoExpandTopLevelItems(TocItem* root) {
    if (!root) {
        return;
    }
    if (root->next && root->next->next) {
        return;
    }

    if (!root->IsExpanded()) {
        root->isOpenToggled = !root->isOpenToggled;
    }
    if (!root->next) {
        return;
    }
    if (!root->next->IsExpanded()) {
        root->next->isOpenToggled = !root->next->isOpenToggled;
    }
}

void LoadTocTree(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        ReportIf(true);
        return;
    }

    if (win->tocLoaded) {
        return;
    }

    win->tocLoaded = true;

    // clear filter when loading new toc
    // null out currToc first so that SetText("") callback doesn't use stale pointer
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;
    tab->currToc = nullptr;
    if (win->tocFilterEdit) {
        win->tocFilterEdit->SetText("");
    }

    auto* tocTree = tab->ctrl->GetToc();
    if (!tocTree || !tocTree->root) {
        return;
    }

    tab->currToc = tocTree;

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tocTree->root, l2r, r2l);
    bool isRTL = r2l > l2r;

    TreeView* treeView = win->tocTreeView;
    HWND hwnd = treeView->hwnd;
    HwndSetRtl(hwnd, isRTL);

    UpdateControlsColors(win);
    SetInitialExpandState(tocTree->root, tab->tocState);
    AutoExpandTopLevelItems(tocTree->root->child);

    treeView->SetTreeModel(tocTree);

    treeView->onCustomDraw = MkFunc1Void(OnTocCustomDraw);
    LayoutTocContainer(win);
    // uint fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    // RedrawWindow(hwnd, nullptr, nullptr, fl);
}

static TocItem* FindTocItemByTitleAndPage(TocItem* item, Str title, int pageNo) {
    while (item) {
        if (item->pageNo == pageNo && str::Eq(item->title, title)) {
            return item;
        }
        if (TocItem* found = FindTocItemByTitleAndPage(item->child, title, pageNo)) {
            return found;
        }
        item = item->next;
    }
    return nullptr;
}

// The controller swapped in a different TocTree (the markdown / html TOC is
// built in the background, see MarkdownModel). Show the new one, keeping the
// selection on the same entry when it's still there.
// Must not return while anything still points into the old tree: the caller
// deletes it as soon as we're done.
void ReloadTocTree(WindowTab* tab) {
    MainWindow* win = tab ? tab->win : nullptr;
    if (!win) {
        return;
    }
    // the tree view only ever shows the current tab; another tab picks up the
    // new tree when it's switched to
    if (win->CurrentTab() != tab || !win->tocLoaded) {
        tab->currToc = nullptr;
        return;
    }

    // the items are about to be freed, so remember the selection the way the
    // user sees it rather than by pointer
    TreeView* treeView = win->tocTreeView;
    TempStr selTitle = nullptr;
    int selPageNo = 0;
    if (treeView) {
        auto* sel = (TocItem*)treeView->GetSelection();
        if (sel) {
            selTitle = str::DupTemp(sel->title);
            selPageNo = sel->pageNo;
        }
    }
    int currPageNo = win->currPageNo;

    ClearTocBox(win);
    LoadTocTree(win);

    if (!treeView || !tab->currToc) {
        return;
    }
    TocItem* toSelect = nullptr;
    if (selTitle) {
        toSelect = FindTocItemByTitleAndPage(tab->currToc->root, selTitle, selPageNo);
    }
    if (toSelect) {
        treeView->SelectItem((TreeItem)toSelect);
        SetTocMultiHighlight(win, treeView, toSelect);
        return;
    }
    // nothing matched (or nothing was selected): fall back to the current page
    UpdateTocSelection(win, currPageNo);
}

// TODO: use https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getobject?redirectedfrom=MSDN
// to get LOGFONT from existing font and then create a derived font
static PlatformFont* UpdateFont(HDC hdc, int fontFlags) {
    bool italic = bit::IsSet(fontFlags, fontBitItalic);
    bool bold = bit::IsSet(fontFlags, fontBitBold);
    PlatformFont* font = GetAppTreeFontEx(bold, italic);
    SelectObject(hdc, font->GetHFont());
    return font;
}

static void GetTocFilterWords(MainWindow* win, StrVec& wordsOut) {
    wordsOut.Reset();
    if (!win || !win->tocFilterEdit) {
        return;
    }
    TempStr filter = win->tocFilterEdit->GetTextTemp();
    if (filter) {
        SplitFilterToWords(filter, wordsOut);
    }
}

static bool HasTocFilter(MainWindow* win) {
    StrVec words;
    GetTocFilterWords(win, words);
    return len(words) > 0;
}

// POSTPAINT: redraw title (optional filter highlight), optional right-aligned
// page label, and multi-match "current page" highlight (issue #4642).
static void DrawTocItemPostPaint(TreeView::CustomDrawEvent* ev, MainWindow* win) {
    TocItem* tocItem = (TocItem*)ev->treeItem;
    if (!tocItem || !tocItem->title) {
        return;
    }

    TreeView* tv = ev->treeView;
    Rect labelRect{};
    if (!tv->GetItemRect(ev->treeItem, true, labelRect)) {
        return;
    }
    Rect itemRect{};
    tv->GetItemRect(ev->treeItem, false, itemRect);

    NMTVCUSTOMDRAW* tvcd = ev->nm;
    HDC hdc = tvcd->nmcd.hdc;
    NMCUSTOMDRAW* cd = &tvcd->nmcd;
    if (cd->rc.right <= cd->rc.left || cd->rc.bottom <= cd->rc.top) {
        return;
    }

    // POSTPAINT often omits CDIS_SELECTED; also check the control selection.
    bool isTreeSelected = (cd->uItemState & CDIS_SELECTED) != 0;
    if (!isTreeSelected) {
        HTREEITEM hSel = TreeView_GetSelection(tv->hwnd);
        HTREEITEM hItem = tv->GetHandleByTreeItem(ev->treeItem);
        isTreeSelected = hSel && hItem && hSel == hItem;
    }
    bool isMultiMatch = TocItemIsMultiHighlight(win, tocItem);
    // Treat multi-match rows like selected for fill/text colors so every
    // current bookmark is visible, not only the TreeView selection.
    bool isSelected = isTreeSelected || isMultiMatch;
    // Focus ring / highlight-text only for the real tree selection.
    bool hasFocus = isTreeSelected && (GetFocus() == tv->hwnd);
    Color bgCol, txtCol;
    ResolveTreeFilterItemColors(hdc, itemRect, tv->bgColor, tv->textColor, isSelected, hasFocus, &bgCol, &txtCol);
    // Per-bookmark color from the document (when not the focused selection).
    if (!(isTreeSelected && hasFocus) && tocItem->color != kColorUnset) {
        txtCol = tocItem->color;
    }

    // check win->ctrl directly, not IsDocLoaded(): a paint can arrive while
    // win->ctrl and CurrentTab()->ctrl transiently disagree (tab close/switch,
    // pending load) and IsDocLoaded() asserts on that mismatch
    bool showPage = gGlobalPrefs->showTocPageNumbers && win && win->ctrl && tocItem->pageNo > 0;
    TempStr pageLabel{};
    if (showPage) {
        pageLabel = win->ctrl->GetPageLabeTemp(tocItem->pageNo);
        if (!pageLabel) {
            showPage = false;
        }
    }

    StrVec words;
    GetTocFilterWords(win, words);
    bool filterActive = len(words) > 0;

    // Always repaint selected / multi-match rows so themed selection colors
    // replace Explorer's light inactive-selection face (issue #5848). Also
    // when page numbers or filter bars need drawing.
    if (!showPage && !filterActive && !isSelected) {
        return;
    }

    // Label area extends to the visible right edge so the page number stays
    // right-aligned against the sidebar, not under a long title.
    RECT drawRc = ToRECT(labelRect);
    drawRc.right = std::min(itemRect.x + itemRect.dx, (int)cd->rc.right);
    if (drawRc.right <= drawRc.left) {
        return;
    }

    PlatformFont* font = tv->GetFont();
    if (tocItem->fontFlags != 0) {
        font = UpdateFont(hdc, tocItem->fontFlags);
    }
    GfxHdc gfx(hdc);

    Size pageSize{};
    int pageReserve = 0;
    if (showPage) {
        if (len(pageLabel) > 0) {
            pageSize = gfx.MeasureText(pageLabel, font);
            pageReserve = pageSize.dx + DpiScale(8);
        } else {
            showPage = false;
        }
    }

    Rect drawRect = ToRect(drawRc);
    gfx.FillRect(drawRect, bgCol);

    Rect titleRect = drawRect;
    titleRect.dx = std::max(0, titleRect.dx - pageReserve);
    titleRect.Inflate(-2, -1);

    if (filterActive) {
        DrawTreeItemFilterHighlight(&gfx, titleRect, tocItem->title, words, bgCol, txtCol, font);
    } else {
        gfx.DrawText(tocItem->title, titleRect, gfxTextVCenter | gfxTextEllipsis, font, txtCol);
    }

    if (showPage && len(pageLabel) > 0) {
        Rect pageRect = drawRect;
        pageRect.Inflate(-2, -1);
        int right = pageRect.x + pageRect.dx;
        pageRect.x = std::max(pageRect.x, right - pageSize.dx);
        pageRect.dx = right - pageRect.x;
        // Slightly muted vs title when not selected (keeps numbers secondary).
        Color pageCol = txtCol;
        if (!(isTreeSelected && hasFocus)) {
            pageCol =
                MkRgb((GetRValue(txtCol) * 2 + GetRValue(bgCol)) / 3, (GetGValue(txtCol) * 2 + GetGValue(bgCol)) / 3,
                      (GetBValue(txtCol) * 2 + GetBValue(bgCol)) / 3);
        }
        gfx.DrawText(pageLabel, pageRect, gfxTextVCenter | gfxTextRight, font, pageCol);
    }

    if ((cd->uItemState & CDIS_FOCUS) && isTreeSelected && hasFocus) {
        gfx.DrawFocusRect(drawRect);
    }
}

// https://docs.microsoft.com/en-us/windows/win32/controls/about-custom-draw
// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-nmtvcustomdraw
void OnTocCustomDraw(TreeView::CustomDrawEvent* ev) {
    ev->result = CDRF_DODEFAULT;
    NMTVCUSTOMDRAW* tvcd = ev->nm;
    NMCUSTOMDRAW* cd = &(tvcd->nmcd);

    if (cd->dwDrawStage == CDDS_PREPAINT) {
        ev->result = CDRF_NOTIFYITEMDRAW;
        return;
    }

    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    bool filterActive = HasTocFilter(win);
    bool showPageNumbers = gGlobalPrefs->showTocPageNumbers;
    bool multiHighlight = gShowAllMatchingTOC && win && len(win->tocMatchingItems) > 0;

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        TocItem* tocItem = (TocItem*)ev->treeItem;
        if (!tocItem) {
            return;
        }
        TreeView* tv = ev->treeView;
        bool isTreeSelected = (cd->uItemState & CDIS_SELECTED) != 0;
        if (!isTreeSelected) {
            HTREEITEM hSel = TreeView_GetSelection(tv->hwnd);
            HTREEITEM hItem = tv->GetHandleByTreeItem(ev->treeItem);
            isTreeSelected = hSel && hItem && hSel == hItem;
        }
        bool isMultiMatch = TocItemIsMultiHighlight(win, tocItem);
        bool isSelected = isTreeSelected || isMultiMatch;
        bool hasFocus = isTreeSelected && (GetFocus() == tv->hwnd);

        LRESULT res = 0;
        if (isSelected) {
            // Theme-aware selection fill/text; strip CDIS_SELECTED so Explorer
            // theme does not paint a light inactive selection over dark text.
            Color bgCol, txtCol;
            ResolveTreeFilterItemColors(cd->hdc, ToRect(cd->rc), tv->bgColor, tv->textColor, true, hasFocus, &bgCol,
                                        &txtCol);
            if (!(isTreeSelected && hasFocus) && tocItem->color != kColorUnset) {
                txtCol = tocItem->color;
            }
            tvcd->clrText = txtCol;
            tvcd->clrTextBk = bgCol;
            cd->uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);
            res |= CDRF_NEWFONT;
        } else if (tocItem->color != kColorUnset) {
            tvcd->clrText = tocItem->color;
        }
        if (tocItem->fontFlags != 0) {
            UpdateFont(cd->hdc, tocItem->fontFlags);
            res |= CDRF_NEWFONT;
        }
        // POSTPAINT: selection colors (issue #5848), page numbers, filter, multi-match.
        bool needPost =
            isSelected || filterActive || (showPageNumbers && tocItem->pageNo > 0) || (multiHighlight && isMultiMatch);
        if (needPost) {
            res |= CDRF_NOTIFYPOSTPAINT;
        }
        ev->result = res;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPOSTPAINT) {
        if (win) {
            DrawTocItemPostPaint(ev, win);
        }
        ev->result = CDRF_DODEFAULT;
        return;
    }
}

// disabled because of https://github.com/sumatrapdfreader/sumatrapdf/issues/2202
// it was added for https://github.com/sumatrapdfreader/sumatrapdf/issues/1716
// but unclear if its still needed
// this calls GoToTocLinkTask) which will eventually call GoToPage()
// which adds nav point. Maybe I should not add nav point
// if going to the same page?
// set when a mouse click changed the tree selection (handled by
// TocTreeSelectionChanged), so the NM_CLICK that follows doesn't navigate again
static bool gTocSelChangedByMouseClick = false;

static void TocTreeClick(TreeView::ClickEvent* ev) {
    bool handledBySelChange = gTocSelChangedByMouseClick;
    gTocSelChangedByMouseClick = false;
    // A normal click changes the selection and is handled by
    // TocTreeSelectionChanged. Clicking the item that is already selected fires
    // no selection-change notification, so handle that here to let the user
    // re-click the current bookmark to jump back to its page (#2465).
    if (ev->isDblClick || !ev->treeItem || handledBySelChange) {
        return;
    }
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (!win) {
        ReportIf(true);
        return;
    }
    GoToTocTreeItem(win, ev->treeItem, true);
}

static void TocTreeSelectionChanged(TreeView::SelectionChangedEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (!win) {
        ReportIf(true);
        return;
    }

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
    if (ev->byMouse) {
        // remember that this click already navigated, so the following
        // NM_CLICK (TocTreeClick) doesn't navigate a second time (#2465)
        gTocSelChangedByMouseClick = true;
    }
    bool allowExternal = ev->byMouse;
    GoToTocTreeItem(win, ev->selectedItem, allowExternal);
}

// Tab / Ctrl+Tab focus movement (also reused by Favorites tree)
void TocTreeKeyDown2(TreeView::KeyDownEvent* ev);

static void FocusTocFilterEdit(MainWindow* win) {
    if (!win || !win->tocFilterEdit || !win->tocFilterEdit->hwnd) {
        return;
    }
    HwndSetFocus(win->tocFilterEdit->hwnd);
    win->tocFilterEdit->SetCursorPositionAtEnd();
}

// Select the first top-level bookmark (Down from the search box).
static void SelectFirstTocTreeItem(MainWindow* win) {
    TreeView* tv = win ? win->tocTreeView : nullptr;
    if (!tv || !tv->treeModel || !tv->hwnd) {
        return;
    }
    TreeModel* tm = tv->treeModel;
    TreeItem root = tm->Root();
    if (tm->ChildCount(root) == 0) {
        return;
    }
    TreeItem first = tm->ChildAt(root, 0);
    tv->SelectItem(first);
    HTREEITEM h = tv->GetHandleByTreeItem(first);
    if (h) {
        TreeView_EnsureVisible(tv->hwnd, h);
    }
}

// TOC tree keyboard: Esc clears filter / focuses search; Up on first row returns
// to the search box. Tab (and Ctrl+Tab) stay in TocTreeKeyDown2 so Favorites can
// reuse that path.
static void TocTreeKeyDown(TreeView::KeyDownEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (ev->keyCode == VK_ESCAPE) {
        if (win && win->tocFilterEdit) {
            win->tocFilterEdit->SetText("");
            FocusTocFilterEdit(win);
            ev->result = 1;
            return;
        }
    }
    if (ev->keyCode == VK_UP && win && win->tocFilterEdit) {
        TreeItem sel = ev->treeView->GetSelection();
        HTREEITEM hSel = sel ? ev->treeView->GetHandleByTreeItem(sel) : nullptr;
        HTREEITEM hFirst = TreeView_GetRoot(ev->treeView->hwnd);
        if (hSel && hFirst && hSel == hFirst) {
            FocusTocFilterEdit(win);
            ev->result = 1;
            return;
        }
    }
    TocTreeKeyDown2(ev);
}

void TocTreeKeyDown2(TreeView::KeyDownEvent* ev) {
    // TODO: trying to fix https://github.com/sumatrapdfreader/sumatrapdf/issues/1841
    // doesn't work i.e. page up / page down seems to be processed anyway by TreeCtrl
#if 0
    if ((ev->keyCode == VK_PRIOR) || (ev->keyCode == VK_NEXT)) {
        // up/down in tree is not very useful, so instead
        // send it to frame so that it scrolls document instead
        MainWindow* win = FindMainWindowByHwnd(ev->hwnd);
        // this is sent as WM_NOTIFY to TreeCtrl but for frame it's WM_KEYDOWN
        // alternatively, we could call FrameOnKeydown(ev->wp, ev->lp, false);
        SendMessageW(win->hwndFrame, WM_KEYDOWN, ev->wp, ev->lp);
        ev->didHandle = true;
        ev->result = 1;
        return;
    }
#endif
    if (ev->keyCode != VK_TAB) {
        ev->result = 0;
        return;
    }

    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (win->tabsVisible && IsCtrlPressed()) {
        TabsOnCtrlTab(win, IsShiftPressed());
        ev->result = 1;
        return;
    }
    AdvanceFocus(win);
    ev->result = 1;
}

// Position label, filter edit, and tree window within toc container using the
// wingui layout engine (VBox built in CreateToc).
static void LayoutTocContainer(MainWindow* win) {
    if (!win->tocLayout) {
        return;
    }
    Rect rc = HwndWindowRect(win->hwndTocBox);
    LayoutTreeToSize(win->hwndTocBox, win->tocLayout, {rc.dx, rc.dy}, &win->tocRoot);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                      DWORD_PTR /*data*/) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    // the panel header (label + close button) is a virtual control tree, so
    // this window paints it and hands it its input
    if (VirtHostOnMessage(hwnd, win->tocRoot, msg, wp, lp, res, ThemeControlBackgroundColor())) {
        return res;
    }

    switch (msg) {
        case WM_SIZE:
            LayoutTocContainer(win);
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SubclassToc(MainWindow* win) {
    HWND hwndTocBox = win->hwndTocBox;

    if (win->tocBoxSubclassId == 0) {
        win->tocBoxSubclassId = NextSubclassId();
        BOOL ok = SetWindowSubclass(hwndTocBox, WndProcTocBox, win->tocBoxSubclassId, (DWORD_PTR)win);
        if (!ok) {
            // can fail under low memory / desktop heap exhaustion, so don't assert
            logf("SubclassToc: SetWindowSubclass() failed, err: %d\n", (int)GetLastError());
            win->tocBoxSubclassId = 0;
        }
    }
}

void UnsubclassToc(MainWindow* win) {
    if (win->tocBoxSubclassId != 0) {
        RemoveWindowSubclass(win->hwndTocBox, WndProcTocBox, win->tocBoxSubclassId);
        win->tocBoxSubclassId = 0;
    }
}

// Append a TocItem linked list onto resultFirst/resultLast (updates last).
static void AppendTocSiblingList(TocItem*& resultFirst, TocItem*& resultLast, TocItem* list) {
    if (!list) {
        return;
    }
    if (!resultFirst) {
        resultFirst = list;
    } else {
        resultLast->next = list;
    }
    resultLast = list;
    while (resultLast->next) {
        resultLast = resultLast->next;
    }
}

// Recursively build a filtered copy of the TocItem tree.
// Multi-word filter (command palette style): every word must appear in the
// item's own title to keep that node. Non-matching ancestors are omitted and
// matching descendants are promoted so only fully-matching rows are shown.
// Returns nullptr if nothing matches.
static TocItem* FilterTocItemRec(TocItem* item, const StrVec& words) {
    if (!item) {
        return nullptr;
    }
    TocItem* resultFirst = nullptr;
    TocItem* resultLast = nullptr;
    for (TocItem* si = item; si; si = si->next) {
        TocItem* filteredChildren = FilterTocItemRec(si->child, words);
        bool titleMatches = si->title && FilterMatches(si->title, words);
        if (titleMatches) {
            // keep this node; only fully-matching children stay nested under it
            auto* copy = AllocTocItem(nullptr, si->title, si->pageNo);
            copy->id = si->id;
            copy->fontFlags = si->fontFlags;
            copy->color = si->color;
            copy->dest = si->dest;
            copy->destNotOwned = true;
            copy->isOpenDefault = true;
            copy->isOpenToggled = false;
            copy->child = filteredChildren;
            for (TocItem* c = copy->child; c; c = c->next) {
                c->parent = copy;
            }
            AppendTocSiblingList(resultFirst, resultLast, copy);
        } else if (filteredChildren) {
            // title does not match every word: drop this node, promote children
            for (TocItem* c = filteredChildren; c; c = c->next) {
                c->parent = nullptr;
            }
            AppendTocSiblingList(resultFirst, resultLast, filteredChildren);
        }
    }
    return resultFirst;
}

static void ApplyTocFilter(MainWindow* win, Str filter) {
    if (!win->tocLoaded) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->currToc) {
        return;
    }
    // free previous filtered tree
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;

    TreeView* treeView = win->tocTreeView;
    TocTree* origTree = tab->currToc;

    StrVec words;
    if (filter) {
        SplitFilterToWords(filter, words);
    }
    if (len(words) == 0) {
        // restore original tree
        SetInitialExpandState(origTree->root, tab->tocState);
        treeView->SetTreeModel(origTree);
        return;
    }

    TocItem* filteredItems = FilterTocItemRec(origTree->root, words);
    if (!filteredItems) {
        treeView->Clear();
        return;
    }
    // TreeView populates Root()'s children only (the root itself is invisible).
    // Promote-filter returns a sibling list of matching items, so wrap them in
    // a dummy root — same shape every engine uses for the unfiltered TocTree.
    auto* wrapRoot = AllocTocItem(nullptr, {}, 0);
    wrapRoot->child = filteredItems;
    for (TocItem* c = filteredItems; c; c = c->next) {
        c->parent = wrapRoot;
    }
    auto* filteredTree = new TocTree(wrapRoot);
    win->tocFilteredTree = filteredTree;
    treeView->SetTreeModel(filteredTree);
}

void TocFilterChanged(MainWindow* win) {
    Edit* edit = win->tocFilterEdit;
    if (!edit) {
        return;
    }
    TempStr filter = edit->GetTextTemp();
    ApplyTocFilter(win, filter);
}

static void OnTocFilterTextChanged(MainWindow* win) {
    TocFilterChanged(win);
}

static LRESULT CALLBACK WndProcTocFilterEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*subclassId*/,
                                             DWORD_PTR data) {
    MainWindow* win = (MainWindow*)data;
    if (msg == WM_KEYDOWN) {
        if (wp == VK_DOWN) {
            // move into the tree: first top-level bookmark
            if (win && win->tocTreeView) {
                SelectFirstTocTreeItem(win);
                HwndSetFocus(win->tocTreeView->hwnd);
            }
            return 0;
        }
        if (wp == VK_ESCAPE) {
            Edit* edit = win ? win->tocFilterEdit : nullptr;
            if (edit) {
                TempStr txt = edit->GetTextTemp();
                if (txt && len(txt) > 0) {
                    edit->SetText("");
                    // onTextChanged will fire and restore the tree
                    return 0;
                }
                // empty: move focus to the tree
                if (win->tocTreeView) {
                    SetFocus(win->tocTreeView->hwnd);
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

void CreateToc(MainWindow* win) {
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    HWND parent = win->hwndFrame;
    win->hwndTocBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    PlatformFont* labelFont = GetAppSidebarLabelFont();
    auto header = NewLabelWithClose(win->hwndTocBox, labelFont, MkFunc0(ToggleTocBox, win));
    win->tocLabel = header.label;
    // label text is set in UpdateToolbarSidebarText()

    auto* filterEdit = new Edit();
    {
        Edit::CreateArgs eargs;
        eargs.parent = win->hwndTocBox;
        eargs.withBorder = false;
        // underline so the filter field is visible on flat sidebar backgrounds
        eargs.withBottomBorder = true;
        eargs.cueText = _TRA("Search Bookmarks");
        eargs.font = GetAppFont();
        filterEdit->Create(eargs);
    }
    win->tocFilterEdit = filterEdit;
    filterEdit->onTextChanged = MkFunc0(OnTocFilterTextChanged, win);
    SetWindowSubclass(filterEdit->hwnd, WndProcTocFilterEdit, NextSubclassId(), (DWORD_PTR)win);

    auto* treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent = win->hwndTocBox;
    args.font = GetAppTreeFont();
    args.fullRowSelect = true;
    args.exStyle = 0;
    args.isRtl = IsUIRtl();

    auto fn = MkFunc1Void(TocContextMenu);
    treeView->onContextMenu = fn;
    treeView->onSelectionChanged = MkFunc1Void(TocTreeSelectionChanged);
    treeView->onKeyDown = MkFunc1Void(TocTreeKeyDown);
    treeView->onGetTooltip = MkFunc1Void(TocCustomizeTooltip);
    treeView->onClick = MkFunc1Void(TocTreeClick);

    treeView->Create(args);
    ReportIf(!treeView->hwnd);
    win->tocTreeView = treeView;

    // stack label, filter edit and tree vertically; the tree flexes to fill the
    // remaining height. The VBox owns these controls/spacer (freed in ~MainWindow).
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(header.box);
    vbox->AddChild(filterEdit);
    vbox->AddChild(new Spacer(0, 2)); // gap under the search field
    vbox->AddChild(treeView, 1);
    win->tocLayout = vbox;

    SubclassToc(win);

    UpdateControlsColors(win);
}
