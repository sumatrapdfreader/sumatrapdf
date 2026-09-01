/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"

#include "gui/Layout.h"
#if IS_DEBUG
#include "base/UtAssert.h"
#endif
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

#if IS_DEBUG

void LayoutWin_UnitTests() {
    // A slot without an HWND still records its bounds for lazily-created windows.
    HwndSlot slot(nullptr, 30, 20);
    Size sz = slot.Layout(Loose(Size{100, 100}));
    utassert(sz.dx == 30 && sz.dy == 20);
    slot.SetBounds(Rect{5, 6, 40, 41});
    utassert(slot.lastBounds.x == 5 && slot.lastBounds.y == 6);
    utassert(slot.lastBounds.dx == 40 && slot.lastBounds.dy == 41);
}
#endif
