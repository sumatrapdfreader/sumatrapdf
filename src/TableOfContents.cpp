/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
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

#include "EngineBase.h"
#include "EngineManager.h"
#include "ParseBKM.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "AppColors.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
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

static DocTocItem* GetDocTocItem(TreeCtrl* treeCtrl, HTREEITEM hItem) {
    // must do the cast in two stages because of multiple inheritance
    TreeItem* treeItem = treeCtrl->GetTreeItemByHandle(hItem);
    if (!treeItem) {
        return nullptr;
    }
    DocTocItem* tocItem = static_cast<DocTocItem*>(treeItem);
    return tocItem;
}

static DocTocItem* GetDocTocItemFromLPARAM(LPARAM lp) {
    if (!lp) {
        CrashMe(); // TODO: not sure if should ever happen
        return nullptr;
    }
    // must do the cast in two stages because of multiple inheritance
    TreeItem* treeItem = reinterpret_cast<TreeItem*>(lp);
    DocTocItem* tocItem = static_cast<DocTocItem*>(treeItem);
    return tocItem;
}

// set tooltip for this item but only if the text isn't fully shown
static void CustomizeTocTooltip(TreeCtrl* w, NMTVGETINFOTIPW* nm) {
    DocTocItem* tocItem = GetDocTocItemFromLPARAM(nm->lParam);
    PageDestination* link = tocItem->GetPageDestination();
    if (!link) {
        return;
    }
    WCHAR* path = link->GetValue();
    if (!path) {
        return;
    }
    CrashIf(!link); // /analyze claims that this could happen - it really can't
    auto k = link->Kind();
    // TODO: DocTocItem from Chm contain other types
    // we probably shouldn't set DocTocItem::dest there
    if (k == kindDestinationScrollTo) {
        return;
    }

    CrashIf(k != kindDestinationLaunchURL && k != kindDestinationLaunchFile && k != kindDestinationLaunchEmbedded);
    CrashIf(nm->hdr.hwndFrom != w->hwnd);

    str::WStr infotip;

    RECT rcLine, rcLabel;
    HTREEITEM item = nm->hItem;
    // Display the item's full label, if it's overlong
    bool ok = w->GetItemRect(item, false, rcLine);
    ok &= w->GetItemRect(item, true, rcLabel);
    if (!ok) {
        return;
    }

    if (rcLine.right + 2 < rcLabel.right) {
        str::WStr currInfoTip = w->GetTooltip(nm->hItem);
        infotip.Append(currInfoTip.data());
        infotip.Append(L"\r\n");
    }

    if (kindDestinationLaunchEmbedded == k) {
        AutoFreeWstr tmp = str::Format(_TR("Attachment: %s"), path);
        infotip.Append(tmp.get());
    } else {
        infotip.Append(path);
    }

    str::BufSet(nm->pszText, nm->cchTextMax, infotip.Get());
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
    DocTocItem* tocItem = (DocTocItem*)item.lParam;
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

static void GoToTocLinkTask(WindowInfo* win, DocTocItem* tocItem, TabInfo* tab, Controller* ctrl) {
    // tocItem is invalid if the Controller has been replaced
    if (!WindowInfoStillValid(win) || win->currentTab != tab || tab->ctrl != ctrl)
        return;

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

static void GoToTocLinkForTVItem(WindowInfo* win, HTREEITEM hItem, bool allowExternal) {
    if (!win->IsDocLoaded()) {
        return;
    }
    TreeCtrl* treeCtrl = win->tocTreeCtrl;
    if (!hItem) {
        hItem = treeCtrl->GetSelection();
    }
    DocTocItem* tocItem = GetDocTocItem(treeCtrl, hItem);
    if (!tocItem) {
        return;
    }
    bool validPage = (tocItem->pageNo > 0);
    bool isScroll = IsScrollToLink(tocItem->GetPageDestination());
    if (validPage || (allowExternal || isScroll)) {
        // delay changing the page until the tree messages have been handled
        TabInfo* tab = win->currentTab;
        Controller* ctrl = win->ctrl;
        uitask::Post([=] { GoToTocLinkTask(win, tocItem, tab, ctrl); });
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
        SetFocus(win->tocTreeCtrl->hwnd);
    }
}

#if 0
static HTREEITEM AddTocItemToView(TreeCtrl* tree, DocTocItem* entry, HTREEITEM parent, bool toggleItem) {
    TV_INSERTSTRUCT toInsert;
    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_LAST;
    toInsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    UINT state = 0;
    if (entry->child && (entry->isOpen != toggleItem)) {
        state = TVIS_EXPANDED;
    }
    toInsert.itemex.state = state;
    toInsert.itemex.stateMask = TVIS_EXPANDED;
    toInsert.itemex.lParam = reinterpret_cast<LPARAM>(entry);
    // Replace unprintable whitespace with regular spaces
    str::NormalizeWS(entry->title);
    toInsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (entry->pageNo && win && win->IsDocLoaded() && !win->AsEbook()) {
        AutoFreeWstr label(win->ctrl->GetPageLabel(entry->pageNo));
        AutoFreeWstr text(str::Format(L"%s  %s", entry->title, label));
        toInsert.itemex.pszText = text;
        return TreeView_InsertItem(hwnd, &tvinsert);
    }
#endif
    return tree->InsertItem(&toInsert);
}
#endif

// find the closest item in tree view to a given page number
static TreeItem* TreeItemForPageNo(TreeCtrl* treeCtrl, int pageNo) {
    DocTocItem* bestMatch = nullptr;
    int bestMatchPageNo = 0;

    TreeModel* tm = treeCtrl->treeModel;
    VisitTreeModelItems(tm, [&](TreeItem* ti) {
        auto* docItem = (DocTocItem*)ti;
        if (!docItem) {
            return true;
        }
        if (!bestMatch) {
            // if nothing else matches, match the root node
            bestMatch = docItem;
        }

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
    return bestMatch;
}

void UpdateTocSelection(WindowInfo* win, int currPageNo) {
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection) {
        return;
    }

    TreeItem* item = TreeItemForPageNo(win->tocTreeCtrl, currPageNo);
    if (item) {
        win->tocTreeCtrl->SelectItem(item);
    }
}

static void UpdateDocTocExpansionState(TreeCtrl* treeCtrl, Vec<int>& tocState, DocTocItem* tocItem) {
    while (tocItem) {
        // items without children cannot be toggled
        if (tocItem->child) {
            // we have to query the state of the tree view item because
            // isOpenToggled is not kept in sync
            // TODO: keep toggle state on DocTocItem in sync
            // by subscribing to the right notifications
            bool isExpanded = treeCtrl->IsExpanded(tocItem);
            bool wasToggled = isExpanded != tocItem->isOpenDefault;
            if (wasToggled) {
                tocState.Append(tocItem->id);
            }
            UpdateDocTocExpansionState(treeCtrl, tocState, tocItem->child);
        }
        tocItem = tocItem->next;
    }
}

void UpdateTocExpansionState(Vec<int>& tocState, TreeCtrl* treeCtrl, DocTocTree* docTree) {
    if (treeCtrl->treeModel != docTree) {
        // CrashMe();
        return;
    }
    tocState.Reset();
    DocTocItem* tocItem = docTree->root;
    UpdateDocTocExpansionState(treeCtrl, tocState, tocItem);
}

void UpdateTocColors(WindowInfo* win) {
    COLORREF labelBgCol = GetSysColor(COLOR_BTNFACE);
    COLORREF labelTxtCol = GetSysColor(COLOR_BTNTEXT);
    COLORREF treeBgCol = GetAppColor(AppColor::DocumentBg);
    COLORREF treeTxtCol = GetAppColor(AppColor::DocumentText);
    COLORREF splitterCol = GetSysColor(COLOR_BTNFACE);
    bool flatTreeWnd = false;

    if (win->AsEbook()) {
        labelBgCol = GetAppColor(AppColor::DocumentBg, true);
        labelTxtCol = GetAppColor(AppColor::DocumentText, true);
        treeTxtCol = labelTxtCol;
        treeBgCol = labelBgCol;
        float factor = 14.f;
        int sign = GetLightness(labelBgCol) + factor > 255 ? 1 : -1;
        splitterCol = AdjustLightness2(labelBgCol, sign * factor);
        flatTreeWnd = true;
    }

    auto treeCtrl = win->tocTreeCtrl;
    treeCtrl->SetBackgroundColor(treeBgCol);
    treeCtrl->SetTextColor(treeTxtCol);

    win->tocLabelWithClose->SetBgCol(labelBgCol);
    win->tocLabelWithClose->SetTextCol(labelTxtCol);
    SetBgCol(win->sidebarSplitter, splitterCol);
    ToggleWindowExStyle(treeCtrl->hwnd, WS_EX_STATICEDGE, !flatTreeWnd);
    UINT flags = SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED;
    SetWindowPos(treeCtrl->hwnd, nullptr, 0, 0, 0, 0, flags);

    // TODO: if we have favorites in ebook view, we'll need this
    // SetBgCol(win->favLabelWithClose, labelBgCol);
    // SetTextCol(win->favLabelWithClose, labelTxtCol);
    // SetBgCol(win->favSplitter, labelTxtCol);

    // TODO: more work needed to to ensure consistent look of the ebook window:
    // - tab bar should match the colort
    // - change the tree item text color
    // - change the tree item background color when selected (for both focused and non-focused cases)
    // - ultimately implement owner-drawn scrollbars in a simpler style (like Chrome or VS 2013)
    //   and match their colors as well
}

// copied from mupdf/fitz/dev_text.c
#define ISLEFTTORIGHTCHAR(c) \
    ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c)                                                                                     \
    ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || \
     (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFE))

static void GetLeftRightCounts(DocTocItem* node, int& l2r, int& r2l) {
    if (!node)
        return;
    if (node->title) {
        for (const WCHAR* c = node->title; *c; c++) {
            if (ISLEFTTORIGHTCHAR(*c))
                l2r++;
            else if (ISRIGHTTOLEFTCHAR(*c))
                r2l++;
        }
    }
    GetLeftRightCounts(node->child, l2r, r2l);
    GetLeftRightCounts(node->next, l2r, r2l);
}

static void SetInitialExpandState(DocTocItem* item, Vec<int>& tocState) {
    while (item) {
        if (tocState.Contains(item->id)) {
            item->isOpenToggled = true;
        }
        SetInitialExpandState(item->child, tocState);
        item = item->next;
    }
}

#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
static MenuDef contextMenuDef[] = {{"Expand All", IDM_EXPAND_ALL, MF_NO_TRANSLATE},
                                   {"Colapse All", IDM_COLLAPSE_ALL, MF_NO_TRANSLATE},
                                   {"Export Bookmarks", IDM_EXPORT_BOOKMARKS, MF_NO_TRANSLATE},
                                   {"New Bookmarks", IDM_NEW_BOOKMARKS, MF_NO_TRANSLATE}};
#else
static MenuDef contextMenuDef[] = {{"Expand All", IDM_EXPAND_ALL, MF_NO_TRANSLATE},
                                   {"Colapse All", IDM_COLLAPSE_ALL, MF_NO_TRANSLATE},
#endif

static void ExportBookmarksFromTab(TabInfo* tab) {
    auto* tocTree = tab->ctrl->GetTocTree();
    AutoFree path = strconv::WstrToUtf8(tab->filePath.get());
    bool ok = ExportBookmarksToFile(tocTree, path);
    str::WStr msg;
    msg.AppendFmt(L"Exported bookmarks to file %s", tab->filePath.get());
    msg.Append(L".bkm");
    str::WStr caption;
    caption.Append(L"Exported bookmarks");
    UINT type = MB_OK | MB_ICONINFORMATION | MbRtlReadingMaybe();
    MessageBoxW(nullptr, msg.Get(), caption.Get(), type);
}

static void BuildAndShowContextMenu(WindowInfo* win, int x, int y) {
    HMENU popup = BuildMenuFromMenuDef(contextMenuDef, dimof(contextMenuDef), CreatePopupMenu());
    POINT pt = {x, y};
    // MapWindowPoints(win->hwndCanvas, HWND_DESKTOP, &pt, 1);
    // MarkMenuOwnerDraw(popup);
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    INT cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    // FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case IDM_EXPORT_BOOKMARKS: {
            auto* tab = win->currentTab;
            ExportBookmarksFromTab(tab);
        } break;
        case IDM_NEW_BOOKMARKS:
            StartTocEditor(win->tocTreeCtrl->treeModel);
            break;
        case IDM_EXPAND_ALL:
            win->tocTreeCtrl->ExpandAll();
            break;
        case IDM_COLLAPSE_ALL:
            win->tocTreeCtrl->CollapseAll();
            break;
    }
}

