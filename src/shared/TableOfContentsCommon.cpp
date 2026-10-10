/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "DisplayModel.h"
#include "Favorites.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Translations.h"
#include "Tabs.h"
#include "Menu.h"
#include "Accelerators.h"
#include "Theme.h"
#include "FilterUtil.h"
#include "TableOfContents.h"
#include "TableOfContentsCommon.h"

// When true, multi-highlight every TOC item that matches the current page
// (issue #4642). Easy to flip for comparison with single-selection behavior.
bool gShowAllMatchingTOC = true;

// Own a stable copy of the destination at post time. Engine-private kinds
// that hold fz_outline/fz_link (mupdf) are converted to scrollTo with
// page/rect/zoom already resolved on the TocItem/dest.
IPageDestination* SnapshotDestForDeferredNav(IPageDestination* dest, int tocPageNo) {
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
    // mupdf, djvu, none → page navigation snapshot
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

TocItem* FindTocItemByTitlePage(TocItem* item, Str title, int pageNo) {
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

// URL / file / embedded targets keep pageNo = -1 by design; only page-nav dests need pageNo >= 1.
static bool DestNeedsValidPageNo(IPageDestination* dest) {
    if (!dest) {
        return false;
    }
    Kind k = dest->GetKind();
    return !IsLaunchLinkKind(k) && k != kindDestinationLaunchEmbedded && k != kindDestinationAttachment;
}

GoToTocLinkData* NewGoToTocLinkData(MainWindow* win, TocItem* tocItem, bool selectInTree) {
    DocController* ctrl = win->ctrl;
    WindowTab* tab = win->CurrentTab();
    if (!ctrl || !tab || tab->ctrl != ctrl) {
        return nullptr;
    }

    int pageNo = tocItem->pageNo;
    IPageDestination* origDest = tocItem->dest;
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

bool IsScrollToLink(IPageDestination* link) {
    if (!link) {
        return false;
    }
    const auto* kind = link->GetKind();
    return kind == kindDestinationScrollTo;
}

void visitTree(VistorForPageNoData* d, TreeItemVisitorData* vd) {
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

void visitTreeForChapter(VisitorForChapterData* d, TreeItemVisitorData* vd) {
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

void visitCollectSamePage(CollectSamePageData* d, TreeItemVisitorData* vd) {
    auto* tocItem = (TocItem*)vd->item;
    if (!tocItem || tocItem->pageNo < 1) {
        return;
    }
    if (tocItem->pageNo == d->pageNo) {
        VecAppend(*d->out, tocItem);
    }
}

bool TocMatchingItemsContains(const Vec<TocItem*>& items, TocItem* item) {
    for (TocItem* t : items) {
        if (t == item) {
            return true;
        }
    }
    return false;
}

bool TocMatchingItemsEq(const Vec<TocItem*>& a, const Vec<TocItem*>& b) {
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

bool TocItemIsMultiHighlight(MainWindow* win, TocItem* item) {
    if (!gShowAllMatchingTOC || !win || !item) {
        return false;
    }
    return TocMatchingItemsContains(win->tocMatchingItems, item);
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

void GetLeftRightCounts(TocItem* node, int& l2r, int& r2l) {
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

void SetInitialExpandState(TocItem* item, Vec<int>& tocState) {
    while (item) {
        item->isOpenToggled = VecContains(tocState, item->id);
        SetInitialExpandState(item->child, tocState);
        item = item->next;
    }
}

void AddFavoriteFromToc(MainWindow* win, TocItem* dti) {
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

// auto-expand root level ToC nodes if there are at most two
void AutoExpandTopLevelItems(TocItem* root) {
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

TocItem* FindTocItemByTitleAndPage(TocItem* item, Str title, int pageNo) {
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
TocItem* FilterTocItemRec(TocItem* item, const StrVec& words) {
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
