/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's TableOfContents.cpp drives a win32 TreeView inside a child window
// and custom-draws every row. Here the model half is orig's - the deferred
// navigation snapshot, the current-page match, the multi-highlight set, the
// expansion state, the bookmark filter - and the presentation is the gpui
// bookmarks pane in src/gui/Sidebar.cpp, which walks the same TocItem tree.
// The expansion flags live on TocItem (isOpenDefault / isOpenToggled), so
// there is no second copy of the tree to keep in sync.

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "FilterUtil.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "DisplayModel.h"
#include "Favorites.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Translations.h"
#include "Tabs.h"
#include "Menu.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Theme.h"
#include "gui/AppShell.h"
#include "gui/Sidebar.h"
#include "TableOfContents.h"

#include "SumatraLog.h"

// When true, multi-highlight every TOC item that matches the current page
// (issue #4642). Easy to flip for comparison with single-selection behavior.
bool gShowAllMatchingTOC = true;

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
        if (len(url) == 0) {
            url = dest->GetValue();
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
        copy->rect = dest->GetRect();
        copy->zoom = dest->GetZoom();
        copy->value = str::Dup(dest->GetValue());
        copy->name = str::Dup(dest->GetName());
        copy->loc = dest->loc;
        return copy;
    }
    // mupdf, djvu, none -> page navigation snapshot
    int pageNo = PageDestGetPageNo(dest);
    if (pageNo <= 0) {
        pageNo = tocPageNo;
    }
    if (pageNo < 1) {
        Str val = dest->GetValue();
        if (val && IsExternalUrl(val)) {
            return new PageDestinationURL(val);
        }
        logf("SnapshotDestForDeferredNav: skip dest kind pageNo=%d (tocPageNo=%d)\n", PageDestGetPageNo(dest),
             tocPageNo);
        return nullptr;
    }
    RectF r = dest->GetRect();
    float zoom = dest->GetZoom();
    if (k == kindDestinationMupdf) {
        // Prefer resolved anchor; outline x/y can be 0 and scroll to the wrong place
        RectF pt = PageDestGetDestPoint(dest);
        if ((r.dx == 0 && r.dy == 0) || (r.dx == kDestUseDefault && r.dy == kDestUseDefault)) {
            if (pt.x != 0 || pt.y != 0 || r.IsEmpty()) {
                r = RectF{pt.x, pt.y, kDestUseDefault, kDestUseDefault};
            }
        }
        zoom = dest->GetZoom();
    }
    IPageDestination* copy = NewSimpleDest(pageNo, r, zoom);
    copy->loc = dest->loc;
    return copy;
}