static void TreeCtrlContextMenu(WindowInfo* win, int xScreen, int yScreen) {
    UNUSED(xScreen);
    UNUSED(yScreen);
    BuildAndShowContextMenu(win, xScreen, yScreen);
}

static void AltBookmarksChanged(WindowInfo* win, TabInfo* tab, int n, std::string_view s) {
    DocTocTree* tocTree = nullptr;
    if (n == 0) {
        tocTree = tab->ctrl->GetTocTree();
    } else {
        tocTree = tab->altBookmarks->at(n - 1)->toc;
    }
    win->tocTreeCtrl->SetTreeModel(tocTree);
}

// TODO: temporary
static Vec<Bookmarks*>* LoadAlterenativeBookmarks(const WCHAR* baseFileName) {
    AutoFree tmp = strconv::WstrToUtf8(baseFileName);
    return LoadAlterenativeBookmarks(tmp.as_view());
}

void LoadTocTree(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab);

    if (win->tocLoaded) {
        return;
    }

    win->tocLoaded = true;

    auto* tocTree = tab->ctrl->GetTocTree();
    if (!tocTree || !tocTree->root) {
        return;
    }

    // TODO: for now just for testing
    auto* altTocs = LoadAlterenativeBookmarks(tab->filePath);
    if (altTocs && altTocs->size() > 0) {
        tab->altBookmarks = altTocs;
        Vec<std::string_view> items;
        items.Append("Default");
        for (size_t i = 0; i < altTocs->size(); i++) {
            DocTocTree* toc = altTocs->at(i)->toc;
            items.Append(toc->name);
        }
        win->altBookmarks->SetItems(items);
    }

    win->altBookmarks->OnDropDownSelectionChanged = [=](int idx, std::string_view s) {
        AltBookmarksChanged(win, tab, idx, s);
    };

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tocTree->root, l2r, r2l);
    bool isRTL = r2l > l2r;

    TreeCtrl* treeCtrl = win->tocTreeCtrl;
    HWND hwnd = treeCtrl->hwnd;
    SetRtl(hwnd, isRTL);

    UpdateTocColors(win);
    SetInitialExpandState(tocTree->root, tab->tocState);
    tocTree->root->OpenSingleNode();

    treeCtrl->SetTreeModel(tocTree);

    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwnd, nullptr, nullptr, fl);
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
static LRESULT OnCustomDraw(WindowInfo* win, NMTVCUSTOMDRAW* tvcd) {
    // TODO: only for PDF
    if (!win->AsFixed()) {
        return CDRF_DODEFAULT;
    }

    // TODO: only for PdfEngine

    NMCUSTOMDRAW* cd = &(tvcd->nmcd);
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        // ask to be notified about each item
        return CDRF_NOTIFYITEMDRAW;
    }

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        // called before drawing each item
        DocTocItem* tocItem = GetDocTocItemFromLPARAM(cd->lItemlParam);
        if (!tocItem) {
            return CDRF_DODEFAULT;
        }
        if (tocItem->color != ColorUnset) {
            tvcd->clrText = tocItem->color;
        }
        if (tocItem->fontFlags != 0) {
            UpdateFont(cd->hdc, tocItem->fontFlags);
            return CDRF_NEWFONT;
        }
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

