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

#include "utils/Log.h"

#define kCloseBtnDx 16
#define kCloseBtnDy 16
#define kButtonSpaceDx 8

static void PaintHDC(LabelWithCloseWnd* w, HDC hdc, const PAINTSTRUCT& ps) {
    HBRUSH br = w->BackgroundBrush();
    FillRect(hdc, &ps.rcPaint, br);

    Rect cr = ClientRect(w->hwnd);

    int x = DpiScale(w->hwnd, w->padX);
    int y = DpiScale(w->hwnd, w->padY);

    HGDIOBJ prevFont = nullptr;
    if (w->font) {
        prevFont = SelectObject(hdc, w->font);
    }
    if (!IsSpecialColor(w->textColor)) {
        SetTextColor(hdc, w->textColor);
    }
    if (!IsSpecialColor(w->bgColor)) {
        SetBkColor(hdc, w->bgColor);
    }

    uint fmt = DT_SINGLELINE | DT_TOP | DT_LEFT;
    if (HwndIsRtl(w->hwnd)) {
        fmt |= DT_RTLREADING;
    }
    char* s = HwndGetTextTemp(w->hwnd);
    RECT rs{x, y, x + cr.dx, y + cr.dy};
    HdcDrawText(hdc, s, &rs, fmt);

    // Text might be too long and invade close button area. We just re-paint
    // the background, which is not the pretties but works.
    // A better way would be to intelligently truncate text or shrink the font
    // size (within reason)
    bool isRtl = HwndIsRtl(w->hwnd);
    // TODO: make this work in rtl
    if (!isRtl) {
        x = w->closeBtnPos.x - DpiScale(w->hwnd, kButtonSpaceDx);
        Rect ri(x, 0, cr.dx - x, cr.dy);
        RECT r = ToRECT(ri);
        FillRect(hdc, &r, br);
    }
    Point curPos = HwndGetCursorPos(w->hwnd);
    // TODO: hack
    UnmirrorRtl(w->hwnd, curPos);
    DrawCloseButtonArgs args;
    args.hdc = hdc;
    args.r = w->closeBtnPos;
    args.isHover = w->closeBtnPos.Contains(curPos);
    // args.noMirror = true;
    DrawCloseButton(args);

    if (w->font) {
        SelectObject(hdc, prevFont);
    }
}

void LabelWithCloseWnd::OnPaint(HDC hdc, PAINTSTRUCT* ps) {
    DoubleBuffer buffer(hwnd, ToRect(ps->rcPaint));
    PaintHDC(this, buffer.GetDC(), *ps);
    buffer.Flush(hdc);
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
        Layout();
        return 0;
    }

    Point cursorPos = HwndGetCursorPos(hwnd);
    // TODO: this is a hack
    // HwhndGetCursorPos() does rtl mirroring but we calculate position
    // in absolute coords. Need to be more principled here
    UnmirrorRtl(hwnd, cursorPos);
    Rect br = closeBtnPos;
    if (WM_MOUSEMOVE == msg) {
        // logf("WM_MOUSEMOVE\n");
        // logf("closeBtnPos: (%d,%d) size: (%d, %d)\n", br.x, br.y, br.dx, br.dy);
        // logf("cursorPos: (%d, %d)\n", cursorPos.x, cursorPos.y);
        HwndScheduleRepaint(hwnd);

        if (closeBtnPos.Contains(cursorPos)) {
            TrackMouseLeave(hwnd);
        }
        goto DoDefault;
    }

    if (WM_MOUSELEAVE == msg) {
        // logf("WM_MOUSELEAVE\n");
        // logf("closeBtnPos: (%d,%d) size: (%d, %d)\n", br.x, br.y, br.dx, br.dy);
        // logf("cursorPos: (%d, %d)\n", cursorPos.x, cursorPos.y);
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        // logf("WM_LBUTTONUP\n");
        // logf("closeBtnPos: (%d,%d) size: (%d, %d)\n", br.x, br.y, br.dx, br.dy);
        // logf("cursorPos: (%d, %d)\n", cursorPos.x, cursorPos.y);
        if (closeBtnPos.Contains(cursorPos)) {
            HWND parent = GetParent(hwnd);
            HwndSendCommand(parent, cmdId);
        }
        return 0;
    }

DoDefault:
    return WndProcDefault(hwnd, msg, wp, lp);
}

void LabelWithCloseWnd::SetLabel(const char* label) {
    HwndSetText(this->hwnd, label);
    this->Layout();
    HwndScheduleRepaint(this->hwnd);
}

void LabelWithCloseWnd::Layout() {
    Rect r = ClientRect(hwnd);
    int dx = r.dx;
    int dy = r.dy;

    int btnDx = DpiScale(hwnd, kCloseBtnDx);
    int btnDy = DpiScale(hwnd, kCloseBtnDy);
    int padXScaled = DpiScale(hwnd, padX);
    auto isRtl = HwndIsRtl(hwnd);
    int x = isRtl ? padX : dx - btnDx - padXScaled;
    int y = 0;
    if (dy > btnDy) {
        y = (dy - btnDy) / 2;
    }
    closeBtnPos = Rect(x, y, btnDx, btnDy);
    // logf("closeBtnPos: (%d,%d) size: (%d, %d)\n", x, y, btnDx, btnDy);
    HwndScheduleRepaint(hwnd);
}

// cmd is both the id of the window as well as id of WM_COMMAND sent
// when close button is clicked
// caller needs to free() the result
HWND LabelWithCloseWnd::Create(const LabelWithCloseWnd::CreateArgs& args) {
    CreateCustomArgs cargs;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.pos = Rect(0, 0, 0, 0);
    cargs.style = WS_VISIBLE;
    cargs.cmdId = cmdId; // TODO: not sure if needed
    cargs.isRtl = args.isRtl;
    cmdId = args.cmdId;

    CreateCustom(cargs);

#if 0
    auto bgCol = GetSysColor(COLOR_BTNFACE);
    auto txtCol = GetSysColor(COLOR_BTNTEXT);
    SetColors(txtCol, bgCol);
#endif
    return hwnd;
}

Size LabelWithCloseWnd::GetIdealSize() {
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
