/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/Log.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/SplitterWnd.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"
#include "wingui/DropDownCtrl.h"

#include "utils/GdiPlusUtil.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EnginePdf.h"
#include "EngineCreate.h"
#include "ParseBKM.h"

#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "Favorites.h"
#include "TabInfo.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "TableOfContents.h"
#include "Translations.h"
#include "Tabs.h"
#include "Menu.h"
#include "TocEditor.h"

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC (WM_APP + 1)
#endif

// set tooltip for this item but only if the text isn't fully shown
// TODO: I might have lost something in translation
static void TocCustomizeTooltip(TreeItmGetTooltipEvent* ev) {
    auto* w = ev->treeCtrl;
    auto* ti = ev->treeItem;
    auto* nm = ev->info;
    TocItem* tocItem = (TocItem*)ti;
    PageDestination* link = tocItem->GetPageDestination();
    if (!link) {
        return;
    }
    WCHAR* path = link->GetValue();
    if (!path) {
        path = tocItem->title;
    }
    if (!path) {
        return;
    }
    auto k = link->Kind();
    // TODO: TocItem from Chm contain other types
    // we probably shouldn't set TocItem::dest there
    if (k == kindDestinationScrollTo) {
        return;
    }
    if (k == kindDestinationNone) {
        return;
    }

    CrashIf(k != kindDestinationLaunchURL && k != kindDestinationLaunchFile && k != kindDestinationLaunchEmbedded);

    str::WStr infotip;

    // Display the item's full label, if it's overlong
    RECT rcLine, rcLabel;
    w->GetItemRect(ev->treeItem, false, rcLine);
    w->GetItemRect(ev->treeItem, true, rcLabel);

    if (rcLine.right + 2 < rcLabel.right) {
        str::WStr currInfoTip = ti->Text();
        infotip.Append(currInfoTip.Get());
        infotip.Append(L"\r\n");
    }

    if (kindDestinationLaunchEmbedded == k) {
        AutoFreeWstr tmp = str::Format(_TR("Attachment: %s"), path);
        infotip.Append(tmp.Get());
    } else {
        infotip.Append(path);
    }

    str::BufSet(nm->pszText, nm->cchTextMax, infotip.Get());
    ev->didHandle = true;
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd) {
    // code inspired by http://www.codeguru.com/cpp/controls/treeview/multiview/article.php/c3985/
    LPNMCUSTOMDRAW ncd = &ntvcd->nmcd;
    HWND hTV = ncd->hdr.hwndFrom;
    HTREEITEM hItem = (HTREEITEM)ncd->dwItemSpec;
    RECT rcItem;
    if (0 == ncd->rc.right - ncd->rc.left || 0 == ncd->rc.bottom - ncd->rc.top)
        return;
    if (!TreeView_GetItemRect(hTV, hItem, &rcItem, TRUE))
        return;
    if (rcItem.right > ncd->rc.right)
        rcItem.right = ncd->rc.right;

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
    WindowInfo* win = FindWindowInfoByHwnd(hTV);
    TocItem* tocItem = (TocItem*)item.lParam;
    AutoFreeWstr label;
    if (tocItem->pageNo && win && win->IsDocLoaded()) {
        label.Set(win->ctrl->GetPageLabel(tocItem->pageNo));
        label.Set(str::Join(L"  ", label));
    }
    if (label && str::EndsWith(item.pszText, label)) {
        RECT rcPageNo = rcFullWidth;
        InflateRect(&rcPageNo, -2, -1);

        SIZE txtSize;
        GetTextExtentPoint32(ncd->hdc, label, str::Len(label), &txtSize);
        rcPageNo.left = rcPageNo.right - txtSize.cx;

        SetTextColor(ncd->hdc, GetSysColor(COLOR_WINDOWTEXT));
        SetBkColor(ncd->hdc, GetSysColor(COLOR_WINDOW));
        DrawText(ncd->hdc, label, -1, &rcPageNo, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

        // Reduce the size of the label and cut off the page number
        rcItem.right = std::max(rcItem.right - txtSize.cx, 0);
        szText[str::Len(szText) - str::Len(label)] = '\0';
    }

    SetTextColor(ncd->hdc, ntvcd->clrText);
    SetBkColor(ncd->hdc, ntvcd->clrTextBk);

    // Draw the focus rectangle (including proper background color)
    HBRUSH brushBg = CreateSolidBrush(ntvcd->clrTextBk);
    FillRect(ncd->hdc, &rcItem, brushBg);
    DeleteObject(brushBg);
    if ((ncd->uItemState & CDIS_FOCUS))
        DrawFocusRect(ncd->hdc, &rcItem);

    InflateRect(&rcItem, -2, -1);
    DrawText(ncd->hdc, szText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

static void GoToTocLinkTask(TocItem* tocItem, TabInfo* tab, Controller* ctrl) {
    WindowInfo* win = tab->win;
    // tocItem is invalid if the Controller has been replaced
    if (!WindowInfoStillValid(win) || win->currentTab != tab || tab->ctrl != ctrl) {
        return;
    }

    // make sure that the tree item that the user selected
    // isn't unselected in UpdateTocSelection right again
    win->tocKeepSelection = true;
    int pageNo = tocItem->pageNo;
    PageDestination* dest = tocItem->GetPageDestination();
    if (dest) {
        win->linkHandler->GotoLink(dest);
    } else if (pageNo) {
        ctrl->GoToPage(pageNo, true);
    }
    win->tocKeepSelection = false;
}

static bool IsScrollToLink(PageDestination* link) {
    if (!link) {
        return false;
    }
    auto kind = link->Kind();
    return kind == kindDestinationScrollTo;
}

static void GoToTocTreeItem(WindowInfo* win, TreeItem* ti, bool allowExternal) {
    if (!ti) {
        return;
    }
    TocItem* tocItem = (TocItem*)ti;
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->GetPageDestination());
    if (validPage || (allowExternal || isScroll)) {
        // delay changing the page until the tree messages have been handled
        TabInfo* tab = win->currentTab;
        Controller* ctrl = win->ctrl;
        uitask::Post([=] { GoToTocLinkTask(tocItem, tab, ctrl); });
    }
}

void ClearTocBox(WindowInfo* win) {
    if (!win->tocLoaded) {
        return;
    }

    win->tocTreeCtrl->Clear();

    win->currPageNo = 0;
    win->tocLoaded = false;
}

void ToggleTocBox(WindowInfo* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    if (win->tocVisible) {
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
        return;
    }
    SetSidebarVisibility(win, true, gGlobalPrefs->showFavorites);
    if (win->tocVisible) {
        win->tocTreeCtrl->SetFocus();
    }
}

// find the closest item in tree view to a given page number
static TreeItem* TreeItemForPageNo(TreeCtrl* treeCtrl, int pageNo) {
    TocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;

    TreeModel* tm = treeCtrl->treeModel;
    if (!tm) {
        return nullptr;
    }
    int nItems = 0;
    VisitTreeModelItems(tm, [&](TreeItem* ti) {
        auto* docItem = (TocItem*)ti;
        if (!docItem) {
            return true;
        }
        if (!bestMatch) {
            // if nothing else matches, match the root node
            bestMatch = docItem;
        }
        ++nItems;
        int page = docItem->pageNo;
        if ((page <= pageNo) && (page >= bestMatchPageNo) && (page >= 1)) {
            bestMatch = docItem;
            bestMatchPageNo = page;
            if (pageNo == bestMatchPageNo) {
                // we can stop earlier if we found the exact match
                return false;
            }
        }
        return true;
    });
    // if there's only one item, we want to unselect it so that it can
    // be selected by the user
    if (nItems < 2) {
        return nullptr;
    }
    return bestMatch;
}

void UpdateTocSelection(WindowInfo* win, int currPageNo) {
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection) {
        return;
    }

    TreeItem* item = TreeItemForPageNo(win->tocTreeCtrl, currPageNo);
    win->tocTreeCtrl->SelectItem(item);
}

static void UpdateDocTocExpansionStateRecur(TreeCtrl* treeCtrl, Vec<int>& tocState, TocItem* tocItem) {
    while (tocItem) {
        // items without children cannot be toggled
        if (tocItem->child) {
            // we have to query the state of the tree view item because
            // isOpenToggled is not kept in sync
            // TODO: keep toggle state on TocItem in sync
            // by subscribing to the right notifications
            bool isExpanded = treeCtrl->IsExpanded(tocItem);
            bool wasToggled = isExpanded != tocItem->isOpenDefault;
            if (wasToggled) {
                tocState.Append(tocItem->id);
            }
            UpdateDocTocExpansionStateRecur(treeCtrl, tocState, tocItem->child);
        }
        tocItem = tocItem->next;
    }
}

void UpdateTocExpansionState(Vec<int>& tocState, TreeCtrl* treeCtrl, TocTree* docTree) {
    if (treeCtrl->treeModel != docTree) {
        // CrashMe();
        return;
    }
    tocState.Reset();
    TocItem* tocItem = docTree->root;
    UpdateDocTocExpansionStateRecur(treeCtrl, tocState, tocItem);
}

// copied from mupdf/fitz/dev_text.c
static bool isLeftToRightChar(WCHAR c) {
    return ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06));
}