static LRESULT OnTocTreeNotify(WindowInfo* win, NMTREEVIEWW* pnmtv) {
    HWND hwndFrom = pnmtv->hdr.hwndFrom;
    auto action = pnmtv->action;
    CrashIf(hwndFrom != win->tocTreeCtrl->hwnd);

    switch (pnmtv->hdr.code) {
        case TVN_SELCHANGED:
            // When the focus is set to the toc window the first item in the treeview is automatically
            // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action ==
            // 0x00001000. We have to ignore this message to prevent the current page to be changed.
            if ((TVC_BYKEYBOARD == action) || (TVC_BYMOUSE == action)) {
                bool allowExternal = (TVC_BYMOUSE == action);
                GoToTocLinkForTVItem(win, pnmtv->itemNew.hItem, allowExternal);
            }
            // The case pnmtv->action==TVC_UNKNOWN is ignored because
            // it corresponds to a notification sent by
            // the function TreeView_DeleteAllItems after deletion of the item.
            break;

        case TVN_KEYDOWN: {
            TV_KEYDOWN* ptvkd = (TV_KEYDOWN*)pnmtv;
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
            TVHITTESTINFO ht = {0};
            DWORD pos = GetMessagePos();
            ht.pt.x = GET_X_LPARAM(pos);
            ht.pt.y = GET_Y_LPARAM(pos);
            MapWindowPoints(HWND_DESKTOP, hwndFrom, &ht.pt, 1);
            TreeView_HitTest(hwndFrom, &ht);

            // let TVN_SELCHANGED handle the click, if it isn't on the already selected item
            bool isOnItem = (ht.flags & TVHT_ONITEM);
            HTREEITEM sel = TreeView_GetSelection(hwndFrom);
            bool isSel = (sel == ht.hItem);
            if (isOnItem && isSel) {
                GoToTocLinkForTVItem(win, ht.hItem, true);
            }
            break;
        }
        case NM_RETURN:
            GoToTocLinkForTVItem(win, nullptr, true);
            break;

        case NM_CUSTOMDRAW:
#ifdef DISPLAY_TOC_PAGE_NUMBERS
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
#else
            return OnCustomDraw(win, (NMTVCUSTOMDRAW*)pnmtv);
#endif
    }
    return -1;
}

