/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/AutoWin.h"
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

static LRESULT CALLBACK WndProcFrameRateAssociated(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR idSubclass,
                                                   DWORD_PTR dwRefData) {
    FrameRateWnd* w = (FrameRateWnd*)dwRefData;
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, WndProcFrameRateAssociated, idSubclass);
        if (w) {
            w->hwndAssociatedWithTopLevel = nullptr;
        }
        return DefSubclassProc(hwnd, msg, wp, lp);
    }
    if (WM_MOVING == msg || WM_SIZING == msg || WM_SIZE == msg || WM_WINDOWPOSCHANGED == msg || WM_MOVE == msg) {
        PositionWindow(w, w->maxSizeSoFar);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool FrameRateWnd::Create(HWND hwndAssociatedWithIn) {
    hwndAssociatedWith = hwndAssociatedWithIn;

    // if hwndAssociatedWith is a child window, we need to find its top-level parent
    // so that we can intercept moving messages and re-position frame rate window
    // during main window moves
    HWND topLevel = GetAncestor(hwndAssociatedWith, GA_ROOT);
    hwndAssociatedWithTopLevel = topLevel ? topLevel : hwndAssociatedWith;

    {
        CreateCustomArgs args;
        args.className = kFrameRateClassName;
        // WS_POPUP removes all decorations
        args.style = WS_POPUP | WS_DISABLED;
        // WS_EX_TRANSPARENT so that the mouse events fall through to the window below
        args.exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT;
        args.owner = hwndAssociatedWithTopLevel;
        args.pos = {0, 0, 1, 1};
        args.bgColor = kColBlack;
        CreateCustom(args);
    }
    if (!hwnd) {
        hwndAssociatedWithTopLevel = nullptr;
        return false;
    }

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
    // Create() failed
    if (!hwnd || !text) {
        return;
    }
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
    if (hwndAssociatedWithTopLevel) {
        RemoveWindowSubclass(hwndAssociatedWithTopLevel, WndProcFrameRateAssociated, 0);
        hwndAssociatedWithTopLevel = nullptr;
    }
}

int FrameRateFromDuration(double durMs) {
    if (durMs <= 0.0) {
        return 9999;
    }
    double fps = double(1000) / durMs;
    if (fps > 9999.0) {
        return 9999;
    }
    return (int)fps;
}