static bool isRightToLeftChar(WCHAR c) {
    return ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) ||
            (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFE));
}

static void GetLeftRightCounts(TocItem* node, int& l2r, int& r2l) {
    if (!node) {
        return;
    }
    if (node->title) {
        for (const WCHAR* c = node->title; *c; c++) {
            if (isLeftToRightChar(*c)) {
                l2r++;
            } else if (isRightToLeftChar(*c)) {
                r2l++;
            }
        }
    }
    GetLeftRightCounts(node->child, l2r, r2l);
    GetLeftRightCounts(node->next, l2r, r2l);
}

static void SetInitialExpandState(TocItem* item, Vec<int>& tocState) {
    while (item) {
        if (tocState.Contains(item->id)) {
            item->isOpenToggled = true;
        }
        SetInitialExpandState(item->child, tocState);
        item = item->next;
    }
}

void ShowExportedBookmarksMsg(const WCHAR* path) {
    str::WStr msg;
    msg.AppendFmt(L"Exported bookmarks to file %s", path);
    str::WStr caption;
    caption.Append(L"Exported bookmarks");
    uint type = MB_OK | MB_ICONINFORMATION | MbRtlReadingMaybe();
    MessageBoxW(nullptr, msg.Get(), caption.Get(), type);
}

static void ExportBookmarksFromTab(TabInfo* tab) {
    // TODO: should I set engineFilePath and nPages on root node so that
    // it's the same as .vbkm?
    TocTree* tocTree = tab->ctrl->GetToc();
    str::Str path = strconv::WstrToUtf8(tab->filePath);
    path.Append(".bkm");
    bool ok = ExportBookmarksToFile(tocTree, "", path.Get());
    if (!ok) {
        log("ExportBookmarsToFile() failed\n");
    }
    ShowExportedBookmarksMsg(tab->filePath);
}

