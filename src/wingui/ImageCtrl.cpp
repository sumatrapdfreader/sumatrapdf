/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ImageCtrl.h"

Kind kindImage = "image";

bool IsImage(Kind kind) {
    return kind == kindImage;
}

bool IsImage(ILayout* l) {
    return IsLayoutOfKind(l, kindImage);
}

ImageCtrl::ImageCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE;
    winClass = WC_STATICW;
    kind = kindImage;
}

ImageCtrl::~ImageCtrl() {
}

using Gdiplus::Color;
using Gdiplus::ImageAttributes;
using Gdiplus::SolidBrush;
using Gdiplus::UnitPixel;

static void OnImageCtrlPaint(ImageCtrl* w, COLORREF bgCol) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w->hwnd, &ps);

    RECT r{};
    GetWindowRect(w->hwnd, &r);
    int dx = RectDx(r);
    int dy = RectDy(r);
    RectI rc{0, 0, dx, dy};
    Gdiplus::Rect rcp = rc.ToGdipRect();

    Color col(bgCol);
    SolidBrush tmpBrush(col);

    Gdiplus::Graphics g(hdc);
    g.FillRectangle(&tmpBrush, rcp);

    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    ImageAttributes imgAttrs;
    imgAttrs.SetWrapMode(Gdiplus::WrapModeClamp);

    // TODO: allow for different image positioning (center, fit, fit-preserve-ratio)

    Gdiplus::Status ok = g.DrawImage(w->bmp, rcp, 0, 0, dx, dy, UnitPixel, &imgAttrs);
    EndPaint(w->hwnd, &ps);
}

static void ImageCtrlWndProc(WndProcArgs* args) {
    UINT msg = args->msg;
    if (WM_ERASEBKGND == msg) {
        args->didHandle = true;
        // do nothing, helps to avoid flicker
        args->result = TRUE;
        return;
    }

    HWND hwnd = args->hwnd;
    ImageCtrl* w = (ImageCtrl*)args->w;
    CrashIf(!w);
    if (!w) {
        return;
    }

    if (WM_PAINT == msg) {
        OnImageCtrlPaint(w, w->backgroundColor);
        args->didHandle = true;
        return;
    }

    // char* msgName = getWinMessageName(msg);
    // dbglogf("hwnd: 0x%6p, msg: 0x%03x (%s), wp: 0x%x\n", hwnd, msg, msgName, args->wparam);
}

bool ImageCtrl::Create() {
    // TODO: for now we require bmp to be set before calling Create()
    CrashIf(!bmp);
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    auto size = GetIdealSize();
    RECT r{0, 0, size.cx, size.cy};
    SetBounds(r);
    msgFilter = ImageCtrlWndProc;
    Subclass();
    return ok;
}

SIZE ImageCtrl::GetIdealSize() {
    UINT dx = bmp->GetWidth();
    UINT dy = bmp->GetHeight();
    return SIZE{(LONG)dx, (LONG)dy};
}

ILayout* NewImageLayout(ImageCtrl* b) {
    return new WindowBaseLayout(b, kindImage);
}
