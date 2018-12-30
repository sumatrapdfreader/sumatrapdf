/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/GdiPlusUtil.h"
#include "wingui/LabelWithCloseWnd.h"
#include "wingui/SplitterWnd.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "wingui/TreeCtrl.h"

#include "BaseEngine.h"
#include "EngineManager.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "GlobalPrefs.h"
#include "Colors.h"
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

constexpr UINT_PTR SUBCLASS_ID = 1;

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC (WM_APP + 1)
#endif

static void CustomizeTocInfoTip(TreeCtrl* w, NMTVGETINFOTIP* nm) {
    auto* tocItem = reinterpret_cast<DocTocItem*>(nm->lParam);
    PageDestination* link = tocItem->GetLink();
    AutoFreeW path(link ? link->GetDestValue() : nullptr);
    if (!path) {
        return;
    }
    CrashIf(!link); // /analyze claims that this could happen - it really can't
    auto dstType = link->GetDestType();
    CrashIf(dstType != PageDestType::LaunchURL && dstType != PageDestType::LaunchFile &&
            dstType != PageDestType::LaunchEmbedded);
    CrashIf(nm->hdr.hwndFrom != w->hwnd);

    str::Str<WCHAR> infotip;

    RECT rcLine, rcLabel;
    HTREEITEM item = nm->hItem;
    // Display the item's full label, if it's overlong
    bool ok = w->GetItemRect(item, false, rcLine);
    ok &= w->GetItemRect(item, true, rcLabel);
    if (!ok) {
        return;
    }

    if (rcLine.right + 2 < rcLabel.right) {
        std::wstring_view currInfoTip = w->GetInfoTip(nm->hItem);
        infotip.Append(currInfoTip.data());
        infotip.Append(L"\r\n");
    }

    if (PageDestType::LaunchEmbedded == dstType) {
        path.Set(str::Format(_TR("Attachment: %s"), path.Get()));
    }

    infotip.Append(path);
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
    AutoFreeW label;
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
    if (tocItem->GetLink())
        win->linkHandler->GotoLink(tocItem->GetLink());
    else if (tocItem->pageNo)
        ctrl->GoToPage(tocItem->pageNo, true);
    win->tocKeepSelection = false;
}

