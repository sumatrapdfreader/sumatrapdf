/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "TableOfContents.h"

#include "AppPrefs.h"
#include "AppTools.h"
#include "Controller.h"
#include "GdiPlusUtil.h"
#include "LabelWithCloseWnd.h"
#include "resource.h"
#include "SumatraPDF.h"
#include "SplitterWnd.h"
#include "Tabs.h"
#include "Translations.h"
#include "UITask.h"
#include "WindowInfo.h"
#include "WinUtil.h"

/* Define if you want page numbers to be displayed in the ToC sidebar */
// #define DISPLAY_TOC_PAGE_NUMBERS

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
    PageDestination *link = ((DocTocItem *)nmit->lParam)->GetLink();
    ScopedMem<WCHAR> path(link ? link->GetDestValue() : NULL);
    if (!path)
        return;
    CrashIf(!link); // /analyze claims that this could happen - it really can't
    CrashIf(link->GetDestType() != Dest_LaunchURL && link->GetDestType() != Dest_LaunchFile && link->GetDestType() != Dest_LaunchEmbedded);

    str::Str<WCHAR> infotip;

    RECT rcLine, rcLabel;
    HWND hTV = nmit->hdr.hwndFrom;
    // Display the item's full label, if it's overlong
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLine, FALSE);
    TreeView_GetItemRect(hTV, nmit->hItem, &rcLabel, TRUE);
    if (rcLine.right + 2 < rcLabel.right) {
        WCHAR buf[INFOTIPSIZE+1] = { 0 };  // +1 just in case
        TVITEM item;
        item.hItem = nmit->hItem;
        item.mask = TVIF_TEXT;
        item.pszText = buf;
        item.cchTextMax = INFOTIPSIZE;
        TreeView_GetItem(hTV, &item);
        infotip.Append(item.pszText);
        infotip.Append(L"\r\n");
    }

    if (Dest_LaunchEmbedded == link->GetDestType())
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
    WindowInfo *win = FindWindowInfoByHwnd(hTV);
    DocTocItem *tocItem = (DocTocItem *)item.lParam;
    ScopedMem<WCHAR> label;
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
        rcItem.right = max(rcItem.right - txtSize.cx, 0);
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

class GoToTocLinkTask : public UITask
{
    DocTocItem *tocItem;
    WindowInfo *win;
    Controller *ctrl;

public:
    GoToTocLinkTask(WindowInfo *win, DocTocItem *tocItem, Controller *ctrl) :
        win(win), tocItem(tocItem), ctrl(ctrl) { }

