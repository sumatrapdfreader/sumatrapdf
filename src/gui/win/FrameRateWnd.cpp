/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/FrameRateWnd.h"

/*
Frame rate window is a debugging tool that shows the frame rate, most likely
of how long it takes to service WM_PAINT. It's good for a rough measure of
how fast painting is.

The window is a top-level window, semi-transparent, without decorations
that sits in the upper right corner of some other window (associated window).

The window must follow associated window so that it maintains an illusion
that it's actually a part of that window.
*/

constexpr const WCHAR* kFrameRateClassName = L"FrameRateWnd";

static void PositionWindow(FrameRateWnd* w, Size s) {
    Rect rc = HwndClientRect(w->hwndAssociatedWith);
    Point p = HwndClientToScreen(w->hwndAssociatedWith, Point(rc.x + rc.dx - s.dx, rc.y));
    MoveWindow(w->hwnd, p.x, p.y, s.dx, s.dy, TRUE);
}

static LRESULT CALLBACK WndProcFrameRateAssociated(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR /*idSubclass*/,
                                                   DWORD_PTR dwRefData) {
    if (WM_MOVING == msg || WM_SIZING == msg || WM_SIZE == msg || WM_WINDOWPOSCHANGED == msg || WM_MOVE == msg) {
        FrameRateWnd* w = (FrameRateWnd*)dwRefData;
        PositionWindow(w, w->maxSizeSoFar);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool FrameRateWnd::Create(HWND hwndAssociatedWithIn) {
    hwndAssociatedWith = hwndAssociatedWithIn;

    // if hwndAssociatedWith is a child window, we need to find its top-level parent
    // so that we can intercept moving messages and re-position frame rate window
    // during main window moves
    HWND topLevel = hwndAssociatedWith;
    while (GetParent(topLevel) != nullptr) {
        topLevel = GetParent(topLevel);
    }
    hwndAssociatedWithTopLevel = topLevel;

    {
        CreateCustomArgs args;
        args.className = kFrameRateClassName;
        // WS_POPUP removes all decorations
        args.style = WS_POPUP | WS_DISABLED;
        // WS_EX_TRANSPARENT so that the mouse events fall through to the window below
        args.exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT;
        args.pos = {0, 0, 1, 1};
        args.bgColor = kColBlack;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    // a WS_POPUP window given a parent at creation would become a child, so the
    // ownership relationship is established after the fact. An owned window always
    // shows up on top of its owner in z-order
    // http://msdn.microsoft.com/en-us/library/ms632599%28v=VS.85%29.aspx#owned_windows
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hwndAssociatedWith);

    text = NewVirtText({
        .s = StrL("0"),
        .font = GetDefaultGuiFont(),
        .textColor = kColWhite,
        .align = VirtTextAlign::Center,
    });
    layout = new Padding(text, Insets{2, 4, 2, 4});

    SetWindowSubclass(hwndAssociatedWithTopLevel, WndProcFrameRateAssociated, 0, (DWORD_PTR)this);
    SetLayeredWindowAttributes(hwnd, 0, 0x7f, LWA_ALPHA);
    ShowFrameRate(0);
    return true;
}

void FrameRateWnd::ShowFrameRate(int frameRateIn) {
    if (frameRate == frameRateIn) {
        return;
    }
    frameRate = frameRateIn;
    text->SetText(fmt("%d", frameRate));

    Size s = layout->Layout(ExpandInf());
    // we wan't to avoid the window to grow/shrink when the number changes
    // so we keep the largest size so far, since the difference isn't big
    maxSizeSoFar.dx = std::max(s.dx, maxSizeSoFar.dx);
    maxSizeSoFar.dy = std::max(s.dy, maxSizeSoFar.dy);

    PositionWindow(this, maxSizeSoFar);
    DoLayout(maxSizeSoFar);
    HwndScheduleRepaint(hwnd);
}

void FrameRateWnd::ShowFrameRateDur(double durMs) {
    ShowFrameRate(FrameRateFromDuration(durMs));
}

FrameRateWnd::~FrameRateWnd() {
    RemoveWindowSubclass(hwndAssociatedWithTopLevel, WndProcFrameRateAssociated, 0);
}

int FrameRateFromDuration(double durMs) {
    return (int)(double(1000) / durMs);
}
