/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/BitManip.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

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

Kind kindTree = "treeView";

bool IsTreeKind(Kind k) {
    return k == kindTree;
}

static void DragMove(TreeCtrl* w, int xCur, int yCur) {
    // logf("dragMove(): x: %d, y: %d\n", xCur, yCur);
    // drag the item to the current position of the mouse pointer
    // first convert the dialog coordinates to control coordinates
    POINT pt{xCur, yCur};
    MapWindowPoints(w->parent, w->hwnd, &pt, 1);
    ImageList_DragMove(pt.x, pt.y);

    // turn off the dragged image so the background can be refreshed.
    ImageList_DragShowNolock(FALSE);

    // find out if the pointer is on the item. If it is,
    // highlight the item as a drop target.
    TVHITTESTINFO tvht{};
    tvht.pt.x = pt.x;
    tvht.pt.y = pt.y;
    HTREEITEM htiTarget = TreeView_HitTest(w->hwnd, &tvht);

    if (htiTarget != nullptr) {
        // TODO: don't know which is better
        SendMessageW(w->hwnd, TVM_SELECTITEM, TVGN_DROPHILITE, (LPARAM)htiTarget);
        // TreeView_SelectDropTarget(hwnd, htiTarget);
    }
    ImageList_DragShowNolock(TRUE);
}

static void DragEnd(TreeCtrl* w) {
    HTREEITEM htiDest = TreeView_GetDropHilight(w->hwnd);
    if (htiDest != nullptr) {
        w->dragTargetItem = w->GetTreeItemByHandle(htiDest);
        // logf("finished dragging 0x%p on 0x%p\n", draggedItem, dragTargetItem);
        TreeItemDraggeddEvent ev;
        ev.treeCtrl = w;
        ev.draggedItem = w->draggedItem;
        ev.dragTargetItem = w->dragTargetItem;
        ev.isStart = false;
        w->onTreeItemDragStartEnd(&ev);
    }
    ImageList_EndDrag();
    TreeView_SelectDropTarget(w->hwnd, nullptr);
    ReleaseCapture();
    SetCursorCached(IDC_ARROW);
    // ShowCursor(TRUE);
    w->isDragging = false;
    w->draggedItem = nullptr;
    w->dragTargetItem = nullptr;
    HWND hwndParent = GetParent(w->hwnd);
    UnregisterHandlerForMessage(hwndParent, WM_MOUSEMOVE);
    UnregisterHandlerForMessage(hwndParent, WM_LBUTTONUP);
}

static void DispatchMouseDuringDrag(void* user, WndEvent* ev) {
    auto w = (TreeCtrl*)user;
    ev->w = w;
    uint msg = ev->msg;

    CrashIf(!w->isDragging);

    if (msg == WM_MOUSEMOVE) {
        if (!w->isDragging) {
            return;
        }
        int x = GET_X_LPARAM(ev->lp);
        int y = GET_Y_LPARAM(ev->lp);
        DragMove(w, x, y);
        ev->didHandle = true;
        return;
    }

    if (msg == WM_LBUTTONUP) {
        if (!w->isDragging) {
            return;
        }
        DragEnd(w);
        ev->didHandle = true;
        return;
    }

    CrashIf(true);
}

// https://docs.microsoft.com/en-us/windows/win32/controls/drag-a-tree-view-item
static void DragStart(TreeCtrl* w, NMTREEVIEWW* nmtv) {
    // need to intercept mouse messages in the parent window during dragging
    HWND hwndParent = GetParent(w->hwnd);
    void* user = w;
    RegisterHandlerForMessage(hwndParent, WM_MOUSEMOVE, DispatchMouseDuringDrag, user);
    RegisterHandlerForMessage(hwndParent, WM_LBUTTONUP, DispatchMouseDuringDrag, user);

    HTREEITEM hitem = nmtv->itemNew.hItem;
    w->draggedItem = w->GetTreeItemByHandle(hitem);
    HIMAGELIST himl = TreeView_CreateDragImage(w->hwnd, hitem);

    ImageList_BeginDrag(himl, 0, 0, 0);
    BOOL ok = ImageList_DragEnter(w->hwnd, nmtv->ptDrag.x, nmtv->ptDrag.x);
    CrashIf(!ok);

    if (w->onTreeItemDragStartEnd) {
        TreeItemDraggeddEvent ev;
        ev.treeCtrl = w;
        ev.draggedItem = w->draggedItem;
        ev.isStart = true;
        w->onTreeItemDragStartEnd(&ev);
    }

    // ShowCursor(FALSE);
    SetCursorCached(IDC_HAND);
    SetCapture(w->parent);
    w->isDragging = true;
}

