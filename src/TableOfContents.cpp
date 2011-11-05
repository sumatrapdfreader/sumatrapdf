/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "resource.h"
#include "TableOfContents.h"
#include "WindowInfo.h"
#include "translations.h"
#include "SumatraPDF.h"
#include "AppTools.h"

#ifdef DISPLAY_TOC_PAGE_NUMBERS
#define WM_APP_REPAINT_TOC     (WM_APP + 1)
#endif

static void TreeView_ExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree=false)
{
    while (hItem) {
        TreeView_Expand(hTree, hItem, flag);
        HTREEITEM child = TreeView_GetChild(hTree, hItem);
        if (child)
            TreeView_ExpandRecursively(hTree, child, flag);
        if (subtree)
            break;
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

static void CustomizeTocInfoTip(LPNMTVGETINFOTIP nmit)
{
    DocTocItem *tocItem = (DocTocItem *)nmit->lParam;
    ScopedMem<TCHAR> path(tocItem->GetLink() ? tocItem->GetLink()->GetDestValue() : NULL);
    if (!path)
        return;

    str::Str<TCHAR> infotip(INFOTIPSIZE);

    RECT rcLine, rcLabel;
    HWND hTV = nmit->hdr.hwndFrom;
    // Display the item's full label, if it's overlong
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLine, FALSE);
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLabel, TRUE);
    if (rcLine.right + 2 < rcLabel.right) {
        TVITEM item;
        item.hItem = nmit->hItem;
        item.mask = TVIF_TEXT;
        item.pszText = infotip.Get();
        item.cchTextMax = INFOTIPSIZE;
        TreeView_GetItem(hTV, &item);
        infotip.LenIncrease(str::Len(item.pszText));
        infotip.Append(_T("\r\n"));
    }

    if (tocItem->GetLink() && str::Eq(tocItem->GetLink()->GetType(), "LaunchEmbedded"))
        path.Set(str::Format(_TR("Attachment: %s"), path));

    infotip.Append(path);
    str::BufSet(nmit->pszText, nmit->cchTextMax, infotip.Get());
}

#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd)
{
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
    HBRUSH brushBg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(ncd->hdc, &rcFullWidth, brushBg);
    DeleteObject(brushBg);

    // Get the label's text
    TCHAR szText[MAX_PATH];
    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_TEXT | TVIF_PARAM;
    item.pszText = szText;
    item.cchTextMax = MAX_PATH;
    TreeView_GetItem(hTV, &item);

    // Draw the page number right-aligned (if there is one)
    WindowInfo *win = FindWindowInfoByHwnd(hTV);
    DocTocItem *tocItem = (DocTocItem *)item.lParam;
    ScopedMem<TCHAR> label;
    if (tocItem->pageNo && win && win->IsDocLoaded() && win->dm->engine) {
        label.Set(win->dm->engine->GetPageLabel(tocItem->pageNo));
        label.Set(str::Join(_T("  "), label));
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
        rcItem.right = max(rcItem.right - txtSize.cx, 0);
        szText[str::Len(szText) - str::Len(label)] = '\0';
    }

    SetTextColor(ncd->hdc, ntvcd->clrText);
    SetBkColor(ncd->hdc, ntvcd->clrTextBk);

    // Draw the focus rectangle (including proper background color)
    brushBg = CreateSolidBrush(ntvcd->clrTextBk);
    FillRect(ncd->hdc, &rcItem, brushBg);
    DeleteObject(brushBg);
    if ((ncd->uItemState & CDIS_FOCUS))
        DrawFocusRect(ncd->hdc, &rcItem);

    InflateRect(&rcItem, -2, -1);
    DrawText(ncd->hdc, szText, -1, &rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_WORD_ELLIPSIS);
}
#endif

class GoToTocLinkWorkItem : public UIThreadWorkItem
{
    DocTocItem *tocItem;

public:
    GoToTocLinkWorkItem(WindowInfo *win, DocTocItem *tocItem) :
        UIThreadWorkItem(win), tocItem(tocItem) { }

    virtual void Execute() {
        if (!WindowInfoStillValid(win) || !win->IsDocLoaded() || !tocItem)
            return;

        if (tocItem->GetLink())
            win->linkHandler->GotoLink(tocItem->GetLink());
        else if (tocItem->pageNo)
            win->dm->GoToPage(tocItem->pageNo, 0, true);
    }
};

