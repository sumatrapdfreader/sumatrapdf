/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

//--- VirtWnd

static Kind kindVirtWnd = "virtWnd";

VirtWnd::VirtWnd() {
    kind = kindVirtWnd;
}

VirtWnd::~VirtWnd() {
    if (root) {
        root->OnWndDestroyed(this);
    }
    RemoveAllChildren(true);
}

bool IsVirtWndOfKind(VirtWnd* w, Kind k) {
    return w && w->kind == k;
}

// ILayout: the containers in Layout.h hand us an absolute (HWND client) rect,
// and a parent is always laid out before its children, so we can rebase it into
// the parent-relative form the paint / hit-test walks expect
void VirtWnd::SetBounds(Rect r) {
    lastBounds = r;
    Point po{0, 0};
    if (parent) {
        po = parent->ChildOriginInWindow();
    } else if (root) {
        po = root->bounds.TL();
    }
    bounds = {r.x - po.x, r.y - po.y, r.dx, r.dy};
}

Size VirtWnd::Layout(Constraints bc) {
    Size sz = GetIdealSize();
    return bc.Constrain(sz);
}

int VirtWnd::MinIntrinsicHeight(int) {
    return GetIdealSize().dy;
}

int VirtWnd::MinIntrinsicWidth(int) {
    return GetIdealSize().dx;
}

Size VirtWnd::GetIdealSize() {
    return {bounds.dx, bounds.dy};
}

void VirtWnd::Paint(VirtPaintCtx&) {}

// origin is the window position of our parent's content origin (already
// adjusted for the parent's scroll offset)
void VirtWnd::PaintTree(Gfx* gfx, Point origin, Rect clip) {
    if (visibility != Visibility::Visible) {
        return;
    }
    Rect b = bounds;
    b.Offset(origin.x, origin.y);
    if (b.Intersect(clip).IsEmpty()) {
        return;
    }
    Rect content = b;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);

    VirtPaintCtx ctx;
    ctx.gfx = gfx;
    ctx.bounds = b;
    ctx.content = content;
    ctx.clip = clip;
    Paint(ctx);

    if (HasFlag(vwfPaintsOwnChildren) || ChildCount() == 0) {
        return;
    }
    if (HasFlag(vwfClipChildren)) {
        ctx.clip = clip.Intersect(b);
        if (ctx.clip.IsEmpty()) {
            return;
        }
    }
    PaintChildren(ctx);
}

void VirtWnd::PaintChildren(VirtPaintCtx& ctx) {
    Point so = ScrollOffset();
    Point origin{ctx.content.x - so.x, ctx.content.y - so.y};
    for (VirtWnd* c : children) {
        c->PaintTree(ctx.gfx, origin, ctx.clip);
    }
}

// paints a standalone (parent-less) VirtWnd at the position it was given by
// SetBounds(). Used for one-off "measure a string and draw it" cases
void VirtWnd::PaintStandalone(Gfx* gfx) {
    PaintTree(gfx, {0, 0}, lastBounds);
}

bool VirtWnd::HitTest(Point ptLocal) {
    Rect r{0, 0, bounds.dx, bounds.dy};
    return r.Contains(ptLocal);
}

Point VirtWnd::ScrollOffset() {
    return {0, 0};
}

// topmost (i.e. last) child wins, so walk back to front
VirtWnd* VirtWnd::WndFromPoint(Point ptWindow, Point* ptLocalOut) {
    if (!IsHitTestable()) {
        return nullptr;
    }
    Rect b = BoundsInWindow();
    if (!b.Contains(ptWindow)) {
        return nullptr;
    }
    if (!HasFlag(vwfPaintsOwnChildren)) {
        for (int i = ChildCount() - 1; i >= 0; i--) {
            VirtWnd* hit = children[i]->WndFromPoint(ptWindow, ptLocalOut);
            if (hit) {
                return hit;
            }
        }
    }
    if (HasFlag(vwfNoHitTest)) {
        return nullptr;
    }
    Point ptLocal{ptWindow.x - b.x, ptWindow.y - b.y};
    if (!HitTest(ptLocal)) {
        return nullptr;
    }
    if (ptLocalOut) {
        *ptLocalOut = ptLocal;
    }
    return this;
}