static TocItem* FindTocItemByTitlePage(TocItem* item, Str title, int pageNo) {
    for (; item; item = item->next) {
        if (pageNo > 0 && item->pageNo != pageNo) {
            // keep searching children; same page can nest under different titles
        } else if (title && item->title && str::Eq(title, item->title)) {
            if (pageNo <= 0 || item->pageNo == pageNo) {
                return item;
            }
        } else if (len(title) == 0 && pageNo > 0 && item->pageNo == pageNo) {
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
    DocController* ctrl = win->ctrl;
    WindowTab* tab = win->CurrentTab();
    if (!ctrl || !tab || tab->ctrl != ctrl) {
        return nullptr;
    }

    int pageNo = tocItem->pageNo;
    IPageDestination* origDest = tocItem->GetPageDestination();
    if (origDest && pageNo < 1) {
        // chaptered docs: pageNo stays -1 until the target chapter lays out.
        // Resolve now, on the UI thread, so ResolveDest can cache the real
        // page/loc on origDest before it gets snapshotted below -- otherwise
        // SnapshotDestForDeferredNav has nothing but pageNo == -1 to go on
        // and drops the destination.
        Location loc = ctrl->ResolveDest(origDest);
        if (loc.IsValid()) {
            pageNo = ctrl->PageNoFromLocation(loc);
        }
    }
    IPageDestination* dest = SnapshotDestForDeferredNav(origDest, pageNo);

    // drop page-navigation destinations that still have no valid page and no
    // chapter to resolve lazily (see DocController::ResolveDest)
    if (dest && DestNeedsValidPageNo(dest) && PageDestGetPageNo(dest) < 1 && dest->loc.chapter < 1) {
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
    data->ctrl = ctrl;
    data->tab = tab;
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

    // validate tab before dereferencing - it may have been freed
    // while this task was queued (e.g. user closed the tab/window)
    if (!IsWindowTabValid(tab)) {
        return;
    }
    MainWindow* win = tab->win;
    // destination snapshot is invalid if the DocController has been replaced
    if (!ctrl || !IsMainWindowValidAndNotClosing(win) || win->CurrentTab() != tab || tab->ctrl != ctrl) {
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
    // the tree still shows the old item. Move the selection to this item now.
    if (d->selectInTree && win->tocLoaded) {
        TocTree* tree = CurrentTocTree(win);
        TocItem* tocItem = nullptr;
        if (tree && tree->root) {
            tocItem = FindTocItemByTitlePage(tree->root, d->title, d->pageNo);
        }
        if (tocItem) {
            SidebarRevealTocItem(win, tocItem);
        }
    }
    AppShellInvalidate(win);
}

// navigate to a TocItem regardless of whether it points to a page in this
// document or to an external destination (used by the command palette, where
// the user explicitly picked the item so we always honor it)
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

static void GoToTocTreeItem(MainWindow* win, TocItem* tocItem, bool allowExternal) {
    if (!tocItem) {
        return;
    }
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->GetPageDestination());
    bool hasChapterDest = tocItem->dest && tocItem->dest->loc.chapter >= 1;
    if (validPage || allowExternal || isScroll || hasChapterDest) {
        // delay changing the page until the tree messages have been handled
        auto* data = NewGoToTocLinkData(win, tocItem, false);
        if (!data) {
            return;
        }
        auto fn = MkFunc0<GoToTocLinkData>(GoToTocLink, data);
        uitask::Post(fn, "TaskGoToTocTreeItem");
    }
}

// ng: orig's TocTreeSelectionChanged + TocTreeClick; a click on a bookmark both
// selects it and navigates (a click on the same item navigates again, #2465)
void TocTreeItemClicked(MainWindow* win, TocItem* tocItem) {
    if (!win || !tocItem) {
        return;
    }
    logf("TocTreeItemClicked: '%s' page %d\n", tocItem->title, tocItem->pageNo);
    SidebarSetTocSelection(win, tocItem);
    GoToTocTreeItem(win, tocItem, true);
}

// orig's TocTreeSelectionChanged for a keyboard selection: it navigates, but
// does not follow a link out of the document (allowExternal = ev->byMouse)
void TocTreeItemSelectedByKey(MainWindow* win, TocItem* tocItem) {
    if (!win || !tocItem) {
        return;
    }
    SidebarSetTocSelection(win, tocItem);
    GoToTocTreeItem(win, tocItem, false);
}

// the tree the bookmarks pane shows: the filter's copy while a filter is on
TocTree* CurrentTocTree(MainWindow* win) {
    if (!win || !win->tocLoaded) {
        return nullptr;
    }
    if (win->tocFilteredTree) {
        return win->tocFilteredTree;
    }
    WindowTab* tab = win->CurrentTab();
    return tab ? tab->currToc : nullptr;
}

void ClearTocBox(MainWindow* win) {
    if (!win->tocLoaded) {
        return;
    }

    win->tocLoaded = false;

    SidebarSetTocSelection(win, nullptr);
    VecReset(win->tocMatchingItems);

    // clear filter state
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;
    SidebarSetTocFilterText(win, StrL(""));

    win->currPageNo = 0;
}

void ToggleTocBox(MainWindow* win) {
    SidebarToggleBookmarks(win);
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
static TocItem* TreeItemForPageNo(TocTree* tm, int pageNo) {
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

struct VisitorForChapterData {
    int chapter = -1;
    int pageNo = 0;
    TocItem* match = nullptr;
    int matchPageNo = 0;
};

static void visitTreeForChapter(VisitorForChapterData* d, TreeItemVisitorData* vd) {
    auto* tocItem = (TocItem*)vd->item;
    if (!tocItem || tocItem->loc.chapter != d->chapter) {
        return;
    }
    if (!d->match) {
        d->match = tocItem;
        d->matchPageNo = tocItem->pageNo;
    }
    int page = tocItem->pageNo;
    if (page >= 1 && page <= d->pageNo && page >= d->matchPageNo) {
        d->match = tocItem;
        d->matchPageNo = page;
        if (page == d->pageNo) {
            vd->stopTraversal = true;
        }
    }
}

// closest item (in tree order) whose loc.chapter matches, preferring an exact
// pageNo match; chaptered docs need chapter-first matching because unresolved
// items all share pageNo == -1, so TreeItemForPageNo can't tell them apart
static TocItem* TreeItemForChapter(TocTree* tm, int chapter, int pageNo) {
    if (!tm || chapter < 1) {
        return nullptr;
    }
    VisitorForChapterData d;
    d.chapter = chapter;
    d.pageNo = pageNo;
    auto fn = MkFunc1<VisitorForChapterData, TreeItemVisitorData*>(visitTreeForChapter, &d);
    VisitTreeModelItems(tm, fn);
    return d.match;
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
        VecAppend(*d->out, tocItem);
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

static bool TocMatchingItemsEq(const Vec<TocItem*>& a, const Vec<TocItem*>& b) {
    if (len(a) != len(b)) {
        return false;
    }
    for (int i = 0; i < len(a); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

// Fill win->tocMatchingItems with every entry that should look "current" for
// bestMatch: all TOC items on the same page, plus the ancestor chain (so a
// nested 6 / 6.1 / 6.1.1 path all highlight together). The tree has only one
// selection; the extras are painted by the pane when gShowAllMatchingTOC.
static void SetTocMultiHighlight(MainWindow* win, TocTree* tm, TocItem* bestMatch) {
    Vec<TocItem*> next;
    if (gShowAllMatchingTOC && bestMatch && tm) {
        // All bookmarks that point at the same page as the best match (the issue's
        // "subsequent" same-page entries that a single selection cannot show).
        if (bestMatch->pageNo >= 1) {
            CollectSamePageData d;
            d.pageNo = bestMatch->pageNo;
            d.out = &next;
            auto fn = MkFunc1<CollectSamePageData, TreeItemVisitorData*>(visitCollectSamePage, &d);
            VisitTreeModelItems(tm, fn);
        }

        // Ancestor chain (chapter -> section -> subsection), including bestMatch.
        // ng: the tree's root is not a row, so it is not part of the chain
        for (TocItem* p = bestMatch; p && p != tm->root; p = p->parent) {
            if (!TocMatchingItemsContains(next, p)) {
                VecAppend(next, p);
            }
        }
    }

    if (TocMatchingItemsEq(win->tocMatchingItems, next)) {
        return;
    }
    win->tocMatchingItems = next;
    AppShellInvalidate(win);
}

bool TocItemIsMultiHighlight(MainWindow* win, TocItem* item) {
    if (!gShowAllMatchingTOC || !win || !item) {
        return false;
    }
    return TocMatchingItemsContains(win->tocMatchingItems, item);
}

// ng: orig asks the TreeView whether the parent is expanded; the tree's root is
// invisible and its children are always shown, so it is expanded by definition
static TocItem* FindVisibleParentTreeItem(TocTree* tm, TocItem* ti) {
    TocItem* root = tm ? tm->root : nullptr;
    if (!ti || ti == root) {
        return nullptr;
    }
    while (true) {
        auto* parent = ti->parent;
        if (parent == nullptr || parent == root) {
            // ti is a top-level node
            return ti;
        }
        if (parent->IsExpanded()) {
            return ti;
        }
        ti = parent;
    }
}

void UpdateTocSelection(MainWindow* win, int currPageNo) {
    TocTree* tm = CurrentTocTree(win);
    if (!win->tocLoaded || !win->uiState.tocVisible || !tm) {
        return;
    }

    // Browser markdown/HTML docs render a whole file as a single "page" and we
    // can't detect which heading is scrolled into view, so a page-based update
    // would select/highlight every heading in the file. Skip it and leave the
    // TOC selection wherever the user's last click put it.
    if (win->ctrl && win->ctrl->AsMarkdown()) {
        return;
    }

    TocItem* item = nullptr;
    if (win->ctrl && win->ctrl->HasChapters()) {
        item = TreeItemForChapter(tm, win->ctrl->CurrentLocation().chapter, currPageNo);
    }
    if (!item) {
        item = TreeItemForPageNo(tm, currPageNo);
    }
    if (win->tocKeepSelection) {
        // the tree selection is deliberately left alone: the user clicked a
        // bookmark and GoToTocLink set tocKeepSelection so the page change
        // doesn't move the selection off it. The multi-match "current page"
        // highlight is a different thing though and must still follow the page.
        SetTocMultiHighlight(win, tm, item);
        return;
    }

    // only select the items that are visible i.e. are top nodes or
    // children of expanded node
    SidebarSetTocSelection(win, FindVisibleParentTreeItem(tm, item));
    SetTocMultiHighlight(win, tm, item);
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
        SetSidebarVisibility(win, true, gSettings->showFavorites);
    }
    TocTree* tm = CurrentTocTree(win);
    if (!win->tocLoaded || !win->uiState.tocVisible || !tm) {
        return;
    }
    int currPageNo = win->ctrl->CurrentPageNo();
    TocItem* item = nullptr;
    if (win->ctrl->HasChapters()) {
        // unresolved chaptered items keep pageNo == -1; match by chapter first
        item = TreeItemForChapter(tm, win->ctrl->CurrentLocation().chapter, currPageNo);
    }
    if (!item) {
        item = TreeItemForPageNo(tm, currPageNo);
    }
    if (!item) {
        return;
    }
    SidebarRevealTocItem(win, item);
    SetTocMultiHighlight(win, tm, item);
}

static void UpdateDocTocExpansionStateRecur(Vec<int>& tocState, TocItem* tocItem) {
    while (tocItem) {
        // items without children cannot be toggled
        if (tocItem->child) {
            bool wasToggled = tocItem->IsExpanded() != tocItem->isOpenDefault;
            if (wasToggled) {
                VecAppend(tocState, tocItem->id);
            }
            UpdateDocTocExpansionStateRecur(tocState, tocItem->child);
        }
        tocItem = tocItem->next;
    }
}

void UpdateTocExpansionState(Vec<int>& tocState, MainWindow* win, TocTree* docTree) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!docTree || !docTree->root || !tab || tab->currToc != docTree) {
        return;
    }
    VecReset(tocState);
    UpdateDocTocExpansionStateRecur(tocState, docTree->root->child);
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
// clang-format on

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
        item->isOpenToggled = VecContains(tocState, item->id);
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

// --- expand / collapse commands ---------------------------------------------

static void SetExpanded(TocItem* item, bool expanded) {
    if (!item->child) {
        return;
    }
    if (item->IsExpanded() != expanded) {
        item->isOpenToggled = !item->isOpenToggled;
    }
}

static void ExpandAllRec(TocItem* item, bool expanded) {
    while (item) {
        SetExpanded(item, expanded);
        ExpandAllRec(item->child, expanded);
        item = item->next;
    }
}

// Expand outline nodes whose depth is < maxDepth (depth 1 = top-level rows).
// Call after a full collapse so the tree ends at exactly that level.
static void TocExpandItemsToDepth(TocItem* item, int depth, int maxDepth) {
    while (item) {
        if (depth < maxDepth) {
            SetExpanded(item, true);
            TocExpandItemsToDepth(item->child, depth + 1, maxDepth);
        }
        item = item->next;
    }
}

void TocExpandAll(MainWindow* win) {
    TocTree* tree = CurrentTocTree(win);
    if (!tree || !tree->root) {
        return;
    }
    ExpandAllRec(tree->root->child, true);
    AppShellInvalidate(win);
}

// Expand outline only through `level` (1 = top-level rows collapsed, 2 = expand
// top-level once, 3 = two levels deep). Issue #5239.
void TocExpandToLevel(MainWindow* win, int level) {
    TocTree* tree = CurrentTocTree(win);
    if (!tree || !tree->root || level < 1) {
        return;
    }
    ExpandAllRec(tree->root->child, false);
    if (level > 1) {
        TocExpandItemsToDepth(tree->root->child, 1, level);
    }
    AppShellInvalidate(win);
}

// Collapse all; if there is a single top-level entry with children (typical
// Word-export TOC), expand it one level so Collapse All is useful (#5239).
void TocCollapseAll(MainWindow* win) {
    TocExpandToLevel(win, 1);
    TocTree* tree = CurrentTocTree(win);
    TocItem* root = tree && tree->root ? tree->root->child : nullptr;
    if (root && !root->next && root->child) {
        SetExpanded(root, true);
    }
    AppShellInvalidate(win);
}

// Collapse every outline row that shares the parent of `ti` (same nesting level
// / siblings). If `ti` is null, use the current selection; if still none, all
// top-level rows. Issue #1895.
void TocCollapseSameLevel(MainWindow* win, TocItem* ti) {
    TocTree* tree = CurrentTocTree(win);
    if (!tree || !tree->root) {
        return;
    }
    if (!ti) {
        ti = SidebarTocSelection(win);
    }
    TocItem* first = tree->root->child;
    if (ti && ti->parent && ti->parent != tree->root) {
        first = ti->parent->child;
    }
    for (TocItem* sibling = first; sibling; sibling = sibling->next) {
        SetExpanded(sibling, false);
    }
    AppShellInvalidate(win);
}

// --- context menu -----------------------------------------------------------

// clang-format off
static MenuDef menuDefContextToc[] = {
    {
        TrN("Expand All"),
        CmdExpandAll,
    },
    {
        TrN("Collapse All"),
        CmdCollapseAll,
    },
    {
        TrN("Expand to Level 1"),
        CmdTocExpandToLevel1,
    },
    {
        TrN("Expand to Level 2"),
        CmdTocExpandToLevel2,
    },
    {
        TrN("Expand to Level 3"),
        CmdTocExpandToLevel3,
    },
    {
        TrN("Collapse Same Level"),
        CmdTocCollapseSameLevel,
    },
    {
        TrN("Expand to Current Page"),
        CmdExpandToCurrentPage,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Open Embedded PDF"),
        CmdOpenEmbeddedPDF,
    },
    {
        TrN("Save Embedded File..."),
        CmdSaveEmbeddedFile,
    },
    {
        TrN("Open Attachment"),
        CmdOpenAttachment,
    },
    {
        TrN("Save Attachment..."),
        CmdSaveAttachment,
    },
    // note: strings cannot be "" or else items are not there
    {
        StrL("Add to favorites"),
        CmdFavoriteAdd,
    },
    {
        StrL("Remove from favorites"),
        CmdFavoriteDel,
    },
    {
        {},
        0,
    },
};
// clang-format on

// ng: orig builds an HMENU and tracks it; the pane builds a gpui PopupMenu from
// this model.
MenuModel* BuildTocContextMenu(MainWindow* win, TocItem* dti) {
    Str filePath = win->ctrl ? win->ctrl->GetFilePath() : Str{};
    int pageNo = 0;
    IPageDestination* dest = dti ? dti->dest : nullptr;
    if (dest) {
        pageNo = PageDestGetPageNo(dest);
    }
    MenuModel* popup = BuildMenuFromDef(menuDefContextToc, nullptr);

    Kind destKind = dest ? dest->GetKind() : nullptr;
    if (destKind == kindDestinationLaunchEmbedded) {
        // this is name of the file as set inside the PDF file
        Str fileName = dest->GetName();
        if (!str::EndsWithI(fileName, StrL(".pdf"))) {
            MenuRemove(popup, CmdOpenEmbeddedPDF);
        }
    } else {
        MenuRemove(popup, CmdSaveEmbeddedFile);
        MenuRemove(popup, CmdOpenEmbeddedPDF);
    }
    if (destKind == kindDestinationAttachment) {
        Str fileName = dest->GetName();
        if (!str::EndsWithI(fileName, StrL(".pdf"))) {
            MenuRemove(popup, CmdOpenAttachment);
        }
    } else {
        MenuRemove(popup, CmdSaveAttachment);
        MenuRemove(popup, CmdOpenAttachment);
    }

    if (pageNo > 0) {
        bool isBookmarked = IsPageInFavorites(filePath, pageNo, win->ctrl);

        TempStr addText;
        TempStr delText;
        if (win->ctrl->HasChapters()) {
            Location loc = win->ctrl->LocationFromPageNo(pageNo);
            addText = fmt(Tr("Add chapter %d page %d to favorites").s, loc.chapter, loc.page);
            delText = fmt(Tr("Remove chapter %d page %d from favorites").s, loc.chapter, loc.page);
        } else {
            TempStr pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
            addText = fmt(Tr("Add page %s to favorites").s, pageLabel);
            delText = fmt(Tr("Remove page %s from favorites").s, pageLabel);
        }

        if (isBookmarked) {
            MenuRemove(popup, CmdFavoriteAdd);
            MenuSetText(popup, CmdFavoriteDel, delText);
        } else {
            MenuRemove(popup, CmdFavoriteDel);
            MenuSetText(popup, CmdFavoriteAdd, addText);
        }
    } else {
        MenuRemove(popup, CmdFavoriteAdd);
        MenuRemove(popup, CmdFavoriteDel);
    }
    RemoveBadMenuSeparators(popup);
    logf("BuildTocContextMenu: '%s', %d rows\n", dti ? dti->title : Str{}, popup->items.len);
    return popup;
}

// --- embedded files and attachments (orig's TableOfContents.cpp) ------------

static void SaveAttachment(WindowTab* tab, Str fileName, int attachmentNo) {
    if (!tab || !tab->AsFixed()) {
        return;
    }
    EngineBase* engine = tab->AsFixed()->GetEngine();
    Str data = EngineMupdfLoadAttachment(engine, attachmentNo);
    if (len(data) == 0) {
        return;
    }
    TempStr dir = path::GetDirTemp(tab->filePath);
    TempStr dstPath = path::JoinTemp(dir, path::GetBaseNameTemp(fileName));
    SaveDataToFile(tab->win, dstPath, data);
    str::Free(data);
}

static void OpenAttachment(WindowTab* tab, Str fileName, int attachmentNo) {
    if (!tab || !tab->AsFixed()) {
        return;
    }
    EngineBase* engine = tab->AsFixed()->GetEngine();
    Str data = EngineMupdfLoadAttachment(engine, attachmentNo);
    if (len(data) == 0) {
        return;
    }
    OpenDocumentFromMemory(tab->win, data, fileName);
    str::Free(data);
}

static void OpenEmbeddedFile(WindowTab* tab, IPageDestination* dest) {
    if (!tab || !dest) {
        return;
    }
    auto* destFile = (PageDestinationFile*)dest;
    Str path = destFile->path;
    Str tabPath = tab->filePath;
    if (!str::StartsWith(path, tabPath)) {
        return;
    }
    LoadDocument(tab->win, path);
}

static void SaveEmbeddedFile(WindowTab* tab, IPageDestination* dest) {
    if (!tab || !dest) {
        return;
    }
    auto* destFile = (PageDestinationFile*)dest;
    Str data = LoadEmbeddedPDFFile(destFile->path);
    if (len(data) == 0) {
        return;
    }
    TempStr dir = path::GetDirTemp(tab->filePath);
    TempStr dstPath = path::JoinTemp(dir, path::GetBaseNameTemp(dest->GetName()));
    SaveDataToFile(tab->win, dstPath, data);
    str::Free(data);
}

void TocContextMenuCommand(MainWindow* win, TocItem* dti, int cmd) {
    logf("TocContextMenuCommand: %d (%s) on '%s'\n", cmd, GetCommandName(cmd), dti ? dti->title : Str{});
    Str filePath = win->ctrl ? win->ctrl->GetFilePath() : Str{};
    int pageNo = 0;
    IPageDestination* dest = dti ? dti->dest : nullptr;
    if (dest) {
        pageNo = PageDestGetPageNo(dest);
    }
    switch (cmd) {
        case CmdExpandAll:
            TocExpandAll(win);
            break;
        case CmdCollapseAll:
            TocCollapseAll(win);
            break;
        case CmdTocExpandToLevel1:
            TocExpandToLevel(win, 1);
            break;
        case CmdTocExpandToLevel2:
            TocExpandToLevel(win, 2);
            break;
        case CmdTocExpandToLevel3:
            TocExpandToLevel(win, 3);
            break;
        case CmdTocCollapseSameLevel:
            TocCollapseSameLevel(win, dti);
            break;
        case CmdExpandToCurrentPage:
            ExpandTocToCurrentPage(win);
            break;
        case CmdFavoriteAdd:
            AddFavoriteFromToc(win, dti);
            break;
        case CmdFavoriteDel:
            DelFavorite(filePath, pageNo, win->ctrl);
            break;
        case CmdSaveEmbeddedFile:
            SaveEmbeddedFile(win->CurrentTab(), dest);
            break;
        case CmdOpenEmbeddedPDF:
            OpenEmbeddedFile(win->CurrentTab(), dest);
            break;
        case CmdSaveAttachment:
            // hack: the attachment number is saved in pageNo, see
            // PdfLoadAttachments and DestFromAttachment
            SaveAttachment(win->CurrentTab(), dest->GetName(), pageNo);
            break;
        case CmdOpenAttachment:
            OpenAttachment(win->CurrentTab(), dest->GetName(), pageNo);
            break;
        default:
            break;
    }
}

// --- loading ----------------------------------------------------------------

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
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;
    tab->currToc = nullptr;
    SidebarSetTocFilterText(win, StrL(""));

    auto* tocTree = tab->ctrl->GetToc();
    if (!tocTree || !tocTree->root) {
        return;
    }

    tab->currToc = tocTree;
    logf("LoadTocTree: %d top-level bookmarks\n", tocTree->ChildCount(tocTree->Root()));

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tocTree->root, l2r, r2l);
    SidebarSetTocRtl(win, r2l > l2r);

    SetInitialExpandState(tocTree->root, tab->tocState);
    AutoExpandTopLevelItems(tocTree->root->child);
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
    TempStr selTitle;
    int selPageNo = 0;
    if (TocItem* sel = SidebarTocSelection(win); sel) {
        selTitle = str::DupTemp(sel->title);
        selPageNo = sel->pageNo;
    }
    int currPageNo = win->currPageNo;

    ClearTocBox(win);
    LoadTocTree(win);

    if (!tab->currToc) {
        return;
    }
    TocItem* toSelect = nullptr;
    if (selTitle) {
        toSelect = FindTocItemByTitleAndPage(tab->currToc->root, selTitle, selPageNo);
    }
    if (toSelect) {
        SidebarSetTocSelection(win, toSelect);
        SetTocMultiHighlight(win, tab->currToc, toSelect);
        return;
    }
    // nothing matched (or nothing was selected): fall back to the current page
    UpdateTocSelection(win, currPageNo);
}

// --- the bookmark filter ----------------------------------------------------

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
    SidebarSetTocSelection(win, nullptr);
    VecReset(win->tocMatchingItems);
    delete win->tocFilteredTree;
    win->tocFilteredTree = nullptr;

    TocTree* origTree = tab->currToc;

    StrVec words;
    if (filter) {
        SplitFilterToWords(filter, words);
    }
    if (len(words) == 0) {
        // restore original tree
        SetInitialExpandState(origTree->root, tab->tocState);
        UpdateTocSelection(win, win->currPageNo);
        return;
    }

    TocItem* filteredItems = FilterTocItemRec(origTree->root, words);
    if (!filteredItems) {
        return;
    }
    // the root itself is invisible, so wrap the promoted sibling list in a
    // dummy root - the same shape every engine uses for a TocTree
    auto* wrapRoot = AllocTocItem(nullptr, {}, 0);
    wrapRoot->child = filteredItems;
    for (TocItem* c = filteredItems; c; c = c->next) {
        c->parent = wrapRoot;
    }
    win->tocFilteredTree = new TocTree(wrapRoot);
}

void TocFilterChanged(MainWindow* win) {
    TempStr filter = SidebarTocFilterTextTemp(win);
    ApplyTocFilter(win, filter);
    TocTree* tree = CurrentTocTree(win);
    int n = tree ? tree->ChildCount(tree->Root()) : 0;
    logf("TocFilterChanged: '%s' -> %d top-level bookmarks\n", filter, n);
    AppShellInvalidate(win);
}
