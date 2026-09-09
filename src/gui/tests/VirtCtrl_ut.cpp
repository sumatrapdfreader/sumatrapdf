/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// Unit tests for Table (ILayout grid). VirtSpacer is the leaf: a fixed
// ideal size and no HWND, so a whole table can be laid out and its geometry
// asserted. CollectVirtCtrls finds the cell VirtCtrls as tops.

static bool VirtCtrlRectEq(const Rect& r, int x, int y, int dx, int dy) {
    return r.x == x && r.y == y && r.dx == dx && r.dy == dy;
}

static void Table_TestGrid() {
    auto* t = new Table();
    t->SetSize(2, 2);
    t->colGap = 10;
    t->rowGap = 4;
    auto* a = new VirtSpacer(20, 10);
    auto* b = new VirtSpacer(40, 30);
    auto* c = new VirtSpacer(30, 20);
    t->SetCell(0, 0, a);
    t->SetCell(0, 1, b);
    t->SetCell(1, 0, c);
    Size sz = t->Layout(ExpandInf());
    // a column is as wide as its widest cell, a row as tall as its tallest
    utassert(t->ColWidth(0) == 30 && t->ColWidth(1) == 40);
    utassert(t->RowHeight(0) == 30 && t->RowHeight(1) == 20);
    utassert(sz.dx == 30 + 10 + 40 && sz.dy == 30 + 4 + 20);
    t->SetBounds(Rect{0, 0, sz.dx, sz.dy});
    utassert(VirtCtrlRectEq(a->lastBounds, 0, 0, 20, 10));
    utassert(VirtCtrlRectEq(b->lastBounds, 40, 0, 40, 30));
    utassert(VirtCtrlRectEq(c->lastBounds, 0, 34, 30, 20));
    // an empty cell doesn't disturb the tracks
    utassert(t->GetCell(1, 1) == nullptr);
    delete t;
}

static void Table_TestAlign() {
    auto* t = new Table();
    t->SetSize(3, 2);
    // sets col 0 to 100 wide and row 0 to 40 tall, so the other cells have
    // room to be aligned in
    auto* big = new VirtSpacer(100, 40);
    auto* bottom = new VirtSpacer(20, 10);
    auto* center = new VirtSpacer(20, 10);
    auto* stretch = new VirtSpacer(20, 10);
    t->SetCell(0, 0, big);
    t->SetCell(0, 1, bottom).alignV = CrossAxisAlign::CrossEnd;
    t->SetCell(1, 0, center).alignH = CrossAxisAlign::CrossCenter;
    t->SetCell(2, 0, stretch).alignH = CrossAxisAlign::Stretch;
    Size sz = t->Layout(ExpandInf());
    t->SetBounds(Rect{0, 0, sz.dx, sz.dy});
    // 20 wide centered in the 100-wide column -> x = 40
    utassert(VirtCtrlRectEq(center->lastBounds, 40, 40, 20, 10));
    // 10 tall pushed to the bottom of the 40-tall row
    utassert(VirtCtrlRectEq(bottom->lastBounds, 100, 30, 20, 10));
    // stretched to the full column width
    utassert(VirtCtrlRectEq(stretch->lastBounds, 0, 50, 100, 10));
    delete t;
}

static void Table_TestSpan() {
    auto* t = new Table();
    t->SetSize(2, 2);
    t->colGap = 10;
    auto* wide = new VirtSpacer(100, 10);
    auto* a = new VirtSpacer(20, 10);
    auto* b = new VirtSpacer(30, 10);
    t->SetCell(0, 0, wide, 1, 2);
    t->SetCell(1, 0, a);
    t->SetCell(1, 1, b);
    utassert(t->CellAt(0, 1)->covered);
    Size sz = t->Layout(ExpandInf());
    // the columns give the spanning cell only 20 + 10 + 30, so both grow by 20
    utassert(t->ColWidth(0) == 40 && t->ColWidth(1) == 50);
    utassert(sz.dx == 100);
    t->SetBounds(Rect{0, 0, sz.dx, sz.dy});
    utassert(VirtCtrlRectEq(wide->lastBounds, 0, 0, 100, 10));
    utassert(b->lastBounds.x == 50);
    delete t;

    // the same for rows
    auto* t2 = new Table();
    t2->SetSize(2, 2);
    t2->rowGap = 6;
    auto* tall = new VirtSpacer(10, 100);
    t2->SetCell(0, 0, tall, 2, 1);
    t2->SetCell(0, 1, new VirtSpacer(10, 20));
    t2->SetCell(1, 1, new VirtSpacer(10, 30));
    Size sz2 = t2->Layout(ExpandInf());
    // rows of 20 and 30 (+ the 6 gap) leave 44 missing, split evenly
    utassert(t2->RowHeight(0) == 42 && t2->RowHeight(1) == 52);
    utassert(sz2.dy == 100);
    t2->SetBounds(Rect{0, 0, sz2.dx, sz2.dy});
    utassert(VirtCtrlRectEq(tall->lastBounds, 0, 0, 10, 100));
    delete t2;
}