static void GoToTocLinkForTVItem(WindowInfo* win, HWND hTV, HTREEITEM hItem=NULL, bool allowExternal=true)
{
    if (!hItem)
        hItem = TreeView_GetSelection(hTV);

    TVITEM item;
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(hTV, &item);
    DocTocItem *tocItem = (DocTocItem *)item.lParam;
    if (win->IsDocLoaded() && tocItem &&
        (allowExternal || tocItem->GetLink() && str::Eq(tocItem->GetLink()->GetType(), "ScrollTo")) || tocItem->pageNo) {
        QueueWorkItem(new GoToTocLinkWorkItem(win, tocItem));
    }
}

void ClearTocBox(WindowInfo *win)
{
    if (!win->tocLoaded) return;

    SendMessage(win->hwndTocTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(win->hwndTocTree);
    SendMessage(win->hwndTocTree, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(win->hwndTocTree, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

    delete win->tocRoot;
    win->tocRoot = NULL;

    win->tocLoaded = false;
    win->currPageNo = 0;
}

void ToggleTocBox(WindowInfo *win)
{
    if (!win->IsDocLoaded())
        return;
    if (win->tocVisible) {
        SetSidebarVisibility(win, false, gGlobalPrefs.favVisible);
    } else {
        SetSidebarVisibility(win, true,  gGlobalPrefs.favVisible);
        SetFocus(win->hwndTocTree);
    }
}

static HTREEITEM AddTocItemToView(HWND hwnd, DocTocItem *entry, HTREEITEM parent, bool toggleItem)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvinsert.itemex.state = entry->open != toggleItem ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)entry;
    // Replace unprintable whitespace with regular spaces
    str::NormalizeWS(entry->title);
    tvinsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (entry->pageNo && win && win->IsDocLoaded() && win->dm->engine) {
        ScopedMem<TCHAR> label(win->dm->engine->GetPageLabel(entry->pageNo));
        ScopedMem<TCHAR> text(str::Format(_T("%s  %s"), entry->title, label));
        tvinsert.itemex.pszText = text;
        return TreeView_InsertItem(hwnd, &tvinsert);
    }
#endif

    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void PopulateTocTreeView(HWND hwnd, DocTocItem *entry, Vec<int>& tocState, HTREEITEM parent = NULL)
{
    for (; entry; entry = entry->next) {
        bool toggle = tocState.Find(entry->id) != -1;
        HTREEITEM node = AddTocItemToView(hwnd, entry, parent, toggle);
        PopulateTocTreeView(hwnd, entry->child, tocState, node);
    }
}

static HTREEITEM TreeItemForPageNo(WindowInfo *win, HTREEITEM hItem, int pageNo)
{
    HTREEITEM hCurrItem = NULL;

    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(win->hwndTocTree, &item);

        // return if this item is on the specified page (or on a latter page)
        if (item.lParam) {
            int page = ((DocTocItem *)item.lParam)->pageNo;
            if (1 <= page && page <= pageNo)
                hCurrItem = hItem;
            if (page >= pageNo)
                break;
        }

        // find any child item closer to the specified page
        HTREEITEM hSubItem = NULL;
        if ((item.state & TVIS_EXPANDED))
            hSubItem = TreeItemForPageNo(win, TreeView_GetChild(win->hwndTocTree, hItem), pageNo);
        if (hSubItem)
            hCurrItem = hSubItem;

        hItem = TreeView_GetNextSibling(win->hwndTocTree, hItem);
    }

    return hCurrItem;
}

void UpdateTocSelection(WindowInfo *win, int currPageNo)
{
    if (!win->tocLoaded || !win->tocVisible)
        return;
    if (GetFocus() == win->hwndTocTree)
        return;

    HTREEITEM hRoot = TreeView_GetRoot(win->hwndTocTree);
    if (!hRoot)
        return;
    // select the item closest to but not after the current page
    // (or the root item, if there's no such item)
    HTREEITEM hItem = TreeItemForPageNo(win, hRoot, currPageNo);
    if (NULL == hItem)
        hItem = hRoot;
    TreeView_SelectItem(win->hwndTocTree, hItem);
}

void UpdateTocExpansionState(WindowInfo *win, HTREEITEM hItem)
{
    while (hItem) {
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        TreeView_GetItem(win->hwndTocTree, &item);

        // add the ids of toggled items to tocState
        DocTocItem *tocItem = item.lParam ? (DocTocItem *)item.lParam : NULL;
        bool wasToggled = tocItem && !(item.state & TVIS_EXPANDED) == tocItem->open;
        if (wasToggled)
            win->tocState.Append(tocItem->id);

        if (tocItem && tocItem->child)
            UpdateTocExpansionState(win, TreeView_GetChild(win->hwndTocTree, hItem));
        hItem = TreeView_GetNextSibling(win->hwndTocTree, hItem);
    }
}

// copied from mupdf/fitz/dev_text.c
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void GetLeftRightCounts(DocTocItem *node, int& l2r, int& r2l)
{
    if (!node)
        return;
    if (node->title) {
        for (const TCHAR *c = node->title; *c; c++) {
            if (ISLEFTTORIGHTCHAR(*c))
                l2r++;
            else if (ISRIGHTTOLEFTCHAR(*c))
                r2l++;
        }
    }
    GetLeftRightCounts(node->child, l2r, r2l);
    GetLeftRightCounts(node->next, l2r, r2l);
}

void LoadTocTree(WindowInfo *win)
{
    if (win->tocLoaded)
        return;
    win->tocLoaded = true;

    win->tocRoot = NULL;
    if (win->dm->engine)
        win->tocRoot = win->dm->engine->GetTocTree();
    if (!win->tocRoot)
        return;

    // consider a ToC tree right-to-left if a more than half of the
    // alphabetic characters are in a right-to-left script
    int l2r = 0, r2l = 0;
    GetLeftRightCounts(win->tocRoot, l2r, r2l);
    bool isRTL = r2l > l2r;

    SendMessage(win->hwndTocTree, WM_SETREDRAW, FALSE, 0);
    ToggleWindowStyle(win->hwndTocTree, WS_EX_LAYOUTRTL | WS_EX_NOINHERITLAYOUT, isRTL, GWL_EXSTYLE);
    PopulateTocTreeView(win->hwndTocTree, win->tocRoot, win->tocState);
    SendMessage(win->hwndTocTree, WM_SETREDRAW, TRUE, 0);
    UINT fl = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(win->hwndTocTree, NULL, NULL, fl);
}

static LRESULT OnTocTreeNotify(WindowInfo *win, LPNMTREEVIEW pnmtv)
{
    switch (pnmtv->hdr.code) 
    {
        case TVN_SELCHANGED: 
            // When the focus is set to the toc window the first item in the treeview is automatically
            // selected and a TVN_SELCHANGEDW notification message is sent with the special code pnmtv->action == 0x00001000.
            // We have to ignore this message to prevent the current page to be changed.
            if (TVC_BYKEYBOARD == pnmtv->action || TVC_BYMOUSE == pnmtv->action)
                GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom, pnmtv->itemNew.hItem, TVC_BYMOUSE == pnmtv->action);
            // The case pnmtv->action==TVC_UNKNOWN is ignored because 
            // it corresponds to a notification sent by
            // the function TreeView_DeleteAllItems after deletion of the item.
            break;

        case TVN_KEYDOWN: {
            TV_KEYDOWN *ptvkd = (TV_KEYDOWN *)pnmtv;
            if (VK_TAB == ptvkd->wVKey) {
                AdvanceFocus(win);
                return 1;
            }
            break;
        }
        case NM_CLICK: {
            // Determine which item has been clicked (if any)
            TVHITTESTINFO ht = { 0 };
            DWORD pos = GetMessagePos();
            ht.pt.x = GET_X_LPARAM(pos);
            ht.pt.y = GET_Y_LPARAM(pos);
            MapWindowPoints(HWND_DESKTOP, pnmtv->hdr.hwndFrom, &ht.pt, 1);
            TreeView_HitTest(pnmtv->hdr.hwndFrom, &ht);

            // let TVN_SELCHANGED handle the click, if it isn't on the already selected item
            if ((ht.flags & TVHT_ONITEM) && TreeView_GetSelection(pnmtv->hdr.hwndFrom) == ht.hItem)
                GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom, ht.hItem);
            break;
        }
        case NM_RETURN:
            GoToTocLinkForTVItem(win, pnmtv->hdr.hwndFrom);
            break;

        case NM_CUSTOMDRAW:
#ifdef DISPLAY_TOC_PAGE_NUMBERS
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

        case TVN_GETINFOTIP:
            CustomizeTocInfoTip((LPNMTVGETINFOTIP)pnmtv);
            break;
    }
    return -1;
}

