/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"
#include "utils/GdiPlusUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "AppTools.h"

#include "wingui/LabelWithCloseWnd.h"

#define kCloseBtnDx 16
#define kCloseBtnDy 16
#define kButtonSpaceDx 8

static void PaintHDC(LabelWithCloseWnd* w, HDC hdc, const PAINTSTRUCT& ps) {
    HBRUSH br = CreateSolidBrush(w->bgCol);
    FillRect(hdc, &ps.rcPaint, br);

    Rect cr = ClientRect(w->hwnd);

    int x = DpiScale(w->hwnd, w->padX);
    int y = DpiScale(w->hwnd, w->padY);

    HGDIOBJ prevFont = nullptr;
    if (w->font) {
        prevFont = SelectObject(hdc, w->font);
    }
    SetTextColor(hdc, w->txtCol);
    SetBkColor(hdc, w->bgCol);

    uint format = DT_SINGLELINE | DT_TOP | DT_LEFT;
    if (IsRtl(w->hwnd)) {
        format |= DT_RTLREADING;
    }
    char* s = HwndGetTextTemp(w->hwnd);
    RECT rs{x, y, x + cr.dx, y + cr.dy};
    HdcDrawText(hdc, s, &rs, format);

    // Text might be too long and invade close button area. We just re-paint
    // the background, which is not the pretties but works.
    // A better way would be to intelligently truncate text or shrink the font
    // size (within reason)
    x = w->closeBtnPos.x - DpiScale(w->hwnd, kButtonSpaceDx);
    Rect ri(x, 0, cr.dx - x, cr.dy);
    RECT r = ToRECT(ri);
    FillRect(hdc, &r, br);

    Point curPos = HwndGetCursorPos(w->hwnd);
    bool isHover = w->closeBtnPos.Contains(curPos);
    DrawCloseButton(hdc, w->closeBtnPos, isHover);
    DeleteObject(br);

    if (w->font) {
        SelectObject(hdc, prevFont);
    }
}

void LabelWithCloseWnd::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
    DoubleBuffer buffer(hwnd, ToRect(ps->rcPaint));
    PaintHDC(this, buffer.GetDC(), *ps);
    buffer.Flush(hdc);
}

static void CalcCloseButtonPos(LabelWithCloseWnd* w, int dx, int dy) {
    int btnDx = DpiScale(w->hwnd, kCloseBtnDx);
    int btnDy = DpiScale(w->hwnd, kCloseBtnDy);
    int x = dx - btnDx - DpiScale(w->hwnd, w->padX);
    int y = 0;
    if (dy > btnDy) {
        y = (dy - btnDy) / 2;
    }
    w->closeBtnPos = Rect(x, y, btnDx, btnDy);
}

LRESULT LabelWithCloseWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_ERASEBKGND == msg) {
        return TRUE; // tells Windows we handle background erasing so it doesn't do it
    }

#if 0
    // to match other controls, preferred way is explict SetFont() call
    if (WM_SETFONT == msg) {
        SetFont((HFONT)wp);
        return 0;
    }

    if (WM_GETFONT == msg) {
        return (LRESULT)font;
    }
#endif

    if (WM_SIZE == msg) {
        int dx = LOWORD(lp);
        int dy = HIWORD(lp);
        CalcCloseButtonPos(this, dx, dy);
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_MOUSEMOVE == msg) {
        HwndScheduleRepaint(hwnd);

        if (IsMouseOverRect(hwnd, closeBtnPos)) {
            TrackMouseLeave(hwnd);
        }
        goto DoDefault;
    }

    if (WM_MOUSELEAVE == msg) {
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        if (IsMouseOverRect(hwnd, closeBtnPos)) {
            HWND parent = GetParent(hwnd);
            HwndSendCommand(parent, cmdId);
        }
        return 0;
    }

DoDefault:
    return WndProcDefault(hwnd, msg, wp, lp);
}

void LabelWithCloseWnd::SetLabel(const WCHAR* label) const {
    HwndSetText(this->hwnd, label);
    HwndScheduleRepaint(this->hwnd);
}

void LabelWithCloseWnd::SetBgCol(COLORREF c) {
    this->bgCol = c;
    HwndScheduleRepaint(this->hwnd);
}

void LabelWithCloseWnd::SetTextCol(COLORREF c) {
    this->txtCol = c;
    HwndScheduleRepaint(this->hwnd);
}

// cmd is both the id of the window as well as id of WM_COMMAND sent
// when close button is clicked
// caller needs to free() the result
HWND LabelWithCloseWnd::Create(const LabelWithCloseCreateArgs& args) {
    cmdId = args.cmdId;
    bgCol = GetSysColor(COLOR_BTNFACE);
    txtCol = GetSysColor(COLOR_BTNTEXT);

    CreateCustomArgs cargs;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.pos = Rect(0, 0, 0, 0);
    cargs.style = WS_VISIBLE;
    cargs.cmdId = cmdId; // TODO: not sure if needed
    CreateCustom(cargs);

    return hwnd;
}

Size LabelWithCloseWnd::GetIdealSize() const {
    char* s = HwndGetTextTemp(this->hwnd);
    Size size = HwndMeasureText(this->hwnd, s);
    int btnDx = DpiScale(this->hwnd, kCloseBtnDx);
    int btnDy = DpiScale(this->hwnd, kCloseBtnDy);
    size.dx += btnDx;
    size.dx += DpiScale(this->hwnd, kButtonSpaceDx);
    size.dx += 2 * DpiScale(this->hwnd, this->padX);
    if (size.dy < btnDy) {
        size.dy = btnDy;
    }
    size.dy += 2 * DpiScale(this->hwnd, this->padY);
    return size;
}

void LabelWithCloseWnd::SetFont(HFONT f) {
    this->font = f;
    // TODO: if created, set on the label?
}

void LabelWithCloseWnd::SetPaddingXY(int x, int y) {
    this->padX = x;
    this->padY = y;
    HwndScheduleRepaint(this->hwnd);
}
