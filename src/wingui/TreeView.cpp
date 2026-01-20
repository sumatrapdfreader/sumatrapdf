/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//--- TreeView

/*
- https://docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-reference

Tree view, checkboxes and other info:
- https://devblogs.microsoft.com/oldnewthing/20171127-00/?p=97465
- https://devblogs.microsoft.com/oldnewthing/20171128-00/?p=97475
- https://devblogs.microsoft.com/oldnewthing/20171129-00/?p=97485
- https://devblogs.microsoft.com/oldnewthing/20171130-00/?p=97495
- https://devblogs.microsoft.com/oldnewthing/20171201-00/?p=97505
- https://devblogs.microsoft.com/oldnewthing/20171204-00/?p=97515
- https://devblogs.microsoft.com/oldnewthing/20171205-00/?p=97525
-
https://stackoverflow.com/questions/34161879/how-to-remove-checkboxes-on-specific-tree-view-items-with-the-tvs-checkboxes-sty
*/

Kind kindTreeView = "treeView";

TreeView::TreeView() {
    kind = kindTreeView;
}

TreeView::~TreeView() {
}

HWND TreeView::Create(const CreateArgs& argsIn) {
    idealSize = {48, 120}; // arbitrary
    fullRowSelect = argsIn.fullRowSelect;

    CreateControlArgs args;
    args.className = WC_TREEVIEWW;
    args.parent = argsIn.parent;
    args.font = argsIn.font;
    args.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER;
    args.style |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
    args.style |= TVS_TRACKSELECT | TVS_NOHSCROLL | TVS_INFOTIP;
    args.exStyle = argsIn.exStyle | TVS_EX_DOUBLEBUFFER;

    if (fullRowSelect) {
        args.style |= TVS_FULLROWSELECT;
        args.style &= ~TVS_HASLINES;
    }

    Wnd::CreateControl(args);

    if (IsWindowsVistaOrGreater()) {
        SendMessageW(hwnd, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    }
    if (DynSetWindowTheme) {
        DynSetWindowTheme(hwnd, L"Explorer", nullptr);
    }

    TreeView_SetUnicodeFormat(hwnd, true);

    SetToolTipsDelayTime(TTDT_AUTOPOP, 32767);

    // TODO:
    // must be done at the end. Doing  SetWindowStyle() sends bogus (?)
    // TVN_ITEMCHANGED notification. As an alternative we could ignore TVN_ITEMCHANGED
    // if hItem doesn't point to an TreeItem

    return hwnd;
}

Size TreeView::GetIdealSize() {
    return {idealSize.dx, idealSize.dy};
}

void TreeView::SetToolTipsDelayTime(int type, int timeInMs) {
    ReportIf(!IsValidDelayType(type));
    ReportIf(timeInMs < 0);
    ReportIf(timeInMs > 32767); // TODO: or is it 65535?
    HWND hwndToolTips = GetToolTipsHwnd();
    SendMessageW(hwndToolTips, TTM_SETDELAYTIME, type, (LPARAM)timeInMs);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/tvm-gettooltips
HWND TreeView::GetToolTipsHwnd() {
    return TreeView_GetToolTips(hwnd);
}

HTREEITEM TreeView::GetHandleByTreeItem(TreeItem item) {
    return treeModel->GetHandle(item);
}

// the result only valid until the next GetItem call
static TVITEMW* GetTVITEM(TreeView* tree, HTREEITEM hItem) {
    TVITEMW* ti = &tree->item;
    ZeroStruct(ti);
    ti->hItem = hItem;
    // https: // docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-tvitemexa
    ti->mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    ti->stateMask = TVIS_SELECTED | TVIS_CUT | TVIS_DROPHILITED | TVIS_BOLD | TVIS_EXPANDED | TVIS_STATEIMAGEMASK;
    BOOL ok = TreeView_GetItem(tree->hwnd, ti);
    if (!ok) {
        return nullptr;
    }
    return ti;
}

TVITEMW* GetTVITEM(TreeView* tree, TreeItem ti) {
    HTREEITEM hi = tree->GetHandleByTreeItem(ti);
    return GetTVITEM(tree, hi);
}

// expand if collapse, collapse if expanded
void TreeViewToggle(TreeView* tree, HTREEITEM hItem, bool recursive) {
    HWND hTree = tree->hwnd;
    HTREEITEM child = TreeView_GetChild(hTree, hItem);
    if (!child) {
        // only applies to nodes with children
        return;
    }

    TVITEMW* item = GetTVITEM(tree, hItem);
    if (!item) {
        return;
    }
    uint flag = TVE_EXPAND;
    bool isExpanded = bitmask::IsSet(item->state, TVIS_EXPANDED);
    if (isExpanded) {
        flag = TVE_COLLAPSE;
    }
    if (recursive) {
        TreeViewExpandRecursively(hTree, hItem, flag, false);
    } else {
        TreeView_Expand(hTree, hItem, flag);
    }
}

void SetTreeItemState(uint uState, TreeItemState& state) {
    state.isExpanded = bitmask::IsSet(uState, TVIS_EXPANDED);
    state.isSelected = bitmask::IsSet(uState, TVIS_SELECTED);
    uint n = (uState >> 12) - 1;
    state.isChecked = n != 0;
}

static bool HandleKey(TreeView* tree, WPARAM wp) {
    HWND hwnd = tree->hwnd;
    // consistently expand/collapse whole (sub)trees
    if (VK_MULTIPLY == wp) {
        if (IsShiftPressed()) {
            TreeViewExpandRecursively(hwnd, TreeView_GetRoot(hwnd), TVE_EXPAND, false);
        } else {
            TreeViewExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_EXPAND, true);
        }
    } else if (VK_DIVIDE == wp) {
        if (IsShiftPressed()) {
            HTREEITEM root = TreeView_GetRoot(hwnd);
            if (!TreeView_GetNextSibling(hwnd, root)) {
                root = TreeView_GetChild(hwnd, root);
            }
            TreeViewExpandRecursively(hwnd, root, TVE_COLLAPSE, false);
        } else {
            TreeViewExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_COLLAPSE, true);
        }
    } else if (wp == 13) {
        // this is Enter key
        bool recursive = IsShiftPressed();
        TreeViewToggle(tree, TreeView_GetSelection(hwnd), recursive);
    } else {
        return false;
    }
    TreeView_EnsureVisible(hwnd, TreeView_GetSelection(hwnd));
    return true;
}

