/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Timer.h"
#include "utils/WinUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/Log.h"

#include "Mui.h"
#include "wingui/FrameRateWnd.h"

namespace mui {

HwndWrapper::HwndWrapper(HWND hwnd)
    : painter(nullptr),
      evtMgr(nullptr),
      layoutRequested(false),
      firstLayout(true),
      sizeToFit(false),
      centerContent(false),
      markedForRepaint(false) {
    if (hwnd)
        SetHwnd(hwnd);
}

HwndWrapper::~HwndWrapper() {
    delete evtMgr;
    delete painter;
}

// Set minimum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMinSize(Size s) {
    evtMgr->SetMinSize(s);
}

// Set maximum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMaxSize(Size s) {
    evtMgr->SetMaxSize(s);
}

void HwndWrapper::SetHwnd(HWND hwnd) {
    CrashIf(nullptr != hwndParent);
    hwndParent = hwnd;
    evtMgr = new EventMgr(this);
    painter = new Painter(this);
}

Size HwndWrapper::Measure(const Size availableSize) {
    if (layout) {
        return layout->Measure(availableSize);
    }
    if (children.size() == 1) {
        ILayout* l = children.at(0);
        return l->Measure(availableSize);
    }
    desiredSize = Size();
    return desiredSize;
}

void HwndWrapper::Arrange(const Rect finalRect) {
    if (layout) {
        // might over-write position if our layout knows about us
        layout->Arrange(finalRect);
    } else {
        if (children.size() == 1) {
            ILayout* l = children.at(0);
            l->Arrange(finalRect);
        }
    }
}

// called when either the window size changed (as a result
// of WM_SIZE) or when the content of the window changes
void HwndWrapper::TopLevelLayout() {
    CrashIf(!hwndParent);
    ClientRect rc(hwndParent);
    Size availableSize(rc.dx, rc.dy);
    // lf("(%3d,%3d) HwndWrapper::TopLevelLayout()", rc.dx, rc.dy);
    Size s = Measure(availableSize);

    if (firstLayout && sizeToFit) {
        firstLayout = false;
        desiredSize = s;
        ResizeHwndToClientArea(hwndParent, s.Width, s.Height, false);
        layoutRequested = false;
        return;
    }

    desiredSize = availableSize;
    Rect r(0, 0, availableSize.Width, availableSize.Height);
    SetPosition(r);
    if (centerContent) {
        int n = availableSize.Width - s.Width;
        if (n > 0) {
            r.X = n / 2;
            r.Width = s.Width;
        }
        n = availableSize.Height - s.Height;
        if (n > 0) {
            r.Y = n / 2;
            r.Height = s.Height;
        }
    }
    Arrange(r);
    layoutRequested = false;
}

// mark for re-layout as soon as possible
void HwndWrapper::RequestLayout() {
    layoutRequested = true;
    markedForRepaint = true;
    // trigger message queue so that the layout request is processed
    InvalidateRect(hwndParent, nullptr, TRUE);
    UpdateWindow(hwndParent);
}

void HwndWrapper::LayoutIfRequested() {
    if (layoutRequested) {
        TopLevelLayout();
    }
}

void HwndWrapper::OnPaint(HWND hwnd) {
    CrashIf(hwnd != hwndParent);
    auto t = TimeGet();
    painter->Paint(hwnd, markedForRepaint);
    if (frameRateWnd) {
        auto dur = TimeSinceInMs(t);
        frameRateWnd->ShowFrameRateDur(dur);
    }
    markedForRepaint = false;
}

bool HwndWrapper::IsInSizeMove() const {
    return evtMgr->IsInSizeMove();
}
} // namespace mui