// clang-format off
static MenuDef menuDefContext[] = {
    {_TRN("Expand All"),                 CmdExpandAll,         0 },
    {_TRN("Collapse All"),               CmdCollapseAll,       0 },
    {SEP_ITEM,                           CmdSeparatorEmbed,    MF_NO_TRANSLATE},
    {_TRN("Open Embedded PDF"),     CmdOpenEmbeddedPDF,      0 },
    {_TRN("Save Embedded File..."), CmdSaveEmbeddedFile,      0 },
    // note: strings cannot be "" or else items are not there
    {"add",                              CmdFavoriteAdd,            MF_NO_TRANSLATE},
    {"del",                              CmdFavoriteDel,            MF_NO_TRANSLATE},
    {SEP_ITEM,                           CmdSeparator,         MF_NO_TRANSLATE},
    {_TRN("Export Bookmarks"),      CmdExportBookmarks,   MF_NO_TRANSLATE},
    {_TRN("New Bookmarks"),         CmdNewBookmarks,      MF_NO_TRANSLATE},
    { 0, 0, 0 },
};

static MenuDef menuDefSortByTag[] = {
    {_TRN("Tag (small first)"),     CmdSortTagSmallFirst, 0 },
    {_TRN("Tag (big first)"),       CmdSortTagBigFirst,   0 },
    {_TRN("Color"),                 CmdSortColor,           0 },
    { 0, 0, 0 },
};
// clang-format on      

static void AddFavoriteFromToc(WindowInfo* win, TocItem* dti) {
    int pageNo = 0;
    if (!dti) {
        return;
    }
    if (dti->dest) {
        pageNo = dti->dest->GetPageNo();
    }
    AutoFreeWstr name = str::Dup(dti->title);
    AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
    AddFavoriteWithLabelAndName(win, pageNo, pageLabel.Get(), name);
}

struct TocItemWithSortInfo {
    TocItem* ti;
    int tag;
    COLORREF color;
};

// extract a tag in <s> in format "(<n>)$"
static int ParseTocTag(const WCHAR* s) {
    const WCHAR* is = str::FindCharLast(s, '(');
    if (is == nullptr) {
        return 0;
    }
    int tag = 0;
    str::Parse(is, L"(%d)$", &tag);
    if (tag == 0) {
        return tag;
    }
    return tag;
}

static bool HasDifferentColors(Vec<TocItemWithSortInfo>& tocItems) {
    int prevColor = tocItems[0].color;
    int n = tocItems.isize();
    for (int i = 0; i < n; i++) {
        int color = tocItems[i].color;
        if (color != prevColor) {
            return true;
        }
        prevColor = color;
    }
    return false;
}

