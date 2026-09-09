/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Layout.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// Unit tests for the core layout engine. They use Spacer as a pure, HWND-free
// leaf (fixed intrinsic size, and its SetBounds records lastBounds), so whole
// layout trees can be exercised and their computed geometry asserted.

static bool LayoutRectEq(const Rect& r, int x, int y, int dx, int dy) {
    return r.x == x && r.y == y && r.dx == dx && r.dy == dy;
}

static void Layout_TestPrimitives() {
    utassert(Clamp(5, 0, 10) == 5);
    utassert(Clamp(-3, 0, 10) == 0);
    utassert(Clamp(50, 0, 10) == 10);

    utassert(Scale(10, 3, 2) == 15);
    utassert(Scale(10, 1, 0) == 0);   // divide-by-zero is guarded
    utassert(Scale(100, 1, 3) == 33); // truncates toward zero

    utassert(GuardInf(Inf, 5) == Inf);
    utassert(GuardInf(3, 5) == 5);

    Constraints t = Tight(Size{100, 50});
    utassert(t.IsTight() && t.HasBoundedWidth() && t.HasBoundedHeight());
    Size ts = t.Constrain(Size{999, 1});
    utassert(ts.dx == 100 && ts.dy == 50); // tight clamps both ways

    Constraints l = Loose(Size{100, 50});
    utassert(!l.IsTight());
    Size ls = l.Constrain(Size{200, 10});
    utassert(ls.dx == 100 && ls.dy == 10);

    Constraints e = ExpandInf();
    utassert(!e.HasBoundedWidth() && !e.HasBoundedHeight());

    Constraints tw = Loose(Size{100, 50}).TightenWidth(40);
    utassert(tw.min.dx == 40 && tw.max.dx == 40 && tw.max.dy == 50);

    // Inset shrinks max, leaving Inf as Inf
    Constraints ins = ExpandInf().Inset(20, 10);
    utassert(!ins.HasBoundedWidth() && !ins.HasBoundedHeight());
    Constraints ins2 = Loose(Size{100, 80}).Inset(20, 10);
    utassert(ins2.max.dx == 80 && ins2.max.dy == 70);
}

static void Layout_TestSpacer() {
    Spacer s(30, 20);
    Size sz = s.Layout(Loose(Size{100, 100}));
    utassert(sz.dx == 30 && sz.dy == 20);
    Size sz2 = s.Layout(Tight(Size{50, 60}));
    utassert(sz2.dx == 50 && sz2.dy == 60); // tight constraints win
    // regression: SetBounds must record lastBounds (used to be a no-op, which
    // left the AI chat panel's webview slot sized 0x0)
    s.SetBounds(Rect{5, 6, 40, 41});
    utassert(LayoutRectEq(s.lastBounds, 5, 6, 40, 41));
}

static void Layout_TestPadding() {
    auto* sp = new Spacer(40, 20);
    auto* pad = new Padding(sp, Insets{5, 10, 5, 10}); // top,right,bottom,left
    Size sz = pad->Layout(Loose(Size{200, 200}));
    utassert(sz.dx == 60 && sz.dy == 30); // child + horizontal/vertical insets
    pad->SetBounds(Rect{0, 0, 60, 30});
    utassert(LayoutRectEq(sp->lastBounds, 10, 5, 40, 20)); // inset by left/top
    delete pad;
}

static void Layout_TestVBoxFixed() {
    auto* s0 = new Spacer(50, 10);
    auto* s1 = new Spacer(30, 20);
    auto* vb = new VBox();
    vb->AddChild(s0);
    vb->AddChild(s1);
    LayoutToSize(vb, Size{100, 100});
    // CrossStart (default): natural width, x=0, stacked vertically
    utassert(LayoutRectEq(s0->lastBounds, 0, 0, 50, 10));
    utassert(LayoutRectEq(s1->lastBounds, 0, 10, 30, 20));
    delete vb;
}

static void Layout_TestVBoxAlign() {
    // CrossStretch: children stretched to full width
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignCross = CrossAxisAlign::Stretch;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(LayoutRectEq(s0->lastBounds, 0, 0, 100, 10));
        utassert(LayoutRectEq(s1->lastBounds, 0, 10, 100, 20));
        delete vb;
    }
    // MainCenter: total height 30 centered in 100 -> offset 35
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignMain = MainAxisAlign::MainCenter;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(s0->lastBounds.y == 35 && s1->lastBounds.y == 45);
        delete vb;
    }
    // MainEnd: bottom-aligned
    {
        auto* s0 = new Spacer(50, 10);
        auto* s1 = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->alignMain = MainAxisAlign::MainEnd;
        vb->AddChild(s0);
        vb->AddChild(s1);
        LayoutToSize(vb, Size{100, 100});
        utassert(s0->lastBounds.y == 70 && s1->lastBounds.y == 80);
        delete vb;
    }
}