static WNDPROC DefWndProcTocTree = NULL;
static LRESULT CALLBACK WndProcTocTree(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcTocTree, hwnd, message, wParam, lParam);

    switch (message) {
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs.escToExit)
                DestroyWindow(win->hwndFrame);
            break;
        case WM_KEYDOWN:
            // consistently expand/collapse whole (sub)trees
            if (VK_MULTIPLY == wParam && IsShiftPressed())
                TreeView_ExpandRecursively(hwnd, TreeView_GetRoot(hwnd), TVE_EXPAND);
            else if (VK_MULTIPLY == wParam)
                TreeView_ExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_EXPAND, true);
            else if (VK_DIVIDE == wParam && IsShiftPressed()) {
                HTREEITEM root = TreeView_GetRoot(hwnd);
                if (!TreeView_GetNextSibling(hwnd, root))
                    root = TreeView_GetChild(hwnd, root);
                TreeView_ExpandRecursively(hwnd, root, TVE_COLLAPSE);
            }
            else if (VK_DIVIDE == wParam)
                TreeView_ExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_COLLAPSE, true);
            else
                break;
            TreeView_EnsureVisible(hwnd, TreeView_GetSelection(hwnd));
            return 0;
        case WM_MOUSEWHEEL:
            // scroll the canvas if the cursor isn't over the ToC tree
            if (!IsCursorOverWindow(win->hwndTocTree))
                return SendMessage(win->hwndCanvas, message, wParam, lParam);
            break;