static void GoToTocLinkForTVItem(WindowInfo* win, HTREEITEM hItem, bool allowExternal) {
    TreeCtrl* tree = win->tocTreeCtrl;
    if (!hItem) {
        hItem = tree->GetSelection();
    }

    auto* item = tree->GetItem(hItem);
    auto* tocItem = reinterpret_cast<DocTocItem*>(item->lParam);
    if (!tocItem || !win->IsDocLoaded()) {
        return;
    }
    if ((allowExternal || tocItem->GetLink() && PageDestType::ScrollTo == tocItem->GetLink()->GetDestType()) ||
        tocItem->pageNo) {
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

static HTREEITEM AddTocItemToView(TreeCtrl* tree, DocTocItem* entry, HTREEITEM parent, bool toggleItem) {
    TV_INSERTSTRUCT toInsert;
    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_LAST;
    toInsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    UINT state = 0;
    if (entry->child && (entry->open != toggleItem)) {
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
        AutoFreeW label(win->ctrl->GetPageLabel(entry->pageNo));
        AutoFreeW text(str::Format(L"%s  %s", entry->title, label));
        toInsert.itemex.pszText = text;
        return TreeView_InsertItem(hwnd, &tvinsert);
    }
#endif
    return tree->InsertItem(&toInsert);
}

static void PopulateTocTreeView(TreeCtrl* tree, DocTocItem* entry, Vec<int>& tocState, HTREEITEM parent) {
    while (entry) {
        bool toggle = tocState.Contains(entry->id);
        HTREEITEM node = AddTocItemToView(tree, entry, parent, toggle);
        PopulateTocTreeView(tree, entry->child, tocState, node);
        entry = entry->next;
    }
}

#if 0
static void TreeItemForPageNoRec(TreeCtrl* tocTreeCtrl, HTREEITEM hItem, int pageNo, HTREEITEM& bestMatchItem,
                                 int& bestMatchPageNo) {
    while (hItem && bestMatchPageNo < pageNo) {
        TVITEMW* item = TreeCtrlGetItem(tocTreeCtrl, hItem);
        CrashIf(!item);
        if (!item) {
            return;
        }
        // remember this item if it is on the specified page (or on a previous page and closer than all other items)
        if (item->lParam) {
            auto* docItem = reinterpret_cast<DocTocItem*>(item->lParam);
            int page = docItem->pageNo;
            if (page <= pageNo && page >= bestMatchPageNo && page >= 1) {
                bestMatchItem = hItem;
                bestMatchPageNo = page;
            }
        }

        // find any child item closer to the specified page
        if ((item->state & TVIS_EXPANDED)) {
            HTREEITEM child = TreeCtrlGetChild(tocTreeCtrl, hItem);
            TreeItemForPageNoRec(tocTreeCtrl, child, pageNo, bestMatchItem, bestMatchPageNo);
        }

        hItem = TreeCtrlGetNextSibling(tocTreeCtrl, hItem);
    }
}

static HTREEITEM TreeItemForPageNo(TreeCtrl* tocTreeCtrl, int pageNo) {
    HTREEITEM hRoot = TreeCtrlGetRoot(tocTreeCtrl);
    if (!hRoot) {
        return nullptr;
    }

    HTREEITEM bestMatchItem = hRoot;
    int bestMatchPageNo = 0;

    TreeItemForPageNoRec(win, hRoot, pageNo, bestMatchItem, bestMatchPageNo);

    return bestMatchItem;
}
#endif

static HTREEITEM TreeItemForPageNo(TreeCtrl* tocTreeCtrl, int pageNo) {
    HTREEITEM bestMatchItem = nullptr;
    int bestMatchPageNo = 0;

    tocTreeCtrl->VisitNodes([&bestMatchItem, &bestMatchPageNo, pageNo](TVITEMW* item) {
        if (!bestMatchItem) {
            // if nothing else matches, match the root node
            bestMatchItem = item->hItem;
        }
        auto* docItem = reinterpret_cast<DocTocItem*>(item->lParam);
        if (!docItem) {
            return true;
        }
        int page = docItem->pageNo;
        if ((page <= pageNo) && (page >= bestMatchPageNo) && (page >= 1)) {
            bestMatchItem = item->hItem;
            bestMatchPageNo = page;
            if (pageNo == bestMatchPageNo) {
                // we can stop earlier if we found the exact match
                return false;
            }
        }
        return true;
    });
    return bestMatchItem;
}

void UpdateTocSelection(WindowInfo* win, int currPageNo) {
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection) {
        return;
    }

    HTREEITEM hItem = TreeItemForPageNo(win->tocTreeCtrl, currPageNo);
    if (hItem) {
        win->tocTreeCtrl->SelectItem(hItem);
    }
}

void UpdateTocExpansionState(TabInfo* tab, TreeCtrl* treeCtrl, HTREEITEM hItem) {
    while (hItem) {
        TVITEM* item = treeCtrl->GetItem(hItem);
        if (!item) {
            return;
        }

        DocTocItem* tocItem = nullptr;
        if (item->lParam) {
            tocItem = reinterpret_cast<DocTocItem*>(item->lParam);
        }
        if (tocItem && tocItem->child) {
            // add the ids of toggled items to tocState
            bool wasToggled = !(item->state & TVIS_EXPANDED) == tocItem->open;
            if (wasToggled) {
                tab->tocState.Append(tocItem->id);
            }
            HTREEITEM child = treeCtrl->GetChild(hItem);
            UpdateTocExpansionState(tab, treeCtrl, child);
        }
        hItem = treeCtrl->GetSiblingNext(hItem);
    }
}

void UpdateTocColors(WindowInfo* win) {
    COLORREF labelBgCol = GetSysColor(COLOR_BTNFACE);
    COLORREF labelTxtCol = GetSysColor(COLOR_BTNTEXT);
    COLORREF treeBgCol = (DWORD)-1;
    COLORREF splitterCol = GetSysColor(COLOR_BTNFACE);
    bool flatTreeWnd = false;

    if (win->AsEbook()) {
        labelBgCol = GetAppColor(AppColor::DocumentBg);
        labelTxtCol = GetAppColor(AppColor::DocumentText);

        treeBgCol = labelBgCol;
        float factor = 14.f;
        int sign = GetLightness(labelBgCol) + factor > 255 ? 1 : -1;
        splitterCol = AdjustLightness2(labelBgCol, sign * factor);
        flatTreeWnd = true;
    }

    // TOOD: move into TreeCtrl
    TreeView_SetBkColor(win->tocTreeCtrl->hwnd, treeBgCol);
    win->tocLabelWithClose->SetBgCol(labelBgCol);
    win->tocLabelWithClose->SetTextCol(labelTxtCol);
    SetBgCol(win->sidebarSplitter, splitterCol);
    ToggleWindowExStyle(win->tocTreeCtrl->hwnd, WS_EX_STATICEDGE, !flatTreeWnd);
    SetWindowPos(win->tocTreeCtrl->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // TODO: if we have favorites in ebook view, we'll need this
    // SetBgCol(win->favLabelWithClose, labelBgCol);
    // SetTextCol(win->favLabelWithClose, labelTxtCol);
    // SetBgCol(win->favSplitter, labelTxtCol);

    // TODO: more work needed to to ensure consistent look of the ebook window:
    // - tab bar should match the color
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
     (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

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

void LoadTocTree(WindowInfo* win) {
    TabInfo* tab = win->currentTab;
    CrashIf(!tab);

    if (win->tocLoaded)
        return;
    win->tocLoaded = true;

    if (!tab->tocRoot) {
        tab->tocRoot = tab->ctrl->GetTocTree();
        if (!tab->tocRoot)
            return;
    }

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(tab->tocRoot, l2r, r2l);
    bool isRTL = r2l > l2r;

    // TODO: make into TreeCtrlSuspendRedraw()/TreeCtrlResumeRedraw()
    SendMessage(win->tocTreeCtrl->hwnd, WM_SETREDRAW, FALSE, 0);
    SetRtl(win->tocTreeCtrl->hwnd, isRTL);
    PopulateTocTreeView(win->tocTreeCtrl, tab->tocRoot, tab->tocState, nullptr);
    UpdateTocColors(win);
    SendMessage(win->tocTreeCtrl->hwnd, WM_SETREDRAW, TRUE, 0);
    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(win->tocTreeCtrl->hwnd, nullptr, nullptr, fl);
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
            return CDRF_DODEFAULT;
#endif
    }
    return -1;
}

static LRESULT CALLBACK WndProcTocTree(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    UNUSED(dwRefData);
    UNUSED(uIdSubclass);
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_CHAR:
            if (VK_ESCAPE == wp && gGlobalPrefs->escToExit && MayCloseWindow(win))
                CloseWindow(win, true);
            break;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->tocTreeCtrl->hwnd))
                return SendMessage(win->hwndCanvas, msg, wp, lp);
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
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    CrashIf(subclassId != SUBCLASS_ID);
    WindowInfo* winFromData = reinterpret_cast<WindowInfo*>(data);
    WindowInfo* win = FindWindowInfoByHwnd(hwnd);
    if (!win) {
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    CrashIf(win != winFromData);

    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->tocLabelWithClose, win->tocTreeCtrl->hwnd);
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_TOC_LABEL_WITH_CLOSE) {
                ToggleTocBox(win);
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// TODO: for unsubclassing, those need to be part of WindowInfo
static UINT_PTR tocTreeSubclassId = 0;
static UINT_PTR tocBoxSubclassId = 0;

// TODO: should unsubclass as well?
static void SubclassToc(WindowInfo* win) {
    TreeCtrl* tree = win->tocTreeCtrl;
    HWND hwndTocBox = win->hwndTocBox;
    HWND hwndTocTree = win->tocTreeCtrl->hwnd;
    if (tocTreeSubclassId == 0) {
        BOOL wasOk = SetWindowSubclass(hwndTocTree, WndProcTocTree, SUBCLASS_ID, (DWORD_PTR)tree);
        CrashIf(!wasOk);
        tocTreeSubclassId = SUBCLASS_ID;
    }

    if (tocBoxSubclassId == 0) {
        BOOL wasOk = SetWindowSubclass(hwndTocBox, WndProcTocBox, SUBCLASS_ID, (DWORD_PTR)win);
        CrashIf(!wasOk);
        tocBoxSubclassId = SUBCLASS_ID;
    }
}

void CreateToc(WindowInfo* win) {
    win->hwndTocBox = CreateWindow(WC_STATIC, L"", WS_CHILD | WS_CLIPCHILDREN, 0, 0, gGlobalPrefs->sidebarDx, 0,
                                   win->hwndFrame, (HMENU)0, GetModuleHandle(nullptr), nullptr);

    auto* l = new LabelWithCloseWnd();
    l->Create(win->hwndTocBox, IDC_TOC_LABEL_WITH_CLOSE);
    win->tocLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    l->SetFont(GetDefaultGuiFont());
    // label is set in UpdateToolbarSidebarText()

    auto* tree = new TreeCtrl(win->hwndTocBox, nullptr);
    tree->dwStyle = TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_TRACKSELECT |
                    TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP | WS_TABSTOP | WS_VISIBLE | WS_CHILD;
    tree->dwExStyle = WS_EX_STATICEDGE;
    tree->menu = (HMENU)IDC_TOC_TREE;
    tree->onTreeNotify = [win](TreeCtrl* w, NMTREEVIEWW* nm, bool& handled) {
        CrashIf(win->tocTreeCtrl != w);
        LRESULT res = OnTocTreeNotify(win, nm);
        handled = (res != -1);
        return res;
    };
    tree->onGetInfoTip = [](TreeCtrl* w, NMTVGETINFOTIP* infoTipInfo) { CustomizeTocInfoTip(w, infoTipInfo); };
    bool ok = tree->Create(L"TOC");
    CrashIf(!ok);
    win->tocTreeCtrl = tree;
    SubclassToc(win);
}