bool VirtWnd::OnMouseDown(VirtMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseUp(VirtMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseMove(VirtMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseWheel(VirtMouseEvent&) {
    return false;
}

bool VirtWnd::OnDoubleClick(VirtMouseEvent&) {
    return false;
}

bool VirtWnd::OnContextMenu(VirtMouseEvent&) {
    return false;
}

void VirtWnd::OnMouseEnter() {}

void VirtWnd::OnMouseLeave() {}

void VirtWnd::OnCaptureLost() {}

bool VirtWnd::OnKeyDown(VirtKeyEvent&) {
    return false;
}

bool VirtWnd::OnChar(int) {
    return false;
}

void VirtWnd::OnFocusChanged(bool) {}

bool VirtWnd::OnSetCursor(Point) {
    return false;
}

TempStr VirtWnd::GetTooltipTemp(Point) {
    return nullptr;
}

void VirtWnd::AddChild(VirtWnd* c) {
    InsertChild(c, -1);
}

void VirtWnd::InsertChild(VirtWnd* c, int idx) {
    ReportIf(!c);
    ReportIf(c->parent);
    c->parent = this;
    c->SetRoot(root);
    if (idx < 0 || idx >= len(children)) {
        children.Append(c);
    } else {
        children.InsertAt(idx, c);
    }
}

void VirtWnd::RemoveChild(VirtWnd* c, bool del) {
    int idx = children.Find(c);
    if (idx < 0) {
        return;
    }
    children.RemoveAt(idx);
    c->parent = nullptr;
    if (del) {
        delete c;
        return;
    }
    if (root) {
        root->OnWndDestroyed(c);
    }
    c->SetRoot(nullptr);
}

void VirtWnd::RemoveAllChildren(bool del) {
    // take a copy of the pointers first: ~VirtWnd() of a child can reach back
    // into us via root->OnWndDestroyed()
    Vec<VirtWnd*> tmp;
    for (VirtWnd* c : children) {
        tmp.Append(c);
    }
    children.Clear();
    for (VirtWnd* c : tmp) {
        c->parent = nullptr;
        if (del) {
            delete c;
        } else {
            if (root) {
                root->OnWndDestroyed(c);
            }
            c->SetRoot(nullptr);
        }
    }
}

int VirtWnd::ChildCount() const {
    return len(children);
}

VirtWnd* VirtWnd::ChildAt(int idx) const {
    if (idx < 0 || idx >= len(children)) {
        return nullptr;
    }
    return children[idx];
}

VirtWnd* VirtWnd::FindById(int wndId) {
    if (id == wndId) {
        return this;
    }
    for (VirtWnd* c : children) {
        VirtWnd* res = c->FindById(wndId);
        if (res) {
            return res;
        }
    }
    return nullptr;
}

Point VirtWnd::OriginInWindow() {
    Point p{0, 0};
    if (parent) {
        p = parent->ChildOriginInWindow();
    } else if (root) {
        p = root->bounds.TL();
    }
    return {p.x + bounds.x, p.y + bounds.y};
}

// where children are positioned: our content origin, shifted by our scroll
Point VirtWnd::ChildOriginInWindow() {
    Point o = OriginInWindow();
    Point so = ScrollOffset();
    return {o.x + padding.left - so.x, o.y + padding.top - so.y};
}

Rect VirtWnd::BoundsInWindow() {
    Point o = OriginInWindow();
    return {o.x, o.y, bounds.dx, bounds.dy};
}

Rect VirtWnd::ContentRectInWindow() {
    Rect r = BoundsInWindow();
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// our bounds clipped by every clipping ancestor, so that a wnd scrolled out of
// its container doesn't invalidate (or hit-test) outside of it
Rect VirtWnd::VisibleRectInWindow() {
    Rect r = BoundsInWindow();
    VirtWnd* p = parent;
    while (p && !r.IsEmpty()) {
        if (p->HasFlag(vwfClipChildren)) {
            r = r.Intersect(p->BoundsInWindow());
        }
        p = p->parent;
    }
    if (root) {
        r = r.Intersect(root->bounds);
    }
    return r;
}

void VirtWnd::Invalidate() {
    if (!root) {
        return;
    }
    root->Invalidate(VisibleRectInWindow());
}

void VirtWnd::Invalidate(Rect rLocal) {
    if (!root) {
        return;
    }
    Point o = OriginInWindow();
    Rect r = rLocal;
    r.Offset(o.x, o.y);
    root->Invalidate(r.Intersect(VisibleRectInWindow()));
}

void VirtWnd::RequestLayout() {
    if (root) {
        root->RequestLayout();
    }
}

HWND VirtWnd::GetHwnd() const {
    return root ? root->hwnd : nullptr;
}

bool VirtWnd::IsPaintable() const {
    return visibility == Visibility::Visible;
}

bool VirtWnd::IsHitTestable() const {
    if (visibility != Visibility::Visible) {
        return false;
    }
    return HasFlag(vwfEnabled);
}

void VirtWnd::SetFlag(u32 f, bool on) {
    if (on) {
        flags |= f;
    } else {
        flags &= ~f;
    }
}

bool VirtWnd::HasFlag(u32 f) const {
    return (flags & f) != 0;
}

void VirtWnd::SetRoot(VirtRoot* r) {
    root = r;
    for (VirtWnd* c : children) {
        c->SetRoot(r);
    }
}

//--- VirtRoot

VirtRoot::VirtRoot(HWND hwnd) {
    this->hwnd = hwnd;
}

VirtRoot::~VirtRoot() {
    delete child;
}

void VirtRoot::SetChild(VirtWnd* c) {
    if (child == c) {
        return;
    }
    delete child;
    child = c;
    hovered = nullptr;
    captured = nullptr;
    focused = nullptr;
    pressed = nullptr;
    if (child) {
        child->parent = nullptr;
        child->SetRoot(this);
    }
    needsLayout = true;
}

void VirtRoot::SetBounds(Rect r) {
    if (bounds == r) {
        return;
    }
    bounds = r;
    needsLayout = true;
}

void VirtRoot::RequestLayout() {
    needsLayout = true;
    HwndInvalidate(hwnd);
}

void VirtRoot::LayoutIfNeeded() {
    if (!needsLayout || !child) {
        return;
    }
    needsLayout = false;
    Constraints bc = Tight({bounds.dx, bounds.dy});
    child->Layout(bc);
    child->SetBounds(bounds);
}

void VirtRoot::Paint(Gfx* gfx, Rect clip) {
    if (!child) {
        return;
    }
    LayoutIfNeeded();
    Rect c = clip.Intersect(bounds);
    if (c.IsEmpty()) {
        return;
    }
    child->PaintTree(gfx, bounds.TL(), c);
}

VirtWnd* VirtRoot::WndFromPoint(Point ptWindow, Point* ptLocalOut) {
    if (!child || !bounds.Contains(ptWindow)) {
        return nullptr;
    }
    LayoutIfNeeded();
    return child->WndFromPoint(ptWindow, ptLocalOut);
}

void VirtRoot::Invalidate(Rect rWindow) {
    if (!hwnd || rWindow.IsEmpty()) {
        return;
    }
    HwndInvalidateRect(hwnd, rWindow, false);
}

void VirtRoot::SetFocus(VirtWnd* w) {
    if (w && !(w->IsHitTestable() && w->HasFlag(vwfFocusable))) {
        w = nullptr;
    }
    if (w == focused) {
        return;
    }
    VirtWnd* prev = focused;
    focused = w;
    if (prev) {
        prev->SetFlag(vwfFocused, false);
        prev->OnFocusChanged(false);
        prev->Invalidate();
    }
    if (focused) {
        focused->SetFlag(vwfFocused, true);
        focused->OnFocusChanged(true);
        focused->Invalidate();
    }
}

static void CollectFocusable(VirtWnd* w, Vec<VirtWnd*>& out) {
    if (!w || !w->IsHitTestable()) {
        return;
    }
    if (w->HasFlag(vwfFocusable) && !w->HasFlag(vwfSkipTabStop)) {
        out.Append(w);
    }
    for (VirtWnd* c : w->children) {
        CollectFocusable(c, out);
    }
}

bool VirtRoot::TabNavigate(bool backwards) {
    Vec<VirtWnd*> all;
    CollectFocusable(child, all);
    int n = len(all);
    if (n == 0) {
        return false;
    }
    int idx = focused ? all.Find(focused) : -1;
    if (idx < 0) {
        idx = backwards ? n - 1 : 0;
    } else {
        idx = backwards ? idx - 1 : idx + 1;
        if (idx < 0) {
            idx = n - 1;
        } else if (idx >= n) {
            idx = 0;
        }
    }
    SetFocus(all[idx]);
    return true;
}

void VirtRoot::SetCapture(VirtWnd* w) {
    captured = w;
    if (w && hwnd) {
        ::SetCapture(hwnd);
    }
}

void VirtRoot::ReleaseCapture() {
    VirtWnd* w = captured;
    captured = nullptr;
    if (!w) {
        return;
    }
    if (hwnd && ::GetCapture() == hwnd) {
        ::ReleaseCapture();
    }
    w->SetFlag(vwfPressed, false);
    w->OnCaptureLost();
    w->Invalidate();
}

void VirtRoot::ClearPressed() {
    VirtWnd* w = pressed;
    if (!w) {
        return;
    }
    pressed = nullptr;
    w->SetFlag(vwfPressed, false);
    w->Invalidate();
}

void VirtRoot::ClearHover() {
    VirtWnd* w = hovered;
    if (!w) {
        return;
    }
    hovered = nullptr;
    w->SetFlag(vwfHovered, false);
    w->OnMouseLeave();
    w->Invalidate();
}

// a wnd is going away: don't leave dangling hover / capture / focus pointers
void VirtRoot::OnWndDestroyed(VirtWnd* w) {
    if (hovered == w) {
        hovered = nullptr;
    }
    if (captured == w) {
        captured = nullptr;
        if (hwnd && ::GetCapture() == hwnd) {
            ::ReleaseCapture();
        }
    }
    if (focused == w) {
        focused = nullptr;
    }
    if (pressed == w) {
        pressed = nullptr;
    }
}

static void FillMouseEvent(VirtMouseEvent& ev, VirtWnd* target, Point ptWindow, Point ptLocal, bool captured) {
    ev.target = target;
    ev.hit = target;
    ev.ptWindow = ptWindow;
    ev.pt = captured ? ptWindow : ptLocal;
    ev.isCtrl = IsCtrlPressed();
    ev.isShift = IsShiftPressed();
    ev.isAlt = IsAltPressed();
}

// walks up from target until someone consumes the event
static bool BubbleMouse(VirtWnd* target, VirtMouseEvent& ev, bool (VirtWnd::*handler)(VirtMouseEvent&)) {
    VirtWnd* w = target;
    while (w) {
        Rect b = w->BoundsInWindow();
        ev.target = w;
        ev.pt = {ev.ptWindow.x - b.x, ev.ptWindow.y - b.y};
        if ((w->*handler)(ev)) {
            return true;
        }
        w = w->parent;
    }
    return false;
}

void VirtRoot::TrackMouseLeaveIfNeeded() {
    if (trackingMouseLeave || !hwnd) {
        return;
    }
    TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    if (TrackMouseEvent(&tme)) {
        trackingMouseLeave = true;
    }
}

bool VirtRoot::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    if (!child) {
        return false;
    }
    res = 0;
    Point ptWindow{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    Point ptLocal{0, 0};

    switch (msg) {
        case WM_MOUSEMOVE: {
            TrackMouseLeaveIfNeeded();
            VirtWnd* target = captured;
            if (target) {
                ptLocal = ptWindow;
            } else {
                target = WndFromPoint(ptWindow, &ptLocal);
            }
            if (target != hovered) {
                ClearHover();
                if (target) {
                    hovered = target;
                    target->SetFlag(vwfHovered, true);
                    target->OnMouseEnter();
                    target->Invalidate();
                }
            }
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, captured != nullptr);
            if (captured) {
                return target->OnMouseMove(ev);
            }
            return BubbleMouse(target, ev, &VirtWnd::OnMouseMove);
        }

        case WM_MOUSELEAVE:
            trackingMouseLeave = false;
            ClearHover();
            return false;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            VirtWnd* target = WndFromPoint(ptWindow, &ptLocal);
            ClearPressed();
            if (!target) {
                SetFocus(nullptr);
                return false;
            }
            if (target->HasFlag(vwfFocusable)) {
                SetFocus(target);
            }
            pressed = target;
            target->SetFlag(vwfPressed, true);
            target->Invalidate();
            if (target->HasFlag(vwfCapturesMouse)) {
                SetCapture(target);
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false);
            ev.button = (msg == WM_LBUTTONDOWN) ? 0 : ((msg == WM_RBUTTONDOWN) ? 1 : 2);
            return BubbleMouse(target, ev, &VirtWnd::OnMouseDown);
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            bool wasCaptured = captured != nullptr;
            VirtWnd* target = captured;
            if (target) {
                ptLocal = ptWindow;
            } else {
                target = WndFromPoint(ptWindow, &ptLocal);
            }
            if (!target) {
                ClearPressed();
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, wasCaptured);
            ev.button = (msg == WM_LBUTTONUP) ? 0 : ((msg == WM_RBUTTONUP) ? 1 : 2);
            // like the rest of the app, a click only counts when the button
            // went down and up on the same wnd
            bool wasPressed = wasCaptured || (pressed == target);
            ClearPressed();
            if (!wasPressed) {
                return false;
            }
            bool didHandle;
            if (wasCaptured) {
                didHandle = target->OnMouseUp(ev);
                ReleaseCapture();
            } else {
                didHandle = BubbleMouse(target, ev, &VirtWnd::OnMouseUp);
            }
            return didHandle;
        }

        case WM_LBUTTONDBLCLK: {
            VirtWnd* target = WndFromPoint(ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false);
            return BubbleMouse(target, ev, &VirtWnd::OnDoubleClick);
        }

        case WM_MOUSEWHEEL: {
            // wheel messages carry screen coords
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            ptWindow = {pt.x, pt.y};
            VirtWnd* target = WndFromPoint(ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false);
            ev.wheelDelta = GET_WHEEL_DELTA_WPARAM(wp);
            return BubbleMouse(target, ev, &VirtWnd::OnMouseWheel);
        }

        case WM_CONTEXTMENU: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (pt.x == -1 && pt.y == -1) {
                return false;
            }
            ScreenToClient(hwnd, &pt);
            ptWindow = {pt.x, pt.y};
            VirtWnd* target = WndFromPoint(ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false);
            return BubbleMouse(target, ev, &VirtWnd::OnContextMenu);
        }

        case WM_SETCURSOR: {
            Point pt = HwndGetCursorPos(hwnd);
            VirtWnd* target = WndFromPoint(pt, &ptLocal);
            while (target) {
                Rect b = target->BoundsInWindow();
                if (target->OnSetCursor({pt.x - b.x, pt.y - b.y})) {
                    res = TRUE;
                    return true;
                }
                target = target->parent;
            }
            return false;
        }

        case WM_KEYDOWN: {
            int vk = (int)wp;
            bool isCtrl = IsCtrlPressed();
            bool isAlt = IsAltPressed();
            if (vk == VK_TAB && !isCtrl && !isAlt) {
                if (TabNavigate(IsShiftPressed())) {
                    return true;
                }
                return false;
            }
            if (!focused) {
                return false;
            }
            VirtKeyEvent ev;
            ev.vkey = vk;
            ev.isCtrl = isCtrl;
            ev.isShift = IsShiftPressed();
            ev.isAlt = isAlt;
            VirtWnd* w = focused;
            while (w) {
                ev.target = w;
                if (w->OnKeyDown(ev)) {
                    return true;
                }
                w = w->parent;
            }
            return false;
        }

        case WM_CHAR: {
            if (!focused) {
                return false;
            }
            return focused->OnChar((int)wp);
        }
    }
    return false;
}

//--- VirtBox

static Kind kindVirtWndBox = "virtWndBox";

VirtBox::VirtBox(bool isVert) {
    kind = kindVirtWndBox;
    isVertical = isVert;
}

// the internal VBox / HBox deletes its children in its destructor, but the
// children are owned by VirtWnd::children, so unhook them first
static void FreeBox(VBox*& vbox, HBox*& hbox) {
    if (vbox) {
        vbox->children.Clear();
        delete vbox;
        vbox = nullptr;
    }
    if (hbox) {
        hbox->children.Clear();
        delete hbox;
        hbox = nullptr;
    }
}

VirtBox::~VirtBox() {
    FreeBox(vbox, hbox);
}

void VirtBox::AddChild(VirtWnd* c, int flexVal) {
    VirtWnd::AddChild(c);
    flexes.Append(flexVal);
    FreeBox(vbox, hbox);
}

void VirtBox::RebuildBox() {
    FreeBox(vbox, hbox);
    int n = ChildCount();
    if (isVertical) {
        vbox = new VBox();
        vbox->alignMain = alignMain;
        vbox->alignCross = alignCross;
        for (int i = 0; i < n; i++) {
            int flexVal = (i < len(flexes)) ? flexes[i] : 0;
            vbox->AddChild(children[i], flexVal);
        }
        return;
    }
    hbox = new HBox();
    hbox->alignMain = alignMain;
    hbox->alignCross = alignCross;
    for (int i = 0; i < n; i++) {
        int flexVal = (i < len(flexes)) ? flexes[i] : 0;
        hbox->AddChild(children[i], flexVal);
    }
}

ILayout* VirtBox::Box() {
    if (!vbox && !hbox) {
        RebuildBox();
    }
    return isVertical ? (ILayout*)vbox : (ILayout*)hbox;
}

int VirtBox::MinIntrinsicHeight(int width) {
    return Box()->MinIntrinsicHeight(width);
}

int VirtBox::MinIntrinsicWidth(int height) {
    return Box()->MinIntrinsicWidth(height);
}

Size VirtBox::Layout(Constraints bc) {
    Constraints inner = bc.Inset(padding.left + padding.right, padding.top + padding.bottom);
    Size sz = Box()->Layout(inner);
    sz.dx += padding.left + padding.right;
    sz.dy += padding.top + padding.bottom;
    return sz;
}

void VirtBox::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    Box()->SetBounds(content);
}