static void Layout_TestVBoxFlex() {
    // one flex slot fills the remaining height (the AI chat panel layout)
    {
        auto* top = new Spacer(0, 10);
        auto* slot = new Spacer(0, 0);
        auto* bottom = new Spacer(0, 10);
        auto* vb = new VBox();
        vb->alignCross = CrossAxisAlign::Stretch;
        vb->AddChild(top);
        vb->AddChild(slot, 1);
        vb->AddChild(bottom);
        LayoutToSize(vb, Size{200, 100});
        utassert(LayoutRectEq(top->lastBounds, 0, 0, 200, 10));
        utassert(LayoutRectEq(slot->lastBounds, 0, 10, 200, 80));
        utassert(LayoutRectEq(bottom->lastBounds, 0, 90, 200, 10));
        delete vb;
    }
    // two flex children with non-zero natural height split the extra evenly
    {
        auto* a = new Spacer(0, 10);
        auto* b = new Spacer(0, 30);
        auto* vb = new VBox();
        vb->AddChild(a, 1);
        vb->AddChild(b, 1);
        LayoutToSize(vb, Size{50, 100});
        // extra = 100-40 = 60, split 30/30 -> 40 and 60
        utassert(a->lastBounds.y == 0 && a->lastBounds.dy == 40);
        utassert(b->lastBounds.y == 40 && b->lastBounds.dy == 60);
        delete vb;
    }
}

static void Layout_TestHBoxFlex() {
    // two flex children with non-zero natural width split the extra evenly.
    // Regression: the old grow formula Scale(w+extra, flex, total) under-filled
    // when flex children had non-zero natural widths.
    auto* a = new Spacer(10, 0);
    auto* b = new Spacer(30, 0);
    auto* hb = new HBox();
    hb->AddChild(a, 1);
    hb->AddChild(b, 1);
    LayoutToSize(hb, Size{100, 20});
    // extra = 100-40 = 60, split 30/30 -> 40 and 60
    utassert(a->lastBounds.x == 0 && a->lastBounds.dx == 40);
    utassert(b->lastBounds.x == 40 && b->lastBounds.dx == 60);
    delete hb;
}

static void Layout_TestHBoxCrossCenter() {
    auto* a = new Spacer(20, 10);
    auto* b = new Spacer(30, 40);
    auto* hb = new HBox();
    hb->alignCross = CrossAxisAlign::CrossCenter;
    hb->AddChild(a);
    hb->AddChild(b);
    LayoutToSize(hb, Size{100, 50});
    // a: height 10 vertically centered in 50 -> y=20; b: height 40 -> y=5
    utassert(LayoutRectEq(a->lastBounds, 0, 20, 20, 10));
    utassert(LayoutRectEq(b->lastBounds, 20, 5, 30, 40));
    delete hb;
}

static void Layout_TestBoxGap() {
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 20);
        auto* hb = new HBox();
        hb->gap = 7;
        hb->AddChild(a);
        hb->AddChild(b);
        Size size = hb->Layout(Loose(Size{100, 100}));
        utassert(size.dx == 57 && size.dy == 20);
        hb->SetBounds(Rect{0, 0, size.dx, size.dy});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 27, 0, 30, 20));
        delete hb;
    }
    {
        auto* a = new Spacer(20, 10);
        auto* collapsed = new Spacer(40, 40);
        auto* b = new Spacer(30, 20);
        auto* vb = new VBox();
        vb->gap = 7;
        vb->AddChild(a);
        vb->AddChild(collapsed);
        vb->AddChild(b);
        collapsed->SetVisibility(Visibility::Collapse);
        Size size = vb->Layout(Loose(Size{100, 100}));
        utassert(size.dx == 30 && size.dy == 37);
        vb->SetBounds(Rect{0, 0, size.dx, size.dy});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 0, 17, 30, 20));
        delete vb;
    }
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->alignMain = MainAxisAlign::Homogeneous;
        hb->gap = 10;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 45, 10));
        utassert(LayoutRectEq(b->lastBounds, 55, 0, 45, 10));
        delete hb;
    }
}

