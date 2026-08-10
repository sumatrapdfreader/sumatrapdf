/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
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

void VirtWnd::Paint(VirtWndPaintCtx&) {}

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

    VirtWndPaintCtx ctx;
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

void VirtWnd::PaintChildren(VirtWndPaintCtx& ctx) {
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

bool VirtWnd::OnMouseDown(VirtWndMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseUp(VirtWndMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseMove(VirtWndMouseEvent&) {
    return false;
}

bool VirtWnd::OnMouseWheel(VirtWndMouseEvent&) {
    return false;
}

bool VirtWnd::OnDoubleClick(VirtWndMouseEvent&) {
    return false;
}

bool VirtWnd::OnContextMenu(VirtWndMouseEvent&) {
    return false;
}

void VirtWnd::OnMouseEnter() {}

void VirtWnd::OnMouseLeave() {}

void VirtWnd::OnCaptureLost() {}

bool VirtWnd::OnKeyDown(VirtWndKeyEvent&) {
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

void VirtWnd::SetRoot(VirtWndRoot* r) {
    root = r;
    for (VirtWnd* c : children) {
        c->SetRoot(r);
    }
}

//--- VirtWndRoot

VirtWndRoot::VirtWndRoot(HWND hwnd) {
    this->hwnd = hwnd;
}

VirtWndRoot::~VirtWndRoot() {
    delete child;
}

void VirtWndRoot::SetChild(VirtWnd* c) {
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

void VirtWndRoot::SetBounds(Rect r) {
    if (bounds == r) {
        return;
    }
    bounds = r;
    needsLayout = true;
}

void VirtWndRoot::RequestLayout() {
    needsLayout = true;
    HwndInvalidate(hwnd);
}

void VirtWndRoot::LayoutIfNeeded() {
    if (!needsLayout || !child) {
        return;
    }
    needsLayout = false;
    Constraints bc = Tight({bounds.dx, bounds.dy});
    child->Layout(bc);
    child->SetBounds(bounds);
}

void VirtWndRoot::Paint(Gfx* gfx, Rect clip) {
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

VirtWnd* VirtWndRoot::WndFromPoint(Point ptWindow, Point* ptLocalOut) {
    if (!child || !bounds.Contains(ptWindow)) {
        return nullptr;
    }
    LayoutIfNeeded();
    return child->WndFromPoint(ptWindow, ptLocalOut);
}

void VirtWndRoot::Invalidate(Rect rWindow) {
    if (!hwnd || rWindow.IsEmpty()) {
        return;
    }
    HwndInvalidateRect(hwnd, rWindow, false);
}

void VirtWndRoot::SetFocus(VirtWnd* w) {
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

bool VirtWndRoot::TabNavigate(bool backwards) {
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

void VirtWndRoot::SetCapture(VirtWnd* w) {
    captured = w;
    if (w && hwnd) {
        ::SetCapture(hwnd);
    }
}

void VirtWndRoot::ReleaseCapture() {
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

void VirtWndRoot::ClearPressed() {
    VirtWnd* w = pressed;
    if (!w) {
        return;
    }
    pressed = nullptr;
    w->SetFlag(vwfPressed, false);
    w->Invalidate();
}

void VirtWndRoot::ClearHover() {
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
void VirtWndRoot::OnWndDestroyed(VirtWnd* w) {
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

static void FillMouseEvent(VirtWndMouseEvent& ev, VirtWnd* target, Point ptWindow, Point ptLocal, bool captured) {
    ev.target = target;
    ev.hit = target;
    ev.ptWindow = ptWindow;
    ev.pt = captured ? ptWindow : ptLocal;
    ev.isCtrl = IsCtrlPressed();
    ev.isShift = IsShiftPressed();
    ev.isAlt = IsAltPressed();
}

// walks up from target until someone consumes the event
static bool BubbleMouse(VirtWnd* target, VirtWndMouseEvent& ev, bool (VirtWnd::*handler)(VirtWndMouseEvent&)) {
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

void VirtWndRoot::TrackMouseLeaveIfNeeded() {
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

bool VirtWndRoot::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
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
            VirtWndMouseEvent ev;
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
            VirtWndMouseEvent ev;
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
            VirtWndMouseEvent ev;
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
            VirtWndMouseEvent ev;
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
            VirtWndMouseEvent ev;
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
            VirtWndMouseEvent ev;
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
            VirtWndKeyEvent ev;
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

//--- VirtWndBox

static Kind kindVirtWndBox = "virtWndBox";

VirtWndBox::VirtWndBox(bool isVert) {
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

VirtWndBox::~VirtWndBox() {
    FreeBox(vbox, hbox);
}

void VirtWndBox::AddChild(VirtWnd* c, int flexVal) {
    VirtWnd::AddChild(c);
    flexes.Append(flexVal);
    FreeBox(vbox, hbox);
}

void VirtWndBox::RebuildBox() {
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

ILayout* VirtWndBox::Box() {
    if (!vbox && !hbox) {
        RebuildBox();
    }
    return isVertical ? (ILayout*)vbox : (ILayout*)hbox;
}

int VirtWndBox::MinIntrinsicHeight(int width) {
    return Box()->MinIntrinsicHeight(width);
}

int VirtWndBox::MinIntrinsicWidth(int height) {
    return Box()->MinIntrinsicWidth(height);
}

Size VirtWndBox::Layout(Constraints bc) {
    Constraints inner = bc.Inset(padding.left + padding.right, padding.top + padding.bottom);
    Size sz = Box()->Layout(inner);
    sz.dx += padding.left + padding.right;
    sz.dy += padding.top + padding.bottom;
    return sz;
}

void VirtWndBox::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    Box()->SetBounds(content);
}

//--- VirtWndTable

static Kind kindVirtWndTable = "virtWndTable";

VirtWndTable::VirtWndTable() {
    kind = kindVirtWndTable;
    // a grid is decorative, only its cells' children are hit targets
    flags |= vwfNoHitTest;
}

// the cells' children are owned by VirtWnd::children, which ~VirtWnd frees
VirtWndTable::~VirtWndTable() = default;

int VirtWndTable::CellIdx(int row, int col) const {
    ReportIf(row < 0 || row >= rows);
    ReportIf(col < 0 || col >= cols);
    return (row * cols) + col;
}

void VirtWndTable::SetSize(int nRows, int nCols) {
    ReportIf(nRows < 0 || nCols < 0);
    if (nRows == rows && nCols == cols) {
        return;
    }
    RemoveAllChildren(true);
    rows = nRows;
    cols = nCols;
    cells.Clear();
    VirtWndTableCell empty;
    for (int i = 0; i < rows * cols; i++) {
        cells.Append(empty);
    }
    colWidths.Clear();
    rowHeights.Clear();
}

void VirtWndTable::MarkCovered(int row, int col, int rowSpan, int colSpan, bool covered) {
    for (int r = row; r < row + rowSpan; r++) {
        for (int c = col; c < col + colSpan; c++) {
            if (r == row && c == col) {
                continue;
            }
            VirtWndTableCell& cell = cells[CellIdx(r, c)];
            // a spanned-over cell can't hold a child of its own
            ReportIf(covered && cell.child);
            cell.covered = covered;
        }
    }
}

// (row, col) is the cell's top-left; a spanning cell covers the ones to its
// right / below, which must stay empty
VirtWndTableCell& VirtWndTable::SetCell(int row, int col, VirtWnd* child, int rowSpan, int colSpan) {
    ReportIf(rowSpan < 1 || colSpan < 1);
    ReportIf(row + rowSpan > rows || col + colSpan > cols);
    VirtWndTableCell& cell = cells[CellIdx(row, col)];
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

VirtWndTableCell* VirtWndTable::CellAt(int row, int col) {
    if (row < 0 || row >= rows || col < 0 || col >= cols) {
        return nullptr;
    }
    return &cells[CellIdx(row, col)];
}

VirtWnd* VirtWndTable::GetCell(int row, int col) {
    VirtWndTableCell* cell = CellAt(row, col);
    return cell ? cell->child : nullptr;
}

void VirtWndTable::RemoveAllCells() {
    RemoveAllChildren(true);
    VirtWndTableCell empty;
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
void VirtWndTable::Measure() {
    colWidths.Clear();
    colWidths.AppendBlanks(cols);
    rowHeights.Clear();
    rowHeights.AppendBlanks(rows);

    Constraints loose = ExpandInf();
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            VirtWndTableCell& cell = cells[CellIdx(row, col)];
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
            VirtWndTableCell& cell = cells[CellIdx(row, col)];
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

Size VirtWndTable::TotalSize() {
    int dx = padding.left + padding.right + TracksSize(colWidths, 0, cols, colGap);
    int dy = padding.top + padding.bottom + TracksSize(rowHeights, 0, rows, rowGap);
    return {dx, dy};
}

int VirtWndTable::MinIntrinsicHeight(int) {
    Measure();
    return TotalSize().dy;
}

int VirtWndTable::MinIntrinsicWidth(int) {
    Measure();
    return TotalSize().dx;
}

Size VirtWndTable::Layout(Constraints bc) {
    Measure();
    return bc.Constrain(TotalSize());
}

int VirtWndTable::ColWidth(int col) {
    if (col < 0 || col >= len(colWidths)) {
        return 0;
    }
    return colWidths[col];
}

int VirtWndTable::RowHeight(int row) {
    if (row < 0 || row >= len(rowHeights)) {
        return 0;
    }
    return rowHeights[row];
}

Rect VirtWndTable::ContentRect() {
    Rect r = lastBounds;
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// in the same coords SetBounds() was given
Rect VirtWndTable::CellRect(int row, int col) {
    VirtWndTableCell* cell = CellAt(row, col);
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

void VirtWndTable::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    if (len(colWidths) != cols || len(rowHeights) != rows) {
        // SetBounds() without a preceding Layout()
        Measure();
    }
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            VirtWndTableCell& cell = cells[CellIdx(row, col)];
            if (!cell.child || IsCollapsed(cell.child)) {
                continue;
            }
            Rect cellRc = CellRect(row, col);
            cell.child->SetBounds(AlignInCell(cellRc, cell.childSize, cell.alignH, cell.alignV));
        }
    }
}

//--- VirtWndScroll

static Kind kindVirtWndScroll = "virtWndScroll";

VirtWndScroll::VirtWndScroll() {
    kind = kindVirtWndScroll;
    flags |= vwfClipChildren;
}

VirtWndScroll::~VirtWndScroll() = default;

Size VirtWndScroll::Layout(Constraints bc) {
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

void VirtWndScroll::SetBounds(Rect r) {
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

Point VirtWndScroll::ScrollOffset() {
    return {0, scrollY};
}

void VirtWndScroll::SetContentDy(int dy) {
    if (contentDy == dy) {
        return;
    }
    contentDy = dy;
    ScrollTo(scrollY);
    UpdateScrollbar();
}

int VirtWndScroll::MaxScrollY() const {
    int visible = bounds.dy - padding.top - padding.bottom;
    int res = contentDy - visible;
    return res > 0 ? res : 0;
}

bool VirtWndScroll::ScrollTo(int y) {
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

bool VirtWndScroll::ScrollBy(int dy) {
    return ScrollTo(scrollY + dy);
}

bool VirtWndScroll::ScrollPage(int dir) {
    int visible = bounds.dy - padding.top - padding.bottom;
    return ScrollBy(dir * visible);
}

void VirtWndScroll::ScrollIntoView(VirtWnd* w) {
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

bool VirtWndScroll::OnMouseWheel(VirtWndMouseEvent& ev) {
    if (ev.wheelDelta == 0) {
        return false;
    }
    int lines = -(ev.wheelDelta * 3) / WHEEL_DELTA;
    return ScrollBy(lines * lineDy);
}

void VirtWndScroll::OnVScroll(WPARAM wp) {
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

void VirtWndScroll::UpdateScrollbar() {
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
void VirtWndScroll::NotifyVisibleRange() {
    if (onVisibleRangeChanged.IsEmpty()) {
        return;
    }
    int visible = bounds.dy - padding.top - padding.bottom;
    if (scrollY == lastNotifiedY && visible == lastNotifiedDy) {
        return;
    }
    lastNotifiedY = scrollY;
    lastNotifiedDy = visible;
    VirtWndScrollRange r;
    r.wnd = this;
    r.visibleY = scrollY;
    r.visibleDy = visible;
    onVisibleRangeChanged.Call(&r);
}

//--- VirtWndCustom

static Kind kindVirtWndCustom = "virtWndCustom";

VirtWndCustom::VirtWndCustom() {
    kind = kindVirtWndCustom;
}

VirtWndCustom::~VirtWndCustom() = default;

Size VirtWndCustom::GetIdealSize() {
    return idealSize;
}

void VirtWndCustom::Paint(VirtWndPaintCtx& ctx) {
    onPaint.Call(&ctx);
}

bool VirtWndCustom::OnMouseUp(VirtWndMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

//--- VirtWndWrapper

static Kind kindVirtWndWrapper = "virtWndWrapper";

VirtWndWrapper::VirtWndWrapper(Wnd* w, bool owns) {
    kind = kindVirtWndWrapper;
    wnd = w;
    ownsWnd = owns;
    // the HWND is on top of whatever the tree paints, and gets its own mouse
    // and keyboard messages
    flags |= vwfNoHitTest;
}

VirtWndWrapper::~VirtWndWrapper() {
    if (ownsWnd) {
        delete wnd;
    }
}

int VirtWndWrapper::MinIntrinsicHeight(int width) {
    return wnd ? wnd->MinIntrinsicHeight(width) : 0;
}

int VirtWndWrapper::MinIntrinsicWidth(int height) {
    return wnd ? wnd->MinIntrinsicWidth(height) : 0;
}

Size VirtWndWrapper::Layout(Constraints bc) {
    if (!wnd) {
        return bc.Constrain({0, 0});
    }
    return wnd->Layout(bc);
}

Size VirtWndWrapper::GetIdealSize() {
    return wnd ? wnd->GetIdealSize() : Size{};
}

// the tree lays out in the root HWND's client coords, which is also what the
// child HWND is positioned in
void VirtWndWrapper::SetBounds(Rect r) {
    VirtWnd::SetBounds(r);
    if (!wnd) {
        return;
    }
    wnd->SetVisibility(visibility);
    if (visibility != Visibility::Collapse) {
        wnd->SetBounds(r);
    }
}

//--- VirtWndText

static Kind kindVirtWndText = "virtWndText";

VirtWndText::VirtWndText(Str str, PlatformFont* f) {
    kind = kindVirtWndText;
    s = str::Dup(str);
    font = f;
    flags |= vwfNoHitTest;
}

VirtWndText::~VirtWndText() {
    str::Free(s);
}

void VirtWndText::SetText(Str str) {
    str::Free(s);
    s = str::Dup(str);
    sz = {0, 0};
}

Size VirtWndText::Layout(const Constraints bc) {
    GetIdealSize();
    return bc.Constrain({sz.dx, sz.dy});
}

int VirtWndText::MinIntrinsicHeight(int) {
    GetIdealSize(true);
    return sz.dy;
}

int VirtWndText::MinIntrinsicWidth(int) {
    GetIdealSize(true);
    return sz.dx;
}

Size VirtWndText::MinIntrinsicSize(int width, int height) {
    int dx = MinIntrinsicWidth(height);
    int dy = MinIntrinsicHeight(width);
    return {dx, dy};
}

Size VirtWndText::GetIdealSize() {
    return GetIdealSize(false);
}

Size VirtWndText::GetIdealSize(bool onlyIfEmpty) {
    if (onlyIfEmpty && !sz.IsEmpty()) {
        return sz;
    }
    sz = PlatformFontMeasureText(font, s);
    return sz;
}

void VirtWndText::Paint(VirtWndPaintCtx& ctx) {
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
        case VirtWndTextAlign::Center:
            fmt |= gfxTextCenter;
            break;
        case VirtWndTextAlign::Right:
            fmt |= gfxTextRight;
            break;
        case VirtWndTextAlign::Left:
            break;
    }
    GfxDrawText(ctx.gfx, s, r, fmt, font, textColor);
    if (withUnderline) {
        GetIdealSize(true);
        Rect lineRect = {r.x, r.y + sz.dy + underlineOffsetY, sz.dx, 0};
        GfxDrawLine(ctx.gfx, lineRect, textColor);
    }
}

//--- VirtWndLink

static Kind kindVirtWndLink = "virtWndLink";

VirtWndLink::VirtWndLink(Str str, PlatformFont* f) : VirtWndText(str, f) {
    kind = kindVirtWndLink;
    flags &= ~vwfNoHitTest;
}

VirtWndLink::~VirtWndLink() {
    str::Free(target);
    str::Free(tooltip);
}

void VirtWndLink::SetTarget(Str s2) {
    str::Free(target);
    target = str::Dup(s2);
}

void VirtWndLink::SetTooltip(Str s2) {
    str::Free(tooltip);
    tooltip = str::Dup(s2);
}

void VirtWndLink::Paint(VirtWndPaintCtx& ctx) {
    bool prevUnderline = withUnderline;
    if (underlineOnHover) {
        withUnderline = HasFlag(vwfHovered);
    }
    VirtWndText::Paint(ctx);
    withUnderline = prevUnderline;
}

void VirtWndLink::OnMouseEnter() {
    if (underlineOnHover) {
        Invalidate();
    }
}

void VirtWndLink::OnMouseLeave() {
    if (underlineOnHover) {
        Invalidate();
    }
}

bool VirtWndLink::OnMouseDown(VirtWndMouseEvent&) {
    // consume so that the click doesn't fall through to the page below
    return true;
}

bool VirtWndLink::OnMouseUp(VirtWndMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtWndLink::OnSetCursor(Point) {
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtWndLink::GetTooltipTemp(Point) {
    if (!tooltip) {
        return nullptr;
    }
    return str::DupTemp(tooltip);
}

//--- VirtWndButton

static Kind kindVirtWndButton = "virtWndButton";

VirtWndButton::VirtWndButton(Str str, PlatformFont* f) : VirtWndText(str, f) {
    kind = kindVirtWndButton;
    flags &= ~vwfNoHitTest;
    align = VirtWndTextAlign::Center;
}

VirtWndButton::~VirtWndButton() = default;

Size VirtWndButton::GetIdealSize() {
    Size s2 = VirtWndText::GetIdealSize(true);
    return {s2.dx + textPadding.left + textPadding.right, s2.dy + textPadding.top + textPadding.bottom};
}

void VirtWndButton::Paint(VirtWndPaintCtx& ctx) {
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
    VirtWndPaintCtx c2 = ctx;
    c2.content = r;
    COLORREF prevCol = textColor;
    if (!isEnabled && textColorDisabled != kColorUnset) {
        textColor = textColorDisabled;
    }
    VirtWndText::Paint(c2);
    textColor = prevCol;
}

void VirtWndButton::OnMouseEnter() {
    Invalidate();
}

void VirtWndButton::OnMouseLeave() {
    Invalidate();
}

bool VirtWndButton::OnMouseDown(VirtWndMouseEvent&) {
    return true;
}

bool VirtWndButton::OnMouseUp(VirtWndMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtWndButton::OnSetCursor(Point) {
    if (!HasFlag(vwfEnabled)) {
        return false;
    }
    SetCursorCached(IDC_HAND);
    return true;
}

//--- VirtWndIconButton

static Kind kindVirtWndIconButton = "virtWndIconButton";

VirtWndIconButton::VirtWndIconButton() {
    kind = kindVirtWndIconButton;
}

VirtWndIconButton::~VirtWndIconButton() {
    str::Free(tooltip);
}

void VirtWndIconButton::SetTooltip(Str s2) {
    str::Free(tooltip);
    tooltip = str::Dup(s2);
}

Size VirtWndIconButton::GetIdealSize() {
    if (!pixmap) {
        return {0, 0};
    }
    return {pixmap->width, pixmap->height};
}

void VirtWndIconButton::Paint(VirtWndPaintCtx& ctx) {
    if (!pixmap) {
        return;
    }
    Size s2 = GetIdealSize();
    Rect r = ctx.content;
    int x = r.x + ((r.dx - s2.dx) / 2);
    int y = r.y + ((r.dy - s2.dy) / 2);
    GfxDrawPixmap(ctx.gfx, pixmap, {x, y, s2.dx, s2.dy});
}

void VirtWndIconButton::OnMouseEnter() {
    Invalidate();
}

void VirtWndIconButton::OnMouseLeave() {
    Invalidate();
}

bool VirtWndIconButton::OnMouseDown(VirtWndMouseEvent&) {
    return true;
}

bool VirtWndIconButton::OnMouseUp(VirtWndMouseEvent& ev) {
    if (onClick.IsEmpty()) {
        return false;
    }
    onClick.Call(&ev);
    return true;
}

bool VirtWndIconButton::OnSetCursor(Point) {
    SetCursorCached(IDC_HAND);
    return true;
}

TempStr VirtWndIconButton::GetTooltipTemp(Point) {
    if (!tooltip) {
        return nullptr;
    }
    return str::DupTemp(tooltip);
}

//--- VirtWndImage

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

VirtWndImage::VirtWndImage() {
    kind = kindVirtWndImage;
    flags |= vwfNoHitTest;
}

VirtWndImage::~VirtWndImage() = default;

Size VirtWndImage::GetIdealSize() {
    if (!pixmap) {
        return {0, 0};
    }
    return {pixmap->width, pixmap->height};
}

void VirtWndImage::Paint(VirtWndPaintCtx& ctx) {
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

//--- VirtWndFill

static Kind kindVirtWndFill = "virtWndFill";

VirtWndFill::VirtWndFill() {
    kind = kindVirtWndFill;
    flags |= vwfNoHitTest;
}

VirtWndFill::~VirtWndFill() = default;

Size VirtWndFill::GetIdealSize() {
    return idealSize;
}

void VirtWndFill::Paint(VirtWndPaintCtx& ctx) {
    GfxFillRect(ctx.gfx, ctx.bounds, color);
}

//--- VirtWndLine

static Kind kindVirtWndLine = "virtWndLine";

VirtWndLine::VirtWndLine() {
    kind = kindVirtWndLine;
    flags |= vwfNoHitTest;
}

VirtWndLine::~VirtWndLine() = default;

Size VirtWndLine::GetIdealSize() {
    if (isVertical) {
        return {thickness, 0};
    }
    return {0, thickness};
}

void VirtWndLine::Paint(VirtWndPaintCtx& ctx) {
    Rect r = ctx.content;
    if (isVertical) {
        r.dx = thickness;
    } else {
        r.dy = thickness;
    }
    GfxFillRect(ctx.gfx, r, color);
}

//--- VirtWndSpacer

static Kind kindVirtWndSpacer = "virtWndSpacer";

VirtWndSpacer::VirtWndSpacer(int dx, int dy) {
    kind = kindVirtWndSpacer;
    idealSize = {dx, dy};
    flags |= vwfNoHitTest;
}

VirtWndSpacer::~VirtWndSpacer() = default;

Size VirtWndSpacer::GetIdealSize() {
    return idealSize;
}

#if defined(DEBUG)
// must be last: UtAssert.h over-writes assert()
#include "base/UtAssert.h"

// Unit tests for VirtWndTable. VirtWndSpacer is the leaf: a fixed ideal size
// and no HWND, so a whole table can be laid out and its geometry asserted.

static bool VirtWndRectEq(const Rect& r, int x, int y, int dx, int dy) {
    return r.x == x && r.y == y && r.dx == dx && r.dy == dy;
}

static void VirtWndTable_TestGrid() {
    auto* t = new VirtWndTable();
    t->SetSize(2, 2);
    t->colGap = 10;
    t->rowGap = 4;
    auto* a = new VirtWndSpacer(20, 10);
    auto* b = new VirtWndSpacer(40, 30);
    auto* c = new VirtWndSpacer(30, 20);
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

static void VirtWndTable_TestAlign() {
    auto* t = new VirtWndTable();
    t->SetSize(3, 2);
    // sets col 0 to 100 wide and row 0 to 40 tall, so the other cells have
    // room to be aligned in
    auto* big = new VirtWndSpacer(100, 40);
    auto* bottom = new VirtWndSpacer(20, 10);
    auto* center = new VirtWndSpacer(20, 10);
    auto* stretch = new VirtWndSpacer(20, 10);
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

static void VirtWndTable_TestSpan() {
    auto* t = new VirtWndTable();
    t->SetSize(2, 2);
    t->colGap = 10;
    auto* wide = new VirtWndSpacer(100, 10);
    auto* a = new VirtWndSpacer(20, 10);
    auto* b = new VirtWndSpacer(30, 10);
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
    auto* t2 = new VirtWndTable();
    t2->SetSize(2, 2);
    t2->rowGap = 6;
    auto* tall = new VirtWndSpacer(10, 100);
    t2->SetCell(0, 0, tall, 2, 1);
    t2->SetCell(0, 1, new VirtWndSpacer(10, 20));
    t2->SetCell(1, 1, new VirtWndSpacer(10, 30));
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
static void VirtWndTable_TestHitTest() {
    auto* t = new VirtWndTable();
    t->SetSize(1, 2);
    t->colGap = 10;
    auto* a = new VirtWndSpacer(20, 10);
    auto* b = new VirtWndSpacer(30, 10);
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
    VirtWndTable_TestGrid();
    VirtWndTable_TestAlign();
    VirtWndTable_TestSpan();
    VirtWndTable_TestHitTest();
}
#endif