//--- VirtTable

static Kind kindVirtWndTable = "virtWndTable";

VirtTable::VirtTable() {
    kind = kindVirtWndTable;
    // a grid is decorative, only its cells' children are hit targets
    flags |= vwfNoHitTest;
}

// the cells' children are owned by VirtWnd::children, which ~VirtWnd frees
VirtTable::~VirtTable() = default;

int VirtTable::CellIdx(int row, int col) const {
    ReportIf(row < 0 || row >= rows);
    ReportIf(col < 0 || col >= cols);
    return (row * cols) + col;
}

void VirtTable::SetSize(int nRows, int nCols) {
    ReportIf(nRows < 0 || nCols < 0);
    if (nRows == rows && nCols == cols) {
        return;
    }
    RemoveAllChildren(true);
    rows = nRows;
    cols = nCols;
    cells.Clear();
    VirtTableCell empty;
    for (int i = 0; i < rows * cols; i++) {
        cells.Append(empty);
    }
    colWidths.Clear();
    rowHeights.Clear();
}

void VirtTable::MarkCovered(int row, int col, int rowSpan, int colSpan, bool covered) {
    for (int r = row; r < row + rowSpan; r++) {
        for (int c = col; c < col + colSpan; c++) {
            if (r == row && c == col) {
                continue;
            }
            VirtTableCell& cell = cells[CellIdx(r, c)];
            // a spanned-over cell can't hold a child of its own
            ReportIf(covered && cell.child);
            cell.covered = covered;
        }
    }
}

// (row, col) is the cell's top-left; a spanning cell covers the ones to its
// right / below, which must stay empty
VirtTableCell& VirtTable::SetCell(int row, int col, VirtWnd* child, int rowSpan, int colSpan) {
    ReportIf(rowSpan < 1 || colSpan < 1);
    ReportIf(row + rowSpan > rows || col + colSpan > cols);
    VirtTableCell& cell = cells[CellIdx(row, col)];
    ReportIf(cell.covered);
    MarkCovered(row, col, cell.rowSpan, cell.colSpan, false);
    if (cell.child) {
        RemoveChild(cell.child, true);
    }
    cell.child = child;
    cell.rowSpan = rowSpan;
    cell.colSpan = colSpan;
    if (child) {
        AddChild(child);
    }
    MarkCovered(row, col, rowSpan, colSpan, true);
    return cell;
}

VirtTableCell* VirtTable::CellAt(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) {
        return nullptr;
    }
    return &cells[CellIdx(row, col)];
}

VirtWnd* VirtTable::GetCell(int row, int col) {
    VirtTableCell* cell = CellAt(row, col);
    return cell ? cell->child : nullptr;
}

void VirtTable::RemoveAllCells() {
    RemoveAllChildren(true);
    VirtTableCell empty;
    for (int i = 0; i < len(cells); i++) {
        cells[i] = empty;
    }
}

// a cell spanning several tracks needs those tracks (plus the gaps between
// them) to be at least as big as the cell; grow them evenly if they aren't
static void GrowTracks(Vec<int>& tracks, int start, int span, int gap, int needed) {
    int have = gap * (span - 1);
    for (int i = start; i < start + span; i++) {
        have += tracks[i];
    }
    int missing = needed - have;
    if (missing <= 0) {
        return;
    }
    for (int i = 0; i < span; i++) {
        // the last track gets what rounding left over
        int add = missing / (span - i);
        tracks[start + i] += add;
        missing -= add;
    }
}

static int TracksSize(Vec<int>& tracks, int start, int span, int gap) {
    if (span < 1) {
        return 0;
    }
    int size = gap * (span - 1);
    for (int i = start; i < start + span; i++) {
        size += tracks[i];
    }
    return size;
}

// where track idx starts, relative to the first track
static int TracksStart(Vec<int>& tracks, int idx, int gap) {
    int pos = 0;
    for (int i = 0; i < idx; i++) {
        pos += tracks[i] + gap;
    }
    return pos;
}

// a column is as wide as its widest cell, a row as tall as its tallest. Cells
// that span several tracks are applied afterwards, so they only stretch the
// tracks they span when what those already give them isn't enough
void VirtTable::Measure() {
    colWidths.Clear();
    colWidths.AppendBlanks(cols);
    rowHeights.Clear();
    rowHeights.AppendBlanks(rows);

    Constraints loose = ExpandInf();
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            VirtTableCell& cell = cells[CellIdx(row, col)];
            cell.childSize = {0, 0};
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            cell.childSize = cell.child->Layout(loose);
            if (cell.colSpan == 1) {
                colWidths[col] = std::max(colWidths[col], cell.childSize.dx);
            }
            if (cell.rowSpan == 1) {
                rowHeights[row] = std::max(rowHeights[row], cell.childSize.dy);
            }
        }
    }

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            VirtTableCell& cell = cells[CellIdx(row, col)];
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            if (cell.colSpan > 1) {
                GrowTracks(colWidths, col, cell.colSpan, colGap, cell.childSize.dx);
            }
            if (cell.rowSpan > 1) {
                GrowTracks(rowHeights, row, cell.rowSpan, rowGap, cell.childSize.dy);
            }
        }
    }
}

Size VirtTable::TotalSize() {
    int dx = padding.left + padding.right + TracksSize(colWidths, 0, cols, colGap);
    int dy = padding.top + padding.bottom + TracksSize(rowHeights, 0, rows, rowGap);
    return {dx, dy};
}

int VirtTable::MinIntrinsicHeight(int) {
    Measure();
    return TotalSize().dy;
}

int VirtTable::MinIntrinsicWidth(int) {
    Measure();
    return TotalSize().dx;
}

Size VirtTable::Layout(Constraints bc) {
    Measure();
    return bc.Constrain(TotalSize());
}

int VirtTable::ColWidth(int col) {
    if (col < 0 || col >= len(colWidths)) {
        return 0;
    }
    return colWidths[col];
}

int VirtTable::RowHeight(int row) {
    if (row < 0 || row >= len(rowHeights)) {
        return 0;
    }
    return rowHeights[row];
}

