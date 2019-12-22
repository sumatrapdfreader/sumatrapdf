/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
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

constexpr UINT_PTR SUBCLASS_ID = 1;

static void Unsubclass(TreeCtrl* w);

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

static bool GetItem(HWND hwnd, HTREEITEM hItem, TVITEMW* item) {
    ZeroStruct(item);
    item->hItem = hItem;
    item->mask = TVIF_PARAM | TVIF_STATE;
    UINT sm = TVIS_SELECTED | TVIS_CUT | TVIS_DROPHILITED | TVIS_BOLD | TVIS_EXPANDED;
    item->stateMask = sm;
    BOOL ok = TreeView_GetItem(hwnd, item);
    return !!ok;
}

#include "utils/BitManip.h"

// expand if collapse, collapse if expanded
static void TreeViewToggle(HWND hTree, HTREEITEM hItem, bool recursive) {
    HTREEITEM child = TreeView_GetChild(hTree, hItem);
    if (!child) {
        // only applies to nodes with children
        return;
    }

    TVITEMW item;
    ::GetItem(hTree, hItem, &item);
    UINT flag = TVE_EXPAND;
    bool isExpanded = bitmask::IsSet(item.state, TVIS_EXPANDED);
    if (isExpanded) {
        flag = TVE_COLLAPSE;
    }
    if (recursive) {
        TreeViewExpandRecursively(hTree, hItem, flag, false);
    } else {
        TreeView_Expand(hTree, hItem, flag);
    }
}

static LRESULT CALLBACK TreeParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    CrashIf(subclassId != SUBCLASS_ID); // this proc is only used in one subclass
    auto* w = (TreeCtrl*)data;
    CrashIf(GetParent(w->hwnd) != (HWND)hwnd);
    if (msg == WM_NOTIFY) {
        NMTREEVIEWW* nm = reinterpret_cast<NMTREEVIEWW*>(lp);
        if (w->onTreeNotify) {
            bool handled = true;
            LRESULT res = w->onTreeNotify(nm, handled);
            if (handled) {
                return res;
            }
        }
        auto code = nm->hdr.code;
        if (code == TVN_GETINFOTIP) {
            if (w->onGetTooltip) {
                auto* arg = reinterpret_cast<NMTVGETINFOTIPW*>(nm);
                w->onGetTooltip(arg);
                return 0;
            }
        }
    }
    if (msg == WM_CONTEXTMENU) {
        if (w->onContextMenu) {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            w->onContextMenu(hwnd, x, y);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static bool HandleKey(HWND hwnd, WPARAM wp) {
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
        TreeViewToggle(hwnd, TreeView_GetSelection(hwnd), recursive);
    } else {
        return false;
    }
    TreeView_EnsureVisible(hwnd, TreeView_GetSelection(hwnd));
    return true;
}

static LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    auto* w = (TreeCtrl*)dwRefData;
    CrashIf(w->hwnd != (HWND)hwnd);

    if (w->preFilter) {
        bool discard = false;
        auto res = w->preFilter(hwnd, msg, wp, lp, discard);
        if (discard) {
            return res;
        }
    }

    if (WM_ERASEBKGND == msg) {
        return FALSE;
    }

    if (WM_KEYDOWN == msg) {
        if (HandleKey(hwnd, wp)) {
            return 0;
        }
    }

    if (WM_NCDESTROY == msg) {
        Unsubclass(w);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Subclass(TreeCtrl* w) {
    BOOL ok = SetWindowSubclass(w->hwnd, TreeProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndSubclassId = SUBCLASS_ID;

    ok = SetWindowSubclass(w->parent, TreeParentProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndParentSubclassId = SUBCLASS_ID;
}

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

TreeCtrl::TreeCtrl(HWND p, RECT* initialPosition) {
    parent = p;
    if (initialPosition) {
        initialPos = *initialPosition;
    } else {
        SetRect(&initialPos, 0, 0, 120, 28);
    }
}

bool TreeCtrl::Create(const WCHAR* title) {
    if (!title) {
        title = L"";
    }

    RECT rc = initialPos;
    HMODULE hmod = GetModuleHandleW(nullptr);
    hwnd = CreateWindowExW(this->dwExStyle, WC_TREEVIEWW, title, this->dwStyle, rc.left, rc.top, RectDx(rc), RectDy(rc),
                           this->parent, this->menu, hmod, nullptr);
    if (!hwnd) {
        return false;
    }
    TreeView_SetUnicodeFormat(this->hwnd, true);
    this->SetFont(GetDefaultGuiFont());

    // TVS_CHECKBOXES has to be set with SetWindowLong before populating with data
    // https: // docs.microsoft.com/en-us/windows/win32/controls/tree-view-control-window-styles
    if (withCheckboxes) {
        ToggleWindowStyle(hwnd, TVS_CHECKBOXES, true);
    }
    Subclass(this);

    return true;
}

TVITEMW* TreeCtrl::GetItem(HTREEITEM hItem) {
    bool ok = ::GetItem(this->hwnd, hItem, &this->item);
    if (!ok) {
        return nullptr;
    }
    return &this->item;
    return &this->item;
}

bool TreeCtrl::IsExpanded(HTREEITEM hItem) {
    auto* item = GetItem(hItem);
    if (item) {
        return (item->state & TVIS_EXPANDED) != 0;
    }
    return false;
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
    HTREEITEM res = TreeView_GetSelection(this->hwnd);
    return res;
}

bool TreeCtrl::SelectItem(HTREEITEM item) {
    BOOL ok = TreeView_SelectItem(this->hwnd, item);
    return (ok == TRUE);
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
    Unsubclass(this);
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

// returns false if we should stop iteration
// TODO: convert to non-recursive version by storing nodes to visit in std::deque
static bool VisitTreeNodesRec(HWND hwnd, HTREEITEM hItem, const TreeItemVisitor& visitor) {
    while (hItem) {
        TVITEMW item = {0};
        item.hItem = hItem;
        item.mask = TVIF_PARAM | TVIF_STATE;
        item.stateMask = TVIS_EXPANDED;
        BOOL ok = TreeView_GetItem(hwnd, &item);
        if (!ok) {
            // we failed to get the node, but we don't want to stop the traversal
            return true;
        }
        bool shouldContinue = visitor(&item);
        if (!shouldContinue) {
            // visitor asked to stop
            return false;
        }

        if ((item.state & TVIS_EXPANDED)) {
            HTREEITEM child = TreeView_GetChild(hwnd, hItem);
            VisitTreeNodesRec(hwnd, child, visitor);
        }

        hItem = TreeView_GetNextSibling(hwnd, hItem);
    }
    return true;
}

void TreeCtrl::VisitNodes(const TreeItemVisitor& visitor) {
    HTREEITEM hRoot = TreeView_GetRoot(this->hwnd);
    VisitTreeNodesRec(this->hwnd, hRoot, visitor);
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
    TV_INSERTSTRUCT toInsert{};
    toInsert.hParent = parent;
    toInsert.hInsertAfter = TVI_LAST;
    toInsert.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
    UINT state = 0;
    if (item->IsExpanded()) {
        state = TVIS_EXPANDED;
    }
    toInsert.itemex.state = state;
    toInsert.itemex.stateMask = TVIS_EXPANDED;
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
