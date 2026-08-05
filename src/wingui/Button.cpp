/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/ScopedWin.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

//--- Button

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

static Kind kindButton = "button";

constexpr uint kBtnTextFmt = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;

Button::Button() {
    kind = kindButton;
}

bool Button::OnCommand(WPARAM wparam, LPARAM /*lparam*/) {
    auto code = HIWORD(wparam);
    if (code == BN_CLICKED) {
        if (onClick.IsValid()) {
            onClick.Call();
            return true;
        }
    }
    return false;
}

// Windows draws a themed push button in a face color of its own ("DarkMode_
// Explorer" gives a fixed #333333), which sits wherever it lands against an
// app palette - a light gray box on a black dialog (issue #5896). Draw it here
// instead, from colors the app supplies, so buttons match the rest of its UI.
void Button::PaintOwnerDrawn(DRAWITEMSTRUCT* dis) {
    HDC hdc = dis->hDC;
    Rect r = ToRect(dis->rcItem);
    bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hasFocus = (dis->itemState & ODS_FOCUS) != 0;

    ButtonColors col{};
    if (!ButtonGetColors(&col)) {
        // an app with no palette of its own still needs the button painted:
        // BS_OWNERDRAW means nothing else will
        RECT rc = ToRECT(r);
        uint state = DFCS_BUTTONPUSH | (isPressed ? DFCS_PUSHED : 0) | (isDisabled ? DFCS_INACTIVE : 0);
        DrawFrameControl(hdc, &rc, DFC_BUTTON, state);
        SetBkMode(hdc, TRANSPARENT);
        HdcDrawText(hdc, HwndGetTextTemp(hwnd), r, kBtnTextFmt, HwndGetFont(hwnd));
        return;
    }

    COLORREF bg = col.bg;
    COLORREF edge = col.edge;
    if (isDisabled) {
        edge = col.edgeDisabled;
    } else if (isPressed) {
        bg = col.bgPressed;
        edge = col.edgeHot;
    } else if (isHot) {
        bg = col.bgHot;
        edge = col.edgeHot;
    } else if (hasFocus || isDefault) {
        // the default button and the focused one read as "this is what Enter
        // does", so give them the brighter edge rather than a fill of their own
        edge = col.edgeHot;
    }

    HdcFillRect(hdc, r, bg);
    {
        ScopedSelectObject selPen(hdc, CreatePen(PS_SOLID, 1, edge), true);
        ScopedSelectObject selBrush(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, r.x, r.y, r.x + r.dx, r.y + r.dy);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, isDisabled ? col.textDisabled : col.text);
    HdcDrawText(hdc, HwndGetTextTemp(hwnd), r, kBtnTextFmt, HwndGetFont(hwnd));

    if (hasFocus) {
        Rect fr = r;
        int inset = DpiScale(hwnd, 3);
        fr.x += inset;
        fr.y += inset;
        fr.dx -= 2 * inset;
        fr.dy -= 2 * inset;
        RECT rr = ToRECT(fr);
        DrawFocusRect(hdc, &rr);
    }
}

// WM_DRAWITEM reports pressed / focused / disabled but never a hot state, so
// track the mouse ourselves and repaint on enter and leave
LRESULT Button::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MOUSEMOVE) {
        if (!trackingMouse) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            if (TrackMouseEvent(&tme)) {
                trackingMouse = true;
            }
        }
        if (!isHot) {
            isHot = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    } else if (msg == WM_MOUSELEAVE) {
        trackingMouse = false;
        if (isHot) {
            isHot = false;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
    return WndProcDefault(hwnd, msg, wp, lp);
}

LRESULT Button::OnMessageReflect(UINT msg, WPARAM /*wparam*/, LPARAM lparam) {
    if (msg == WM_DRAWITEM) {
        PaintOwnerDrawn((DRAWITEMSTRUCT*)lparam);
        return TRUE;
    }
    return 0;
}

HWND Button::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = WC_BUTTONW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (isDefault) {
        cargs.style |= BS_DEFPUSHBUTTON;
    } else {
        cargs.style |= BS_PUSHBUTTON;
    }
    // measure the text before BS_OWNERDRAW hides it from BCM_GETIDEALSIZE
    cargs.style |= BS_OWNERDRAW;
    cargs.text = args.text;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Button::GetIdealSize() {
    // BCM_GETIDEALSIZE returns the text size without any button padding for a
    // BS_OWNERDRAW button, so measure the text and add our own
    TempStr s = HwndGetTextTemp(hwnd);
    Size sz = HwndMeasureText(hwnd, s, HwndGetFont(hwnd));
    int dx = sz.dx + DpiScale(hwnd, 2 * 12);
    int dy = sz.dy + DpiScale(hwnd, 2 * 5);
    int minDx = DpiScale(hwnd, 70);
    dx = std::max(dx, minDx);
    return {dx, dy};
}

#if 0
Size Button::SetTextAndResize(const WCHAR* s) {
    HwndSetText(this->hwnd, s);
    Size size = this->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

Button* CreateButton(HWND parent, Str s, const Func0& onClick, bool isRtl) {
    Button::CreateArgs args;
    args.parent = parent;
    args.text = s;
    args.isRtl = isRtl;

    auto b = new Button();
    b->onClick = onClick;
    b->Create(args);
    return b;
}

#define kButtonMargin 8

Button* CreateDefaultButton(HWND parent, Str s, bool isRtl) {
    Button::CreateArgs args;
    args.parent = parent;
    args.text = s;
    args.isRtl = isRtl;

    auto* b = new Button();
    b->Create(args);

    Rect rc = HwndClientRect(parent);
    Size size = b->GetIdealSize();
    int margin = DpiScale(parent, kButtonMargin);
    int x = rc.dx - size.dx - margin;
    int y = rc.dy - size.dy - margin;
    Rect r = {x, y, size.dx, size.dy};
    b->SetPos(&r);
    return b;
}