static bool HasDifferentTags(Vec<TocItemWithSortInfo>& tocItems) {
    int prevTag = tocItems[0].tag;
    int n = tocItems.isize();
    for (int i = 0; i < n; i++) {
        int tag = tocItems[i].tag;
        if (tag != prevTag) {
            return true;
        }
        prevTag = tag;
    }
    return false;
}

int sortByTagSmallFirst(const TocItemWithSortInfo* t1, const TocItemWithSortInfo* t2) {
    if (t1->tag == t2->tag) {
        return 0;
    }
    if (t1->tag > t2->tag) {
        return 1;
    }
    return -1;
}

int sortByTagBigFirst(const TocItemWithSortInfo* t1, const TocItemWithSortInfo* t2) {
    if (t1->tag == t2->tag) {
        return 0;
    }
    if (t1->tag > t2->tag) {
        return -1;
    }
    return 1;
}

static COLORREF fixupColor(COLORREF c) {
    if (c == ColorUnset) {
        return 0;
    }
    return c;
}

// order of sorting by color is rather arbitrary
int sortByColor(const TocItemWithSortInfo* t1, const TocItemWithSortInfo* t2) {
    auto c1 = fixupColor(t1->color);
    auto c2 = fixupColor(t2->color);
    if (c1 == c2) {
        return 0;
    }
    if (c1 > c2) {
        return 1;
    }
    return -1;
}

static TocItem* SortTocItemsByTag(Vec<TocItemWithSortInfo>& tocItems, TocSort tocSort) {
    int n = tocItems.isize();
    if (n == 0) {
        return nullptr;
    }
    TocItem* first = tocItems[0].ti;
    if (n == 1) {
        return first;
    }
    bool wasSorted = false;
    if (tocSort == TocSort::TagSmallFirst || tocSort == TocSort::TagBigFirst) {
        if (!HasDifferentTags(tocItems)) {
            return first;
        }
        wasSorted = true;
        switch (tocSort) {
            case TocSort::TagSmallFirst:
                tocItems.SortTyped(sortByTagSmallFirst);
                break;
            case TocSort::TagBigFirst:
                tocItems.SortTyped(sortByTagBigFirst);
                break;
        }
    } else if (tocSort == TocSort::Color) {
        if (!HasDifferentColors(tocItems)) {
            return first;
        }
        tocItems.SortTyped(sortByColor);
        wasSorted = true;
    } else {
        CrashMe();
    }

    if (!wasSorted) {
        return first;
    }

    // rebuild siblings order after sorting
    first = tocItems[0].ti;
    TocItem* curr = first;
    for (int i = 1; i < n; i++) {
        TocItem* next = tocItems[i].ti;
        curr->next = next;
        curr = next;
    }
    curr->next = nullptr;
    return first;
}

static TocItem* SortTocItemSiblingsRec(TocItem* ti, TocSort tocSort) {
    if (ti == nullptr) {
        return nullptr;
    }
    TocItem* curr = ti;
    Vec<TocItemWithSortInfo> items;
    while (curr) {
        int tag = ParseTocTag(curr->title);
        TocItemWithSortInfo tii{curr, tag, curr->color};
        items.Append(tii);
        curr->child = SortTocItemSiblingsRec(curr->child, tocSort);
        curr = curr->next;
    }
    TocItem* res = SortTocItemsByTag(items, tocSort);
    return res;
}

static TocTree* SortTocTree(TocTree* tree, TocSort tocSort) {
    if (tocSort == TocSort::None) {
        return nullptr;
    }
    auto res = CloneTocTree(tree, false);
    res->root = SortTocItemSiblingsRec(res->root, tocSort);
    return res;
}

static void SortAndSetTocTree(TabInfo* tab) {
    delete tab->tocSorted;
    tab->tocSorted = SortTocTree(tab->currToc, tab->tocSort);

    TocTree* toShow = tab->tocSorted;
    if (toShow == nullptr) {
        toShow = tab->currToc;
    }
    tab->win->tocTreeCtrl->SetTreeModel(toShow);
}

static void OpenEmbeddedFile(TabInfo* tab, PageDestination* dest) {
    CrashIf(!tab || !dest);
    if (!tab || !dest) {
        return;
    }
    auto win = tab->win;
    WCHAR* path = dest->GetValue();
    if (!str::StartsWith(path, tab->filePath.Get())) {
        return;
    }
    WindowInfo* newWin = FindWindowInfoByFile(path, true);
    if (!newWin) {
        LoadArgs args(path, win);
        newWin = LoadDocument(args);
    }
    if (newWin) {
        newWin->Focus();
    }
}