Rect VirtTable::ContentRect() {
    Rect r = lastBounds;
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// in the same coords SetBounds() was given
Rect VirtTable::CellRect(int row, int col) {
    VirtTableCell* cell = CellAt(row, col);
    if (!cell || len(colWidths) != cols || len(rowHeights) != rows) {
        return {};
    }
    Rect content = ContentRect();
    int x = content.x + TracksStart(colWidths, col, colGap);
    int y = content.y + TracksStart(rowHeights, row, rowGap);
    int dx = TracksSize(colWidths, col, cell->colSpan, colGap);
    int dy = TracksSize(rowHeights, row, cell->rowSpan, rowGap);
    return {x, y, dx, dy};
}

// where a child of size sz sits inside its cell
static Rect AlignInCell(const Rect& cell, Size sz, CrossAxisAlign alignH, CrossAxisAlign alignV) {
    Rect r{cell.x, cell.y, sz.dx, sz.dy};
    switch (alignH) {
        case CrossAxisAlign::Stretch:
            r.dx = cell.dx;
            break;
        case CrossAxisAlign::CrossCenter:
            r.x += (cell.dx - sz.dx) / 2;
            break;
        case CrossAxisAlign::CrossEnd:
            r.x += cell.dx - sz.dx;
            break;
        case CrossAxisAlign::CrossStart:
            break;
    }
    switch (alignV) {
        case CrossAxisAlign::Stretch:
            r.dy = cell.dy;
            break;
        case CrossAxisAlign::CrossCenter:
            r.y += (cell.dy - sz.dy) / 2;
            break;
        case CrossAxisAlign::CrossEnd:
            r.y += cell.dy - sz.dy;
            break;
        case CrossAxisAlign::CrossStart:
            break;
    }
    return r;
}

void VirtTable::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    if (len(colWidths) != cols || len(rowHeights) != rows) {
        // SetBounds() without a preceding Layout()
        Measure();
    }
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            VirtTableCell& cell = cells[CellIdx(row, col)];
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            Rect cellRc = CellRect(row, col);
            cell.child->SetBounds(AlignInCell(cellRc, cell.childSize, cell.alignH, cell.alignV));
        }
    }
}

//--- VirtScroll

static Kind kindVirtWndScroll = "virtWndScroll";

VirtScroll::VirtScroll() {
    kind = kindVirtWndScroll;
    flags |= vwfClipChildren;
}

VirtScroll::~VirtScroll() = default;

Size VirtScroll::Layout(Constraints bc) {
    // a viewport takes whatever it is given; the content decides contentDy
    Size sz = bc.Constrain({bc.max.dx, bc.max.dy});
    for (VirtWnd* c : children) {
        Constraints cc = bc;
        cc.min = {0, 0};
        cc.max.dy = Inf;
        c->Layout(cc);
    }
    return sz;
}

void VirtScroll::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    int y = content.y - scrollY;
    for (VirtWnd* c : children) {
        int dy = c->MinIntrinsicHeight(content.dx);
        c->SetBounds({content.x, y, content.dx, dy});
        y += dy;
    }
    ScrollTo(scrollY);
    NotifyVisibleRange();
}

Point VirtScroll::ScrollOffset() {
    return {0, scrollY};
}

void VirtScroll::SetContentDy(int dy) {
    if (contentDy == dy) {
        return;
    }
    contentDy = dy;
    ScrollTo(scrollY);
    UpdateScrollbar();
}

int VirtScroll::MaxScrollY() const {
    int visible = bounds.dy - padding.top - padding.bottom;
    int res = contentDy - visible;
    return res > 0 ? res : 0;
}

bool VirtScroll::ScrollTo(int y) {
    int maxY = MaxScrollY();
    y = Clamp(y, 0, maxY);
    if (y == scrollY) {
        return false;
    }
    scrollY = y;
    UpdateScrollbar();
    NotifyVisibleRange();
    Invalidate();
    return true;
}

bool VirtScroll::ScrollBy(int dy) {
    return ScrollTo(scrollY + dy);
}

bool VirtScroll::ScrollPage(int dir) {
    int visible = bounds.dy - padding.top - padding.bottom;
    return ScrollBy(dir * visible);
}

void VirtScroll::ScrollIntoView(VirtWnd* w) {
    if (!w) {
        return;
    }
    Rect wr = w->BoundsInWindow();
    Rect vr = ContentRectInWindow();
    if (wr.y < vr.y) {
        ScrollBy(wr.y - vr.y);
        return;
    }
    if (wr.Bottom() > vr.Bottom()) {
        ScrollBy(wr.Bottom() - vr.Bottom());
    }
}

bool VirtScroll::OnMouseWheel(VirtMouseEvent& ev) {
    if (ev.wheelDelta == 0) {
        return false;
    }
    int lines = -(ev.wheelDelta * 3) / WHEEL_DELTA;
    return ScrollBy(lines * lineDy);
}

void VirtScroll::OnVScroll(WPARAM wp) {
    int visible = bounds.dy - padding.top - padding.bottom;
    switch (LOWORD(wp)) {
        case SB_TOP:
            ScrollTo(0);
            break;
        case SB_BOTTOM:
            ScrollTo(MaxScrollY());
            break;
        case SB_LINEUP:
            ScrollBy(-lineDy);
            break;
        case SB_LINEDOWN:
            ScrollBy(lineDy);
            break;
        case SB_PAGEUP:
            ScrollBy(-visible);
            break;
        case SB_PAGEDOWN:
            ScrollBy(visible);
            break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            ScrollTo(HIWORD(wp));
            break;
    }
}