static void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, uint flag, bool subtree) {
    while (hItem) {
        TreeView_Expand(hTree, hItem, flag);
        HTREEITEM child = TreeView_GetChild(hTree, hItem);
        if (child) {
            TreeViewExpandRecursively(hTree, child, flag, false);
        }
        if (subtree) {
            break;
        }
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

// the result only valid until the next GetItem call
static TVITEMW* GetTVITEM(TreeCtrl* tree, HTREEITEM hItem) {
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

static TVITEMW* GetTVITEM(TreeCtrl* tree, TreeItem* ti) {
    HTREEITEM hi = tree->GetHandleByTreeItem(ti);
    return GetTVITEM(tree, hi);
}

// expand if collapse, collapse if expanded
static void TreeViewToggle(TreeCtrl* tree, HTREEITEM hItem, bool recursive) {
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

static void SetTreeItemState(uint uState, TreeItemState& state) {
    state.isExpanded = bitmask::IsSet(uState, TVIS_EXPANDED);
    state.isSelected = bitmask::IsSet(uState, TVIS_SELECTED);
    uint n = (uState >> 12) - 1;
    state.isChecked = n != 0;
}

static void Handle_WM_NOTIFY(void* user, WndEvent* ev) {
    uint msg = ev->msg;

    CrashIf(msg != WM_NOTIFY);

    TreeCtrl* w = (TreeCtrl*)user;
    CrashIf(GetParent(w->hwnd) != (HWND)ev->hwnd);

    ev->w = w;
    LPARAM lp = ev->lp;
    NMTREEVIEWW* nmtv = (NMTREEVIEWW*)(lp);

    if (w->onNotify) {
        WmNotifyEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeView = nmtv;

        w->onNotify(&a);
        if (a.didHandle) {
            return;
        }
    }

    auto code = nmtv->hdr.code;

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-getinfotip
    if (code == TVN_GETINFOTIP) {
        if (!w->onGetTooltip) {
            return;
        }
        TreeItmGetTooltipEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.info = (NMTVGETINFOTIPW*)(nmtv);
        a.treeItem = w->GetTreeItemByHandle(a.info->hItem);
        w->onGetTooltip(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-customdraw-tree-view
    if (code == NM_CUSTOMDRAW) {
        if (!w->onTreeItemCustomDraw) {
            return;
        }
        TreeItemCustomDrawEvent a;
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.nm = (NMTVCUSTOMDRAW*)lp;
        HTREEITEM hItem = (HTREEITEM)a.nm->nmcd.dwItemSpec;
        // it can be 0 in CDDS_PREPAINT state
        if (hItem) {
            a.treeItem = w->GetTreeItemByHandle(hItem);
            // TODO: log more info
            SubmitCrashIf(!a.treeItem);
            if (!a.treeItem) {
                return;
            }
        }
        w->onTreeItemCustomDraw(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-selchanged
    if (code == TVN_SELCHANGED) {
        if (!w->onTreeSelectionChanged) {
            return;
        }
        TreeSelectionChangedEvent a;
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.nmtv = nmtv;
        auto action = a.nmtv->action;
        if (action == TVC_BYKEYBOARD) {
            a.byKeyboard = true;
        } else if (action == TVC_BYMOUSE) {
            a.byMouse = true;
        }
        a.prevSelectedItem = w->GetTreeItemByHandle(nmtv->itemOld.hItem);
        a.selectedItem = w->GetTreeItemByHandle(nmtv->itemNew.hItem);
        w->onTreeSelectionChanged(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-itemchanged
    if (code == TVN_ITEMCHANGED) {
        if (!w->onTreeItemChanged) {
            return;
        }
        TreeItemChangedEvent a;
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.nmic = (NMTVITEMCHANGE*)lp;
        a.treeItem = w->GetTreeItemByHandle(a.nmic->hItem);
        SetTreeItemState(a.nmic->uStateOld, a.prevState);
        SetTreeItemState(a.nmic->uStateNew, a.newState);
        a.expandedChanged = (a.prevState.isExpanded != a.newState.isExpanded);
        a.checkedChanged = (a.prevState.isChecked != a.newState.isChecked);
        a.selectedChanged = (a.prevState.isSelected != a.newState.isSelected);
        w->onTreeItemChanged(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-itemexpanded
    if (code == TVN_ITEMEXPANDED) {
        if (!w->onTreeItemExpanded) {
            return;
        }
        bool doNotify = false;
        bool isExpanded = false;
        if (nmtv->action == TVE_COLLAPSE) {
            isExpanded = false;
            doNotify = true;
        } else if (nmtv->action == TVE_EXPAND) {
            isExpanded = true;
            doNotify = true;
        }
        if (!doNotify) {
            return;
        }
        TreeItemExpandedEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.treeItem = w->GetTreeItemByHandle(nmtv->itemNew.hItem);
        a.isExpanded = isExpanded;
        w->onTreeItemExpanded(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-click-tree-view
    if (code == NM_CLICK || code == NM_DBLCLK) {
        if (!w->onTreeClick) {
            return;
        }
        NMHDR* nmhdr = (NMHDR*)lp;
        TreeClickEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.isDblClick = (code == NM_DBLCLK);

        DWORD pos = GetMessagePos();
        a.mouseGlobal.x = GET_X_LPARAM(pos);
        a.mouseGlobal.y = GET_Y_LPARAM(pos);
        POINT pt{a.mouseGlobal.x, a.mouseGlobal.y};
        if (pt.x != -1) {
            MapWindowPoints(HWND_DESKTOP, nmhdr->hwndFrom, &pt, 1);
        }
        a.mouseWindow.x = pt.x;
        a.mouseWindow.y = pt.y;

        // determine which item has been clicked (if any)
        TVHITTESTINFO ht = {0};
        ht.pt.x = a.mouseWindow.x;
        ht.pt.y = a.mouseWindow.y;
        TreeView_HitTest(nmhdr->hwndFrom, &ht);
        if ((ht.flags & TVHT_ONITEM)) {
            a.treeItem = w->GetTreeItemByHandle(ht.hItem);
        }
        w->onTreeClick(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-keydown
    if (code == TVN_KEYDOWN) {
        if (!w->onTreeKeyDown) {
            return;
        }
        NMTVKEYDOWN* nmkd = (NMTVKEYDOWN*)nmtv;
        TreeKeyDownEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.nmkd = nmkd;
        a.keyCode = nmkd->wVKey;
        a.flags = nmkd->flags;
        w->onTreeKeyDown(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/tvn-getdispinfo
    if (code == TVN_GETDISPINFO) {
        if (!w->onTreeGetDispInfo) {
            return;
        }
        TreeGetDispInfoEvent a{};
        CopyWndEvent cp(&a, ev);
        a.treeCtrl = w;
        a.dispInfo = (NMTVDISPINFOEXW*)lp;
        a.treeItem = w->GetTreeItemByHandle(a.dispInfo->item.hItem);
        w->onTreeGetDispInfo(&a);
        return;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/drag-a-tree-view-item
    if (code == TVN_BEGINDRAG) {
        // we don't do dragging if not asked for drag end notification
        if (!w->onTreeItemDragStartEnd) {
            return;
        }
        DragStart(w, (NMTREEVIEWW*)lp);
        ev->didHandle = true;
        return;
    }
}

static bool HandleKey(TreeCtrl* tree, WPARAM wp) {
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

void TreeCtrl::WndProc(WndEvent* ev) {
    HWND hwnd = ev->hwnd;
    UINT msg = ev->msg;
    WPARAM wp = ev->wp;
    LPARAM lp = ev->lp;

    // DbgLogMsg("tree:", hwnd, msg, wp, ev->lp);

    TreeCtrl* w = this;
    CrashIf(w->hwnd != (HWND)hwnd);

    if (WM_RBUTTONDOWN == msg) {
        DefWindowProc(hwnd, msg, wp, lp);
        return;
    }

    if (WM_CONTEXTMENU == msg && onContextMenu) {
        WindowBase* wb = (WindowBase*)this;
        Handle_WM_CONTEXTMENU(wb, ev);
        return;
    }

    if (w->msgFilter) {
        w->msgFilter(ev);
        if (ev->didHandle) {
            return;
        }
    }

    if (WM_ERASEBKGND == msg) {
        ev->didHandle = true;
        ev->result = FALSE;
        return;
    }

    if (WM_KEYDOWN == msg) {
        if (HandleKey(w, wp)) {
            ev->didHandle = true;
            return;
        }
    }
}

TreeCtrl::TreeCtrl(HWND p) : WindowBase(p) {
    kind = kindTree;
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    dwStyle |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
    dwStyle |= TVS_TRACKSELECT | TVS_NOHSCROLL | TVS_INFOTIP;
    dwExStyle = TVS_EX_DOUBLEBUFFER;
    winClass = WC_TREEVIEWW;
    parent = p;
    initialSize = {48, 120};
}

bool TreeCtrl::Create() {
    if (!supportDragDrop) {
        dwStyle |= TVS_DISABLEDRAGDROP;
    }

    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }

    if (supportDragDrop) {
        // we need image list to create drag image in DragStart()
        HIMAGELIST himl = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 1);
        CrashIf(!himl);
        TreeView_SetImageList(hwnd, himl, TVSIL_NORMAL);
    }

    if (IsVistaOrGreater()) {
        SendMessageW(hwnd, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    }
    if (DynSetWindowTheme) {
        DynSetWindowTheme(hwnd, L"Explorer", nullptr);
    }

    TreeView_SetUnicodeFormat(hwnd, true);

    // TVS_CHECKBOXES has to be set with SetWindowLong before populating with data
    // https: // docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles
    if (withCheckboxes) {
        SetWindowStyle(hwnd, TVS_CHECKBOXES, true);
    }

    SetToolTipsDelayTime(TTDT_AUTOPOP, 32767);

    // must be done at the end. Doing  SetWindowStyle() sends bogus (?)
    // TVN_ITEMCHANGED notification. As an alternative we could ignore TVN_ITEMCHANGED
    // if hItem doesn't point to an TreeItem
    Subclass();

    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_NOTIFY, Handle_WM_NOTIFY, user);

    return true;
}

bool TreeCtrl::IsExpanded(TreeItem* ti) {
    auto state = GetItemState(ti);
    return state.isExpanded;
}

// https://docs.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-treeview_getitemrect
bool TreeCtrl::GetItemRect(TreeItem* ti, bool justText, RECT& r) {
    HTREEITEM hi = GetHandleByTreeItem(ti);
    BOOL b = toBOOL(justText);
    BOOL ok = TreeView_GetItemRect(hwnd, hi, &r, b);
    return ok == TRUE;
}

TreeItem* TreeCtrl::GetSelection() {
    HTREEITEM hi = TreeView_GetSelection(hwnd);
    return GetTreeItemByHandle(hi);
}

bool TreeCtrl::SelectItem(TreeItem* ti) {
    HTREEITEM hi{nullptr};
    if (ti != nullptr) {
        hi = GetHandleByTreeItem(ti);
    }
    BOOL ok = TreeView_SelectItem(hwnd, hi);
    return ok == TRUE;
}

void TreeCtrl::SetBackgroundColor(COLORREF bgCol) {
    this->backgroundColor = bgCol;
    TreeView_SetBkColor(this->hwnd, bgCol);
}

void TreeCtrl::SetTextColor(COLORREF col) {
    this->textColor = col;
    TreeView_SetTextColor(this->hwnd, col);
}

void TreeCtrl::ExpandAll() {
    SuspendRedraw();
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_EXPAND, false);
    ResumeRedraw();
}

void TreeCtrl::CollapseAll() {
    SuspendRedraw();
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_COLLAPSE, false);
    ResumeRedraw();
}

void TreeCtrl::Clear() {
    treeModel = nullptr;
    insertedItems.Reset();

    HWND hwnd = this->hwnd;
    ::SendMessageW(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessageW(hwnd, WM_SETREDRAW, TRUE, 0);
    uint flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    ::RedrawWindow(hwnd, nullptr, nullptr, flags);
}

TreeCtrl::~TreeCtrl() {
    // DeleteObject(w->bgBrush);
}

str::WStr TreeCtrl::GetDefaultTooltip(TreeItem* ti) {
    auto hItem = GetHandleByTreeItem(ti);
    WCHAR buf[INFOTIPSIZE + 1] = {}; // +1 just in case

    TVITEMW item = {0};
    item.hItem = hItem;
    item.mask = TVIF_TEXT;
    item.pszText = buf;
    item.cchTextMax = dimof(buf);
    TreeView_GetItem(hwnd, &item);

    return str::WStr(buf);
}

// get the item at a given (x,y) position in the window
TreeItem* TreeCtrl::GetItemAt(int x, int y) {
    if (x < 0 || y < 0) {
        return nullptr;
    }
    TVHITTESTINFO ht{};
    ht.pt.x = x;
    ht.pt.y = y;

    TreeView_HitTest(hwnd, &ht);
    if ((ht.flags & TVHT_ONITEM) == 0) {
        return nullptr;
    }
    return GetTreeItemByHandle(ht.hItem);
}

// TODO: speed up by storing the pointer as TVINSERTSTRUCTW.lParam
HTREEITEM TreeCtrl::GetHandleByTreeItem(TreeItem* item) {
    for (auto t : this->insertedItems) {
        auto* i = std::get<0>(t);
        if (i == item) {
            return std::get<1>(t);
        }
    }
    return nullptr;
}

TreeItem* TreeCtrl::GetTreeItemByHandle(HTREEITEM item) {
    for (auto t : this->insertedItems) {
        auto* i = std::get<1>(t);
        if (i == item) {
            return std::get<0>(t);
        }
    }
    return nullptr;
}

void FillTVITEM(TVITEMEXW* tvitem, TreeItem* ti, bool withCheckboxes) {
    uint mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    tvitem->mask = mask;

    uint stateMask = TVIS_EXPANDED;
    uint state = 0;
    if (ti->IsExpanded()) {
        state = TVIS_EXPANDED;
    }

    if (withCheckboxes) {
        stateMask |= TVIS_STATEIMAGEMASK;
        bool isChecked = ti->IsChecked();
        uint imgIdx = isChecked ? 2 : 1;
        uint imgState = INDEXTOSTATEIMAGEMASK(imgIdx);
        state |= imgState;
    }

    tvitem->state = state;
    tvitem->stateMask = stateMask;
    tvitem->lParam = reinterpret_cast<LPARAM>(ti);
    auto title = ti->Text();
    tvitem->pszText = title;
}

static HTREEITEM insertItem(TreeCtrl* tree, HTREEITEM parent, TreeItem* ti) {
    TVINSERTSTRUCTW toInsert{};

    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_LAST;

    TVITEMEXW* tvitem = &toInsert.itemex;
    FillTVITEM(tvitem, ti, tree->withCheckboxes);
    bool onDemand = tree->onTreeGetDispInfo != nullptr;
    if (onDemand) {
        tvitem->pszText = LPSTR_TEXTCALLBACK;
    }
    HTREEITEM res = TreeView_InsertItem(tree->hwnd, &toInsert);
    return res;
}

bool TreeCtrl::UpdateItem(TreeItem* ti) {
    HTREEITEM ht = GetHandleByTreeItem(ti);
    CrashIf(!ht);
    if (!ht) {
        return false;
    }

    TVITEMEXW tvitem;
    tvitem.hItem = ht;
    FillTVITEM(&tvitem, ti, withCheckboxes);
    bool onDemand = onTreeGetDispInfo != nullptr;
    if (onDemand) {
        tvitem.pszText = LPSTR_TEXTCALLBACK;
    }
    BOOL ok = TreeView_SetItem(hwnd, &tvitem);
    return ok ? true : false;
}

static void PopulateTreeItem(TreeCtrl* tree, TreeItem* item, HTREEITEM parent) {
    int n = item->ChildCount();
    for (int i = 0; i < n; i++) {
        auto* ti = item->ChildAt(i);
        HTREEITEM h = insertItem(tree, parent, ti);
        auto v = std::make_tuple(ti, h);
        tree->insertedItems.Append(v);
        PopulateTreeItem(tree, ti, h);
    }
}

static void PopulateTree(TreeCtrl* tree, TreeModel* tm) {
    HTREEITEM parent = nullptr;
    int n = tm->RootCount();
    for (int i = 0; i < n; i++) {
        auto* ti = tm->RootAt(i);
        HTREEITEM h = insertItem(tree, parent, ti);
        auto v = std::make_tuple(ti, h);
        tree->insertedItems.Append(v);
        PopulateTreeItem(tree, ti, h);
    }
}

void TreeCtrl::SetTreeModel(TreeModel* tm) {
    CrashIf(!tm);

    SuspendRedraw();

    insertedItems.Reset();
    TreeView_DeleteAllItems(hwnd);

    treeModel = tm;
    PopulateTree(this, tm);
    ResumeRedraw();

    uint flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwnd, nullptr, nullptr, flags);
}

void TreeCtrl::SetCheckState(TreeItem* item, bool enable) {
    HTREEITEM hi = GetHandleByTreeItem(item);
    CrashIf(!hi);
    TreeView_SetCheckState(hwnd, hi, enable);
}

bool TreeCtrl::GetCheckState(TreeItem* item) {
    HTREEITEM hi = GetHandleByTreeItem(item);
    CrashIf(!hi);
    auto res = TreeView_GetCheckState(hwnd, hi);
    return res != 0;
}

TreeItemState TreeCtrl::GetItemState(TreeItem* ti) {
    TreeItemState res;

    TVITEMW* item = GetTVITEM(this, ti);
    CrashIf(!item);
    if (!item) {
        return res;
    }
    SetTreeItemState(item->state, res);
    res.nChildren = item->cChildren;

    return res;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/tvm-gettooltips
HWND TreeCtrl::GetToolTipsHwnd() {
    return TreeView_GetToolTips(hwnd);
}

void TreeCtrl::SetToolTipsDelayTime(int type, int timeInMs) {
    CrashIf(!IsValidDelayType(type));
    CrashIf(timeInMs < 0);
    CrashIf(timeInMs > 32767); // TODO: or is it 65535?
    HWND hwndToolTips = GetToolTipsHwnd();
    SendMessageW(hwndToolTips, TTM_SETDELAYTIME, type, (LPARAM)timeInMs);
}

Size TreeCtrl::GetIdealSize() {
    return {idealSize.dx, idealSize.dy};
}

// if context menu invoked via keyboard, get selected item
// if via right-click, selects the item under the cursor
// in both cases can return null
// sets pt to screen position (for context menu coordinates)
TreeItem* GetOrSelectTreeItemAtPos(ContextMenuEvent* args, POINT& pt) {
    TreeCtrl* treeCtrl = (TreeCtrl*)args->w;
    HWND hwnd = treeCtrl->hwnd;

    TreeItem* ti = nullptr;
    pt = {args->mouseWindow.x, args->mouseWindow.y};
    if (pt.x == -1 || pt.y == -1) {
        // no mouse position when launched via keyboard shortcut
        // use position of selected item to show menu
        ti = treeCtrl->GetSelection();
        if (!ti) {
            return nullptr;
        }
        RECT rcItem;
        if (treeCtrl->GetItemRect(ti, true, rcItem)) {
            // rcItem is local to window, map to global screen position
            MapWindowPoints(hwnd, HWND_DESKTOP, (POINT*)&rcItem, 2);
            pt.x = rcItem.left;
            pt.y = rcItem.bottom;
        }
    } else {
        ti = treeCtrl->GetItemAt(pt.x, pt.y);
        if (!ti) {
            // only show context menu if over a node in tree
            return nullptr;
        }
        // context menu acts on this item so select it
        // for better visual feedback to the user
        treeCtrl->SelectItem(ti);
        pt.x = args->mouseGlobal.x;
        pt.y = args->mouseGlobal.y;
    }
    return ti;
}