static void SaveEmbeddedFile(TabInfo* tab, PageDestination* dest) {
    CrashIf(!tab || !dest);
    if (!tab || !dest) {
        return;
    }
    auto filePath = dest->GetValue();
    auto data = LoadEmbeddedPDFFile(filePath);
    AutoFreeWstr dir = path::GetDir(filePath);
    auto fileName = dest->GetName();
    AutoFreeWstr dstPath = path::Join(dir, fileName);
#if 0 // TODO: why did I have it here?
    int streamNo = -1;
    AutoFreeWstr fileSystemPath = ParseEmbeddedStreamNumber(filePath, &streamNo);
#endif
    SaveDataToFile(tab->win->hwndFrame, dstPath, data);
    str::Free(data.data());
}

static void TocContextMenu(ContextMenuEvent* ev) {
    WindowInfo* win = FindWindowInfoByHwnd(ev->w->hwnd);
    const WCHAR* filePath = win->ctrl->FilePath();

    POINT pt{};
    TreeItem* ti = GetOrSelectTreeItemAtPos(ev, pt);
    if (!ti) {
        pt = {ev->mouseGlobal.x, ev->mouseGlobal.y};
    }
    int pageNo = 0;
    TocItem* dti = (TocItem*)ti;
    if (dti && dti->dest) {
        pageNo = dti->dest->GetPageNo();
    }

    TabInfo* tab = win->currentTab;
    bool showBookmarksMenu = IsTocEditorEnabledForWindowInfo(tab);
    HMENU popup = BuildMenuFromMenuDef(menuDefContext, CreatePopupMenu());

    if (showBookmarksMenu) {
        HMENU popupSort = BuildMenuFromMenuDef(menuDefSortByTag, CreatePopupMenu());
        uint flags = MF_BYCOMMAND | MF_ENABLED | MF_POPUP;
        InsertMenuW(popup, 0, flags, (UINT_PTR)popupSort, _TR("Sort By"));

        win::menu::SetChecked(popupSort, CmdSortTagSmallFirst, false);
        win::menu::SetChecked(popupSort, CmdSortTagBigFirst, false);
        win::menu::SetChecked(popupSort, CmdSortColor, false);

        switch (tab->tocSort) {
            case TocSort::TagBigFirst:
                win::menu::SetChecked(popupSort, CmdSortTagBigFirst, true);
                break;
            case TocSort::TagSmallFirst:
                win::menu::SetChecked(popupSort, CmdSortTagSmallFirst, true);
                break;
            case TocSort::Color:
                win::menu::SetChecked(popupSort, CmdSortColor, true);
                break;
        }
    }

    bool isEmbeddedFile = false;
    PageDestination* dest = nullptr;
    WCHAR* path = nullptr;
    if (dti && dti->dest) {
        dest = dti->dest;
        path = dest->GetValue();
        isEmbeddedFile = (path != nullptr) && (dest->kind == kindDestinationLaunchEmbedded);
    }
    if (isEmbeddedFile) {
        auto embeddedName = dest->GetName();
        const WCHAR* ext = path::GetExtNoFree(embeddedName);
        bool canOpenEmbedded = str::EqI(ext, L".pdf");
        if (!canOpenEmbedded) {
            win::menu::Remove(popup, CmdOpenEmbeddedPDF);
        }
    } else {
        win::menu::Remove(popup, CmdSeparatorEmbed);
        win::menu::Remove(popup, CmdSaveEmbeddedFile);
        win::menu::Remove(popup, CmdOpenEmbeddedPDF);
    }

    if (!showBookmarksMenu) {
        win::menu::Remove(popup, CmdSeparator);
        win::menu::Remove(popup, CmdExportBookmarks);
        win::menu::Remove(popup, CmdNewBookmarks);
    } else {
        path = win->currentTab->filePath.Get();
        if (str::EndsWithI(path, L".vbkm")) {
            // for .vbkm change wording from "New Bookmarks" => "Edit Bookmarks"
            win::menu::SetText(popup, CmdNewBookmarks, _TR("Edit Bookmarks"));
        }
    }

    if (pageNo > 0) {
        AutoFreeWstr pageLabel = win->ctrl->GetPageLabel(pageNo);
        bool isBookmarked = gFavorites.IsPageInFavorites(filePath, pageNo);
        if (isBookmarked) {
            win::menu::Remove(popup, CmdFavoriteAdd);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Remove page %s from favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, CmdFavoriteDel, s);
        } else {
            win::menu::Remove(popup, CmdFavoriteDel);

            // %s and not %d because re-using translation from RebuildFavMenu()
            auto tr = _TR("Add page %s to favorites");
            AutoFreeWstr s = str::Format(tr, pageLabel.Get());
            win::menu::SetText(popup, CmdFavoriteAdd, s);
        }
    } else {
        win::menu::Remove(popup, CmdFavoriteAdd);
        win::menu::Remove(popup, CmdFavoriteDel);
    }

    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case CmdExportBookmarks:
            ExportBookmarksFromTab(tab);
            break;
        case CmdNewBookmarks:
            StartTocEditorForWindowInfo(win);
            break;
        case CmdExpandAll:
            win->tocTreeCtrl->ExpandAll();
            break;
        case CmdCollapseAll:
            win->tocTreeCtrl->CollapseAll();
            break;
        case CmdFavoriteAdd:
            AddFavoriteFromToc(win, dti);
            break;
        case CmdFavoriteDel:
            DelFavorite(filePath, pageNo);
            break;
        case CmdSortTagBigFirst:
            if (tab->tocSort == TocSort::TagBigFirst) {
                tab->tocSort = TocSort::None;
            } else {
                tab->tocSort = TocSort::TagBigFirst;
            }
            SortAndSetTocTree(tab);
            break;
        case CmdSortTagSmallFirst:
            if (tab->tocSort == TocSort::TagSmallFirst) {
                tab->tocSort = TocSort::None;
            } else {
                tab->tocSort = TocSort::TagSmallFirst;
            }
            SortAndSetTocTree(tab);
            break;
        case CmdSortColor:
            if (tab->tocSort == TocSort::Color) {
                tab->tocSort = TocSort::None;
            } else {
                tab->tocSort = TocSort::Color;
            }
            SortAndSetTocTree(tab);
            break;
        case CmdSaveEmbeddedFile:
            SaveEmbeddedFile(tab, dest);
            break;
        case CmdOpenEmbeddedPDF:
            OpenEmbeddedFile(tab, dest);
            break;
    }
}