static LRESULT TocTreePreFilter(TreeCtrl* tree, WndProcArgs* args) {
    CrashIf(tree->hwnd != args->hwnd);

    UINT msg = args->msg;
    WPARAM wp = args->wparam;
    LPARAM lp = args->lparam;

    HWND hwnd = tree->hwnd;
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return 0;
    }

    switch (msg) {
        case WM_CHAR:
            if (VK_ESCAPE == wp && gGlobalPrefs->escToExit && MayCloseWindow(win)) {
                CloseWindow(win, true);
                args->didHandle = true;
            }
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->tocTreeCtrl->hwnd)) {
                return SendMessage(win->hwndCanvas, msg, wp, lp);
                args->didHandle = true;
            }
            break;
#ifdef DISPLAY_TOC_PAGE_NUMBERS
        case WM_SIZE:
        case WM_HSCROLL:
            // Repaint the ToC so that RelayoutTocItem is called for all items
            PostMessage(hwnd, WM_APP_REPAINT_TOC, 0, 0);
            break;
        case WM_APP_REPAINT_TOC:
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            break;
#endif
    }
    return 0;
}

// Position label with close button and tree window within their parent.
// Used for toc and favorites.
void LayoutTreeContainer(LabelWithCloseWnd* l, DropDownCtrl* altBookmarks, HWND hwndTree) {
    HWND hwndContainer = GetParent(hwndTree);
    SizeI labelSize = l->GetIdealSize();
    WindowRect rc(hwndContainer);
    bool altBookmarksVisible = altBookmarks && altBookmarks->items.size() > 0;
    int dy = rc.dy;
    int y = 0;
    MoveWindow(l->hwnd, y, 0, rc.dx, labelSize.dy, TRUE);
    dy -= labelSize.dy;
    y += labelSize.dy;
    if (altBookmarks) {
        altBookmarks->SetIsVisible(altBookmarksVisible);
        if (altBookmarksVisible) {
            SIZE bs = altBookmarks->GetIdealSize();
            int elDy = bs.cy;
            RECT r{0, y, rc.dx, y + elDy};
            altBookmarks->SetBounds(r);
            elDy += 4;
            dy -= elDy;
            y += elDy;
        }
    }
    MoveWindow(hwndTree, 0, y, rc.dx, dy, TRUE);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    WindowInfo* winFromData = reinterpret_cast<WindowInfo*>(data);
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

// TODO: should unsubclass as well?
static void SubclassToc(WindowInfo* win) {
    TreeCtrl* tree = win->tocTreeCtrl;
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
    }
}

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
    l->SetFont(GetDefaultGuiFont());
    // label is set in UpdateToolbarSidebarText()

    win->altBookmarks = new DropDownCtrl(win->hwndTocBox);
    win->altBookmarks->Create();

    auto* treeCtrl = new TreeCtrl(win->hwndTocBox);
    // TODO: remove, for easy testing
    // treeCtrl->withCheckboxes = true;

    DWORD dwStyle = TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
    dwStyle |= TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;
    dwStyle |= WS_TABSTOP | WS_VISIBLE | WS_CHILD;
    treeCtrl->dwStyle = dwStyle;
    treeCtrl->dwExStyle = WS_EX_STATICEDGE;
    treeCtrl->menuId = IDC_TOC_TREE;
    treeCtrl->msgFilter = [treeCtrl](WndProcArgs* args) { return TocTreePreFilter(treeCtrl, args); };
    treeCtrl->onTreeNotify = [win, treeCtrl](NMTREEVIEWW* nm, bool& handled) {
        CrashIf(win->tocTreeCtrl != treeCtrl);
        LRESULT res = OnTocTreeNotify(win, nm);
        handled = (res != -1);
        return res;
    };
    treeCtrl->onGetTooltip = [treeCtrl](NMTVGETINFOTIP* infoTipInfo) { CustomizeTocTooltip(treeCtrl, infoTipInfo); };
    treeCtrl->onContextMenu = [win](HWND, int x, int y) { TreeCtrlContextMenu(win, x, y); };
    bool ok = treeCtrl->Create(L"TOC");
    CrashIf(!ok);
    win->tocTreeCtrl = treeCtrl;
    SubclassToc(win);
}