    virtual void Execute() {
        // tocItem is invalid if the DisplayModel has been replaced
        if (!WindowInfoStillValid(win) || !win->tocLoaded || win->ctrl != ctrl)
            return;

        // make sure that the tree item that the user selected
        // isn't unselected in UpdateTocSelection right again
        win->tocKeepSelection = true;
        if (tocItem->GetLink())
            win->linkHandler->GotoLink(tocItem->GetLink());
        else if (tocItem->pageNo)
            win->ctrl->GoToPage(tocItem->pageNo, true);
        win->tocKeepSelection = false;
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
    if (!tocItem || !win->IsDocLoaded())
        return;
    if ((allowExternal || tocItem->GetLink() && Dest_ScrollTo == tocItem->GetLink()->GetDestType()) || tocItem->pageNo) {
        // delay changing the page until the tree messages have been handled
        uitask::Post(new GoToTocLinkTask(win, tocItem, win->ctrl));
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
        SetSidebarVisibility(win, false, gGlobalPrefs->showFavorites);
    } else {
        SetSidebarVisibility(win, true,  gGlobalPrefs->showFavorites);
        if (win->tocVisible)
            SetFocus(win->hwndTocTree);
    }
}

static HTREEITEM AddTocItemToView(HWND hwnd, DocTocItem *entry, HTREEITEM parent, bool toggleItem)
{
    TV_INSERTSTRUCT tvinsert;
    tvinsert.hParent = parent;
    tvinsert.hInsertAfter = TVI_LAST;
    tvinsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvinsert.itemex.state = entry->child && entry->open != toggleItem ? TVIS_EXPANDED : 0;
    tvinsert.itemex.stateMask = TVIS_EXPANDED;
    tvinsert.itemex.lParam = (LPARAM)entry;
    // Replace unprintable whitespace with regular spaces
    str::NormalizeWS(entry->title);
    tvinsert.itemex.pszText = entry->title;

#ifdef DISPLAY_TOC_PAGE_NUMBERS
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (entry->pageNo && win && win->IsDocLoaded()) {
        ScopedMem<WCHAR> label(win->ctrl->GetPageLabel(entry->pageNo));
        ScopedMem<WCHAR> text(str::Format(L"%s  %s", entry->title, label));
        tvinsert.itemex.pszText = text;
        return TreeView_InsertItem(hwnd, &tvinsert);
    }
#endif

    return TreeView_InsertItem(hwnd, &tvinsert);
}

static void PopulateTocTreeView(HWND hwnd, DocTocItem *entry, Vec<int>& tocState, HTREEITEM parent = NULL)
{
    for (; entry; entry = entry->next) {
        bool toggle = tocState.Contains(entry->id);
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
    if (!win->tocLoaded || !win->tocVisible || win->tocKeepSelection)
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

void UpdateTocColors(WindowInfo *win)
{
    COLORREF bgCol = RGB(0xff, 0xff, 0xff);
    COLORREF txtCol = GetSysColor(COLOR_WINDOWTEXT);
    COLORREF treeBgCol = (DWORD)-1;
    COLORREF splitterCol = GetSysColor(COLOR_BTNFACE);
    if (win->AsEbook() && !gGlobalPrefs->useSysColors) {
        bgCol = gGlobalPrefs->ebookUI.backgroundColor;
        txtCol = gGlobalPrefs->ebookUI.textColor;
        treeBgCol = bgCol;
        float factor = 14.f;
        int sign = GetLightness(bgCol) + factor > 255 ? 1 : -1;
        splitterCol = AdjustLightness2(bgCol, sign * factor);
    }

    TreeView_SetBkColor(win->hwndTocTree, treeBgCol);
    SetBgCol(win->tocLabelWithClose, bgCol);
    SetTextCol(win->tocLabelWithClose, txtCol);
    SetBgCol(win->sidebarSplitter, splitterCol);

    // TODO: if we have favorites in ebook view, we'll need this
    //SetBgCol(win->favLabelWithClose, bgCol);
    //SetTextCol(win->favLabelWithClose, txtCol);
    //SetBgCol(win->favSplitter, txtCol);

    // TODO: more work needed to to ensure consistent look of the ebook window:
    // - tab bar should match the color
    // - change the tree item text color
    // - change the tree item background color when selected (for both focused and non-focused cases)
    // - ultimately implement owner-drawn scrollbars in a simpler style (like Chrome or VS 2013)
    //   and match their colors as well
}

// copied from mupdf/fitz/dev_text.c
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void GetLeftRightCounts(DocTocItem *node, int& l2r, int& r2l)
{
    if (!node)
        return;
    if (node->title) {
        for (const WCHAR *c = node->title; *c; c++) {
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
    CrashIf(!win->IsDocLoaded());

    if (win->tocLoaded)
        return;
    win->tocLoaded = true;

    win->tocRoot = win->ctrl->GetTocTree();
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
    UpdateTocColors(win);
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
        case WM_ERASEBKGND:
            return FALSE;
        case WM_CHAR:
            if (VK_ESCAPE == wParam && gGlobalPrefs->escToExit)
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
        case WM_MOUSEHWHEEL:
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

static WNDPROC DefWndProcTocBox = NULL;
static LRESULT CALLBACK WndProcTocBox(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WindowInfo *win = FindWindowInfoByHwnd(hwnd);
    if (!win)
        return CallWindowProc(DefWndProcTocBox, hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_SIZE:
            LayoutTreeContainer(win->tocLabelWithClose, win->hwndTocTree);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_TOC_LABEL_WITH_CLOSE)
                ToggleTocBox(win);
            break;

        case WM_NOTIFY:
            if (LOWORD(wParam) == IDC_TOC_TREE) {
                LRESULT res = OnTocTreeNotify(win, (LPNMTREEVIEW)lParam);
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
    win->hwndTocBox = CreateWindow(WC_STATIC, L"", WS_CHILD|WS_CLIPCHILDREN,
                                   0, 0, gGlobalPrefs->sidebarDx, 0,
                                   win->hwndFrame, (HMENU)0, GetModuleHandle(NULL), NULL);

    LabelWithCloseWnd *l = CreateLabelWithCloseWnd(win->hwndTocBox, IDC_TOC_LABEL_WITH_CLOSE);
    win->tocLabelWithClose = l;
    int padXY = (int)(2 * win->uiDPIFactor);
    SetPaddingXY(l, padXY, padXY);
    SetFont(l, gDefaultGuiFont);
    // label is set in UpdateSidebarTitles()

    win->hwndTocTree = CreateWindowEx(0, WC_TREEVIEW, L"TOC",
                                      TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|
                                      TVS_TRACKSELECT|TVS_DISABLEDRAGDROP|TVS_NOHSCROLL|TVS_INFOTIP|
                                      WS_TABSTOP|WS_VISIBLE|WS_CHILD,
                                      0, 0, 0, 0, win->hwndTocBox, (HMENU)IDC_TOC_TREE, GetModuleHandle(NULL), NULL);

    TreeView_SetUnicodeFormat(win->hwndTocTree, true);

    if (NULL == DefWndProcTocTree)
        DefWndProcTocTree = (WNDPROC)GetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocTree, GWLP_WNDPROC, (LONG_PTR)WndProcTocTree);

    if (NULL == DefWndProcTocBox)
        DefWndProcTocBox = (WNDPROC)GetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC);
    SetWindowLongPtr(win->hwndTocBox, GWLP_WNDPROC, (LONG_PTR)WndProcTocBox);
}
