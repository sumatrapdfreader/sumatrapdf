#include "BaseUtil.h"
#include "TreeCtrl.h"
#include "WinUtil.h"

constexpr UINT_PTR SUBCLASS_ID = 1;

static void Unsubclass(TreeCtrl* w);

void TreeViewExpandRecursively(HWND hTree, HTREEITEM hItem, UINT flag, bool subtree) {
    while (hItem) {
        TreeView_Expand(hTree, hItem, flag);
        HTREEITEM child = TreeView_GetChild(hTree, hItem);
        if (child)
            TreeViewExpandRecursively(hTree, child, flag);
        if (subtree)
            break;
        hItem = TreeView_GetNextSibling(hTree, hItem);
    }
}

static LRESULT CALLBACK TreeParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    UNUSED(dwRefData);
    TreeCtrl* w = (TreeCtrl*)dwRefData;
    CrashIf(GetParent(w->hwnd) != (HWND)hwnd);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static bool HandleKey(HWND hwnd, WPARAM wp) {
    // consistently expand/collapse whole (sub)trees
    if (VK_MULTIPLY == wp && IsShiftPressed()) {
        TreeViewExpandRecursively(hwnd, TreeView_GetRoot(hwnd), TVE_EXPAND);
    } else if (VK_MULTIPLY == wp) {
        TreeViewExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_EXPAND, true);
    } else if (VK_DIVIDE == wp && IsShiftPressed()) {
        HTREEITEM root = TreeView_GetRoot(hwnd);
        if (!TreeView_GetNextSibling(hwnd, root))
            root = TreeView_GetChild(hwnd, root);
        TreeViewExpandRecursively(hwnd, root, TVE_COLLAPSE);
    } else if (VK_DIVIDE == wp) {
        TreeViewExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_COLLAPSE, true);
    } else {
        return false;
    }
    TreeView_EnsureVisible(hwnd, TreeView_GetSelection(hwnd));
    return true;
}

static LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    TreeCtrl* w = (TreeCtrl*)dwRefData;
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

TreeCtrl* AllocTreeCtrl(HWND parent, RECT* initialPosition) {
    auto w = AllocStruct<TreeCtrl>();
    w->parent = parent;
    if (initialPosition) {
        w->initialPos = *initialPosition;
    } else {
        SetRect(&w->initialPos, 0, 0, 120, 28);
    }

    w->dwExStyle = 0;
    w->dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT |
                 TVS_SHOWSELALWAYS | TVS_TRACKSELECT | TVS_DISABLEDRAGDROP | TVS_NOHSCROLL | TVS_INFOTIP;

    return w;
}

bool CreateTreeCtrl(TreeCtrl* w, const WCHAR* title) {
    if (!title) {
        title = L"";
    }

    RECT rc = w->initialPos;
    HMODULE hmod = GetModuleHandleW(nullptr);
    w->hwnd = CreateWindowExW(w->dwExStyle, WC_TREEVIEWW, title, w->dwStyle, rc.left, rc.top, RectDx(rc), RectDy(rc),
                              w->parent, w->menu, hmod, nullptr);

    if (!w->hwnd) {
        return false;
    }
    TreeView_SetUnicodeFormat(w->hwnd, true);
    SetFont(w, GetDefaultGuiFont());
    Subclass(w);

    return true;
}

TVITEM* TreeCtrlGetItem(TreeCtrl* w, HTREEITEM hItem) {
    TVITEM* item = &w->item;
    ZeroStruct(item);
    item->hItem = hItem;
    item->mask = TVIF_PARAM | TVIF_STATE;
    item->stateMask = TVIS_EXPANDED;
    BOOL ok = TreeView_GetItem(w->hwnd, item);
    if (!ok) {
        return nullptr;
    }
    return item;
}

HTREEITEM TreeCtrlGetRoot(TreeCtrl* w) {
    HTREEITEM res = TreeView_GetRoot(w->hwnd);
    return res;
}

HTREEITEM TreeCtrlGetChild(TreeCtrl* w, HTREEITEM item) {
    HTREEITEM res = TreeView_GetChild(w->hwnd, item);
    return res;
}

HTREEITEM TreeCtrlGetNextSibling(TreeCtrl* w, HTREEITEM item) {
    HTREEITEM res = TreeView_GetNextSibling(w->hwnd, item);
    return res;
}

HTREEITEM TreeCtrlGetSelection(TreeCtrl* w) {
    HTREEITEM res = TreeView_GetSelection(w->hwnd);
    return res;
}

bool TreeCtrlSelectItem(TreeCtrl* w, HTREEITEM item) {
    BOOL ok = TreeView_SelectItem(w->hwnd, item);
    return (ok == TRUE);
}

HTREEITEM TreeCtrlInsertItem(TreeCtrl* w, TV_INSERTSTRUCT* item) {
    HTREEITEM res = TreeView_InsertItem(w->hwnd, item);
    return res;
}

void ClearTreeCtrl(TreeCtrl* w) {
    HWND hwnd = w->hwnd;
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void DeleteTreeCtrl(TreeCtrl* w) {
    if (!w)
        return;
    Unsubclass(w);
    // DeleteObject(w->bgBrush);
    free(w);
}

void SetFont(TreeCtrl* w, HFONT f) {
    SetWindowFont(w->hwnd, f, TRUE);
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

void TreeCtrlVisitNodes(TreeCtrl* w, const TreeItemVisitor& visitor) {
    HTREEITEM hRoot = TreeView_GetRoot(w->hwnd);
    VisitTreeNodesRec(w->hwnd, hRoot, visitor);
}

std::wstring_view TreeCtrlGetInfoTip(TreeCtrl* w, HTREEITEM hItem) {
    ZeroArray(w->infotipBuf);
    WCHAR buf[INFOTIPSIZE + 1] = {0};
    TVITEMW item = {0};
    item.hItem = hItem;
    item.mask = TVIF_TEXT;
    item.pszText = buf;
    item.cchTextMax = INFOTIPSIZE;
    TreeView_GetItem(w->hwnd, &item);
    auto res = std::wstring_view(buf);
    return res;
}