LRESULT TreeView::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT res;
    TreeView* w = this;

    if (WM_ERASEBKGND == msg) {
        return FALSE;
    }

    if (WM_RBUTTONDOWN == msg) {
        // this is needed to make right click trigger context menu
        // otherwise it gets turned into NM_CLICK and it somehow
        // blocks WM_RBUTTONUP, which is a trigger for WM_CONTEXTMENU
        res = DefWindowProcW(hwnd, msg, wparam, lparam);
        return res;
    }

    if (WM_KEYDOWN == msg) {
        if (HandleKey(w, wparam)) {
            return 0;
        }
    }

    res = WndProcDefault(hwnd, msg, wparam, lparam);
    return res;
}

bool TreeView::IsExpanded(TreeItem ti) {
    auto state = GetItemState(ti);
    return state.isExpanded;
}

// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-treeview_getitemrect
bool TreeView::GetItemRect(TreeItem ti, bool justText, RECT& r) {
    HTREEITEM hi = GetHandleByTreeItem(ti);
    BOOL b = toBOOL(justText);
    BOOL ok = TreeView_GetItemRect(hwnd, hi, &r, b);
    return ok == TRUE;
}

TreeItem TreeView::GetSelection() {
    HTREEITEM hi = TreeView_GetSelection(hwnd);
    return GetTreeItemByHandle(hi);
}