void VirtScroll::UpdateScrollbar() {
    HWND hwnd = GetHwnd();
    if (!syncScrollbar || !hwnd) {
        return;
    }
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = contentDy > 0 ? contentDy - 1 : 0;
    si.nPage = (UINT)(bounds.dy - padding.top - padding.bottom);
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

// only tell the owner when the visible band actually changed, so that
// virtualized owners can rebuild their children cheaply
void VirtScroll::NotifyVisibleRange() {
    if (onVisibleRangeChanged.IsEmpty()) {
        return;
    }
    int visible = bounds.dy - padding.top - padding.bottom;
    if (scrollY == lastNotifiedY && visible == lastNotifiedDy) {
        return;
    }
    lastNotifiedY = scrollY;
    lastNotifiedDy = visible;
    VirtScrollRange r;
    r.wnd = this;
    r.visibleY = scrollY;
    r.visibleDy = visible;
    onVisibleRangeChanged.Call(&r);
}

//--- VirtCustom

static Kind kindVirtWndCustom = "virtWndCustom";

VirtCustom::VirtCustom() {
    kind = kindVirtWndCustom;
}

VirtCustom::~VirtCustom() = default;

Size VirtCustom::GetIdealSize() {
    return idealSize;
}

void VirtCustom::Paint(VirtPaintCtx& ctx) {
    onPaint.Call(&ctx);
}

bool VirtCustom::OnMouseUp(VirtMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

//--- VirtWrapper

static Kind kindVirtWndWrapper = "virtWndWrapper";

VirtWrapper::VirtWrapper(ControlBase* w, bool owns) {
    kind = kindVirtWndWrapper;
    wnd = w;
    ownsWnd = owns;
    // the HWND is on top of whatever the tree paints, and gets its own mouse
    // and keyboard messages
    flags |= vwfNoHitTest;
}

VirtWrapper::~VirtWrapper() {
    if (ownsWnd) {
        delete wnd;
    }
}

int VirtWrapper::MinIntrinsicHeight(int width) {
    return wnd ? wnd->MinIntrinsicHeight(width) : 0;
}

int VirtWrapper::MinIntrinsicWidth(int height) {
    return wnd ? wnd->MinIntrinsicWidth(height) : 0;
}

Size VirtWrapper::Layout(Constraints bc) {
    if (!wnd) {
        return bc.Constrain({0, 0});
    }
    return wnd->Layout(bc);
}

Size VirtWrapper::GetIdealSize() {
    return wnd ? wnd->GetIdealSize() : Size{};
}

// the tree lays out in the root HWND's client coords, which is also what the
// child HWND is positioned in
void VirtWrapper::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    if (!wnd) {
        return;
    }
    wnd->SetVisibility(visibility);
    if (visibility != Visibility::Collapse) {
        wnd->SetBounds(r);
    }
}

//--- VirtText

static Kind kindVirtWndText = "virtWndText";

VirtText::VirtText(Str str, PlatformFont* f) {
    kind = kindVirtWndText;
    s = str::Dup(str);
    font = f;
    flags |= vwfNoHitTest;
}

VirtText::~VirtText() {
    str::Free(s);
}

void VirtText::SetText(Str str) {
    str::Free(s);
    s = str::Dup(str);
    sz = {0, 0};
}

// all three go through the virtual GetIdealSize(), so a subclass that adds to
// the text's size (a button's textPadding, a section header's gap) is measured
// with it rather than as bare text
Size VirtText::Layout(const Constraints bc) {
    return bc.Constrain(GetIdealSize());
}

int VirtText::MinIntrinsicHeight(int) {
    return GetIdealSize().dy;
}

int VirtText::MinIntrinsicWidth(int) {
    return GetIdealSize().dx;
}

Size VirtText::MinIntrinsicSize(int width, int height) {
    int dx = MinIntrinsicWidth(height);
    int dy = MinIntrinsicHeight(width);
    return {dx, dy};
}

Size VirtText::GetIdealSize() {
    Size res = GetIdealSize(false);
    res.dx += padding.left + padding.right;
    res.dy += padding.top + padding.bottom;
    return res;
}

// the size of the text itself, without padding. Paint() draws into the content
// rect, which is already deflated by it
Size VirtText::GetIdealSize(bool onlyIfEmpty) {
    if (onlyIfEmpty && !sz.IsEmpty()) {
        return sz;
    }
    sz = PlatformFontMeasureText(font, s);
    return sz;
}

void VirtText::Paint(VirtPaintCtx& ctx) {
    Rect r = ctx.content;
    if (r.IsEmpty()) {
        return;
    }
    u32 fmt = 0;
    if (ellipsis) {
        fmt |= gfxTextEllipsis;
    }
    if (isRtl) {
        fmt |= gfxTextRtl;
    }
    switch (align) {
        case VirtTextAlign::Center:
            fmt |= gfxTextCenter;
            break;
        case VirtTextAlign::Right:
            fmt |= gfxTextRight;
            break;
        case VirtTextAlign::Left:
            break;
    }
    GfxDrawText(ctx.gfx, s, r, fmt, font, textColor);
    if (withUnderline) {
        GetIdealSize(true);
        Rect lineRect = {r.x, r.y + sz.dy + underlineOffsetY, sz.dx, 0};
        GfxDrawLine(ctx.gfx, lineRect, textColor);
    }
}

VirtText* NewVirtText(const VirtTextArgs& args) {
    auto* w = new VirtText(args.s, args.font);
    w->textColor = args.textColor;
    w->align = args.align;
    w->withUnderline = args.withUnderline;
    w->isRtl = args.isRtl;
    w->ellipsis = args.ellipsis;
    w->underlineOffsetY = args.underlineOffsetY;
    w->padding = args.padding;
    return w;
}

//--- VirtLink

static Kind kindVirtWndLink = "virtWndLink";

VirtLink::VirtLink(Str str, PlatformFont* f) : VirtText(str, f) {
    kind = kindVirtWndLink;
    flags &= ~vwfNoHitTest;
}

VirtLink::~VirtLink() {
    str::Free(target);
    str::Free(tooltip);
}

void VirtLink::SetTarget(Str s2) {
    str::Free(target);
    target = str::Dup(s2);
}

void VirtLink::SetTooltip(Str s2) {
    str::Free(tooltip);
    tooltip = str::Dup(s2);
}

void VirtLink::Paint(VirtPaintCtx& ctx) {
    bool prevUnderline = withUnderline;
    if (underlineOnHover) {
        withUnderline = HasFlag(vwfHovered);
    }
    VirtText::Paint(ctx);
    withUnderline = prevUnderline;
}

void VirtLink::OnMouseEnter() {
    if (underlineOnHover) {
        Invalidate();
    }
}

void VirtLink::OnMouseLeave() {
    if (underlineOnHover) {
        Invalidate();
    }
}

bool VirtLink::OnMouseDown(VirtMouseEvent&) {
    // consume so that the click doesn't fall through to the page below
    return true;
}

bool VirtLink::OnMouseUp(VirtMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtLink::OnSetCursor(Point) {
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtLink::GetTooltipTemp(Point) {
    if (!tooltip) {
        return nullptr;
    }
    return str::DupTemp(tooltip);
}

//--- VirtButton

static Kind kindVirtWndButton = "virtWndButton";

VirtButton::VirtButton(Str str, PlatformFont* f) : VirtText(str, f) {
    kind = kindVirtWndButton;
    flags &= ~vwfNoHitTest;
    align = VirtTextAlign::Center;
}

VirtButton::~VirtButton() = default;

Size VirtButton::GetIdealSize() {
    Size s2 = VirtText::GetIdealSize();
    return {s2.dx + textPadding.left + textPadding.right, s2.dy + textPadding.top + textPadding.bottom};
}

void VirtButton::Paint(VirtPaintCtx& ctx) {
    bool isEnabled = HasFlag(vwfEnabled);
    COLORREF bg = (isEnabled && HasFlag(vwfHovered)) ? bgColorHover : bgColor;
    GfxFillRect(ctx.gfx, ctx.bounds, bg);
    if (borderColor != kColorUnset) {
        Rect b = ctx.bounds;
        GfxFillRect(ctx.gfx, {b.x, b.y, b.dx, 1}, borderColor);
        GfxFillRect(ctx.gfx, {b.x, b.Bottom() - 1, b.dx, 1}, borderColor);
        GfxFillRect(ctx.gfx, {b.x, b.y, 1, b.dy}, borderColor);
        GfxFillRect(ctx.gfx, {b.Right() - 1, b.y, 1, b.dy}, borderColor);
    }
    Rect r = ctx.content;
    r.SubTB(textPadding.top, textPadding.bottom);
    r.SubLR(textPadding.left, textPadding.right);
    VirtPaintCtx c2 = ctx;
    c2.content = r;
    COLORREF prevCol = textColor;
    if (!isEnabled && textColorDisabled != kColorUnset) {
        textColor = textColorDisabled;
    }
    VirtText::Paint(c2);
    textColor = prevCol;
}

void VirtButton::OnMouseEnter() {
    Invalidate();
}

void VirtButton::OnMouseLeave() {
    Invalidate();
}

bool VirtButton::OnMouseDown(VirtMouseEvent&) {
    return true;
}

bool VirtButton::OnMouseUp(VirtMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtButton::OnSetCursor(Point) {
    if (!HasFlag(vwfEnabled)) {
        return false;
    }
    SetCursorCached(IDC_HAND);
    return true;
}

//--- VirtIconButton

static Kind kindVirtWndIconButton = "virtWndIconButton";

VirtIconButton::VirtIconButton() {
    kind = kindVirtWndIconButton;
}

VirtIconButton::~VirtIconButton() {
    str::Free(tooltip);
}

void VirtIconButton::SetTooltip(Str s2) {
    str::Free(tooltip);
    tooltip = str::Dup(s2);
}

Size VirtIconButton::GetIdealSize() {
    if (!pixmap) {
        return {0, 0};
    }
    return {pixmap->width, pixmap->height};
}

void VirtIconButton::Paint(VirtPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Size s2 = GetIdealSize();
    Rect r = ctx.content;
    int x = r.x + ((r.dx - s2.dx) / 2);
    int y = r.y + ((r.dy - s2.dy) / 2);
    GfxDrawPixmap(ctx.gfx, pixmap, {x, y, s2.dx, s2.dy});
}

void VirtIconButton::OnMouseEnter() {
    Invalidate();
}

void VirtIconButton::OnMouseLeave() {
    Invalidate();
}

bool VirtIconButton::OnMouseDown(VirtMouseEvent&) {
    return true;
}

bool VirtIconButton::OnMouseUp(VirtMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtIconButton::OnSetCursor(Point) {
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtIconButton::GetTooltipTemp(Point) {
    if (!tooltip) {
        return nullptr;
    }
    return str::DupTemp(tooltip);
}

//--- VirtCloseButton

static Kind kindVirtWndCloseButton = "virtWndCloseButton";

VirtCloseButton::VirtCloseButton() {
    kind = kindVirtWndCloseButton;
}

VirtCloseButton::~VirtCloseButton() {
    str::Free(tooltip);
}

void VirtCloseButton::SetTooltip(Str s) {
    str::Free(tooltip);
    tooltip = str::Dup(s);
}

Size VirtCloseButton::GetIdealSize() {
    return idealSize;
}

void VirtCloseButton::Paint(VirtPaintCtx& ctx) {
    bool isHover = HasFlag(vwfHovered);
    // the glyph goes in the content rect, so padding makes the hit area bigger
    // than the ✕ itself (the tab bar's close gutter)
    Rect r = ctx.content;
    Gdiplus::Graphics g(GfxHdc(ctx.gfx));
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPageUnit(Gdiplus::UnitPixel);
    // gdiplus doesn't pick up the window's orientation from the device context,
    // so mirroring has to be explicit
    HWND hwnd = GetHwnd();
    if (HwndIsRtl(hwnd)) {
        g.ScaleTransform(-1, 1);
        g.TranslateTransform((float)HwndClientRect(hwnd).dx, 0, Gdiplus::MatrixOrderAppend);
    }

    // slightly translucent when it sits on content it doesn't own
    u8 a = (withCircle && !isHover) ? 215 : 255;
    COLORREF circle = isHover ? circleColorHover : circleColor;
    if (isHover && circle == kColorUnset) {
        circle = kColCloseXHoverBg;
    } else if (!isHover && circle == kColorUnset) {
        circle = MkGray(0xff);
    }
    if (isHover || withCircle) {
        Gdiplus::SolidBrush br(Gdiplus::Color(a, GetRValue(circle), GetGValue(circle), GetBValue(circle)));
        g.FillEllipse(&br, r.x, r.y, r.dx - 1, r.dy - 1);
    }

    COLORREF xcol = isHover ? xColorHover : xColor;
    if (xcol == kColorUnset) {
        xcol = isHover ? kColCloseXHover : kColCloseX;
    }
    Gdiplus::Pen pen(Gdiplus::Color(a, GetRValue(xcol), GetGValue(xcol), GetBValue(xcol)), 2.0f);
    int pad = r.dx / 3;
    g.DrawLine(&pen, r.x + pad, r.y + pad, r.x + r.dx - pad, r.y + r.dy - pad);
    g.DrawLine(&pen, r.x + r.dx - pad, r.y + pad, r.x + pad, r.y + r.dy - pad);
}

void VirtCloseButton::OnMouseEnter() {
    Invalidate();
}

void VirtCloseButton::OnMouseLeave() {
    Invalidate();
}

bool VirtCloseButton::OnMouseDown(VirtMouseEvent&) {
    return true;
}

bool VirtCloseButton::OnMouseUp(VirtMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtCloseButton::OnSetCursor(Point) {
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtCloseButton::GetTooltipTemp(Point) {
    if (!tooltip) {
        return nullptr;
    }
    return str::DupTemp(tooltip);
}

//--- VirtImage

static Kind kindVirtWndImage = "virtWndImage";

// scale src down (never up) to fit dst, centered
Rect FitSizeInRect(Size src, Rect dst) {
    if (src.dx <= 0 || src.dy <= 0) {
        return dst;
    }
    int dx = src.dx;
    int dy = src.dy;
    if (dx > dst.dx) {
        dy = Scale(dy, dst.dx, dx);
        dx = dst.dx;
    }
    if (dy > dst.dy) {
        dx = Scale(dx, dst.dy, dy);
        dy = dst.dy;
    }
    int x = dst.x + (dst.dx - dx) / 2;
    int y = dst.y + (dst.dy - dy) / 2;
    return {x, y, dx, dy};
}

VirtImage::VirtImage() {
    kind = kindVirtWndImage;
    flags |= vwfNoHitTest;
}

VirtImage::~VirtImage() = default;

Size VirtImage::GetIdealSize() {
    if (!pixmap) {
        return {0, 0};
    }
    return {pixmap->width, pixmap->height};
}

void VirtImage::Paint(VirtPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Size sz2 = GetIdealSize();
    Rect r = ctx.content;
    if (fitToBounds) {
        r = FitSizeInRect(sz2, r);
    } else {
        r.dx = sz2.dx;
        r.dy = sz2.dy;
    }
    GfxDrawPixmap(ctx.gfx, pixmap, r);
}

//--- VirtFill

static Kind kindVirtWndFill = "virtWndFill";

VirtFill::VirtFill() {
    kind = kindVirtWndFill;
    flags |= vwfNoHitTest;
}

VirtFill::~VirtFill() = default;

Size VirtFill::GetIdealSize() {
    return idealSize;
}

void VirtFill::Paint(VirtPaintCtx& ctx) {
    GfxFillRect(ctx.gfx, ctx.bounds, color);
}

//--- VirtLine

static Kind kindVirtWndLine = "virtWndLine";

VirtLine::VirtLine() {
    kind = kindVirtWndLine;
    flags |= vwfNoHitTest;
}

VirtLine::~VirtLine() = default;

Size VirtLine::GetIdealSize() {
    if (isVertical) {
        return {thickness, 0};
    }
    return {0, thickness};
}

void VirtLine::Paint(VirtPaintCtx& ctx) {
    Rect r = ctx.content;
    if (isVertical) {
        r.dx = thickness;
    } else {
        r.dy = thickness;
    }
    GfxFillRect(ctx.gfx, r, color);
}

//--- VirtSpacer

static Kind kindVirtWndSpacer = "virtWndSpacer";

VirtSpacer::VirtSpacer(int dx, int dy) {
    kind = kindVirtWndSpacer;
    idealSize = {dx, dy};
    flags |= vwfNoHitTest;
}

VirtSpacer::~VirtSpacer() = default;

Size VirtSpacer::GetIdealSize() {
    return idealSize;
}

#if defined(DEBUG)
// must be last: UtAssert.h over-writes assert()
#include "base/UtAssert.h"

// Unit tests for VirtTable. VirtSpacer is the leaf: a fixed ideal size
// and no HWND, so a whole table can be laid out and its geometry asserted.

static bool VirtWndRectEq(const Rect& r, int x, int y, int dx, int dy) {
    return r.x == x && r.y == y && r.dx == dx && r.dy == dy;
}

static void VirtTable_TestGrid() {
    auto* t = new VirtTable();
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
    utassert(VirtWndRectEq(a->lastBounds, 0, 0, 20, 10));
    utassert(VirtWndRectEq(b->lastBounds, 40, 0, 40, 30));
    utassert(VirtWndRectEq(c->lastBounds, 0, 34, 30, 20));
    // an empty cell doesn't disturb the tracks
    utassert(t->GetCell(1, 1) == nullptr);
    delete t;
}

static void VirtTable_TestAlign() {
    auto* t = new VirtTable();
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
    utassert(VirtWndRectEq(center->lastBounds, 40, 40, 20, 10));
    // 10 tall pushed to the bottom of the 40-tall row
    utassert(VirtWndRectEq(bottom->lastBounds, 100, 30, 20, 10));
    // stretched to the full column width
    utassert(VirtWndRectEq(stretch->lastBounds, 0, 50, 100, 10));
    delete t;
}

static void VirtTable_TestSpan() {
    auto* t = new VirtTable();
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
    utassert(VirtWndRectEq(wide->lastBounds, 0, 0, 100, 10));
    utassert(b->lastBounds.x == 50);
    delete t;

    // the same for rows
    auto* t2 = new VirtTable();
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
    utassert(VirtWndRectEq(tall->lastBounds, 0, 0, 10, 100));
    delete t2;
}

// the cells' children must be reachable through the table's own bounds, or the
// links of a table-laid-out screen (About) stop being clickable
static void VirtTable_TestHitTest() {
    auto* t = new VirtTable();
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
    Point local{0, 0};
    utassert(t->WndFromPoint({6, 8}, &local) == a);
    utassert(t->WndFromPoint({40, 8}, &local) == b);
    utassert(local.x == 5 && local.y == 1);
    // the table itself is never a target, so the gap between the columns is a miss
    utassert(t->WndFromPoint({30, 8}, &local) == nullptr);
    delete t;
}

void VirtWnd_UnitTests() {
    VirtTable_TestGrid();
    VirtTable_TestAlign();
    VirtTable_TestSpan();
    VirtTable_TestHitTest();
}
#endif

//--- VirtRichText

// A small markup with links, keyboard shortcuts and bold runs, parsed into a
// VirtRichText: a virtual control that wraps, paints and hit-tests itself.
// Shared by SumatraPDF's home page and notifications, and by other apps in the
// family. What it needs from the app is a CommandsContext (see VirtWnd.h) and a
// way to open a url.

void (*gTipOpenUrl)(Str url) = nullptr;
CommandsContext* gCommandsContext = nullptr;

static Kind kindVirtRichText = "virtRichText";

VirtRichText::VirtRichText() {
    kind = kindVirtRichText;
}

VirtRichText::~VirtRichText() {
    Reset();
}

void VirtRichText::Reset() {
    TipWord* w = words.next;
    while (w) {
        TipWord* next = w->next;
        str::Free(w->text);
        delete w;
        w = next;
    }
    TipLink* l = links.next;
    while (l) {
        TipLink* next = l->next;
        str::Free(l->cmd);
        delete l;
        l = next;
    }
    words.next = nullptr;
    links.next = nullptr;
    lastWord = nullptr;
    lastLink = nullptr;
    totalDx = 0;
    totalDy = 0;
    layoutDx = -1;
}

int TipWordCount(VirtRichText* tip) {
    int n = 0;
    for (TipWord* w = tip->words.next; w; w = w->next) {
        n++;
    }
    return n;
}

int TipLinkCount(VirtRichText* tip) {
    int n = 0;
    for (TipLink* l = tip->links.next; l; l = l->next) {
        n++;
    }
    return n;
}

// appends at the end, so words and links stay in source order
static TipWord* AppendTipWord(VirtRichText& tip, Str text) {
    auto* w = new TipWord();
    str::ReplaceWithCopy(&w->text, text);
    if (tip.lastWord) {
        tip.lastWord->next = w;
    } else {
        tip.words.next = w;
    }
    tip.lastWord = w;
    return w;
}

static TipLink* AppendTipLink(VirtRichText& tip, Str cmd) {
    auto* l = new TipLink();
    str::ReplaceWithCopy(&l->cmd, cmd);
    if (tip.lastLink) {
        tip.lastLink->next = l;
    } else {
        tip.links.next = l;
    }
    tip.lastLink = l;
    return l;
}

// drops the link appended last; the parser adds it before it knows whether the
// link text produced any words
static void RemoveLastTipLink(VirtRichText& tip) {
    TipLink* link = tip.lastLink;
    if (!link) {
        return;
    }
    TipLink* prev = nullptr;
    for (TipLink* l = tip.links.next; l && l != link; l = l->next) {
        prev = l;
    }
    if (prev) {
        prev->next = nullptr;
    } else {
        tip.links.next = nullptr;
    }
    tip.lastLink = prev;
    str::Free(link->cmd);
    delete link;
}

static bool IsTipWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void AdvanceTipText(Str& s, int n = 1) {
    ReportIf(n < 0 || n > s.len);
    if (n >= s.len) {
        s = {};
        return;
    }
    s.s += n;
    s.len -= n;
}

static void SkipTipWhitespace(Str& s) {
    while (len(s) > 0 && IsTipWhitespace(s.s[0])) {
        AdvanceTipText(s);
    }
}

// closing ']' for the '[' at (textStart - 1); supports nested brackets in link text
static Str FindMarkdownLinkTextEnd(Str textStart) {
    int depth = 1;
    for (int i = 0; i < textStart.len; i++) {
        char c = textStart.s[i];
        if (c == '[') {
            depth++;
        } else if (c == ']') {
            depth--;
            if (depth == 0) {
                return Str(textStart.s + i, textStart.len - i);
            }
        }
    }
    return {};
}

// closing ')' for the '(' before cmdStart; balances parens in http(s) targets
static Str FindMarkdownLinkCmdEnd(Str cmdStart) {
    if (str::StartsWith(cmdStart, StrL("http://")) || str::StartsWith(cmdStart, StrL("https://"))) {
        int depth = 0;
        for (int i = 0; i < cmdStart.len; i++) {
            char c = cmdStart.s[i];
            if (c == '(') {
                depth++;
            } else if (c == ')') {
                if (depth > 0) {
                    depth--;
                } else {
                    return Str(cmdStart.s + i, cmdStart.len - i);
                }
            }
        }
        return {};
    }
    return str::SliceFromChar(cmdStart, ')');
}

static void AppendTipWordsFromText(VirtRichText& tip, Str text, bool isLink, TipLink* link, bool isBold = false) {
    int i = 0;
    while (i < text.len) {
        while (i < text.len && IsTipWhitespace(text.s[i])) {
            i++;
        }
        if (i >= text.len) {
            break;
        }
        int wordStart = i;
        while (i < text.len && !IsTipWhitespace(text.s[i])) {
            i++;
        }
        TipWord* w = AppendTipWord(tip, Str(text.s + wordStart, i - wordStart));
        w->isLink = isLink;
        w->isBold = isBold;
        w->link = link;
    }
}

// the first word emitted after `prev` (the list tail before the token was
// parsed), or null when the token emitted nothing
static TipWord* FirstWordAfter(VirtRichText& tip, TipWord* prev) {
    return prev ? prev->next : tip.words.next;
}

// mark the first word emitted for a token as having no space before it, so the
// layout draws it flush against the preceding word (e.g. "**foo**:" -> "foo:")
static void SetNoSpaceBefore(VirtRichText& tip, TipWord* prev, bool noSpace) {
    TipWord* first = FirstWordAfter(tip, prev);
    if (noSpace && first) {
        first->noSpaceBefore = true;
    }
}

// index of the ')' that matches s[0]=='(', or -1
static int MatchingCloseParen(Str s) {
    if (len(s) == 0 || s.s[0] != '(') {
        return -1;
    }
    int depth = 0;
    for (int i = 0; i < s.len; i++) {
        char c = s.s[i];
        if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth == 0) {
                return i;
            }
        }
    }
    return -1;
}

// true if s starts with "(prefix/" (prefix e.g. "Key" or "Kbd")
static bool StartsWithParenPrefix(Str s, Str prefix) {
    // "(" + prefix + "/"
    if (len(s) < 2 + len(prefix) + 1) {
        return false;
    }
    if (s.s[0] != '(') {
        return false;
    }
    if (!str::StartsWith(Str(s.s + 1, s.len - 1), prefix)) {
        return false;
    }
    return s.s[1 + len(prefix)] == '/';
}

// emit (Kbd/...) content as one or more key-cap words (", "-separated, like
// the keyboard help sheet when a command has multiple bindings)
static void AppendKbdWords(VirtRichText& tip, Str content, bool noSpace) {
    // trim leading/trailing whitespace on the whole content
    while (len(content) > 0 && IsTipWhitespace(content.s[0])) {
        content.s++;
        content.len--;
    }
    while (len(content) > 0 && IsTipWhitespace(content.s[content.len - 1])) {
        content.len--;
    }
    if (len(content) == 0) {
        return;
    }
    StrVec toks;
    Split(&toks, content, StrL(", "));
    int n = len(toks);
    bool any = false;
    for (int i = 0; i < n; i++) {
        Str t = toks.At(i);
        while (len(t) > 0 && IsTipWhitespace(t.s[0])) {
            t.s++;
            t.len--;
        }
        while (len(t) > 0 && IsTipWhitespace(t.s[t.len - 1])) {
            t.len--;
        }
        if (len(t) == 0) {
            continue;
        }
        TipWord* w = AppendTipWord(tip, t);
        w->isKbd = true;
        if (!any) {
            w->noSpaceBefore = noSpace;
            any = true;
        }
    }
    if (!any) {
        TipWord* w = AppendTipWord(tip, content);
        w->isKbd = true;
        w->noSpaceBefore = noSpace;
    }
}

// resolve a link command to the target a tip link stores
static TempStr ResolveLinkCmdTemp(Str cmd) {
    if (str::StartsWith(cmd, StrL("https://")) || str::StartsWith(cmd, StrL("http://"))) {
        return str::DupTemp(cmd);
    }
    if (str::TrimPrefix(cmd, StrL("Help/"))) {
        // cmd is a non-NUL-terminated view into the tip line, so %s must get a
        // zero-terminated copy of exactly the remainder -- otherwise it reads
        // past the link, pulling in trailing chars like ")."
        return fmt("https://www.sumatrapdfreader.org/docs/%s", cmd);
    }
    // Cmd* - use as-is, will be resolved to command ID on click
    return str::DupTemp(cmd);
}

// Text from outside the app -- a clipboard string, a file name -- can contain
// anything the markup uses, so this adds it as plain words with nothing
// interpreted.
// adds text with no markup interpreted, for strings from outside the app
void VirtRichText::AddPlainText(Str text) {
    AppendTipWordsFromText(*this, text, false, nullptr);
    layoutDx = -1;
}

// The app decides what a command name means; without a context (Key/...) is
// left as literal text and command links do nothing.
static TempStr CommandShortcutTemp(Str cmdName) {
    if (!gCommandsContext) {
        return {};
    }
    return gCommandsContext->GetCommandShortcutTemp(cmdName);
}

void ParseTipInto(VirtRichText* tipIn, Str s) {
    VirtRichText& tip = *tipIn;
    if (!s) {
        return;
    }
    str::Builder expanded;
    Str sp = s;
    // first pass: expand (Key/CmdXxx) to shortcut strings (only for real commands).
    // Uses balanced parens so nesting works: (Kbd/(Key/CmdFoo)) → (Kbd/Ctrl + …)
    while (len(sp) > 0) {
        if (StartsWithParenPrefix(sp, StrL("Key"))) {
            int end = MatchingCloseParen(sp);
            if (end > 5) {
                Str cmdName(sp.s + 5, end - 5); // skip "(Key/"
                TempStr shortcut = CommandShortcutTemp(cmdName);
                if (shortcut.s) {
                    expanded.Append(shortcut);
                    AdvanceTipText(sp, end + 1);
                    continue;
                }
            }
        }
        expanded.AppendChar(sp.s[0]);
        AdvanceTipText(sp);
    }

    // second pass: split into words, detecting [text](link), (Kbd/...), **bold**
    Str p = ToStr(expanded);
    while (len(p) > 0) {
        const char* beforeWs = p.s;
        SkipTipWhitespace(p);
        if (len(p) == 0) {
            break;
        }
        // this token abuts the previous with no whitespace between them (e.g. the
        // ':' right after "**foo**"), so it draws with no space before it
        TipWord* prevWord = tip.lastWord;
        bool noSpace = (p.s == beforeWs) && (prevWord != nullptr);

        // (Kbd/shortcut text) — key-cap(s); content already has (Key/...) expanded
        if (StartsWithParenPrefix(p, StrL("Kbd"))) {
            int end = MatchingCloseParen(p);
            if (end > 5) {
                Str content(p.s + 5, end - 5); // skip "(Kbd/"
                AppendKbdWords(tip, content, noSpace);
                AdvanceTipText(p, end + 1);
                continue;
            }
        }

        // **bold text**
        if (p.len >= 4 && p.s[0] == '*' && p.s[1] == '*') {
            Str after(p.s + 2, p.len - 2);
            int end = str::IndexOf(after, StrL("**"));
            if (end >= 0) {
                Str boldText(after.s, end);
                AppendTipWordsFromText(tip, boldText, false, nullptr, true);
                SetNoSpaceBefore(tip, prevWord, noSpace);
                AdvanceTipText(p, 2 + end + 2);
                continue;
            }
        }

        // a standalone '*' (not the '**' that starts bold) is a bullet used to
        // separate items visually; render it as a middle dot
        if (p.s[0] == '*' && (p.len == 1 || IsTipWhitespace(p.s[1]))) {
            TipWord* w = AppendTipWord(tip, StrL("\xc2\xb7")); // U+00B7 MIDDLE DOT
            w->noSpaceBefore = noSpace;
            AdvanceTipText(p, 1);
            continue;
        }

        if (p.s[0] == '[') {
            Str textStart(p.s + 1, p.len - 1);
            Str textEnd = FindMarkdownLinkTextEnd(textStart);
            if (textEnd && textEnd.len > 1 && textEnd.s[1] == '(') {
                Str cmdStart(textEnd.s + 2, textEnd.len - 2);
                Str cmdEnd = FindMarkdownLinkCmdEnd(cmdStart);
                if (cmdEnd) {
                    if (textEnd.s > textStart.s) {
                        Str linkCmd(cmdStart.s, (int)(cmdEnd.s - cmdStart.s));
                        Str linkText(textStart.s, (int)(textEnd.s - textStart.s));

                        TipLink* link = AppendTipLink(tip, ResolveLinkCmdTemp(linkCmd));
                        AppendTipWordsFromText(tip, linkText, true, link);
                        link->firstWord = FirstWordAfter(tip, prevWord);
                        link->lastWord = tip.lastWord;

                        if (link->firstWord) {
                            SetNoSpaceBefore(tip, prevWord, noSpace);
                            AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                            continue;
                        }
                        // the link text was empty, so it produced no words
                        RemoveLastTipLink(tip);
                    } else {
                        // empty [text]: treat the whole markup as literal text
                        TipWord* w = AppendTipWord(tip, Str(p.s, (int)(cmdEnd.s - p.s) + 1));
                        w->noSpaceBefore = noSpace;
                        AdvanceTipText(p, (int)(cmdEnd.s - p.s) + 1);
                        continue;
                    }
                }
            }
            // not a valid [text](link) — fall through (e.g. "[CIW]" in a filename)
        }

        // regular word; stop at '[', '**', or '(Kbd/' so those stay separate tokens
        int wordStart = 0;
        int i = 0;
        while (i < p.len && !IsTipWhitespace(p.s[i])) {
            if (p.s[i] == '*' && i + 1 < p.len && p.s[i + 1] == '*') {
                break; // start of **bold**
            }
            if (p.s[i] == '(' && StartsWithParenPrefix(Str(p.s + i, p.len - i), StrL("Kbd"))) {
                break;
            }
            if (p.s[i] == '[') {
                Str textStart(p.s + i + 1, p.len - i - 1);
                Str textEnd = FindMarkdownLinkTextEnd(textStart);
                if (textEnd && textEnd.len > 1 && textEnd.s[1] == '(' &&
                    FindMarkdownLinkCmdEnd(Str(textEnd.s + 2, textEnd.len - 2))) {
                    break;
                }
            }
            i++;
        }
        if (i > wordStart) {
            TipWord* w = AppendTipWord(tip, Str(p.s + wordStart, i - wordStart));
            w->noSpaceBefore = noSpace;
        }
        if (i < p.len) {
            AdvanceTipText(p, i);
        } else {
            break;
        }
    }
}

// measures every word and wraps them into areaWidth. Positions are relative to
// the control's content origin, so they survive the control being moved
void VirtRichText::LayoutText(int areaWidth) {
    if (areaWidth == layoutDx) {
        return;
    }
    layoutDx = areaWidth;
    HWND hwnd = GetHwnd();
    PlatformFont* boldFont = nullptr;
    int kbdPadX = DpiScale(hwnd, 7);
    int kbdPadY = DpiScale(hwnd, 5);
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        Size sz = PlatformFontMeasureText(use, w->text);
        if (w->isKbd) {
            // key-cap padding matches KeyboardHelp's key caps
            w->dx = sz.dx + (2 * kbdPadX);
            w->dy = sz.dy + kbdPadY;
        } else {
            w->dx = sz.dx;
            w->dy = sz.dy;
        }
    }

    int startX = 0;
    int startY = 0;
    int x = startX;
    int y = startY;
    int lineHeight = 0;
    int spaceWidth = 4; // approximate space between words
    int maxX = startX;
    for (TipWord* w = words.next; w; w = w->next) {
        // space goes before the word, so words abutting the previous token
        // (noSpaceBefore, e.g. the ':' in "**foo**:") draw flush against it
        int space = (x > startX && !w->noSpaceBefore) ? spaceWidth : 0;
        if (x > startX && x + space + w->dx > startX + areaWidth) {
            // wrap to next line
            x = startX;
            y += lineHeight + 2;
            lineHeight = 0;
            space = 0;
        }
        x += space;
        w->x = x;
        w->y = y;
        x += w->dx;
        maxX = std::max(x, maxX);
        lineHeight = std::max(w->dy, lineHeight);
    }
    // A key-cap's box is taller than a word: its text is centered in it, which
    // put the cap's baseline below the baseline of the words around it. Nudge
    // the plain words down by the cap's top padding instead, so everything on a
    // line with caps sits on one baseline. Words on a line all share the same y
    Vec<int> capLineYs;
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isKbd && !capLineYs.Contains(w->y)) {
            capLineYs.Append(w->y);
        }
    }
    if (len(capLineYs) > 0) {
        int capLift = kbdPadY / 2;
        for (TipWord* w = words.next; w; w = w->next) {
            if (!w->isKbd && capLineYs.Contains(w->y)) {
                w->y += capLift;
            }
        }
    }

    totalDx = maxX - startX;
    totalDy = (y - startY) + lineHeight;
}

// the sizes below include padding: SetBounds() lays the words out in the
// content rect, so a box that didn't count it would hand us too little room
// and the text would wrap or get cut off
int VirtRichText::MinIntrinsicWidth(int) {
    LayoutText(1 << 20);
    return totalDx + padding.left + padding.right;
}

int VirtRichText::MinIntrinsicHeight(int width) {
    int padX = padding.left + padding.right;
    int dx = (width > padX) ? (width - padX) : (1 << 20);
    LayoutText(dx);
    return totalDy + padding.top + padding.bottom;
}

Size VirtRichText::GetIdealSize() {
    LayoutText(layoutDx > 0 ? layoutDx : (1 << 20));
    return {totalDx + padding.left + padding.right, totalDy + padding.top + padding.bottom};
}

Size VirtRichText::Layout(Constraints bc) {
    int padX = padding.left + padding.right;
    int dx = (bc.max.dx == Inf) ? (1 << 20) : std::max(bc.max.dx - padX, 1);
    LayoutText(dx);
    return bc.Constrain({totalDx + padX, totalDy + padding.top + padding.bottom});
}

void VirtRichText::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    LayoutText(content.dx);
}

// draws the words (link words in linkColor, underlined; others in textColor;
// isKbd words as key-caps like the keyboard help sheet)
void VirtRichText::Paint(VirtPaintCtx& ctx) {
    HDC hdc = GfxHdc(ctx.gfx);
    uint fmt = DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE;
    PlatformFont* boldFont = nullptr;
    COLORREF textCol = textColor;
    COLORREF linkCol = (linkColor == kColorUnset) ? textCol : linkColor;
    COLORREF bgCol = bgColor;
    // key-cap colors: AccentColor on the background the text sits on
    if (bgCol == kColorUnset) {
        bgCol = IsLightColor(textCol) ? MkGray(0x22) : MkGray(0xf2);
    }
    COLORREF capBg = AccentColor(bgCol, 16);
    COLORREF capBorder = AccentColor(bgCol, 40);
    int rad = DpiScale(GetHwnd(), 5);
    // words are laid out at (0, 0); shift them to where we are
    int offX = ctx.content.x;
    int offY = ctx.content.y;

    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isKbd) {
            HPEN pen = CreatePen(PS_SOLID, 1, capBorder);
            HBRUSH br = CreateSolidBrush(capBg);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBr = SelectObject(hdc, br);
            RoundRect(hdc, offX + w->x, offY + w->y, offX + w->x + w->dx, offY + w->y + w->dy, rad, rad);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBr);
            DeleteObject(pen);
            DeleteObject(br);
            SetTextColor(hdc, textCol);
            Rect capRc{offX + w->x, offY + w->y, w->dx, w->dy};
            HdcDrawText(hdc, w->text, capRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX,
                        font ? font->GetHFont() : nullptr);
            continue;
        }
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        Point pt = {offX + w->x, offY + w->y};
        SetTextColor(hdc, w->isLink ? linkCol : textCol);
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        HdcDrawText(hdc, w->text, pt, fmt, use ? use->GetHFont() : nullptr);
    }
    // underline each link
    HPEN pen = CreatePen(PS_SOLID, 1, linkCol);
    HGDIOBJ prevPen = SelectObject(hdc, pen);
    for (TipLink* link = links.next; link; link = link->next) {
        TipWord* first = link->firstWord;
        TipWord* last = link->lastWord;
        if (!first || !last) {
            continue;
        }
        int underlineY = offY + first->y + first->dy - 3;
        int x1 = offX + first->x;
        int x2 = offX + last->x + last->dx;
        HdcDrawLine(hdc, Rect(x1, underlineY, x2 - x1, 0));
    }
    SelectObject(hdc, prevPen);
    DeleteObject(pen);
}

