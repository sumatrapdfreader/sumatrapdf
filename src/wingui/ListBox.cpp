/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

static Kind kindListBox = "listbox";

ListBox::ListBox() {
    kind = kindListBox;
#if 0
    ctrlID = 0;
#endif
}

ListBox::~ListBox() {
    delete this->model;
}

HWND ListBox::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = L"LISTBOX";
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;

    idealSizeLines = args.idealSizeLines;
    idealSizeLines = std::max(idealSizeLines, 0);
    idealSize = {DpiScale(args.parent, 120), DpiScale(args.parent, 32)};

    // https://docs.microsoft.com/en-us/windows/win32/controls/list-box-styles
    cargs.style = WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL;
    cargs.style |= LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
    if (onDrawItem.IsValid()) {
        cargs.style |= LBS_OWNERDRAWFIXED;
    }
    // args.style |= WS_BORDER;
    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    if (hwnd) {
        // For owner-draw, set item height manually since WM_MEASUREITEM
        // is sent during CreateWindowEx before we're registered in WndList.
        // We measure using our font, not LB_GETITEMHEIGHT (which has the wrong default).
        if (onDrawItem.IsValid()) {
            Size sz = HwndMeasureText(hwnd, "Ag", font);
            int itemHeight = sz.dy + DpiScale(hwnd, 4);
            LbSetItemHeight(hwnd, 0, itemHeight);
        }
        if (model != nullptr) {
            FillWithItems(this->hwnd, model);
        }
        // every list box scrolls, and the stock scrollbar doesn't follow the
        // app's theme on its own. Doing it here rather than at each call site
        // is why the find window's results list was the odd one out (#5894)
        ListBoxMaybeApplyTheme(hwnd);
    }

    return hwnd;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/lb-getitemheight
int ListBox::GetItemHeight(int idx) {
    // idx only valid for LBS_OWNERDRAWVARIABLE, otherwise should be 0
    int res = LbGetItemHeight(hwnd, idx);
    if (res < 0) {
        Size sz = HwndMeasureText(hwnd, "Ag", font);
        res = sz.dy;
    }
    return res;
}

Size ListBox::GetIdealSize() {
    Size res = idealSize;
    int nLines = idealSizeLines;
    if (nLines <= 0) {
        int nItems = 0;
        if (model) {
            nItems = model->ItemsCount();
        } else if (hwnd) {
            nItems = GetCount();
        }
        nLines = std::min(nItems, 16);
        if (nLines <= 0) {
            nLines = 1;
        }
    }
    if (hwnd) {
        int dy = (GetItemHeight(0) * nLines) + DpiScale(hwnd, 2 + 2); // padding of 2 at top and bottom
        res.dy = dy;
    }
    return res;
}

int ListBox::GetCount() {
    return LbGetCount(hwnd);
}

int ListBox::GetCurrentSelection() {
    return LbGetCurrentSelection(hwnd);
}

// -1 to clear selection
// returns false on error
bool ListBox::SetCurrentSelection(int n) {
    if (n < 0) {
        LbSetCurrentSelection(hwnd, -1);
        return true;
    }
    int nItems = model->ItemsCount();
    if (n >= nItems) {
        return false;
    }
    return LbSetCurrentSelection(hwnd, n);
}

// for efficiency you can re-use model:
// get the model, change data, call SetModel() again
void ListBox::SetModel(ListBoxModel* model) {
    if (this->model && (this->model != model)) {
        delete this->model;
    }
    this->model = model;
    if (model != nullptr) {
        FillWithItems(this->hwnd, model);
    }
    SetCurrentSelection(-1);
}

