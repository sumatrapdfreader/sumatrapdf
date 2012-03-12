/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "DebugLog.h"

namespace mui {

// Set minimum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMinSize(Size s)
{
    evtMgr->SetMinSize(s);
}

// Set maximum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMaxSize(Size s)
{
    evtMgr->SetMaxSize(s);
}

void HwndWrapper::SetHwnd(HWND hwnd)
{
    CrashIf(NULL != hwndParent);
    hwndParent = hwnd;
    evtMgr = new EventMgr(this);
    painter = new Painter(this);
}

// called when either the window size changed (as a result
// of WM_SIZE) or when window tree changes
void HwndWrapper::TopLevelLayout()
{
    CrashIf(!hwndParent);
    ClientRect rc(hwndParent);
    Size availableSize(rc.dx, rc.dy);
    //lf("(%3d,%3d) HwndWrapper::TopLevelLayout()", rc.dx, rc.dy);
    Measure(availableSize);
    desiredSize = availableSize;
    Rect r(0, 0, desiredSize.Width, desiredSize.Height);
    Arrange(r);
    layoutRequested = false;
}

HwndWrapper::HwndWrapper(HWND hwnd)
    : painter(NULL), evtMgr(NULL), layoutRequested(false)
{
    if (hwnd)
        SetHwnd(hwnd);
}

HwndWrapper::~HwndWrapper()
{
    delete evtMgr;
    delete painter;
}

// mark for re-layout at the earliest convenience
void HwndWrapper::RequestLayout()
{
    layoutRequested = true;
    repaintRequested = true;
    // trigger message queue so that the layout request is processed
    InvalidateRect(hwndParent, NULL, TRUE);
    UpdateWindow(hwndParent);
}

void HwndWrapper::LayoutIfRequested()
{
    if (layoutRequested)
        TopLevelLayout();
}

void HwndWrapper::OnPaint(HWND hwnd)
{
    CrashIf(hwnd != hwndParent);
    painter->Paint(hwnd, repaintRequested);
    repaintRequested = false;
}

}
