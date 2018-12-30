#include "utils/BaseUtil.h"
#include "TreeCtrl.h"
#include "utils/WinUtil.h"

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

static LRESULT CALLBACK TreeParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    CrashIf(subclassId != SUBCLASS_ID); // this proc is only used in one subclass
    TreeCtrl* w = (TreeCtrl*)data;
    CrashIf(GetParent(w->hwnd) != (HWND)hwnd);
    if (msg == WM_NOTIFY) {
        NMTREEVIEWW* nm = reinterpret_cast<NMTREEVIEWW*>(lp);
        if (w->onTreeNotify) {
            bool handled = true;
            LRESULT res = w->onTreeNotify(w, nm, handled);
            if (handled) {
                return res;
            }
        }
        auto code = nm->hdr.code;
        if (code == TVN_GETINFOTIP) {
            if (w->onGetInfoTip) {
                auto* arg = reinterpret_cast<NMTVGETINFOTIP*>(nm);
                w->onGetInfoTip(w, arg);
                return 0;
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static bool HandleKey(HWND hwnd, WPARAM wp) {
    // consistently expand/collapse whole (sub)trees
    if (VK_MULTIPLY == wp && IsShiftPressed()) {
        TreeViewExpandRecursively(hwnd, TreeView_GetRoot(hwnd), TVE_EXPAND, false);
    } else if (VK_MULTIPLY == wp) {
        TreeViewExpandRecursively(hwnd, TreeView_GetSelection(hwnd), TVE_EXPAND, true);
    } else if (VK_DIVIDE == wp && IsShiftPressed()) {
        HTREEITEM root = TreeView_GetRoot(hwnd);
        if (!TreeView_GetNextSibling(hwnd, root))
            root = TreeView_GetChild(hwnd, root);
        TreeViewExpandRecursively(hwnd, root, TVE_COLLAPSE, false);
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

TreeCtrl::TreeCtrl(HWND parent, RECT* initialPosition) {
    this->parent = parent;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    } else {
        SetRect(&this->initialPos, 0, 0, 120, 28);
    }
}

bool TreeCtrl::Create(const WCHAR* title) {
    if (!title) {
        title = L"";
    }

    RECT rc = this->initialPos;
    HMODULE hmod = GetModuleHandleW(nullptr);
    this->hwnd = CreateWindowExW(this->dwExStyle, WC_TREEVIEWW, title, this->dwStyle, rc.left, rc.top, RectDx(rc),
                                 RectDy(rc), this->parent, this->menu, hmod, nullptr);
    if (!this->hwnd) {
        return false;
    }
    TreeView_SetUnicodeFormat(this->hwnd, true);
    this->SetFont(GetDefaultGuiFont());
    Subclass(this);

    return true;
}

TVITEM* TreeCtrl::GetItem(HTREEITEM hItem) {
    TVITEM* item = &this->item;
    ZeroStruct(item);
    item->hItem = hItem;
    item->mask = TVIF_PARAM | TVIF_STATE;
    item->stateMask = TVIS_EXPANDED;
    BOOL ok = TreeView_GetItem(this->hwnd, item);
    if (!ok) {
        return nullptr;
    }
    return item;
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

HTREEITEM TreeCtrl::InsertItem(TV_INSERTSTRUCT* item) {
    HTREEITEM res = TreeView_InsertItem(this->hwnd, item);
    return res;
}

void TreeCtrl::Clear() {
    HWND hwnd = this->hwnd;
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    UINT flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    RedrawWindow(hwnd, nullptr, nullptr, flags);
}

TreeCtrl::~TreeCtrl() {
    Unsubclass(this);
    // DeleteObject(w->bgBrush);
}

void TreeCtrl::SetFont(HFONT f) {
    SetWindowFont(this->hwnd, f, TRUE);
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

std::wstring TreeCtrl::GetInfoTip(HTREEITEM hItem) {
    ZeroArray(this->infotipBuf);
    TVITEMW item = {0};
    item.hItem = hItem;
    item.mask = TVIF_TEXT;
    item.pszText = this->infotipBuf;
    item.cchTextMax = dimof(this->infotipBuf);
    TreeView_GetItem(this->hwnd, &item);
    return std::wstring(this->infotipBuf);
}
