/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/Layout.h"
#include "gui/Layout_win.h"

void LayoutAndSizeToContent(ILayout* layout, int minDx, int minDy, HWND hwnd) {
    DpiSetFromHwnd(hwnd);
    dbglayout(fmt("\nLayoutAndSizeToContent() %d,%d\n", minDx, minDy));

    Constraints c = ExpandInf();
    c.min = {minDx, minDy};
    auto size = layout->Layout(c);
    Point min{0, 0};
    Point max{size.dx, size.dy};
    Rect bounds{min, max};
    layout->SetBounds(bounds);
    ResizeHwndToClientArea(hwnd, size.dx, size.dy, false);
    HwndScheduleRepaint(hwnd);
}

static Kind kindHwndSlot = "hwnd-slot";

HwndSlot::HwndSlot(HWND hwndIn, int dxIn, int dyIn) {
    kind = kindHwndSlot;
    hwnd = hwndIn;
    dx = dxIn;
    dy = dyIn;
}

HwndSlot::~HwndSlot() {
    // does not own hwnd
}

Size HwndSlot::Layout(const Constraints bc) {
    return bc.Constrain({dx, dy});
}

int HwndSlot::MinIntrinsicHeight(int /*width*/) {
    return dy;
}

int HwndSlot::MinIntrinsicWidth(int /*height*/) {
    return dx;
}

// Move the HWND into bounds (batched when winPos is set). A null or collapsed
// slot still records lastBounds so callers can place a lazily-created window.
void HwndSlot::SetBounds(Rect bounds) {
    lastBounds = bounds;
    if (!hwnd || IsCollapsed(this)) {
        return;
    }
    if (mapRtlX) {
        HWND parent = GetParent(hwnd);
        bounds.x = HwndMapChildXForRtlParent(parent, bounds.x, bounds.dx);
    }
    // A no-op SetWindowPos still sends WM_WINDOWPOSCHANGED and the TOC tree
    // shimmers 1-2px. Window resize must not touch the sidebar when its
    // client rect did not change (width is independent of the frame).
    if (ChildPosWithinParent(hwnd) == bounds) {
        return;
    }
    if (winPos) {
        winPos->MoveWindow(hwnd, bounds);
        return;
    }
    HwndMoveWindow(hwnd, &bounds);
}
