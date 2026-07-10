/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/BitManip.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "wingui/LabelWithCloseWnd.h"

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

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC (WM_APP + 1)
#endif

static void LayoutTocContainer(MainWindow* win);

// set tooltip for this item but only if the text isn't fully shown
// TODO: I might have lost something in translation
static void TocCustomizeTooltip(TreeView::GetTooltipEvent* ev) {
    auto treeView = ev->treeView;
    auto tm = treeView->treeModel;
    auto ti = ev->treeItem;
    auto nm = ev->info;
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
    auto k = link->GetKind();
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
    RECT rcLine, rcLabel;
    treeView->GetItemRect(ev->treeItem, false, rcLine);
    treeView->GetItemRect(ev->treeItem, true, rcLabel);

    // TODO: this causes a duplicate. Not sure what changed
    if (false && rcLine.right + 2 < rcLabel.right) {
        Str currInfoTip = tm->Text(ti);
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

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd) {
    // code inspired by http://www.codeguru.com/cpp/controls/treeview/multiview/article.php/c3985/
    LPNMCUSTOMDRAW ncd = &ntvcd->nmcd;
    HWND hTV = ncd->hdr.hwndFrom;
    HTREEITEM hItem = (HTREEITEM)ncd->dwItemSpec;
    RECT rcItem;
    if (0 == ncd->rc.right - ncd->rc.left || 0 == ncd->rc.bottom - ncd->rc.top) return;
    if (!TreeView_GetItemRect(hTV, hItem, &rcItem, TRUE)) return;
    if (rcItem.right > ncd->rc.right) rcItem.right = ncd->rc.right;

    // Clear the label
    RECT rcFullWidth = rcItem;
    rcFullWidth.right = ncd->rc.right;
    FillRect(ncd->hdc, &rcFullWidth, GetSysColorBrush(COLOR_WINDOW));

    // Get the label's text
    WCHAR szText[MAX_PATH];
    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.pszText = szText;
    item.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTV, &item);

    // Draw the page number right-aligned (if there is one)
    MainWindow* win = FindMainWindowByHwnd(hTV);
    TocItem* tocItem = (TocItem*)item.lParam;
    TempStr label = nullptr;
    if (tocItem->pageNo && win && win->IsDocLoaded()) {
        label = win->ctrl->GetPageLabeTemp(tocItem->pageNo);
        label = str::JoinTemp(StrL("  "), label);
    }
    if (label && str::EndsWith(item.pszText, label)) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, label, len(label), &txtSize);
        rcPageNo.left = rcPageNo.right - txtSize.cx;

        SetTextColor(ncd->hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(ncd->hdc, GetSysColor(COLOR_WINDOW));
        DrawTextW(ncd->hdc, label, -1, &rcPageNo, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // Reduce the size of the label and cut off the page number
        rcItem.right = std::max(rcItem.right - txtSize.cx, 0);
        szText[len(szText) - len(label)] = '\0';
    }

    SetTextColor(ncd->hdc, ntvcd->clrText);
    SetBkColor(ncd->hdc, ntvcd->clrTextBk);

    // Draw the focus rectangle (including proper background color)
    HBRUSH brushBg = CreateSolidBrush(ntvcd->clrTextBk);
    FillRect(ncd->hdc, &rcItem, brushBg);
    DeleteObject(brushBg);
    if ((ncd->uItemState & CDIS_FOCUS)) DrawFocusRect(ncd->hdc, &rcItem);

    InflateRect(&rcItem, -2, -1);
    DrawTextW(ncd->hdc, szText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

struct GoToTocLinkData {
    TocItem* tocItem;
    WindowTab* tab;
    DocController* ctrl;
    // true when the navigation was driven from outside the tree (e.g. the
    // command palette), so afterwards we must move the tree's selection to the
    // item ourselves. For tree-driven navigation the tree is already selected.
    bool selectInTree = false;
};

static void GoToTocLink(GoToTocLinkData* d) {
    AutoDelete delData(d);

    auto tab = d->tab;
    auto tocItem = d->tocItem;
    auto ctrl = d->ctrl;

    // validate tab before dereferencing — it may have been freed
    // while this task was queued (e.g. user closed the tab/window)
    if (!IsWindowTabValid(tab)) {
        return;
    }
    MainWindow* win = tab->win;
    // tocItem is invalid if the DocController has been replaced
    if (!IsMainWindowValid(win) || win->CurrentTab() != tab || tab->ctrl != ctrl) {
        return;
    }

    // make sure that the tree item that the user selected
    // isn't unselected in UpdateTocSelection right again
    win->tocKeepSelection = true;
    int pageNo = tocItem->pageNo;
    IPageDestination* dest = tocItem->GetPageDestination();
    if (dest) {
        ctrl->HandleLink(dest, win->linkHandler);
    } else if (pageNo) {
        ctrl->GoToPage(pageNo, true);
    }
    win->tocKeepSelection = false;

    // when driven from the command palette the tree wasn't the source of the
    // navigation, so the page-based UpdateTocSelection was suppressed above and
    // the tree still shows the old item. Move the selection to this item now
    // (programmatic SelectItem doesn't re-navigate -- see TocTreeSelectionChanged).
    if (d->selectInTree && win->tocLoaded && win->tocTreeView) {
        TreeView* treeView = win->tocTreeView;
        HTREEITEM hi = treeView->GetHandleByTreeItem((TreeItem)tocItem);
        if (hi) {
            TreeView_EnsureVisible(treeView->hwnd, hi);
        }
        treeView->SelectItem((TreeItem)tocItem);
    }
}

// navigate to a TocItem regardless of whether it points to a page in this
// document or to an external destination (used by the command palette, where
// the user explicitly picked the item so we always honor it)
void GoToTocItem(MainWindow* win, TocItem* tocItem) {
    if (!win || !tocItem) {
        return;
    }
    auto data = new GoToTocLinkData;
    data->ctrl = win->ctrl;
    data->tocItem = tocItem;
    data->tab = win->CurrentTab();
    data->selectInTree = true; // palette-driven: sync the tree selection too
    auto fn = MkFunc0<GoToTocLinkData>(GoToTocLink, data);
    uitask::Post(fn, "TaskGoToTocFromPalette");
}

static bool IsScrollToLink(IPageDestination* link) {
    if (!link) {
        return false;
    }
    auto kind = link->GetKind();
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
        auto data = new GoToTocLinkData;
        data->ctrl = win->ctrl;
        data->tocItem = tocItem;
        data->tab = win->CurrentTab();
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
    if (win->tocVisible) {
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
        return;
    }
    SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    if (win->tocVisible) {
        HwndSetFocus(win->tocTreeView->hwnd);
    }
}

struct VistorForPageNoData {
    int pageNo = -1;

    TocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;
    int nItems = 0;
};

void visitTree(VistorForPageNoData* d, TreeItemVisitorData* vd) {
    auto tocItem = (TocItem*)vd->item;
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
        return 0;
    }
    VistorForPageNoData d;
    d.pageNo = pageNo;
    auto fn = MkFunc1<VistorForPageNoData, TreeItemVisitorData*>(visitTree, &d);
    VisitTreeModelItems(tm, fn);
    // if there's only one item, we want to unselect it so that it can
    // be selected by the user
    if (d.nItems < 2) {
        return 0;
    }
    return d.bestMatch;
}

// TODO: I can't use TreeItem->IsExpanded() because it's not in sync with
// the changes user makes to TreeCtrl
static TocItem* FindVisibleParentTreeItem(TreeView* treeView, TocItem* ti) {
    if (!ti) {
        return nullptr;
    }
    while (true) {
        auto parent = ti->parent;
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
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection) {
        return;
    }

    auto treeView = win->tocTreeView;
    auto item = TreeItemForPageNo(treeView, currPageNo);
    // only select the items that are visible i.e. are top nodes or
    // children of expanded node
    TreeItem toSelect = (TreeItem)FindVisibleParentTreeItem(treeView, item);
    treeView->SelectItem(toSelect);
}

// expand the table of contents tree down to the entry matching the current
// page, then select and scroll to it (issue #1998, like Explorer's
// "Expand to current folder")
void ExpandTocToCurrentPage(MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    // make sure the bookmarks (table of contents) sidebar is visible
    if (!win->tocVisible) {
        SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    }
    if (!win->tocLoaded || !win->tocVisible) {
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

    POINT pt{};

    TreeView* treeView = (TreeView*)ev->w;
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
        auto embeddedFile = (PageDestinationFile*)dest;
        // this is a path to a file on disk, e.g. a path to opened PDF
        // with the embedded stream number
        path = embeddedFile->path;
        // this is name of the file as set inside PDF file
        fileName = PageDestGetName(dest);
        bool canOpenEmbedded = str::EndsWithI(fileName, ".pdf");
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
        auto attachment = (PageDestinationFile*)dest;
        // this is a path to a file on disk, e.g. a path to opened PDF
        // with the embedded stream number
        path = attachment->path;
        // this is name of the file as set inside PDF file
        fileName = PageDestGetName(dest);
        // hack: attachmentNo is saved in pageNo see
        // PdfLoadAttachments and DestFromAttachment
        attachmentNo = pageNo;
        bool canOpenEmbedded = str::EndsWithI(fileName, ".pdf");
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
            win->tocTreeView->CollapseAll();
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

void OnTocCustomDraw(TreeView::CustomDrawEvent*);

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
    ReportIf(!tab);

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

// TODO: use https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getobject?redirectedfrom=MSDN
// to get LOGFONT from existing font and then create a derived font
static void UpdateFont(HDC hdc, HWND hwnd, int fontFlags) {
    bool italic = bit::IsSet(fontFlags, fontBitItalic);
    bool bold = bit::IsSet(fontFlags, fontBitBold);
    HFONT hfont = GetAppTreeFontEx(hwnd, bold, italic);
    SelectObject(hdc, hfont);
}

static bool HasTocFilter(MainWindow* win) {
    if (!win || !win->tocFilterEdit) {
        return false;
    }
    TempStr filter = win->tocFilterEdit->GetTextTemp();
    return filter && len(filter) > 0;
}

static void DrawTocItemHighlight(TreeView::CustomDrawEvent* ev, MainWindow* win) {
    TocItem* tocItem = (TocItem*)ev->treeItem;
    if (!tocItem || !tocItem->title) {
        return;
    }
    Edit* edit = win->tocFilterEdit;
    if (!edit) {
        return;
    }
    TempStr filter = edit->GetTextTemp();
    if (!filter || len(filter) == 0) {
        return;
    }
    Str title = tocItem->title;
    int titleLen = title.len;
    if (titleLen == 0) {
        return;
    }

    // mark which bytes are part of a match
    u8* highlighted = AllocArrayTemp<u8>(titleLen);
    int filterLen = filter.len;
    Str rest = title;
    while (len(rest) > 0) {
        int idx = str::IndexOfI(rest, filter);
        if (idx < 0) {
            break;
        }
        int off = (int)(rest.s - title.s) + idx;
        for (int k = 0; k < filterLen && off + k < titleLen; k++) {
            highlighted[off + k] = 1;
        }
        int skip = idx + filterLen;
        rest.s += skip;
        rest.len -= skip;
    }

    // collect contiguous highlighted ranges (up to 16)
    struct ByteRange {
        int start;
        int end;
    };
    ByteRange byteRanges[16];
    int nRanges = 0;
    {
        int pos = 0;
        while (pos < titleLen && nRanges < 16) {
            if (highlighted[pos]) {
                int start = pos;
                while (pos < titleLen && highlighted[pos]) {
                    pos++;
                }
                byteRanges[nRanges++] = {start, pos};
            } else {
                pos++;
            }
        }
    }
    if (nRanges == 0) {
        return;
    }

    // get the label rect for this tree item
    RECT labelRect;
    TreeView* tv = ev->treeView;
    if (!tv->GetItemRect(ev->treeItem, true, labelRect)) {
        return;
    }

    NMTVCUSTOMDRAW* tvcd = ev->nm;
    HDC hdc = tvcd->nmcd.hdc;

    WCHAR* titleW = CWStrTemp(title);

    // compute pixel rectangles for each highlighted range
    RECT highlightRects[16];
    for (int i = 0; i < nRanges; i++) {
        TempWStr prefixToStart = ToWStrTemp(Str(title.s, byteRanges[i].start));
        int wStart = len(prefixToStart);
        TempWStr prefixToEnd = ToWStrTemp(Str(title.s, byteRanges[i].end));
        int wEnd = len(prefixToEnd);

        SIZE szStart, szEnd;
        GetTextExtentPoint32W(hdc, titleW, wStart, &szStart);
        GetTextExtentPoint32W(hdc, titleW, wEnd, &szEnd);

        highlightRects[i].top = labelRect.top;
        highlightRects[i].bottom = labelRect.bottom;
        highlightRects[i].left = labelRect.left + szStart.cx;
        highlightRects[i].right = labelRect.left + szEnd.cx;
    }

    // erase the label area with the correct background color
    // so we can redraw text cleanly without double-draw artifacts
    NMCUSTOMDRAW* cd = &tvcd->nmcd;
    bool isSelected = (cd->uItemState & CDIS_SELECTED) != 0;
    bool hasFocus = (GetFocus() == tv->hwnd);
    COLORREF bgCol;
    if (isSelected) {
        bgCol = GetSysColor(hasFocus ? COLOR_HIGHLIGHT : COLOR_BTNFACE);
    } else {
        bgCol = IsSpecialColor(tv->bgColor) ? GetSysColor(COLOR_WINDOW) : tv->bgColor;
    }
    HBRUSH hbrBg = CreateSolidBrush(bgCol);
    FillRect(hdc, &labelRect, hbrBg);
    DeleteObject(hbrBg);

    // draw highlight background rectangles
    COLORREF highlightCol;
    if (IsCurrentThemeDefault()) {
        highlightCol = RGB(255, 255, 0);
    } else {
        highlightCol = AccentColor(bgCol, 40);
    }
    HBRUSH hbrHighlight = CreateSolidBrush(highlightCol);
    for (int i = 0; i < nRanges; i++) {
        FillRect(hdc, &highlightRects[i], hbrHighlight);
    }
    DeleteObject(hbrHighlight);

    // draw the text on top
    COLORREF txtCol;
    if (isSelected && hasFocus) {
        txtCol = GetSysColor(COLOR_HIGHLIGHTTEXT);
    } else if (tocItem->color != kColorUnset) {
        txtCol = tocItem->color;
    } else {
        txtCol = IsSpecialColor(tv->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : tv->textColor;
    }
    COLORREF oldTxtCol = SetTextColor(hdc, txtCol);
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, titleW, -1, &labelRect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldTxtCol);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/about-custom-draw
// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-nmtvcustomdraw
void OnTocCustomDraw(TreeView::CustomDrawEvent* ev) {
#if defined(DISPLAY_TOC_PAGE_NUMBERS)
    if (false) return CDRF_DODEFAULT;
    switch (((LPNMCUSTOMDRAW)pnmtv)->dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT:
            return CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT;
        case CDDS_ITEMPOSTPAINT:
            RelayoutTocItem((LPNMTVCUSTOMDRAW)pnmtv);
            // fall through
        default:
            return CDRF_DODEFAULT;
    }
    break;
#endif

    ev->result = CDRF_DODEFAULT;
    NMTVCUSTOMDRAW* tvcd = ev->nm;
    NMCUSTOMDRAW* cd = &(tvcd->nmcd);

    if (cd->dwDrawStage == CDDS_PREPAINT) {
        ev->result = CDRF_NOTIFYITEMDRAW;
        return;
    }

    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    bool filterActive = HasTocFilter(win);

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        TocItem* tocItem = (TocItem*)ev->treeItem;
        if (!tocItem) {
            return;
        }
        LRESULT res = 0;
        if (tocItem->color != kColorUnset) {
            tvcd->clrText = tocItem->color;
        }
        if (tocItem->fontFlags != 0) {
            UpdateFont(cd->hdc, ev->treeView->hwnd, tocItem->fontFlags);
            res = CDRF_NEWFONT;
        }
        if (filterActive) {
            res |= CDRF_NOTIFYPOSTPAINT;
        }
        ev->result = res;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPOSTPAINT) {
        if (filterActive && win) {
            DrawTocItemHighlight(ev, win);
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

void TocTreeClick(TreeView::ClickEvent* ev) {
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
    ReportIf(!win);
    GoToTocTreeItem(win, ev->treeItem, true);
}

static void TocTreeSelectionChanged(TreeView::SelectionChangedEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    ReportIf(!win);

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

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void TocTreeMsgFilter(WndEvent*) {
    switch (msg) {
        case WM_SIZE:
        case WM_HSCROLL:
            // Repaint the ToC so that RelayoutTocItem is called for all items
            PostMessageW(hwnd, WM_APP_REPAINT_TOC, 0, 0);
            break;
        case WM_APP_REPAINT_TOC:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            break;
    }
}
#endif

// Position label, filter edit, and tree window within toc container using the
// wingui layout engine (VBox built in CreateToc).
static void LayoutTocContainer(MainWindow* win) {
    if (!win->tocLayout) {
        return;
    }
    Rect rc = WindowRect(win->hwndTocBox);
    win->tocLayout->Layout(Tight(Size{rc.dx, rc.dy}));
    win->tocLayout->SetBounds(Rect{0, 0, rc.dx, rc.dy});
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    LRESULT res = 0;
    res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    switch (msg) {
        case WM_SIZE:
            LayoutTocContainer(win);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_TOC_LABEL_WITH_CLOSE) {
                ToggleTocBox(win);
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SubclassToc(MainWindow* win) {
    HWND hwndTocBox = win->hwndTocBox;

    if (win->tocBoxSubclassId == 0) {
        win->tocBoxSubclassId = NextSubclassId();
        BOOL ok = SetWindowSubclass(hwndTocBox, WndProcTocBox, win->tocBoxSubclassId, (DWORD_PTR)win);
        ReportIf(!ok);
    }
}

void UnsubclassToc(MainWindow* win) {
    if (win->tocBoxSubclassId != 0) {
        RemoveWindowSubclass(win->hwndTocBox, WndProcTocBox, win->tocBoxSubclassId);
        win->tocBoxSubclassId = 0;
    }
}

// Recursively build a filtered copy of the TocItem tree.
// Includes items whose title matches the filter, plus ancestors needed to reach them.
// Returns nullptr if nothing matches.
static TocItem* FilterTocItemRec(TocItem* item, Str filter) {
    if (!item) {
        return nullptr;
    }
    TocItem* resultFirst = nullptr;
    TocItem* resultLast = nullptr;
    for (TocItem* si = item; si; si = si->next) {
        // recursively filter children
        TocItem* filteredChildren = FilterTocItemRec(si->child, filter);
        bool titleMatches = si->title && str::ContainsI(si->title, filter);
        if (!titleMatches && !filteredChildren) {
            continue;
        }
        // create a copy of this item
        auto* copy = new TocItem();
        copy->title = str::Dup(si->title);
        copy->pageNo = si->pageNo;
        copy->id = si->id;
        copy->fontFlags = si->fontFlags;
        copy->color = si->color;
        copy->dest = si->dest;
        copy->destNotOwned = true;
        copy->isOpenDefault = true;
        copy->isOpenToggled = false;
        copy->child = filteredChildren;
        // set parent pointers on children
        for (TocItem* c = copy->child; c; c = c->next) {
            c->parent = copy;
        }
        if (!resultFirst) {
            resultFirst = copy;
            resultLast = copy;
        } else {
            resultLast->next = copy;
            resultLast = copy;
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

    if (!filter || len(filter) == 0) {
        // restore original tree
        SetInitialExpandState(origTree->root, tab->tocState);
        treeView->SetTreeModel(origTree);
        return;
    }

    TocItem* filteredRoot = FilterTocItemRec(origTree->root, filter);
    if (!filteredRoot) {
        treeView->Clear();
        return;
    }
    auto* filteredTree = new TocTree(filteredRoot);
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

static LRESULT CALLBACK WndProcTocFilterEdit(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId,
                                             DWORD_PTR data) {
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        MainWindow* win = (MainWindow*)data;
        Edit* edit = win->tocFilterEdit;
        if (edit) {
            TempStr txt = edit->GetTextTemp();
            if (txt && len(txt) > 0) {
                edit->SetText("");
                // onTextChanged will fire and restore the tree
                return 0;
            }
            // if already empty, move focus to tree
            SetFocus(win->tocTreeView->hwnd);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void CreateToc(MainWindow* win) {
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndTocBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, nullptr, hmod, nullptr);

    auto l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndTocBox;
        args.cmdId = IDC_TOC_LABEL_WITH_CLOSE;
        args.isRtl = IsUIRtl();
        args.font = GetAppSidebarLabelFont(win->hwndFrame);
        l->Create(args);
    }
    win->tocLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    // label is set in UpdateToolbarSidebarText()

    auto filterEdit = new Edit();
    {
        Edit::CreateArgs eargs;
        eargs.parent = win->hwndTocBox;
        eargs.withBorder = false;
        eargs.cueText = _TRA("Search Bookmarks");
        eargs.font = GetAppFont(win->hwndFrame);
        filterEdit->Create(eargs);
    }
    win->tocFilterEdit = filterEdit;
    filterEdit->onTextChanged = MkFunc0(OnTocFilterTextChanged, win);
    SetWindowSubclass(filterEdit->hwnd, WndProcTocFilterEdit, NextSubclassId(), (DWORD_PTR)win);

    auto treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent = win->hwndTocBox;
    args.font = GetAppTreeFont(win->hwndFrame);
    args.fullRowSelect = true;
    args.exStyle = 0;
    args.isRtl = IsUIRtl();

    auto fn = MkFunc1Void(TocContextMenu);
    treeView->onContextMenu = fn;
    treeView->onSelectionChanged = MkFunc1Void(TocTreeSelectionChanged);
    treeView->onKeyDown = MkFunc1Void(TocTreeKeyDown2);
    treeView->onGetTooltip = MkFunc1Void(TocCustomizeTooltip);
    treeView->onClick = MkFunc1Void(TocTreeClick);

    treeView->Create(args);
    ReportIf(!treeView->hwnd);
    win->tocTreeView = treeView;

    // stack label, filter edit and tree vertically; the tree flexes to fill the
    // remaining height. The VBox owns these three controls (freed in ~MainWindow).
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(l);
    vbox->AddChild(filterEdit);
    vbox->AddChild(treeView, 1);
    win->tocLayout = vbox;

    SubclassToc(win);

    UpdateControlsColors(win);
}