static void AltBookmarksChanged(TabInfo* tab, int n, std::string_view s) {
    if (n == 0) {
        tab->currToc = tab->ctrl->GetToc();
    } else {
        tab->currToc = tab->altBookmarks[0]->tree;
    }
    SortAndSetTocTree(tab);
}

// TODO: temporary
static bool LoadAlterenativeBookmarks(const WCHAR* baseFileName, VbkmFile& vbkm) {
    AutoFreeStr tmp = strconv::WstrToUtf8(baseFileName);
    return LoadAlterenativeBookmarks(tmp.AsView(), vbkm);
}

static bool ShouldCustomDraw(WindowInfo* win) {
    // we only want custom drawing for pdf and pdf multi engines
    // as they are the only ones supporting custom colors and fonts
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }
    Kind kind = dm->GetEngineType();
    if (kind == kindEnginePdf || kind == kindEngineMulti) {
        return true;
    }
    return false;
}

void OnTocCustomDraw(TreeItemCustomDrawEvent*);

static void dropDownSelectionChanged(DropDownSelectionChangedEvent* ev) {
    WindowInfo* win = FindWindowInfoByHwnd(ev->hwnd);
    TabInfo* tab = win->currentTab;
    DebugCrashIf(!tab);
    if (!tab) {
        return;
    }
    AltBookmarksChanged(tab, ev->idx, ev->item);
    ev->didHandle = true;
}

