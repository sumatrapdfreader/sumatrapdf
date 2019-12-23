/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/TreeCtrl.h"

/*
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

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree) {
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
static TVITEMW* GetItem(TreeCtrl* tree, HTREEITEM hItem) {
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

#include "utils/BitManip.h"

// expand if collapse, collapse if expanded
static void TreeViewToggle(TreeCtrl* tree, HTREEITEM hItem, bool recursive) {
    HWND hTree = tree->hwnd;
    HTREEITEM child = TreeView_GetChild(hTree, hItem);
    if (!child) {
        // only applies to nodes with children
        return;
    }

    TVITEMW* item = GetItem(tree, hItem);
    if (!item) {
        return;
    }
    UINT flag = TVE_EXPAND;
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

void TreeCtrl::WndProcParent(WndProcArgs* args) {
    auto* w = (TreeCtrl*)this;
    HWND hwnd = args->hwnd;
    UINT msg = args->msg;
    WPARAM wp = args->wparam;
    LPARAM lp = args->lparam;

    CrashIf(GetParent(w->hwnd) != (HWND)hwnd);

    if (msg == WM_NOTIFY) {
        NMTREEVIEWW* nm = reinterpret_cast<NMTREEVIEWW*>(lp);
        if (w->onTreeNotify) {
            bool didHandle = false;
            LRESULT res = w->onTreeNotify(nm, didHandle);
            if (didHandle) {
                args->didHandle = true;
                args->result = res;
                return;
            }
        }

        auto code = nm->hdr.code;
        if (code == TVN_GETINFOTIP) {
            if (w->onGetTooltip) {
                auto* arg = reinterpret_cast<NMTVGETINFOTIPW*>(nm);
                w->onGetTooltip(arg);
                args->didHandle = true;
                args->result = 0;
                return;
            }
        }
    }

    if (msg == WM_CONTEXTMENU) {
        if (w->onContextMenu) {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            w->onContextMenu(hwnd, x, y);
            args->didHandle = 0;
            return;
        }
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
            if (!TreeView_GetNextSibling(hwnd, root))
                root = TreeView_GetChild(hwnd, root);
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

void TreeCtrl::WndProc(WndProcArgs* args) {
    HWND hwnd = args->hwnd;
    UINT msg = args->msg;
    WPARAM wp = args->wparam;

    TreeCtrl* w = this;
    CrashIf(w->hwnd != (HWND)hwnd);

    if (w->msgFilter) {
        w->msgFilter(args);
        if (args->didHandle) {
            return;
        }
    }

    if (WM_ERASEBKGND == msg) {
        args->didHandle = true;
        args->result = FALSE;
        return;
    }

    if (WM_KEYDOWN == msg) {
        if (HandleKey(w, wp)) {
            args->didHandle = true;
            return;
        }
    }
}

#if 0
static void Unsubclass(TreeCtrl* w) {
    if (!w) {
        return;
    }

    if (w->hwndSubclassId != 0) {
        BOOL ok = RemoveWindowSubclass(w->hwnd, TreeProc, SUBCLASS_ID);
        CrashIf(false && !ok);
        w->hwndSubclassId = 0;
    }

    if (w->hwndParentSubclassId != 0) {
        BOOL ok = RemoveWindowSubclass(w->parent, TreeParentProc, SUBCLASS_ID);
        CrashIf(false && !ok);
        w->hwndParentSubclassId = 0;
    }
}
#endif

TreeCtrl::TreeCtrl(HWND p) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
              TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;
    dwExStyle = 0;
    winClass = WC_TREEVIEWW;
    parent = p;
    SetRect(&initialPos, 0, 0, 48, 120);
}

bool TreeCtrl::Create(const WCHAR* title) {
    if (!title) {
        title = L"";
    }

    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    Subclass();
    SubclassParent();

    TreeView_SetUnicodeFormat(hwnd, true);

    // TVS_CHECKBOXES has to be set with SetWindowLong before populating with data
    // https: // docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles
    if (withCheckboxes) {
        ToggleWindowStyle(hwnd, TVS_CHECKBOXES, true);
    }

    return true;
}

TVITEMW* TreeCtrl::GetItem(TreeItem* ti) {
    auto hi = GetHandleByTreeItem(ti);
    return ::GetItem(this, hi);
}

bool TreeCtrl::IsExpanded(TreeItem* ti) {
    auto state = GetItemState(ti);
    return state.isExpanded;
}

bool TreeCtrl::GetItemRect(HTREEITEM item, bool fItemRect, RECT& r) {
    BOOL ok = TreeView_GetItemRect(this->hwnd, item, &r, (BOOL)fItemRect);
    return fromBOOL(ok);
}

HTREEITEM TreeCtrl::GetRoot() {
    HTREEITEM res = TreeView_GetRoot(this->hwnd);
    return res;
}

HTREEITEM TreeCtrl::GetChild(HTREEITEM item) {
    HTREEITEM res = TreeView_GetChild(this->hwnd, item);
    return res;
}

HTREEITEM TreeCtrl::GetSiblingNext(HTREEITEM item) {
    HTREEITEM res = TreeView_GetNextSibling(this->hwnd, item);
    return res;
}

HTREEITEM TreeCtrl::GetSelection() {
    HTREEITEM res = TreeView_GetSelection(hwnd);
    return res;
}

bool TreeCtrl::SelectItem(TreeItem* ti) {
    auto hi = GetHandleByTreeItem(ti);
    BOOL ok = TreeView_SelectItem(hwnd, hi);
    return ok == TRUE;
}

HTREEITEM TreeCtrl::InsertItem(TVINSERTSTRUCTW* item) {
    HTREEITEM res = TreeView_InsertItem(this->hwnd, item);
    return res;
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
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_EXPAND, false);
}

void TreeCtrl::CollapseAll() {
    auto root = TreeView_GetRoot(this->hwnd);
    TreeViewExpandRecursively(this->hwnd, root, TVE_COLLAPSE, false);
}

void TreeCtrl::Clear() {
    treeModel = nullptr;
    insertedItems.clear();

    HWND hwnd = this->hwnd;
    ::SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    UINT flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    ::RedrawWindow(hwnd, nullptr, nullptr, flags);
}

TreeCtrl::~TreeCtrl() {
    // DeleteObject(w->bgBrush);
}

void TreeCtrl::SetFont(HFONT f) {
    SetWindowFont(this->hwnd, f, TRUE);
}

HFONT TreeCtrl::GetFont() {
    return GetWindowFont(this->hwnd);
}

void TreeCtrl::SuspendRedraw() {
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
}

void TreeCtrl::ResumeRedraw() {
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
}

str::WStr TreeCtrl::GetTooltip(HTREEITEM hItem) {
    WCHAR buf[INFOTIPSIZE + 1] = {}; // +1 just in case

    TVITEMW item = {0};
    item.hItem = hItem;
    item.mask = TVIF_TEXT;
    item.pszText = buf;
    item.cchTextMax = dimof(buf);
    TreeView_GetItem(hwnd, &item);

    return str::WStr(buf);
}

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

static HTREEITEM InsertItem(TreeCtrl* tree, HTREEITEM parent, TreeItem* item) {
    TVINSERTSTRUCTW toInsert{};
    UINT mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;

    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_LAST;
    toInsert.itemex.mask = mask;

    UINT stateMask = TVIS_EXPANDED;
    UINT state = 0;
    if (item->IsExpanded()) {
        state = TVIS_EXPANDED;
    }

    if (tree->withCheckboxes) {
        stateMask |= TVIS_STATEIMAGEMASK;
        bool isChecked = item->IsChecked();
        UINT imgIdx = isChecked ? 2 : 1;
        UINT imgState = INDEXTOSTATEIMAGEMASK(imgIdx);
        state |= imgState;
    }

    toInsert.itemex.state = state;
    toInsert.itemex.stateMask = stateMask;
    toInsert.itemex.lParam = reinterpret_cast<LPARAM>(item);
    auto title = item->Text();
    toInsert.itemex.pszText = title;
    return tree->InsertItem(&toInsert);
}

static void PopulateTreeItem(TreeCtrl* tree, TreeItem* item, HTREEITEM parent) {
    int n = item->ChildCount();
    for (int i = 0; i < n; i++) {
        auto* ti = item->ChildAt(i);
        HTREEITEM h = InsertItem(tree, parent, ti);
        auto v = std::make_tuple(ti, h);
        tree->insertedItems.push_back(v);
        PopulateTreeItem(tree, ti, h);
    }
}

static void PopulateTree(TreeCtrl* tree, TreeModel* tm) {
    HTREEITEM parent = nullptr;
    int n = tm->RootCount();
    for (int i = 0; i < n; i++) {
        auto* ti = tm->RootAt(i);
        HTREEITEM h = InsertItem(tree, parent, ti);
        auto v = std::make_tuple(ti, h);
        tree->insertedItems.push_back(v);
        PopulateTreeItem(tree, ti, h);
    }
}

void TreeCtrl::SetTreeModel(TreeModel* tm) {
    CrashIf(!tm);

    Clear();
    this->treeModel = tm;
    SuspendRedraw();
    PopulateTree(this, tm);
    ResumeRedraw();
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
    HTREEITEM hi = GetHandleByTreeItem(ti);
    TreeItemState res;
    TVITEMW* item = GetItem(ti);
    CrashIf(!item);

    res.isExpanded = bitmask::IsSet(item->state, TVIS_EXPANDED);
    res.isSelected = bitmask::IsSet(item->state, TVIS_SELECTED);
    res.nChildren = item->cChildren;

    UINT n = (item->state >> 12) - 1;
    res.isChecked = n != 0;
    return res;
}