bool TreeView::SelectItem(TreeItem ti) {
    HTREEITEM hi = nullptr;
    if (ti != TreeModel::kNullItem) {
        hi = GetHandleByTreeItem(ti);
    }
    BOOL ok = TreeView_SelectItem(hwnd, hi);
    return ok == TRUE;
}

void TreeView::SetColors(COLORREF textCol, COLORREF bgCol) {
    Wnd::SetColors(textCol, bgCol);
    if (!IsSpecialColor(textCol)) {
        TreeView_SetTextColor(hwnd, textCol);
    } else if (textColor == kColorUnset) {
        TreeView_SetTextColor(hwnd, CLR_DEFAULT);
    }
    if (!IsSpecialColor(bgCol)) {
        TreeView_SetBkColor(hwnd, bgCol);
    } else if (bgCol == kColorUnset) {
        TreeView_SetBkColor(hwnd, CLR_DEFAULT);
    }
}

void TreeView::ExpandAll() {
    SuspendRedraw();
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_EXPAND, false);
    ResumeRedraw();
}

void TreeView::CollapseAll() {
    SuspendRedraw();
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_COLLAPSE, false);
    ResumeRedraw();
}

void TreeView::Clear() {
    treeModel = nullptr;

    HWND hwnd = this->hwnd;
    ::SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    uint flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    ::RedrawWindow(hwnd, nullptr, nullptr, flags);
}

char* TreeView::GetDefaultTooltipTemp(TreeItem ti) {
    auto hItem = GetHandleByTreeItem(ti);
    WCHAR buf[INFOTIPSIZE + 1]{}; // +1 just in case

    TVITEMW it{};
    it.hItem = hItem;
    it.mask = TVIF_TEXT;
    it.pszText = buf;
    it.cchTextMax = dimof(buf);
    TreeView_GetItem(hwnd, &it);

    return ToUtf8Temp(buf);
}

// get the item at a given (x,y) position in the window
TreeItem TreeView::GetItemAt(int x, int y) {
    TVHITTESTINFO ht{};
    ht.pt = {x, y};
    TreeView_HitTest(hwnd, &ht);
    return GetTreeItemByHandle(ht.hItem);
}

TreeItem TreeView::GetTreeItemByHandle(HTREEITEM item) {
    if (item == nullptr) {
        return TreeModel::kNullItem;
    }
    auto tvi = GetTVITEM(this, item);
    if (!tvi) {
        return TreeModel::kNullItem;
    }
    TreeItem res = (TreeItem)(tvi->lParam);
    return res;
}

static void FillTVITEM(TVITEMEXW* tvitem, TreeModel* tm, TreeItem ti) {
    uint mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvitem->mask = mask;

    uint stateMask = TVIS_EXPANDED;
    uint state = 0;
    if (tm->IsExpanded(ti)) {
        state = TVIS_EXPANDED;
    }

    tvitem->state = state;
    tvitem->stateMask = stateMask;
    tvitem->lParam = static_cast<LPARAM>(ti);
    char* title = tm->Text(ti);
    tvitem->pszText = ToWStrTemp(title);
}

// inserting in front is faster:
// https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033
HTREEITEM insertItemFront(TreeView* treeView, TreeItem ti, HTREEITEM parent) {
    TVINSERTSTRUCTW toInsert{};

    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_FIRST;

    TVITEMEXW* tvitem = &toInsert.itemex;
    FillTVITEM(tvitem, treeView->treeModel, ti);
    HTREEITEM res = TreeView_InsertItem(treeView->hwnd, &toInsert);
    return res;
}

bool TreeView::UpdateItem(TreeItem ti) {
    HTREEITEM ht = GetHandleByTreeItem(ti);
    ReportIf(!ht);
    if (!ht) {
        return false;
    }

    TVITEMEXW tvitem;
    tvitem.hItem = ht;
    FillTVITEM(&tvitem, treeModel, ti);
    BOOL ok = TreeView_SetItem(hwnd, &tvitem);
    return ok != 0;
}

