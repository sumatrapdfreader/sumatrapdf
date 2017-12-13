#include "BaseUtil.h"
#include "TreeCtrl.h"
#include "WinUtil.h"

static LRESULT CALLBACK TreeParentProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass,
                                       DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    TreeCtrl* w = (TreeCtrl*)dwRefData;
    CrashIf(GetParent(w->hwnd) != (HWND)lp);
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK TreeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    UNUSED(uIdSubclass);
    TreeCtrl* w = (TreeCtrl*)dwRefData;
    CrashIf(w->hwnd != (HWND)lp);

    if (w->preFilter) {
        bool discard = false;
        auto res = w->preFilter(hwnd, msg, wp, lp, discard);
        if (discard) {
            return res;
        }
    }

    if (WM_NCDESTROY == msg) {
        RemoveWindowSubclass(GetParent(w->hwnd), TreeParentProc, 0);
        RemoveWindowSubclass(w->hwnd, TreeProc, 0);
        return DefSubclassProc(hwnd, msg, wp, lp);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
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
    w->dwStyle = WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL;

    return w;
}

bool CreateEditCtrl(TreeCtrl* w) {
    // Note: has to remember this here because when I GetWindowStyle() later on,
    // WS_BORDER is not set, which is a mystery, because it is being drawn.
    // also, WS_BORDER seems to be painted in client areay
    // w->hasBorder = bit::IsMaskSet<DWORD>(w->dwStyle, WS_BORDER);

    RECT rc = w->initialPos;
    w->hwnd = CreateWindowExW(w->dwExStyle, WC_EDIT, L"", w->dwStyle, rc.left, rc.top, RectDx(rc), RectDy(rc),
                              w->parent, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!w->hwnd) {
        return false;
    }
    SetFont(w, GetDefaultGuiFont());
    SetWindowSubclass(w->hwnd, TreeProc, 0, (DWORD_PTR)w);
    SetWindowSubclass(GetParent(w->hwnd), TreeParentProc, 0, (DWORD_PTR)w);
    return true;
}

void DeleteTreeCtrl(TreeCtrl* w) {
    if (!w)
        return;

    // DeleteObject(w->bgBrush);
    free(w);
}

void SetFont(TreeCtrl* w, HFONT f) {
    SetWindowFont(w->hwnd, f, TRUE);
}