// the link under a point in our own coordinates, or null
TipLink* VirtRichText::LinkAt(Point ptLocal) {
    int x = ptLocal.x - padding.left;
    int y = ptLocal.y - padding.top;
    for (TipWord* w = words.next; w; w = w->next) {
        if (!w->isLink) {
            continue;
        }
        Rect wr = {w->x, w->y, w->dx, w->dy};
        if (wr.Contains(Point(x, y))) {
            return w->link;
        }
    }
    return nullptr;
}

// a click on a link runs it; anything else bubbles up to whoever hosts us
bool VirtRichText::OnMouseDown(VirtMouseEvent& ev) {
    return LinkAt(ev.pt) != nullptr;
}

bool VirtRichText::OnMouseUp(VirtMouseEvent& ev) {
    TipLink* link = LinkAt(ev.pt);
    if (!link) {
        if (onClick.IsValid()) {
            onClick.Call(&ev);
            return true;
        }
        return false;
    }
    HWND hwnd = hwndForCmds ? hwndForCmds : GetHwnd();
    ExecuteTipLink(hwnd, link->cmd);
    return true;
}

bool VirtRichText::OnSetCursor(Point ptLocal) {
    if (!LinkAt(ptLocal) && !onClick.IsValid()) {
        return false;
    }
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtRichText::GetTooltipTemp(Point ptLocal) {
    TipLink* link = LinkAt(ptLocal);
    if (!link) {
        return nullptr;
    }
    return str::DupTemp(link->cmd);
}

// runs a link target: "Cmd..." sends the command to hwnd, a url goes to gTipOpenUrl.
// Cmd targets may include arguments (e.g. "CmdFixDefaultApp .pdf"); those go through
// CreateCommandFromDefinition so FrameOnCommand sees a CustomCommand with args.
void ExecuteTipLink(HWND hwnd, Str cmd) {
    if (len(cmd) == 0) {
        return;
    }
    if (str::StartsWith(cmd, StrL("Cmd"))) {
        if (gCommandsContext) {
            gCommandsContext->ExecuteCommand(hwnd, cmd);
        }
        return;
    }
    if (str::StartsWith(cmd, StrL("http://")) || str::StartsWith(cmd, StrL("https://"))) {
        if (gTipOpenUrl) {
            gTipOpenUrl(cmd);
        }
    }
}

bool VirtRichText::HasRichContent() {
    if (links.next) {
        return true;
    }
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isBold || w->isKbd) {
            return true;
        }
    }
    return false;
}

// reconstructs the plain (link markup removed, Key/ expanded) text
TempStr VirtRichText::PlainTextTemp() {
    str::Builder sb;
    for (TipWord* w = words.next; w; w = w->next) {
        if (w != words.next) {
            sb.AppendChar(' ');
        }
        sb.Append(w->text);
    }
    return ToStrTemp(sb);
}

VirtRichText* ParseTip(Str s) {
    auto* tip = new VirtRichText();
    ParseTipInto(tip, s);
    return tip;
}
