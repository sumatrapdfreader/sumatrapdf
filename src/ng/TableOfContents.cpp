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
#include "TableOfContentsCommon.h"

#include "SumatraLog.h"

// Deferred TOC navigation must not hold raw TocItem* / IPageDestination*
// pointers: the TOC tree can be rebuilt or freed before the uitask runs
// (while tab->ctrl still matches), which caused UAF in HandleLink
// (crash 8bfe7adb1000001: EngineMupdf::HandleLink / dest->GetKind).

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

static void GoToTocTreeItem(MainWindow* win, TocItem* tocItem, bool allowExternal) {
    if (!tocItem) {
        return;
    }
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->dest);
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

// clang-format on

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