#ifdef DISPLAY_TOC_PAGE_NUMBERS
        case WM_SIZE:
        case WM_HSCROLL:
            // Repaint the ToC so that RelayoutTocItem is called for all items
            PostMessage(hwnd, WM_APP_REPAINT_TOC, 0, 0);
            break;
        case WM_APP_REPAINT_TOC:
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateWindow(hwnd);
            break;
#endif
    }
    return CallWindowProc(DefWndProcTocTree, hwnd, message, wParam, lParam);
}

static void CustomizeTocInfoTip(LPNMTVGETINFOTIP nmit);
#ifdef DISPLAY_TOC_PAGE_NUMBERS
static void RelayoutTocItem(LPNMTVCUSTOMDRAW ntvcd);
#endif

static WNDPROC DefWndProcTocBox = NULL;
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcTocBox, hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(hwnd, IDC_TOC_BOX);
            break;

        case WM_DRAWITEM:
            if (IDC_TOC_CLOSE == wParam) {
                DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
                DrawCloseButton(dis);
                return TRUE;
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_TOC_CLOSE && HIWORD(wParam) == STN_CLICKED)
                ToggleTocBox(win);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_TOC_TREE) {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;
                LRESULT res = OnTocTreeNotify(win, pnmtv);
                if (res != -1)
                    return res;
            }
            break;
    }
    return CallWindowProc(DefWndProcTocBox, hwnd, msg, wParam, lParam);
}

void CreateToc(WindowInfo *win)
{
   // toc windows
   win->hwndTocBox = CreateWindow(WC_STATIC, _T(""), WS_CHILD,
                                  0, 0, gGlobalPrefs.sidebarDx, 0,
                                  win->hwndFrame, (HMENU)0, ghinst, NULL);
   HWND title = CreateWindow(WC_STATIC, _T(""), WS_VISIBLE | WS_CHILD,
                             0, 0, 0, 0, win->hwndTocBox, (HMENU)IDC_TOC_TITLE, ghinst, NULL);
   SetWindowFont(title, gDefaultGuiFont, FALSE);
   win::SetText(title, _TR("Bookmarks"));

   HWND hwndClose = CreateWindow(WC_STATIC, _T(""),
                                 SS_OWNERDRAW | SS_NOTIFY | WS_CHILD | WS_VISIBLE,
                                 0, 0, 16, 16, win->hwndTocBox, (HMENU)IDC_TOC_CLOSE, ghinst, NULL);

   win->hwndTocTree = CreateWindowEx(WS_EX_STATICEDGE, WC_TREEVIEW, _T("TOC"),
                                     TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                                     TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                                     WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                     0, 0, 0, 0, win->hwndTocBox, (HMENU)IDC_TOC_TREE, ghinst, NULL);

   // Note: those must be consecutive numbers and in title/close/tree order
   CASSERT(IDC_TOC_BOX + 1 == IDC_TOC_TITLE &&
           IDC_TOC_BOX + 2 == IDC_TOC_CLOSE &&
           IDC_TOC_BOX + 3 == IDC_TOC_TREE, consecutive_toc_ids);

#ifdef UNICODE
   TreeView_SetUnicodeFormat(win->hwndTocTree, true);
#endif

   if (NULL == DefWndProcTocTree)
       DefWndProcTocTree = (WNDPROC)GetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC);
   SetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC, (LONG_PTR)WndProcTocTree);

   if (NULL == DefWndProcTocBox)
       DefWndProcTocBox = (WNDPROC)GetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC);
   SetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC, (LONG_PTR)WndProcTocBox);

   if (NULL == DefWndProcCloseButton)
       DefWndProcCloseButton = (WNDPROC)GetWindowLongPtr(hwndClose, GWLP_WNDPROC);
   SetWindowLongPtr(hwndClose, GWLP_WNDPROC, (LONG_PTR)WndProcCloseButton);
}
