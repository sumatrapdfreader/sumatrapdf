/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/Layout.h"
#include "gui/Layout_win.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

void LayoutWin_UnitTests() {
    // A slot without an HWND still records its bounds for lazily-created windows.
    HwndSlot slot(nullptr, 30, 20);
    Size sz = slot.Layout(Loose(Size{100, 100}));
    utassert(sz.dx == 30 && sz.dy == 20);
    slot.SetBounds(Rect{5, 6, 40, 41});
    utassert(slot.lastBounds.x == 5 && slot.lastBounds.y == 6);
    utassert(slot.lastBounds.dx == 40 && slot.lastBounds.dy == 41);
}