// the cells' children must be reachable as tops through CollectVirtCtrls, or
// the links of a table-laid-out screen (About) stop being clickable
static void Table_TestHitTest() {
    auto* t = new Table();
    t->SetSize(1, 2);
    t->colGap = 10;
    auto* a = new VirtSpacer(20, 10);
    auto* b = new VirtSpacer(30, 10);
    // a spacer is decorative by default; make these hit targets
    a->SetFlag(vwfNoHitTest, false);
    b->SetFlag(vwfNoHitTest, false);
    t->SetCell(0, 0, a);
    t->SetCell(0, 1, b);
    Size sz = t->Layout(ExpandInf());
    t->SetBounds(Rect{5, 7, sz.dx, sz.dy});

    VirtRoot root((HWND)1);
    root.bounds = {0, 0, 200, 100};
    Vec<VirtCtrl*> tops;
    CollectVirtCtrls(t, tops);
    root.SetTops(tops);

    Point local{0, 0};
    utassert(ElementFromPoint(&root, {6, 8}, &local) == a);
    utassert(ElementFromPoint(&root, {40, 8}, &local) == b);
    // the gap between the columns is a miss
    utassert(ElementFromPoint(&root, {30, 8}, &local) == nullptr);
    delete t;
}

// a layout tree mixing plain layouts and virtual controls yields the virtual
// ones, in layout order, without descending into their own children
static void CollectVirtCtrls_Test() {
    Vec<VirtCtrl*> out;
    CollectVirtCtrls(nullptr, out);
    utassert(len(out) == 0);

    // a tree of no virtual controls yields none
    auto* plain = new VBox();
    plain->AddChild(new Spacer(10, 10));
    CollectVirtCtrls(plain, out);
    utassert(len(out) == 0);
    delete plain;

    auto* box = new VBox();
    auto* first = new VirtSpacer(10, 10);
    auto* nested = new VirtSpacer(10, 10);
    auto* inner = new VirtSpacer(10, 10);
    // a child of a virtual control is not top-level: `nested` paints it
    nested->AddChild(inner);
    box->AddChild(new Spacer(5, 5));
    box->AddChild(first);
    box->AddChild(new Padding(nested, DefaultInsets()));
    CollectVirtCtrls(box, out);
    utassert(len(out) == 2);
    utassert(out[0] == first);
    utassert(out[1] == nested);
    delete box;
}

static void CollectTabStops_Test() {
    Vec<TabStop> out;
    CollectTabStops(nullptr, out);
    utassert(len(out) == 0);

    // only what can take focus is a stop, in layout order
    auto* box = new VBox();
    auto* b1 = new VirtButton(StrL("one"));
    auto* b2 = new VirtButton(StrL("two"));
    box->AddChild(new Spacer(5, 5));
    box->AddChild(new VirtSpacer(10, 10));
    box->AddChild(b1);
    box->AddChild(new Padding(b2, DefaultInsets()));
    CollectTabStops(box, out);
    utassert(len(out) == 2);
    utassert(out[0].vwnd == b1 && !out[0].ctrl);
    utassert(out[1].vwnd == b2);

    // a collapsed subtree is out of the ring
    VecReset(out);
    b1->SetVisibility(Visibility::Collapse);
    CollectTabStops(box, out);
    utassert(len(out) == 1);
    utassert(out[0].vwnd == b2);
    delete box;
}

static void ScrollBox_Test() {
    auto* inner = new VBox();
    inner->AddChild(new Spacer(40, 200));
    auto* sb = new ScrollBox(inner);
    Size full = sb->Layout(ExpandInf());
    utassert(full.dy == 200);
    Size view = sb->Layout(Tight({40, 80}));
    utassert(view.dy == 80);
    utassert(sb->contentSize.dy == 200);
    sb->SetBounds({0, 0, 40, 80});
    utassert(sb->MaxScrollY() == 120);
    utassert(sb->ScrollTo(50));
    utassert(sb->scrollY == 50);
    utassert(!sb->ScrollTo(50));
    utassert(sb->ScrollTo(999));
    utassert(sb->scrollY == 120);
    delete sb;
}

void VirtCtrl_UnitTests() {
    Table_TestGrid();
    Table_TestAlign();
    Table_TestSpan();
    Table_TestHitTest();
    CollectVirtCtrls_Test();
    CollectTabStops_Test();
    ScrollBox_Test();
}