void LoadTocTree(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab);

    if (win->tocLoaded) {
        return;
    }

    win->tocLoaded = true;

    auto* tocTree = tab->ctrl->GetToc();
    if (!tocTree || !tocTree->root) {
        return;
    }

    tab->currToc = tocTree;

    DeleteVecMembers(tab->altBookmarks);

    // TODO: for now just for testing
    // TODO: restore showing alternative bookmarks
    VbkmFile* vbkm = new VbkmFile();
    bool ok = LoadAlterenativeBookmarks(tab->filePath, *vbkm);
    if (ok) {
        tab->altBookmarks.Append(vbkm);
        Vec<std::string_view> items;
        items.Append("Default");
        char* name = vbkm->name;
        if (name) {
            items.Append(name);
        }
        win->altBookmarks->SetItems(items);
    } else {
        delete vbkm;
    }

    if (tab->altBookmarks.size() > 0) {
        win->altBookmarks->onSelectionChanged = dropDownSelectionChanged;
        win->altBookmarks->SetVisibility(Visibility::Visible);
    } else {
        win->altBookmarks->onSelectionChanged = nullptr;
        win->altBookmarks->SetVisibility(Visibility::Collapse);
    }

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tocTree->root, l2r, r2l);
    bool isRTL = r2l > l2r;

    TreeCtrl* treeCtrl = win->tocTreeCtrl;
    HWND hwnd = treeCtrl->hwnd;
    SetRtl(hwnd, isRTL);

    UpdateTreeCtrlColors(win);
    SetInitialExpandState(tocTree->root, tab->tocState);
    tocTree->root->OpenSingleNode();

    treeCtrl->SetTreeModel(tocTree);

    treeCtrl->onTreeItemCustomDraw = nullptr;
    if (ShouldCustomDraw(win)) {
        treeCtrl->onTreeItemCustomDraw = OnTocCustomDraw;
    }
    LayoutTreeContainer(win->tocLabelWithClose, win->altBookmarks, win->tocTreeCtrl->hwnd);
    // uint fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    // RedrawWindow(hwnd, nullptr, nullptr, fl);
}

// TODO: use https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getobject?redirectedfrom=MSDN
// to get LOGFONT from existing font and then create a derived font
static void UpdateFont(HDC hdc, int fontFlags) {
    // TODO: this is a bit hacky, in that we use default font
    // and not the font from TreeCtrl. But in this case they are the same
    bool italic = bit::IsSet(fontFlags, fontBitItalic);
    bool bold = bit::IsSet(fontFlags, fontBitBold);
    HFONT hfont = GetDefaultGuiFont(bold, italic);
    SelectObject(hdc, hfont);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/about-custom-draw
// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-nmtvcustomdraw
void OnTocCustomDraw(TreeItemCustomDrawEvent* ev) {
#if defined(DISPLAY_TOC_PAGE_NUMBERS)
    if (win->AsEbook())
        return CDRF_DODEFAULT;
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
    ev->didHandle = true;

    NMTVCUSTOMDRAW* tvcd = ev->nm;
    NMCUSTOMDRAW* cd = &(tvcd->nmcd);
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        // ask to be notified about each item
        ev->result = CDRF_NOTIFYITEMDRAW;
        return;
    }

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        // called before drawing each item
        TocItem* tocItem = (TocItem*)ev->treeItem;
        if (!tocItem) {
            return;
        }
        if (tocItem->color != ColorUnset) {
            tvcd->clrText = tocItem->color;
        }
        if (tocItem->fontFlags != 0) {
            UpdateFont(cd->hdc, tocItem->fontFlags);
            ev->result = CDRF_NEWFONT;
            return;
        }
        return;
    }
    return;
}

static void TocTreeClick(TreeClickEvent* ev) {
    ev->didHandle = true;
    if (!ev->treeItem) {
        return;
    }
    WindowInfo* win = FindWindowInfoByHwnd(ev->w->hwnd);
    CrashIf(!win);
    bool allowExternal = false;
    GoToTocTreeItem(win, ev->treeItem, allowExternal);
}

static void TocTreeSelectionChanged(TreeSelectionChangedEvent* ev) {
    WindowInfo* win = FindWindowInfoByHwnd(ev->w->hwnd);
    CrashIf(!win);

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
    bool allowExternal = ev->byMouse;
    GoToTocTreeItem(win, ev->selectedItem, allowExternal);
    ev->didHandle = true;
}

