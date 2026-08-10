/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
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
void VirtWnd::PaintTree(HDC hdc, Point origin, Rect clip) {
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
    ctx.hdc = hdc;
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
        c->PaintTree(ctx.hdc, origin, ctx.clip);
    }
}

// paints a standalone (parent-less) VirtWnd at the position it was given by
// SetBounds(). Used for one-off "measure a string and draw it" cases
void VirtWnd::PaintStandalone(HDC hdc) {
    PaintTree(hdc, {0, 0}, lastBounds);
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

void VirtWndRoot::Paint(HDC hdc, Rect clip) {
    if (!child) {
        return;
    }
    LayoutIfNeeded();
    Rect c = clip.Intersect(bounds);
    if (c.IsEmpty()) {
        return;
    }
    child->PaintTree(hdc, bounds.TL(), c);
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

//--- VirtWndText

static Kind kindVirtWndText = "virtWndText";

VirtWndText::VirtWndText(HWND hwnd, Str str, HFONT f) {
    kind = kindVirtWndText;
    s = str::Dup(str);
    hwndForMeasure = hwnd;
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
    HWND hwnd = hwndForMeasure ? hwndForMeasure : GetHwnd();
    sz = HwndMeasureText(hwnd, s, font);
    return sz;
}

void VirtWndText::Paint(VirtWndPaintCtx& ctx) {
    Rect r = ctx.content;
    if (r.IsEmpty()) {
        return;
    }
    COLORREF prevCol = kColorUnset;
    if (textColor != kColorUnset) {
        prevCol = SetTextColor(ctx.hdc, textColor);
    }
    int prevBkMode = SetBkMode(ctx.hdc, TRANSPARENT);
    uint fmt = DT_NOPREFIX;
    if (ellipsis) {
        fmt |= DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER;
    } else {
        fmt |= DT_NOCLIP;
    }
    if (isRtl) {
        fmt |= DT_RTLREADING;
    }
    switch (align) {
        case VirtWndTextAlign::Center:
            fmt |= DT_CENTER;
            break;
        case VirtWndTextAlign::Right:
            fmt |= DT_RIGHT;
            break;
        case VirtWndTextAlign::Left:
            break;
    }
    HdcDrawText(ctx.hdc, s, r, fmt, font);
    if (withUnderline) {
        GetIdealSize(true);
        Rect lineRect = {r.x, r.y + sz.dy + underlineOffsetY, sz.dx, 0};
        auto col = GetTextColor(ctx.hdc);
        ScopedSelectObject pen(ctx.hdc, CreatePen(PS_SOLID, 1, col), true);
        HdcDrawLine(ctx.hdc, lineRect);
    }
    if (textColor != kColorUnset) {
        SetTextColor(ctx.hdc, prevCol);
    }
    if (prevBkMode != 0) {
        SetBkMode(ctx.hdc, prevBkMode);
    }
}

//--- VirtWndLink

static Kind kindVirtWndLink = "virtWndLink";

VirtWndLink::VirtWndLink(HWND hwnd, Str str, HFONT f) : VirtWndText(hwnd, str, f) {
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

VirtWndButton::VirtWndButton(HWND hwnd, Str str, HFONT f) : VirtWndText(hwnd, str, f) {
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
    COLORREF bg = HasFlag(vwfHovered) ? bgColorHover : bgColor;
    if (bg != kColorUnset) {
        HdcFillRect(ctx.hdc, ctx.bounds, bg);
    }
    Rect r = ctx.content;
    r.SubTB(textPadding.top, textPadding.bottom);
    r.SubLR(textPadding.left, textPadding.right);
    VirtWndPaintCtx c2 = ctx;
    c2.content = r;
    VirtWndText::Paint(c2);
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
    if (!himl) {
        return {0, 0};
    }
    int dx = 0, dy = 0;
    ImageList_GetIconSize(himl, &dx, &dy);
    return {dx, dy};
}

void VirtWndIconButton::Paint(VirtWndPaintCtx& ctx) {
    if (!himl) {
        return;
    }
    Size s2 = GetIdealSize();
    Rect r = ctx.content;
    int x = r.x + (r.dx - s2.dx) / 2;
    int y = r.y + (r.dy - s2.dy) / 2;
    uint style = ILD_NORMAL;
    if (isSelected || HasFlag(vwfHovered)) {
        style = ILD_NORMAL | ILD_FOCUS;
    }
    ImageList_Draw(himl, iconIdx, ctx.hdc, x, y, style);
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
    return bmpSize;
}

void VirtWndImage::Paint(VirtWndPaintCtx& ctx) {
    if (!bmp) {
        return;
    }
    Rect r = ctx.content;
    if (fitToBounds) {
        r = FitSizeInRect(bmpSize, r);
    } else {
        r.dx = bmpSize.dx;
        r.dy = bmpSize.dy;
    }
    BlitHBITMAP(bmp, ctx.hdc, r);
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
    if (color == kColorUnset) {
        return;
    }
    HdcFillRect(ctx.hdc, ctx.bounds, color);
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
    if (color == kColorUnset) {
        return;
    }
    Rect r = ctx.content;
    if (isVertical) {
        r.dx = thickness;
    } else {
        r.dy = thickness;
    }
    HdcFillRect(ctx.hdc, r, color);
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
