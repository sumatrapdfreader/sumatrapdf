/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "TreeCtrl.h" // for TreeViewExpandRecursively
#include "TreeCtrl2.h"
#include "utils/WinUtil.h"

constexpr UINT_PTR SUBCLASS_ID = 1;

static void Unsubclass(TreeCtrl2* w);

int Event::Attach(const EventHandler& handler) {
    this->handlers.push_back(handler);
    return (int)this->handlers.size() -1;
}

void Event::Detach(int handle) {
    this->handlers[handle] = nullptr;
}


void EventPublisher::Publish() {
    for (auto& h : this->event.handlers) {
        if (h != nullptr) {
            h();
        }
    }
}

int TreeItemEvent::Attach(const TreeItemEventHandler& h) {
    this->handlers.push_back(h);
    return (int)this->handlers.size() - 1;
}

void TreeItemEvent::Detach(int idx) {
    this->handlers[idx] = nullptr;
}

void TreeItemEventPublisher::Publish(TreeItem* item) {
    for (const auto& h : this->event.handlers) {
        if (h != nullptr) {
            h(item);
        }
    }
}


TreeCtrl2::TreeCtrl2(HWND parent, RECT* initialPosition) {
    this->parent = parent;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    } else {
        SetRect(&this->initialPos, 0, 0, 120, 28);
    }
}

static LRESULT CALLBACK TreeParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR subclassId, DWORD_PTR data) {
    CrashIf(subclassId != SUBCLASS_ID); // this proc is only used in one subclass
    auto* w = (TreeCtrl2*)data;
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
    auto* w = (TreeCtrl2*)dwRefData;
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

static void Subclass(TreeCtrl2* w) {
    BOOL ok = SetWindowSubclass(w->hwnd, TreeProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndSubclassId = SUBCLASS_ID;

    ok = SetWindowSubclass(w->parent, TreeParentProc, SUBCLASS_ID, (DWORD_PTR)w);
    CrashIf(!ok);
    w->hwndParentSubclassId = SUBCLASS_ID;
}

static void Unsubclass(TreeCtrl2* w) {
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

bool TreeCtrl2::Create(const WCHAR* title) {
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

void TreeCtrl2::Clear() {
    HWND hwnd = this->hwnd;
    ::SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwnd);
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    UINT flags = RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN;
    ::RedrawWindow(hwnd, nullptr, nullptr, flags);
}

TreeCtrl2::~TreeCtrl2() {
    Unsubclass(this);
    // DeleteObject(w->bgBrush);
}

void TreeCtrl2::SetFont(HFONT f) {
    SetWindowFont(this->hwnd, f, TRUE);
}