// also used in Favorites.cpp
void TocTreeKeyDown(TreeKeyDownEvent* ev) {
    if (ev->keyCode != VK_TAB) {
        return;
    }
    ev->didHandle = true;
    ev->result = 1;

    WindowInfo* win = FindWindowInfoByHwnd(ev->hwnd);
    if (win->tabsVisible && IsCtrlPressed()) {
        TabsOnCtrlTab(win, IsShiftPressed());
        return;
    }
    AdvanceFocus(win);
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void TocTreeMsgFilter([[maybe_unused]] WndEvent* ev) {
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

// Position label with close button and tree window within their parent.
// Used for toc and favorites.
void LayoutTreeContainer(LabelWithCloseWnd* l, DropDownCtrl* altBookmarks, HWND hwndTree) {
    HWND hwndContainer = GetParent(hwndTree);
    Size labelSize = l->GetIdealSize();
    Rect rc = WindowRect (hwndContainer);
    bool altBookmarksVisible = altBookmarks && altBookmarks->IsVisible();
    int dy = rc.dy;
    int y = 0;
    MoveWindow(l->hwnd, y, 0, rc.dx, labelSize.dy, TRUE);
    dy -= labelSize.dy;
    y += labelSize.dy;
    if (altBookmarksVisible) {
        Size bs = altBookmarks->GetIdealSize();
        int elDy = bs.dy;
        RECT r{0, y, rc.dx, y + elDy};
        altBookmarks->SetBounds(r);
        elDy += 4;
        dy -= elDy;
        y += elDy;
    }
    MoveWindow(hwndTree, 0, y, rc.dx, dy, TRUE);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    // this is a parent of TreeCtrl and DropDownCtrl
    // TODO: TreeCtrl and DropDownCtrl should be children of frame

    LRESULT res = 0;
    if (HandleRegisteredMessages(hwnd, msg, wp, lp, res)) {
        return res;
    }

    WindowInfo* winFromData = (WindowInfo*)(data);
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    CrashIf(subclassId != win->tocBoxSubclassId);
    CrashIf(win != winFromData);

    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->tocLabelWithClose, win->altBookmarks, win->tocTreeCtrl->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_TOC_LABEL_WITH_CLOSE) {
                ToggleTocBox(win);
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void SubclassToc(WindowInfo* win) {
    HWND hwndTocBox = win->hwndTocBox;

    if (win->tocBoxSubclassId == 0) {
        win->tocBoxSubclassId = NextSubclassId();
        BOOL ok = SetWindowSubclass(hwndTocBox, WndProcTocBox, win->tocBoxSubclassId, (DWORD_PTR)win);
        CrashIf(!ok);
    }
}

void UnsubclassToc(WindowInfo* win) {
    if (win->tocBoxSubclassId != 0) {
        RemoveWindowSubclass(win->hwndTocBox, WndProcTocBox, win->tocBoxSubclassId);
        win->tocBoxSubclassId = 0;
    }
}

void TocTreeMouseWheelHandler(MouseWheelEvent* ev) {
    WindowInfo* win = FindWindowInfoByHwnd(ev->hwnd);
    CrashIf(!win);
    if (!win) {
        return;
    }
    // scroll the canvas if the cursor isn't over the ToC tree
    if (!IsCursorOverWindow(ev->hwnd)) {
        ev->didHandle = true;
        ev->result = SendMessageW(win->hwndCanvas, ev->msg, ev->wp, ev->lp);
    }
}

void TocTreeCharHandler(CharEvent* ev) {
    WindowInfo* win = FindWindowInfoByHwnd(ev->hwnd);
    CrashIf(!win);
    if (!win) {
        return;
    }
    if (VK_ESCAPE != ev->keyCode) {
        return;
    }
    if (!gGlobalPrefs->escToExit) {
        return;
    }
    if (!MayCloseWindow(win)) {
        return;
    }

    CloseWindow(win, true);
    ev->didHandle = true;
}

extern  HFONT GetTreeFont();

void CreateToc(WindowInfo* win) {
    HMODULE hmod = GetModuleHandle(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD style = WS_CHILD | WS_CLIPCHILDREN;
    HWND parent = win->hwndFrame;
    win->hwndTocBox = CreateWindowExW(0, WC_STATIC, L"", style, 0, 0, dx, 0, parent, 0, hmod, nullptr);

    auto* l = new LabelWithCloseWnd();
    l->Create(win->hwndTocBox, IDC_TOC_LABEL_WITH_CLOSE);
    win->tocLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    // TODO: only ramicro?
    // TODO: use the same font size as in GetTreeFont()?
    l->SetFont(GetDefaultGuiFont(true, false));
    // label is set in UpdateToolbarSidebarText()

    win->altBookmarks = new DropDownCtrl(win->hwndTocBox);
    win->altBookmarks->Create();

    auto* treeCtrl = new TreeCtrl(win->hwndTocBox);
    treeCtrl->dwExStyle = WS_EX_STATICEDGE;
    treeCtrl->onGetTooltip = TocCustomizeTooltip;
    treeCtrl->onContextMenu = TocContextMenu;
    treeCtrl->onChar = TocTreeCharHandler;
    treeCtrl->onMouseWheel = TocTreeMouseWheelHandler;
    treeCtrl->onTreeSelectionChanged = TocTreeSelectionChanged;
    treeCtrl->onTreeClick = TocTreeClick;
    treeCtrl->onTreeKeyDown = TocTreeKeyDown;

    // TODO: leaks font?
    HFONT fnt = GetTreeFont();
    treeCtrl->SetFont(fnt);

    bool ok = treeCtrl->Create();
    CrashIf(!ok);
    win->tocTreeCtrl = treeCtrl;
    SubclassToc(win);
}