static void Layout_TestCollapsed() {
    auto* s0 = new Spacer(50, 10);
    auto* s1 = new Spacer(50, 20);
    auto* s2 = new Spacer(50, 30);
    auto* vb = new VBox();
    vb->AddChild(s0);
    vb->AddChild(s1);
    vb->AddChild(s2);
    s1->SetVisibility(Visibility::Collapse);
    LayoutToSize(vb, Size{50, 40});
    // collapsed s1 takes no space; s2 stacks right after s0
    utassert(LayoutRectEq(s0->lastBounds, 0, 0, 50, 10));
    utassert(s2->lastBounds.y == 10 && s2->lastBounds.dy == 30);
    utassert(LayoutRectEq(s1->lastBounds, 0, 0, 0, 0)); // never positioned
    delete vb;
}

static void Layout_TestAlign() {
    auto* child = new Spacer(20, 10);
    auto* al = new Align(child);
    al->HAlign = AlignCenter;
    al->VAlign = AlignCenter;
    al->Layout(Loose(Size{100, 100}));
    al->SetBounds(Rect{0, 0, 100, 100});
    // 20x10 centered in 100x100 -> x=40, y=45
    utassert(LayoutRectEq(child->lastBounds, 40, 45, 20, 10));
    delete al;
}

static void Layout_TestOverlay() {
    auto* body = new Spacer(80, 20);
    auto* close = new Spacer(10, 10);
    auto* ov = new Overlay();
    ov->AddChild(body);
    ov->AddChild(close, CrossAxisAlign::CrossEnd, CrossAxisAlign::CrossCenter);
    LayoutToSize(ov, Size{80, 20});
    // body fills the overlay; close sits at the right, vertically centered
    utassert(LayoutRectEq(body->lastBounds, 0, 0, 80, 20));
    utassert(LayoutRectEq(close->lastBounds, 70, 5, 10, 10));
    delete ov;
}

static void Layout_TestWrap() {
    // three 40-wide children in a 90-wide box wrap to two + one
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* c = new Spacer(40, 10);
        auto* w = new Wrap();
        w->AddChild(a);
        w->AddChild(b);
        w->AddChild(c);
        LayoutToSize(w, Size{90, 100});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 40, 10));
        utassert(LayoutRectEq(b->lastBounds, 40, 0, 40, 10));
        utassert(LayoutRectEq(c->lastBounds, 0, 10, 40, 10));
        delete w;
    }
    // flex extra on a row goes to the flex child
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* w = new Wrap();
        w->AddChild(a, 1);
        w->AddChild(b);
        LayoutToSize(w, Size{100, 20});
        utassert(LayoutRectEq(a->lastBounds, 0, 0, 60, 10));
        utassert(LayoutRectEq(b->lastBounds, 60, 0, 40, 10));
        delete w;
    }
    // rtl: each row packs from the right
    {
        auto* a = new Spacer(40, 10);
        auto* b = new Spacer(40, 10);
        auto* c = new Spacer(40, 10);
        auto* w = new Wrap();
        w->rtl = true;
        w->AddChild(a);
        w->AddChild(b);
        w->AddChild(c);
        LayoutToSize(w, Size{90, 100});
        utassert(LayoutRectEq(a->lastBounds, 50, 0, 40, 10));
        utassert(LayoutRectEq(b->lastBounds, 10, 0, 40, 10));
        utassert(LayoutRectEq(c->lastBounds, 50, 10, 40, 10));
        delete w;
    }
}

static void Layout_TestHBoxRtl() {
    // MainStart packs to the right: first child at the right edge
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->rtl = true;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 80, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 50, 0, 30, 10));
        delete hb;
    }
    // MainEnd packs toward the left (the RTL end)
    {
        auto* a = new Spacer(20, 10);
        auto* b = new Spacer(30, 10);
        auto* hb = new HBox();
        hb->rtl = true;
        hb->alignMain = MainAxisAlign::MainEnd;
        hb->AddChild(a);
        hb->AddChild(b);
        LayoutToSize(hb, Size{100, 10});
        utassert(LayoutRectEq(a->lastBounds, 30, 0, 20, 10));
        utassert(LayoutRectEq(b->lastBounds, 0, 0, 30, 10));
        delete hb;
    }
}

void Layout_UnitTests() {
    Layout_TestPrimitives();
    Layout_TestSpacer();
    Layout_TestPadding();
    Layout_TestVBoxFixed();
    Layout_TestVBoxAlign();
    Layout_TestVBoxFlex();
    Layout_TestHBoxFlex();
    Layout_TestHBoxCrossCenter();
    Layout_TestBoxGap();
    Layout_TestCollapsed();
    Layout_TestAlign();
    Layout_TestOverlay();
    Layout_TestWrap();
    Layout_TestHBoxRtl();
}

#if OS_WIN

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