// complicated because it inserts items backwards, as described in
// https://devblogs.microsoft.com/oldnewthing/20111125-00/?p=9033
void PopulateTreeItem(TreeView* treeView, TreeItem item, HTREEITEM parent) {
    auto tm = treeView->treeModel;
    int n = tm->ChildCount(item);
    TreeItem* a = AllocArrayTemp<TreeItem>(n);
    // ChildAt() is optimized for sequential access and we need to
    // insert backwards, so gather the items in v first
    for (int i = 0; i < n; i++) {
        auto ti = tm->ChildAt(item, i);
        ReportIf(ti == 0);
        a[n - 1 - i] = ti;
    }

    for (int i = 0; i < n; i++) {
        auto ti = a[i];
        HTREEITEM h = insertItemFront(treeView, ti, parent);
        tm->SetHandle(ti, h);
        // avoid recursing if not needed because we use a lot of stack space
        if (tm->ChildCount(ti) > 0) {
            PopulateTreeItem(treeView, ti, h);
        }
    }
}

static void PopulateTree(TreeView* treeView, TreeModel* tm) {
    TreeItem root = tm->Root();
    PopulateTreeItem(treeView, root, nullptr);
}

void TreeView::SetTreeModel(TreeModel* tm) {
    ReportIf(!tm);

    SuspendRedraw();

    TreeView_DeleteAllItems(hwnd);

    treeModel = tm;
    PopulateTree(this, tm);
    ResumeRedraw();

    uint flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwnd, nullptr, nullptr, flags);
}

void TreeView::SetState(TreeItem item, bool enable) {
    HTREEITEM hi = GetHandleByTreeItem(item);
    ReportIf(!hi);
    TreeView_SetCheckState(hwnd, hi, enable);
}

bool TreeView::GetState(TreeItem item) {
    HTREEITEM hi = GetHandleByTreeItem(item);
    ReportIf(!hi);
    auto res = TreeView_GetCheckState(hwnd, hi);
    return res != 0;
}

TreeItemState TreeView::GetItemState(TreeItem ti) {
    TreeItemState res;

    TVITEMW* it = GetTVITEM(this, ti);
    ReportIf(!it);
    if (!it) {
        return res;
    }
    SetTreeItemState(it->state, res);
    res.nChildren = it->cChildren;

    return res;
}

// if context menu invoked via keyboard, get selected item
// if via right-click, selects the item under the cursor
// in both cases can return null
// sets pt to screen position (for context menu coordinates)
TreeItem GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt) {
    TreeView* treeView = (TreeView*)args->w;
    // TreeModel* tm = treeView->treeModel;
    HWND hwnd = treeView->hwnd;

    TreeItem ti;
    pt = {args->mouseWindow.x, args->mouseWindow.y};
    if (pt.x == -1 || pt.y == -1) {
        // no mouse position when launched via keyboard shortcut
        // use position of selected item to show menu
        ti = treeView->GetSelection();
        if (ti == TreeModel::kNullItem) {
            return TreeModel::kNullItem;
        }
        RECT rcItem;
        if (treeView->GetItemRect(ti, true, rcItem)) {
            // rcItem is local to window, map to global screen position
            MapWindowPoints(hwnd, HWND_DESKTOP, (POINT*)&rcItem, 2);
            pt.x = rcItem.left;
            pt.y = rcItem.bottom;
        }
    } else {
        ti = treeView->GetItemAt(pt.x, pt.y);
        if (ti == TreeModel::kNullItem) {
            // only show context menu if over a node in tree
            return TreeModel::kNullItem;
        }
        // context menu acts on this item so select it
        // for better visual feedback to the user
        treeView->SelectItem(ti);
        pt.x = args->mouseScreen.x;
        pt.y = args->mouseScreen.y;
    }
    return ti;
}