// After LB_SETCURSEL(-1) / content rebuild, the first mouse click can
// jump-scroll a fully visible item to the bottom of the viewport (issue
// #5829). Default listbox handling selects the item, then "ensures" the
// caret is visible using a bad caret index — which changes TOPINDEX even
// though the item was already on screen. Restore the pre-click top when
// the clicked item was fully visible.
LRESULT ListBox::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        // redraw so the focus ring appears/disappears
        InvalidateRect(hwnd, nullptr, FALSE);
        return WndProcDefault(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_PAINT) {
        // default paints items; then dotted focus ring around the control
        LRESULT res = WndProcDefault(hwnd, msg, wparam, lparam);
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            // Owner-draw items are clipped to leave this 1px border alone, so
            // they never cover the ring (see WM_DRAWITEM in OnMessageReflect).
            // DrawFocusRect is XOR: if the 1px border still holds a previous
            // ring (not erased by item paint), the next draw toggles it off —
            // which made the ring appear only every other focus. Clear the
            // border to the list background first, then draw if focused.
            HBRUSH br = BackgroundBrush();
            RECT edge = {rc.left, rc.top, rc.right, rc.top + 1};
            FillRect(hdc, &edge, br);
            edge = {rc.left, rc.bottom - 1, rc.right, rc.bottom};
            FillRect(hdc, &edge, br);
            edge = {rc.left, rc.top, rc.left + 1, rc.bottom};
            FillRect(hdc, &edge, br);
            edge = {rc.right - 1, rc.top, rc.right, rc.bottom};
            FillRect(hdc, &edge, br);
            if (::GetFocus() == hwnd) {
                DrawFocusRect(hdc, &rc);
            }
            ReleaseDC(hwnd, hdc);
        }
        return res;
    }

    // Scrolling moves the existing pixels and only invalidates the strip that
    // was newly uncovered. Anything a row didn't paint (the row cut off by the
    // client edge, the 1px focus-ring border) then travels up into the middle of
    // the list as stale pixels. Repaint the lot whenever the top item changes;
    // these lists are small and items paint their own background, so there's
    // nothing to flicker (issue #5882).
    if (msg == WM_VSCROLL || msg == WM_MOUSEWHEEL || msg == WM_KEYDOWN) {
        int topBefore = LbGetTopIndex(hwnd);
        LRESULT res = WndProcDefault(hwnd, msg, wparam, lparam);
        if (LbGetTopIndex(hwnd) != topBefore) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return res;
    }

    if (msg == WM_LBUTTONDOWN) {
        int topBefore = LbGetTopIndex(hwnd);
        int count = GetCount();
        Point pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        bool outside = false;
        int idx = LbItemFromPoint(hwnd, pt, &outside);
        bool wasFullyVisible = false;
        if (!outside && idx >= 0 && idx < count) {
            Rect itemRc = LbGetItemRect(hwnd, idx);
            if (!itemRc.IsEmpty()) {
                Rect client = HwndClientRect(hwnd);
                wasFullyVisible = itemRc.y >= client.y && itemRc.y + itemRc.dy <= client.y + client.dy;
            }
        }

        LRESULT res = FinalWindowProc(msg, wparam, lparam);

        if (wasFullyVisible) {
            int topAfter = LbGetTopIndex(hwnd);
            if (topAfter != topBefore) {
                LbSetTopIndex(hwnd, topBefore);
            }
        }
        if (LbGetTopIndex(hwnd) != topBefore) {
            InvalidateRect(hwnd, nullptr, FALSE); // clicking a cut-off row scrolls; see above
        }
        return res;
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

bool ListBox::OnCommand(WPARAM wparam, LPARAM /*lparam*/) {
    auto code = HIWORD(wparam);
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-selchange
    if (code == LBN_SELCHANGE && onSelectionChanged.IsValid()) {
        onSelectionChanged.Call();
        return true;
    }
    // https://docs.microsoft.com/en-us/windows/win32/controls/lbn-dblclk
    if (code == LBN_DBLCLK && onDoubleClick.IsValid()) {
        onDoubleClick.Call();
        return true;
    }
    return false;
}

LRESULT ListBox::OnMessageReflect(UINT msg, WPARAM wp, LPARAM lparam) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-ctlcolorlistbox
    if (msg == WM_CTLCOLORLISTBOX) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
        }
        auto br = BackgroundBrush();
        return (LRESULT)br;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-measureitem
    if (msg == WM_MEASUREITEM) {
        if (!onDrawItem.IsValid()) {
            return 0;
        }
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lparam;
        Size sz = HwndMeasureText(hwnd, "Ag", font);
        mis->itemHeight = sz.dy + DpiScale(hwnd, 4);
        return TRUE;
    }

    // https://docs.microsoft.com/en-us/windows/win32/controls/wm-drawitem
    if (msg == WM_DRAWITEM) {
        if (!onDrawItem.IsValid()) {
            return 0;
        }
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lparam;
        DrawItemEvent ev;
        ev.listBox = this;
        ev.hdc = dis->hDC;
        Rect itemRc = ToRect(dis->rcItem);
        Rect client = HwndClientRect(hwnd);
        // with LBS_NOINTEGRALHEIGHT the last row can be cut in half by the bottom
        // of the list, which owner-draw handlers may want to paint differently
        ev.clippedAtBottom = (itemRc.y + itemRc.dy) > (client.y + client.dy);
        // The handler gets the row's true rectangle, not one shrunk to fit the
        // client: it has to fill the whole row (or DT_VCENTER lands the text off
        // center and the unpainted strip keeps whatever scrolled under it --
        // rows drawn over each other, issue #5882). The 1px client border, where
        // WM_PAINT draws the focus ring, is kept free by clipping instead, so a
        // solid selection fill can't paint the ring away.
        ev.itemRect = itemRc;
        ev.itemIndex = (int)dis->itemID;
        ev.selected = (dis->itemState & ODS_SELECTED) != 0;
        int savedDC = SaveDC(dis->hDC);
        IntersectClipRect(dis->hDC, client.x + 1, client.y + 1, client.x + client.dx - 1, client.y + client.dy - 1);
        onDrawItem.Call(&ev);
        RestoreDC(dis->hDC, savedDC);
        return TRUE;
    }

    return 0;
}