LRESULT TreeView::OnNotifyReflect(WPARAM wp, LPARAM lp) {
    TreeView* w = this;
    NMTREEVIEWW* nmtv = (NMTREEVIEWW*)(lp);
    LRESULT res;

    auto code = nmtv->hdr.code;
    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-getinfotip
    if (code == TVN_GETINFOTIP) {
        if (!onGetTooltip.IsValid()) {
            return 0;
        }
        TreeView::GetTooltipEvent ev;
        ev.treeView = w;
        ev.info = (NMTVGETINFOTIPW*)(nmtv);
        ev.treeItem = GetTreeItemByHandle(ev.info->hItem);
        onGetTooltip.Call(&ev);
        return 0;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-customdraw-tree-view
    if (code == NM_CUSTOMDRAW) {
        if (!onCustomDraw.IsValid()) {
            return CDRF_DODEFAULT;
        }
        TreeView::CustomDrawEvent ev;
        ev.treeView = w;
        ev.nm = (NMTVCUSTOMDRAW*)lp;
        HTREEITEM hItem = (HTREEITEM)ev.nm->nmcd.dwItemSpec;
        // it can be 0 in CDDS_PREPAINT state
        ev.treeItem = GetTreeItemByHandle(hItem);
        // TODO: seeing this in crash reports because GetTVITEM() returns nullptr
        // should log more info
        // SubmitBugReportIf(!a.treeItem);
        if (!ev.treeItem) {
            return CDRF_DODEFAULT;
        }
        onCustomDraw.Call(&ev);
        res = ev.result;
        if (res < 0) {
            return CDRF_DODEFAULT;
        }
        return res;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-selchanged
    if (code == TVN_SELCHANGED) {
        // log("tv: TVN_SELCHANGED\n");
        if (!onSelectionChanged.IsValid()) {
            return 0;
        }
        TreeView::SelectionChangedEvent ev;
        ev.treeView = w;
        ev.nmtv = nmtv;
        auto action = ev.nmtv->action;
        if (action == TVC_BYKEYBOARD) {
            ev.byKeyboard = true;
        } else if (action == TVC_BYMOUSE) {
            ev.byMouse = true;
        }
        ev.prevSelectedItem = w->GetTreeItemByHandle(nmtv->itemOld.hItem);
        ev.selectedItem = w->GetTreeItemByHandle(nmtv->itemNew.hItem);
        onSelectionChanged.Call(&ev);
        return 0;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-click-tree-view
    if (code == NM_CLICK || code == NM_DBLCLK) {
        // log("tv: NM_CLICK\n");
        if (!onClick.IsValid()) {
            return 0;
        }
        NMHDR* nmhdr = (NMHDR*)lp;
        TreeView::ClickEvent ev{};
        ev.treeView = w;
        ev.isDblClick = (code == NM_DBLCLK);

        DWORD pos = GetMessagePos();
        ev.mouseScreen.x = GET_X_LPARAM(pos);
        ev.mouseScreen.y = GET_Y_LPARAM(pos);
        POINT pt = ToPOINT(ev.mouseScreen);
        if (pt.x != -1) {
            MapWindowPoints(HWND_DESKTOP, nmhdr->hwndFrom, &pt, 1);
        }
        ev.mouseWindow.x = pt.x;
        ev.mouseWindow.y = pt.y;

        // determine which item has been clicked (if any)
        TVHITTESTINFO ht{};
        ht.pt.x = ev.mouseWindow.x;
        ht.pt.y = ev.mouseWindow.y;
        TreeView_HitTest(nmhdr->hwndFrom, &ht);
        if ((ht.flags & TVHT_ONITEM)) {
            ev.treeItem = GetTreeItemByHandle(ht.hItem);
        }
        onClick.Call(&ev);
        return ev.result;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-keydown
    if (code == TVN_KEYDOWN) {
        if (!onKeyDown.IsValid()) {
            return 0;
        }
        NMTVKEYDOWN* nmkd = (NMTVKEYDOWN*)nmtv;
        TreeView::KeyDownEvent ev{};
        ev.treeView = w;
        ev.nmkd = nmkd;
        ev.keyCode = nmkd->wVKey;
        ev.flags = nmkd->flags;
        onKeyDown.Call(&ev);
        return 0;
    }

    return 0;
}
