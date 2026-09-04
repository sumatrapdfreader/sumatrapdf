/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtHost.h"
#if IS_DEBUG
#include "base/UtAssert.h"
#endif
#include "gui/VirtCtrl.h"

//--- VirtCtrl

static Kind kindVirtCtrl = "virtCtrl";

VirtCtrl::VirtCtrl() {
    kind = kindVirtCtrl;
    GuiColorsInitIfNeeded();
}

VirtCtrl::~VirtCtrl() {
    if (root) {
        root->OnWndDestroyed(this);
    }
    RemoveAllChildren(true);
    str::Free(tooltip);
    free(colors);
}

Color VirtCtrl::GetColor(int idx) const {
    ReportIf(idx >= nColors);
    return GetCol(colorDefaults, colors, idx);
}

// the override array is allocated on the first SetColor(), so the common case
// of a control that paints in its class's colors costs nothing
void VirtCtrl::SetColor(int idx, Color col) {
    ReportIf(idx >= nColors);
    if (idx >= nColors) {
        return;
    }
    if (!colors) {
        colors = AllocArray<Color>(nColors);
        for (int i = 0; i < nColors; i++) {
            colors[i] = kColorUnset;
        }
    }
    colors[idx] = col;
}

// back to painting in the class's colors
void VirtCtrl::ResetColors() {
    free(colors);
    colors = nullptr;
}

void VirtCtrl::SetTooltip(Str s) {
    str::ReplaceWithCopy(&tooltip, s);
}

int VirtCtrl::LayoutChildCount() {
    return len(children);
}

ILayout* VirtCtrl::LayoutChildAt(int idx) {
    return children[idx];
}

VirtCtrl* VirtCtrl::AsVirtCtrl() {
    return this;
}

void CollectVirtCtrls(ILayout* root, Vec<VirtCtrl*>& out) {
    if (!root) {
        return;
    }
    VirtCtrl* w = root->AsVirtCtrl();
    if (w) {
        // its children come with it: it paints and hit-tests them itself
        VecAppend(out, w);
        return;
    }
    int n = root->LayoutChildCount();
    for (int i = 0; i < n; i++) {
        CollectVirtCtrls(root->LayoutChildAt(i), out);
    }
}

bool IsVirtCtrlOfKind(VirtCtrl* w, Kind k) {
    return w && w->kind == k;
}

// ILayout: the containers in Layout.h hand us an absolute (HWND client) rect,
// and a parent is always laid out before its children, so we can rebase it into
// the parent-relative form the paint / hit-test walks expect
void VirtCtrl::SetBounds(Rect r) {
    lastBounds = r;
    Point po{0, 0};
    if (parent) {
        po = parent->ChildOriginInWindow();
    } else if (root) {
        po = root->bounds.TL();
    }
    bounds = {r.x - po.x, r.y - po.y, r.dx, r.dy};
}

Size VirtCtrl::Layout(Constraints bc) {
    Size sz = GetIdealSize();
    return bc.Constrain(sz);
}

int VirtCtrl::MinIntrinsicHeight(int) {
    return GetIdealSize().dy;
}

int VirtCtrl::MinIntrinsicWidth(int) {
    return GetIdealSize().dx;
}

Size VirtCtrl::GetIdealSize() {
    return {bounds.dx, bounds.dy};
}

void VirtCtrl::Paint(VirtPaintCtx&) {}

// origin is the window position of our parent's content origin (already
// adjusted for the parent's scroll offset)
void VirtCtrl::PaintTree(Gfx* gfx, Point origin, Rect clip) {
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

void VirtCtrl::PaintChildren(VirtPaintCtx& ctx) {
    Point so = ScrollOffset();
    Point origin{ctx.content.x - so.x, ctx.content.y - so.y};
    for (VirtCtrl* c : children) {
        c->PaintTree(ctx.gfx, origin, ctx.clip);
    }
}

// paints a standalone (parent-less) VirtCtrl at the position it was given by
// SetBounds(). Used for one-off "measure a string and draw it" cases
void VirtCtrl::PaintStandalone(Gfx* gfx) {
    PaintTree(gfx, {0, 0}, lastBounds);
}

bool VirtCtrl::HitTest(Point ptLocal) {
    Rect r{0, 0, bounds.dx, bounds.dy};
    return r.Contains(ptLocal);
}

Point VirtCtrl::ScrollOffset() {
    return {0, 0};
}

// Hit-test the ILayout subtree at ptWindow. Topmost (last) child wins.
// Walks LayoutChildAt so containers like Table are included (AboutCtrl).
// vhfIncludeDisabled also matches disabled-but-visible VirtCtrls so gray
// toolbar buttons still show their tooltips.
ILayout* ElementFromPoint(ILayout* root, Point ptWindow, Point* ptLocalOut, u32 flags) {
    if (!root || root->visibility != Visibility::Visible) {
        return nullptr;
    }
    VirtCtrl* vc = root->AsVirtCtrl();
    if (vc) {
        if (!(flags & vhfIncludeDisabled) && !vc->HasFlag(vwfEnabled)) {
            return nullptr;
        }
        Rect b = vc->BoundsInWindow();
        if (!b.Contains(ptWindow)) {
            return nullptr;
        }
        if (!vc->HasFlag(vwfPaintsOwnChildren)) {
            for (int i = root->LayoutChildCount() - 1; i >= 0; i--) {
                ILayout* hit = ElementFromPoint(root->LayoutChildAt(i), ptWindow, ptLocalOut, flags);
                if (hit) {
                    return hit;
                }
            }
        }
        if (vc->HasFlag(vwfNoHitTest) || !vc->HitTest({ptWindow.x - b.x, ptWindow.y - b.y})) {
            return nullptr;
        }
        if (ptLocalOut) {
            *ptLocalOut = {ptWindow.x - b.x, ptWindow.y - b.y};
        }
        return root;
    }
    Rect b = root->lastBounds;
    if (!b.Contains(ptWindow)) {
        return nullptr;
    }
    for (int i = root->LayoutChildCount() - 1; i >= 0; i--) {
        ILayout* hit = ElementFromPoint(root->LayoutChildAt(i), ptWindow, ptLocalOut, flags);
        if (hit) {
            return hit;
        }
    }
    if (root->AsControl()) {
        if (ptLocalOut) {
            *ptLocalOut = {ptWindow.x - b.x, ptWindow.y - b.y};
        }
        return root;
    }
    return nullptr;
}

bool VirtCtrl::OnMouseDown(VirtMouseEvent& ev) {
    if (onMouseDown.IsValid()) {
        ev.didHandle = false;
        onMouseDown.Call(&ev);
        return ev.didHandle;
    }
    // clickable by default: swallow press so a parent does not steal the click
    return onClick.IsValid();
}

bool VirtCtrl::OnMouseUp(VirtMouseEvent& ev) {
    if (onMouseUp.IsValid()) {
        ev.didHandle = false;
        onMouseUp.Call(&ev);
        return ev.didHandle;
    }
    if (onClick.IsValid()) {
        onClick.Call(&ev);
        return true;
    }
    return false;
}

bool VirtCtrl::OnMouseMove(VirtMouseEvent& ev) {
    if (!onMouseMove.IsValid()) {
        return false;
    }
    ev.didHandle = false;
    onMouseMove.Call(&ev);
    return ev.didHandle;
}

bool VirtCtrl::OnMouseWheel(VirtMouseEvent& ev) {
    if (!onMouseWheel.IsValid()) {
        return false;
    }
    ev.didHandle = false;
    onMouseWheel.Call(&ev);
    return ev.didHandle;
}

bool VirtCtrl::OnDoubleClick(VirtMouseEvent& ev) {
    if (!onDoubleClick.IsValid()) {
        return false;
    }
    ev.didHandle = false;
    onDoubleClick.Call(&ev);
    return ev.didHandle;
}

bool VirtCtrl::OnContextMenu(VirtMouseEvent& ev) {
    if (!onContextMenu.IsValid()) {
        return false;
    }
    ev.didHandle = false;
    onContextMenu.Call(&ev);
    return ev.didHandle;
}

void VirtCtrl::OnMouseEnter() {
    if (onMouseEnter.IsValid()) {
        onMouseEnter.Call();
    }
}

void VirtCtrl::OnMouseLeave() {
    if (onMouseLeave.IsValid()) {
        onMouseLeave.Call();
    }
}

void VirtCtrl::OnCaptureLost() {
    if (onCaptureLost.IsValid()) {
        onCaptureLost.Call();
    }
}

bool VirtCtrl::OnKeyDown(VirtKeyEvent& ev) {
    if (!onKeyDown.IsValid()) {
        return false;
    }
    ev.didHandle = false;
    onKeyDown.Call(&ev);
    return ev.didHandle;
}

bool VirtCtrl::OnChar(int c) {
    if (!onChar.IsValid()) {
        return false;
    }
    VirtCharEvent ev;
    ev.w = this;
    ev.c = c;
    onChar.Call(&ev);
    return ev.didHandle;
}

void VirtCtrl::OnFocusChanged(bool gotFocus) {
    if (!onFocusChanged.IsValid()) {
        return;
    }
    VirtFocusEvent ev;
    ev.w = this;
    ev.gotFocus = gotFocus;
    onFocusChanged.Call(&ev);
}

bool VirtCtrl::OnSetCursor(Point ptLocal) {
    if (onSetCursor.IsValid()) {
        VirtSetCursorEvent ev;
        ev.w = this;
        ev.ptLocal = ptLocal;
        onSetCursor.Call(&ev);
        return ev.didHandle;
    }
    if (HasFlag(vwfEnabled) && cursor != CursorId::None) {
        UiSetCursor(cursor);
        return true;
    }
    return false;
}

TempStr VirtCtrl::GetTooltipTemp(Point ptLocal) {
    if (onGetTooltip.IsValid()) {
        VirtTooltipEvent ev;
        ev.w = this;
        ev.ptLocal = ptLocal;
        onGetTooltip.Call(&ev);
        return ev.tip;
    }
    if (tooltip) {
        return str::DupTemp(tooltip);
    }
    return {};
}

void VirtCtrl::AddChild(VirtCtrl* c) {
    InsertChild(c, -1);
}

void VirtCtrl::InsertChild(VirtCtrl* c, int idx) {
    ReportIf(!c);
    ReportIf(c->parent);
    c->parent = this;
    c->SetRoot(root);
    if (idx < 0 || idx >= len(children)) {
        VecAppend(children, c);
    } else {
        VecInsertAt(children, idx, c);
    }
}

void VirtCtrl::RemoveChild(VirtCtrl* c, bool del) {
    int idx = VecFind(children, c);
    if (idx < 0) {
        return;
    }
    VecRemoveAt(children, idx);
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

void VirtCtrl::RemoveAllChildren(bool del) {
    // take a copy of the pointers first: ~VirtCtrl() of a child can reach back
    // into us via root->OnWndDestroyed()
    Vec<VirtCtrl*> tmp;
    for (VirtCtrl* c : children) {
        VecAppend(tmp, c);
    }
    VecClear(children);
    for (VirtCtrl* c : tmp) {
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

int VirtCtrl::ChildCount() const {
    return len(children);
}

VirtCtrl* VirtCtrl::ChildAt(int idx) const {
    if (idx < 0 || idx >= len(children)) {
        return nullptr;
    }
    return children[idx];
}

VirtCtrl* VirtCtrl::FindById(int wndId) {
    if (id == wndId) {
        return this;
    }
    for (VirtCtrl* c : children) {
        VirtCtrl* res = c->FindById(wndId);
        if (res) {
            return res;
        }
    }
    return nullptr;
}

Point VirtCtrl::OriginInWindow() {
    Point p{0, 0};
    if (parent) {
        p = parent->ChildOriginInWindow();
    } else if (root) {
        p = root->bounds.TL();
    }
    return {p.x + bounds.x, p.y + bounds.y};
}

// where children are positioned: our content origin, shifted by our scroll
Point VirtCtrl::ChildOriginInWindow() {
    Point o = OriginInWindow();
    Point so = ScrollOffset();
    return {o.x + padding.left - so.x, o.y + padding.top - so.y};
}

Rect VirtCtrl::BoundsInWindow() {
    Point o = OriginInWindow();
    return {o.x, o.y, bounds.dx, bounds.dy};
}

Rect VirtCtrl::ContentRectInWindow() {
    Rect r = BoundsInWindow();
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// our bounds clipped by every clipping ancestor, so that a wnd scrolled out of
// its container doesn't invalidate (or hit-test) outside of it
Rect VirtCtrl::VisibleRectInWindow() {
    Rect r = BoundsInWindow();
    VirtCtrl* p = parent;
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

void VirtCtrl::Invalidate() {
    if (!root) {
        return;
    }
    root->Invalidate(VisibleRectInWindow());
}

void VirtCtrl::Invalidate(Rect rLocal) {
    if (!root) {
        return;
    }
    Point o = OriginInWindow();
    Rect r = rLocal;
    r.Offset(o.x, o.y);
    root->Invalidate(r.Intersect(VisibleRectInWindow()));
}

void VirtCtrl::RequestLayout() {
    if (root) {
        root->RequestLayout();
    }
}

void VirtCtrl::SetIsVisible(bool isVisible) {
    Visibility v = isVisible ? Visibility::Visible : Visibility::Collapse;
    if (v == visibility) {
        return;
    }
    // the owner re-lays out after showing / hiding; invalidate the space we
    // occupied so a hidden control doesn't stay on screen until it does
    Invalidate();
    visibility = v;
    Invalidate();
}

bool VirtCtrl::IsVisible() const {
    return visibility == Visibility::Visible;
}

void VirtCtrl::SetIsEnabled(bool isEnabled) {
    if (isEnabled == HasFlag(vwfEnabled)) {
        return;
    }
    SetFlag(vwfEnabled, isEnabled);
    Invalidate();
}

bool VirtCtrl::IsEnabled() const {
    return HasFlag(vwfEnabled);
}

HWND VirtCtrl::GetHwnd() const {
    return root ? root->hwnd : nullptr;
}

bool VirtCtrl::IsPaintable() const {
    return visibility == Visibility::Visible;
}

bool VirtCtrl::IsHitTestable() const {
    if (visibility != Visibility::Visible) {
        return false;
    }
    return HasFlag(vwfEnabled);
}

void VirtCtrl::SetFlag(u32 f, bool on) {
    if (on) {
        flags |= f;
    } else {
        flags &= ~f;
    }
}

bool VirtCtrl::HasFlag(u32 f) const {
    return (flags & f) != 0;
}

void VirtCtrl::SetRoot(VirtRoot* r) {
    root = r;
    for (VirtCtrl* c : children) {
        c->SetRoot(r);
    }
}

//--- VirtRoot

VirtRoot::VirtRoot(HWND hwnd) {
    this->hwnd = hwnd;
}

VirtRoot::~VirtRoot() {
    // `tops` belong to the layout tree and can outlive us; make sure they don't
    // report their destruction to a root that is gone
    for (VirtCtrl* w : tops) {
        w->SetRoot(nullptr);
    }
    delete owned;
    delete tooltip;
    GfxDestroyDoubleBuffer(gfxBuf);
    delete gfxBuf;
    gfxBuf = nullptr;
}

void VirtRoot::SetChild(VirtCtrl* c) {
    if (owned == c) {
        return;
    }
    delete owned;
    owned = c;
    VecReset(tops);
    hovered = nullptr;
    captured = nullptr;
    focused = nullptr;
    pressed = nullptr;
    HideTooltip();
    if (c) {
        c->parent = nullptr;
        c->SetRoot(this);
        VecAppend(tops, c);
    }
    // the whole window is this one tree, so it can be laid out lazily, from
    // Paint(). A tree that also holds HWND controls can't: laying out would
    // move child windows in the middle of WM_PAINT
    layoutInPaint = true;
    needsLayout = true;
}

void VirtRoot::SetTops(const Vec<VirtCtrl*>& newTops) {
    ReportIf(owned);
    VecReset(tops);
    hovered = nullptr;
    captured = nullptr;
    focused = nullptr;
    pressed = nullptr;
    HideTooltip();
    for (VirtCtrl* w : newTops) {
        w->SetRoot(this);
        VecAppend(tops, w);
    }
    layoutInPaint = false;
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
    if (!needsLayout || !owned) {
        return;
    }
    needsLayout = false;
    Constraints bc = Tight({bounds.dx, bounds.dy});
    owned->Layout(bc);
    owned->SetBounds(bounds);
}

void VirtRoot::Paint(Gfx* gfx, Rect clip) {
    if (len(tops) == 0) {
        return;
    }
    if (layoutInPaint) {
        LayoutIfNeeded();
    }
    Rect c = clip.Intersect(bounds);
    if (c.IsEmpty()) {
        return;
    }
    // painted in layout order, so a later one draws over an earlier one
    for (VirtCtrl* w : tops) {
        w->PaintTree(gfx, bounds.TL(), c);
    }
}

ILayout* ElementFromPoint(VirtRoot* root, Point ptWindow, Point* ptLocalOut, u32 flags) {
    if (!root || len(root->tops) == 0 || !root->bounds.Contains(ptWindow)) {
        return nullptr;
    }
    if (root->layoutInPaint) {
        root->LayoutIfNeeded();
    }
    // reverse of the paint order: whatever is drawn last is on top
    for (int i = len(root->tops) - 1; i >= 0; i--) {
        ILayout* w = ElementFromPoint(root->tops[i], ptWindow, ptLocalOut, flags);
        if (w) {
            return w;
        }
    }
    return nullptr;
}

void VirtRoot::HideTooltip() {
    tooltipWnd = nullptr;
    if (tooltip) {
        tooltip->Delete();
    }
}

static VirtCtrl* VirtAtPoint(VirtRoot* root, Point ptWindow, Point* ptLocalOut, u32 flags = 0) {
    ILayout* el = ElementFromPoint(root, ptWindow, ptLocalOut, flags);
    return el ? el->AsVirtCtrl() : nullptr;
}

void VirtRoot::UpdateTooltip(Point ptWindow) {
    Point ptLocal{};
    VirtCtrl* w = VirtAtPoint(this, ptWindow, &ptLocal, vhfIncludeDisabled);
    TempStr tip{};
    Rect tipRc{};
    while (w) {
        tip = w->GetTooltipTemp(ptLocal);
        if (tip) {
            tipRc = w->BoundsInWindow();
            break;
        }
        w = w->parent;
    }
    if (len(tip) == 0) {
        HideTooltip();
        return;
    }
    // already showing for this control: leave the bubble where it first appeared
    if (w == tooltipWnd && tooltip && tooltip->Count() > 0) {
        tooltip->SetSingle(tip, tipRc, false);
        return;
    }
    if (!tooltip && hwnd) {
        Tooltip::CreateArgs args;
        args.parent = hwnd;
        tooltip = new Tooltip();
        tooltip->Create(args);
    }
    if (tooltip) {
        tooltip->SetSingle(tip, tipRc, false);
        tooltipWnd = w;
    }
}

void VirtRoot::Invalidate(Rect rWindow) {
    if (!hwnd || rWindow.IsEmpty()) {
        return;
    }
    HwndInvalidateRect(hwnd, rWindow, false);
}

void VirtRoot::SetFocus(VirtCtrl* w) {
    if (w && !(w->IsHitTestable() && w->HasFlag(vwfFocusable))) {
        w = nullptr;
    }
    if (w == focused) {
        return;
    }
    VirtCtrl* prev = focused;
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

static void CollectFocusable(VirtCtrl* w, Vec<VirtCtrl*>& out) {
    if (!w || !w->IsHitTestable()) {
        return;
    }
    if (w->HasFlag(vwfFocusable) && !w->HasFlag(vwfSkipTabStop)) {
        VecAppend(out, w);
    }
    for (VirtCtrl* c : w->children) {
        CollectFocusable(c, out);
    }
}

// a win32 control is in the ring if it says so (WS_TABSTOP) and can take focus
static bool IsCtrlTabStop(ControlBase* c) {
    HWND hwnd = c->hwnd;
    if (!hwnd || !::IsWindowVisible(hwnd) || !::IsWindowEnabled(hwnd)) {
        return false;
    }
    DWORD style = (DWORD)GetWindowLong(hwnd, GWL_STYLE);
    return (style & WS_TABSTOP) != 0;
}

void CollectTabStops(ILayout* root, Vec<TabStop>& out) {
    if (!root || IsCollapsed(root)) {
        return;
    }
    ControlBase* c = root->AsControl();
    if (c) {
        if (IsCtrlTabStop(c)) {
            VecAppend(out, TabStop{c, nullptr});
        }
        return;
    }
    VirtCtrl* w = root->AsVirtCtrl();
    if (w) {
        // a virtual control can hold more than one stop: its children are part
        // of the same tree
        Vec<VirtCtrl*> focusable;
        CollectFocusable(w, focusable);
        for (VirtCtrl* f : focusable) {
            VecAppend(out, TabStop{nullptr, f});
        }
        return;
    }
    int n = root->LayoutChildCount();
    for (int i = 0; i < n; i++) {
        CollectTabStops(root->LayoutChildAt(i), out);
    }
}

bool VirtRoot::TabNavigate(bool backwards) {
    Vec<VirtCtrl*> all;
    for (VirtCtrl* w : tops) {
        CollectFocusable(w, all);
    }
    int n = len(all);
    if (n == 0) {
        return false;
    }
    int idx = focused ? VecFind(all, focused) : -1;
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
    HwndSetFocusForce(hwnd);
    return true;
}

void VirtRoot::SetCapture(VirtCtrl* w) {
    captured = w;
    if (w && hwnd) {
        ::SetCapture(hwnd);
    }
}

void VirtRoot::ReleaseCapture() {
    VirtCtrl* w = captured;
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
    VirtCtrl* w = pressed;
    if (!w) {
        return;
    }
    pressed = nullptr;
    w->SetFlag(vwfPressed, false);
    w->Invalidate();
}

void VirtRoot::ClearHover() {
    VirtCtrl* w = hovered;
    if (!w) {
        return;
    }
    hovered = nullptr;
    w->SetFlag(vwfHovered, false);
    w->OnMouseLeave();
    w->Invalidate();
}

// a wnd is going away: don't leave dangling hover / capture / focus pointers
void VirtRoot::OnWndDestroyed(VirtCtrl* w) {
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
    HideTooltip();
    // it can be one of the tops (the tree is rebuilt by deleting nodes and
    // laying out again), and those we must not paint or hit-test any more
    int idx = VecFind(tops, w);
    if (idx >= 0) {
        VecRemoveAt(tops, idx);
    }
}

static void FillMouseEvent(VirtMouseEvent& ev, VirtCtrl* target, Point ptWindow, Point ptLocal, bool captured,
                           WPARAM wp = 0) {
    ev.target = target;
    ev.hit = target;
    ev.ptWindow = ptWindow;
    ev.pt = captured ? ptWindow : ptLocal;
    // MK_* on the mouse message so posted clicks (tests) match a real Ctrl/Shift
    ev.isCtrl = IsCtrlPressed() || (wp & MK_CONTROL) != 0;
    ev.isShift = IsShiftPressed() || (wp & MK_SHIFT) != 0;
    ev.isAlt = IsAltPressed();
}

// walks up from target until someone consumes the event
static bool BubbleMouse(VirtCtrl* target, VirtMouseEvent& ev, bool (VirtCtrl::*handler)(VirtMouseEvent&)) {
    VirtCtrl* w = target;
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
    // posted moves (tests) are not the real cursor; TME_LEAVE would fire
    // at once and clear hover the move just set
    if (!HwndWindowRect(hwnd).Contains(GetCursorPosition())) {
        return;
    }
    TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    if (TrackMouseEvent(&tme)) {
        trackingMouseLeave = true;
    }
}

// press a virtual control (mouse down, or a DBLCLK that is really a second click)
static bool BeginVirtPress(VirtRoot* root, VirtCtrl* target, Point ptWindow, Point ptLocal, int button, WPARAM wp = 0) {
    root->ClearPressed();
    HWND hwnd = root->hwnd;
    if (target->HasFlag(vwfFocusable)) {
        // virtual controls have no HWND. Keys go to whoever has Win32
        // focus, so a child Edit (Contents, filter) would keep them
        // after this click unless we take them back (issue #6033).
        if (hwnd && ::GetFocus() != hwnd) {
            ::SetFocus(hwnd);
        }
        root->SetFocus(target);
    }
    root->pressed = target;
    target->SetFlag(vwfPressed, true);
    target->Invalidate();
    if (target->HasFlag(vwfCapturesMouse)) {
        root->SetCapture(target);
    }
    VirtMouseEvent ev;
    FillMouseEvent(ev, target, ptWindow, ptLocal, false, wp);
    ev.button = button;
    return BubbleMouse(target, ev, &VirtCtrl::OnMouseDown);
}

bool VirtRoot::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    if (len(tops) == 0) {
        return false;
    }
    res = 0;
    Point ptWindow{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    Point ptLocal{0, 0};

    switch (msg) {
        case WM_MOUSEMOVE: {
            TrackMouseLeaveIfNeeded();
            VirtCtrl* target = captured;
            if (target) {
                ptLocal = ptWindow;
            } else {
                target = VirtAtPoint(this, ptWindow, &ptLocal);
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
            FillMouseEvent(ev, target, ptWindow, ptLocal, captured != nullptr, wp);
            if (captured) {
                return target->OnMouseMove(ev);
            }
            return BubbleMouse(target, ev, &VirtCtrl::OnMouseMove);
        }

        case WM_MOUSELEAVE:
            trackingMouseLeave = false;
            ClearHover();
            HideTooltip();
            return false;

        case WM_CAPTURECHANGED: {
            // someone else took the mouse (or the window lost it): whoever was
            // tracking it has to stop, otherwise it keeps reacting to plain
            // mouse moves. Our own ReleaseCapture() clears `captured` first,
            // so this only fires for captures lost from the outside
            if (captured) {
                VirtCtrl* w = captured;
                captured = nullptr;
                w->SetFlag(vwfPressed, false);
                w->OnCaptureLost();
                w->Invalidate();
            }
            return false; // the window may still want it
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            VirtCtrl* target = VirtAtPoint(this, ptWindow, &ptLocal);
            if (!target) {
                ClearPressed();
                SetFocus(nullptr);
                return false;
            }
            int button = (msg == WM_LBUTTONDOWN) ? 0 : ((msg == WM_RBUTTONDOWN) ? 1 : 2);
            return BeginVirtPress(this, target, ptWindow, ptLocal, button, wp);
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            bool wasCaptured = captured != nullptr;
            VirtCtrl* target = captured;
            if (target) {
                ptLocal = ptWindow;
            } else {
                target = VirtAtPoint(this, ptWindow, &ptLocal);
            }
            if (!target) {
                ClearPressed();
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, wasCaptured, wp);
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
                didHandle = BubbleMouse(target, ev, &VirtCtrl::OnMouseUp);
            }
            return didHandle;
        }

        case WM_LBUTTONDBLCLK: {
            VirtCtrl* target = VirtAtPoint(this, ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false, wp);
            if (BubbleMouse(target, ev, &VirtCtrl::OnDoubleClick)) {
                return true;
            }
            // CS_DBLCLKS turns a fast second press into DBLCLK instead of DOWN.
            // onClick-only buttons (Find Next/Prev, issue #6035) would otherwise
            // ignore it until the double-click timeout. Treat it as another press
            // so the following UP fires onClick.
            return BeginVirtPress(this, target, ptWindow, ptLocal, 0, wp);
        }

        case WM_MOUSEWHEEL: {
            // wheel messages carry screen coords
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            ptWindow = {pt.x, pt.y};
            VirtCtrl* target = VirtAtPoint(this, ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false, wp);
            ev.wheelDelta = GET_WHEEL_DELTA_WPARAM(wp);
            return BubbleMouse(target, ev, &VirtCtrl::OnMouseWheel);
        }

        case WM_CONTEXTMENU: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (pt.x == -1 && pt.y == -1) {
                return false;
            }
            ScreenToClient(hwnd, &pt);
            ptWindow = {pt.x, pt.y};
            VirtCtrl* target = VirtAtPoint(this, ptWindow, &ptLocal);
            if (!target) {
                return false;
            }
            VirtMouseEvent ev;
            FillMouseEvent(ev, target, ptWindow, ptLocal, false, wp);
            return BubbleMouse(target, ev, &VirtCtrl::OnContextMenu);
        }

        case WM_SETCURSOR: {
            Point pt = HwndGetCursorPos(hwnd);
            VirtCtrl* target = VirtAtPoint(this, pt, &ptLocal);
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
            VirtCtrl* w = focused;
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

//--- VirtScroll

static Kind kindVirtCtrlScroll = "virtCtrlScroll";

VirtScroll::VirtScroll() {
    onMouseWheel = MkMethod1<VirtScroll, VirtMouseEvent*, &VirtScroll::OnMouseWheel>(this);

    kind = kindVirtCtrlScroll;
    flags |= vwfClipChildren;
}

VirtScroll::~VirtScroll() = default;

Size VirtScroll::Layout(Constraints bc) {
    // a viewport takes whatever it is given; the content decides contentDy
    Size sz = bc.Constrain({bc.max.dx, bc.max.dy});
    for (VirtCtrl* c : children) {
        Constraints cc = bc;
        cc.min = {0, 0};
        cc.max.dy = Inf;
        c->Layout(cc);
    }
    return sz;
}

void VirtScroll::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    int y = content.y - scrollY;
    for (VirtCtrl* c : children) {
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

void VirtScroll::ScrollIntoView(VirtCtrl* w) {
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

void VirtScroll::OnMouseWheel(VirtMouseEvent* ev) {
    if (ev->wheelDelta == 0) {
        return;
    }
    int lines = -(ev->wheelDelta * 3) / WHEEL_DELTA;
    if (ScrollBy(lines * lineDy)) {
        ev->didHandle = true;
    }
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
    if (!onVisibleRangeChanged.IsValid()) {
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

//--- ScrollBox

static Kind kindScrollBox = "scrollBox";

ScrollBox::ScrollBox(ILayout* childIn) {
    kind = kindScrollBox;
    child = childIn;
    flags |= vwfClipChildren;
    onMouseWheel = MkMethod1<ScrollBox, VirtMouseEvent*, &ScrollBox::OnMouseWheel>(this);
}

ScrollBox::~ScrollBox() {
    delete child;
    child = nullptr;
}

Size ScrollBox::Layout(Constraints bc) {
    Constraints cc = bc;
    cc.min = {0, 0};
    cc.max.dy = Inf;
    if (child) {
        contentSize = child->Layout(cc);
    } else {
        contentSize = {};
    }
    Size sz = contentSize;
    if (bc.HasBoundedHeight()) {
        sz.dy = std::min(sz.dy, bc.max.dy);
    }
    return bc.Constrain(sz);
}

void ScrollBox::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    if (child) {
        child->SetBounds({r.x, r.y - scrollY, r.dx, contentSize.dy});
    }
    ScrollTo(scrollY);
    UpdateScrollbar();
}

Point ScrollBox::ScrollOffset() {
    return {0, scrollY};
}

void ScrollBox::Paint(VirtPaintCtx& ctx) {
    if (!child) {
        return;
    }
    Rect clip = ctx.clip.Intersect(ctx.bounds);
    if (clip.IsEmpty()) {
        return;
    }
    ctx.gfx->PushClip(clip);
    Vec<VirtCtrl*> inner;
    CollectVirtCtrls(child, inner);
    for (VirtCtrl* w : inner) {
        w->PaintTree(ctx.gfx, {0, 0}, clip);
    }
    ctx.gfx->PopClip();
}

int ScrollBox::MinIntrinsicHeight(int width) {
    return child ? child->MinIntrinsicHeight(width) : 0;
}

int ScrollBox::MinIntrinsicWidth(int height) {
    return child ? child->MinIntrinsicWidth(height) : 0;
}

Size ScrollBox::GetIdealSize() {
    return contentSize;
}

int ScrollBox::LayoutChildCount() {
    return child ? 1 : 0;
}

ILayout* ScrollBox::LayoutChildAt(int) {
    return child;
}

int ScrollBox::MaxScrollY() const {
    int visible = bounds.dy;
    int res = contentSize.dy - visible;
    return res > 0 ? res : 0;
}

bool ScrollBox::ScrollTo(int y) {
    int maxY = MaxScrollY();
    y = Clamp(y, 0, maxY);
    if (y == scrollY) {
        UpdateScrollbar();
        return false;
    }
    scrollY = y;
    if (child) {
        Rect r = lastBounds;
        child->SetBounds({r.x, r.y - scrollY, r.dx, contentSize.dy});
    }
    UpdateScrollbar();
    Invalidate();
    return true;
}

bool ScrollBox::ScrollBy(int dy) {
    return ScrollTo(scrollY + dy);
}

bool ScrollBox::ScrollPage(int dir) {
    return ScrollBy(dir * bounds.dy);
}

void ScrollBox::OnMouseWheel(VirtMouseEvent* ev) {
    if (ev->wheelDelta == 0) {
        return;
    }
    int lines = -(ev->wheelDelta * 3) / WHEEL_DELTA;
    if (ScrollBy(lines * lineDy)) {
        ev->didHandle = true;
    }
}

void ScrollBox::OnVScroll(WPARAM wp) {
    int visible = bounds.dy;
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

void ScrollBox::UpdateScrollbar() {
    HWND hwnd = GetHwnd();
    if (!syncScrollbar || !hwnd) {
        return;
    }
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = contentSize.dy > 0 ? contentSize.dy - 1 : 0;
    si.nPage = (UINT)std::max(bounds.dy, 0);
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

//--- VirtListBox

static Kind kindVirtCtrlListBox = "virtCtrlListBox";

VirtListBox::VirtListBox() {
    onMouseDown = MkMethod1<VirtListBox, VirtMouseEvent*, &VirtListBox::OnMouseDown>(this);
    onMouseUp = MkMethod1<VirtListBox, VirtMouseEvent*, &VirtListBox::OnMouseUp>(this);
    onMouseMove = MkMethod1<VirtListBox, VirtMouseEvent*, &VirtListBox::OnMouseMove>(this);
    onMouseWheel = MkMethod1<VirtListBox, VirtMouseEvent*, &VirtListBox::OnMouseWheel>(this);
    // onDoubleClick is the public item-activated Func0; wire the mouse path on VirtCtrl
    VirtCtrl::onDoubleClick = MkMethod1<VirtListBox, VirtMouseEvent*, &VirtListBox::OnDoubleClick>(this);
    onKeyDown = MkMethod1<VirtListBox, VirtKeyEvent*, &VirtListBox::OnKeyDown>(this);
    onCaptureLost = MkMethod0<VirtListBox, &VirtListBox::OnCaptureLost>(this);

    kind = kindVirtCtrlListBox;
    colorDefaults = gColsListBox;
    nColors = kColListCount;
    // like a win32 listbox: takes the focus and is in the window's tab ring, so
    // the arrow keys reach OnKeyDown()
    flags |= vwfClipChildren | vwfFocusable;
}

VirtListBox::~VirtListBox() {
    delete model;
}

// scaling has to work before the tree is attached to a window, which is when
// the owner asks for the row height to decide how tall to make the window
int VirtListBox::GetDpi() {
    HWND h = GetHwnd();
    return h ? RoundUp(DpiGetForHwnd(h), 4) : dpi;
}

int VirtListBox::ItemsCount() {
    return model ? model->ItemsCount() : 0;
}

int VirtListBox::GetItemHeight() {
    if (itemDy > 0) {
        return itemDy;
    }
    Size sz = PlatformFontMeasureText(font, StrL("Ag"));
    int dy = sz.dy + DpiScaleByDpi(GetDpi(), 4);
    if (dy < 1) {
        dy = 1;
    }
    return dy;
}

int VirtListBox::ViewportDy() {
    int dy = bounds.dy - padding.top - padding.bottom;
    return dy > 0 ? dy : 0;
}

// the viewport rounded down to whole rows: a row cut in half by the bottom of
// the list reads as a rendering glitch, so the strip below the last whole row
// is left empty and never scrolled into
int VirtListBox::UsableDy() {
    int dy = GetItemHeight();
    int usable = (ViewportDy() / dy) * dy;
    return usable > 0 ? usable : ViewportDy();
}

int VirtListBox::MaxScrollY() {
    int res = (ItemsCount() * GetItemHeight()) - UsableDy();
    return res > 0 ? res : 0;
}

int VirtListBox::ScrollbarDx() {
    if (MaxScrollY() <= 0) {
        return 0;
    }
    return DpiScaleByDpi(GetDpi(), 10);
}

Rect VirtListBox::ContentRectLocal() {
    Rect r{0, 0, bounds.dx, bounds.dy};
    r.SubTB(padding.top, padding.bottom);
    r.SubLR(padding.left, padding.right);
    return r;
}

// the content minus the strip the scrollbar sits in
Rect VirtListBox::ItemsRectLocal() {
    Rect r = ContentRectLocal();
    r.dx -= ScrollbarDx();
    return r;
}

Rect VirtListBox::ScrollbarRectLocal() {
    int dx = ScrollbarDx();
    if (dx == 0) {
        return {};
    }
    Rect r = ContentRectLocal();
    return {r.Right() - dx, r.y, dx, r.dy};
}

// null when there is nothing to scroll
Rect VirtListBox::ThumbRectLocal() {
    Rect sb = ScrollbarRectLocal();
    if (sb.IsEmpty()) {
        return {};
    }
    int contentDy = ItemsCount() * GetItemHeight();
    int visibleDy = UsableDy();
    int minDy = DpiScaleByDpi(GetDpi(), 20);
    int thumbDy = Scale(sb.dy, visibleDy, contentDy);
    thumbDy = Clamp(thumbDy, std::min(minDy, sb.dy), sb.dy);
    int maxY = MaxScrollY();
    int y = (maxY > 0) ? Scale(sb.dy - thumbDy, scrollY, maxY) : 0;
    return {sb.x, sb.y + y, sb.dx, thumbDy};
}

Size VirtListBox::GetIdealSize() {
    int nLines = idealSizeLines;
    if (nLines <= 0) {
        nLines = std::min(ItemsCount(), 16);
        nLines = std::max(nLines, 1);
    }
    int dx = (idealSizeDx > 0) ? idealSizeDx : DpiScaleByDpi(GetDpi(), 120);
    int dy = (GetItemHeight() * nLines) + padding.top + padding.bottom;
    return {dx, dy};
}

void VirtListBox::SetBounds(Rect r) {
    VirtCtrl::SetBounds(r);
    // a taller viewport can make the current scroll position invalid
    scrollY = Clamp(scrollY, 0, MaxScrollY());
    if (pendingVisibleIdx >= 0) {
        int idx = pendingVisibleIdx;
        pendingVisibleIdx = -1;
        EnsureVisible(idx);
    }
}

bool VirtListBox::ScrollTo(int y) {
    y = Clamp(y, 0, MaxScrollY());
    if (y == scrollY) {
        return false;
    }
    scrollY = y;
    Invalidate();
    return true;
}

bool VirtListBox::ScrollBy(int dy) {
    return ScrollTo(scrollY + dy);
}

void VirtListBox::EnsureVisible(int idx) {
    int n = ItemsCount();
    if (idx < 0 || idx >= n) {
        return;
    }
    int visibleDy = UsableDy();
    if (visibleDy <= 0) {
        // not laid out yet (a caller can select before the first layout);
        // scrolling now would be against a zero-height viewport
        pendingVisibleIdx = idx;
        return;
    }
    int dy = GetItemHeight();
    int top = idx * dy;
    if (top < scrollY) {
        ScrollTo(top);
        return;
    }
    if (top + dy > scrollY + visibleDy) {
        ScrollTo(top + dy - visibleDy);
    }
}

int VirtListBox::GetCurrentSelection() {
    return selIdx;
}

void VirtListBox::EnsureSelectedSize() {
    int n = ItemsCount();
    if (len(selected) == n) {
        return;
    }
    VecReset(selected);
    if (n <= 0) {
        return;
    }
    u8* p = VecAppendBlanks(selected, n);
    if (p) {
        memset(p, 0, (size_t)n);
    }
    if (selIdx >= 0 && selIdx < n) {
        selected[selIdx] = 1;
    }
}

bool VirtListBox::IsSelected(int idx) {
    if (idx < 0 || idx >= ItemsCount()) {
        return false;
    }
    if (!multiSelect) {
        return idx == selIdx;
    }
    EnsureSelectedSize();
    return selected[idx] != 0;
}

int VirtListBox::SelectedCount() {
    if (!multiSelect) {
        return selIdx >= 0 ? 1 : 0;
    }
    EnsureSelectedSize();
    int n = 0;
    for (u8 v : selected) {
        if (v) {
            n++;
        }
    }
    return n;
}

void VirtListBox::GetSelectedIndices(Vec<int>& out) {
    VecReset(out);
    if (!multiSelect) {
        if (selIdx >= 0) {
            VecAppend(out, selIdx);
        }
        return;
    }
    EnsureSelectedSize();
    int n = ItemsCount();
    for (int i = 0; i < n; i++) {
        if (selected[i]) {
            VecAppend(out, i);
        }
    }
}

void VirtListBox::ToggleSelected(int idx) {
    if (idx < 0 || idx >= ItemsCount()) {
        return;
    }
    EnsureSelectedSize();
    selected[idx] = selected[idx] ? 0 : 1;
}

// Exclusive of other items; caret at `to`, anchor at `from`.
void VirtListBox::SelectRange(int from, int to) {
    int n = ItemsCount();
    if (n == 0) {
        return;
    }
    from = Clamp(from, 0, n - 1);
    to = Clamp(to, 0, n - 1);
    if (!multiSelect) {
        SetCurrentSelection(to);
        return;
    }
    EnsureSelectedSize();
    int a = std::min(from, to);
    int b = std::max(from, to);
    for (int i = 0; i < n; i++) {
        selected[i] = (i >= a && i <= b) ? 1 : 0;
    }
    anchorIdx = from;
    selIdx = to;
    EnsureVisible(to);
    Invalidate();
}

void VirtListBox::SelectAll() {
    int n = ItemsCount();
    if (n == 0 || !multiSelect) {
        return;
    }
    EnsureSelectedSize();
    for (int i = 0; i < n; i++) {
        selected[i] = 1;
    }
    if (selIdx < 0) {
        selIdx = 0;
    }
    if (anchorIdx < 0) {
        anchorIdx = selIdx;
    }
    Invalidate();
    onSelectionChanged.Call();
}

bool VirtListBox::SetCurrentSelection(int idx) {
    if (idx < 0) {
        idx = -1;
    } else if (idx >= ItemsCount()) {
        return false;
    }
    selIdx = idx;
    anchorIdx = idx;
    if (multiSelect) {
        int n = ItemsCount();
        VecReset(selected);
        if (n > 0) {
            u8* p = VecAppendBlanks(selected, n);
            if (p) {
                memset(p, 0, (size_t)n);
            }
        }
        if (idx >= 0) {
            selected[idx] = 1;
        }
    }
    Invalidate();
    EnsureVisible(idx);
    return true;
}

bool VirtListBox::SelectAndNotify(int idx) {
    if (!multiSelect && idx == selIdx) {
        return false;
    }
    if (!SetCurrentSelection(idx)) {
        return false;
    }
    onSelectionChanged.Call();
    return true;
}

void VirtListBox::ApplyClick(int idx, bool ctrl, bool shift) {
    if (!multiSelect) {
        SelectAndNotify(idx);
        return;
    }
    if (shift) {
        if (anchorIdx < 0) {
            anchorIdx = (selIdx >= 0) ? selIdx : idx;
        }
        SelectRange(anchorIdx, idx);
    } else if (ctrl) {
        ToggleSelected(idx);
        selIdx = idx;
        EnsureVisible(idx);
        Invalidate();
    } else {
        SetCurrentSelection(idx);
    }
    onSelectionChanged.Call();
}

void VirtListBox::ApplyNav(int idx, bool ctrl, bool shift) {
    if (!multiSelect) {
        SelectAndNotify(idx);
        return;
    }
    if (shift) {
        if (anchorIdx < 0) {
            anchorIdx = (selIdx >= 0) ? selIdx : idx;
        }
        SelectRange(anchorIdx, idx);
    } else if (ctrl) {
        selIdx = idx;
        EnsureVisible(idx);
        Invalidate();
    } else {
        SetCurrentSelection(idx);
    }
    onSelectionChanged.Call();
}

void VirtListBox::SetModel(ListBoxModel* m) {
    if (model && (model != m)) {
        delete model;
    }
    model = m;
    selIdx = -1;
    anchorIdx = -1;
    VecReset(selected);
    // the items are new even when the model object is the same one refilled
    scrollY = 0;
    Invalidate();
}

// ptLocal is relative to our bounds; -1 when it isn't on an item
int VirtListBox::ItemFromPoint(Point ptLocal) {
    Rect r = ItemsRectLocal();
    if (!r.Contains(ptLocal)) {
        return -1;
    }
    int idx = (ptLocal.y - r.y + scrollY) / GetItemHeight();
    if (idx < 0 || idx >= ItemsCount()) {
        return -1;
    }
    return idx;
}

// what an owner draws into, and where an in-place editor goes
Rect VirtListBox::ItemRect(int idx) {
    if (idx < 0 || idx >= ItemsCount()) {
        return {};
    }
    Rect content = ContentRectInWindow();
    content.dy = UsableDy();
    int dy = GetItemHeight();
    Rect r = {content.x, content.y + (idx * dy) - scrollY, content.dx - ScrollbarDx(), dy};
    if (r.y < content.y || r.Bottom() > content.Bottom()) {
        return {}; // not (fully) visible
    }
    return r;
}

void VirtListBox::Paint(VirtPaintCtx& ctx) {
    Color colBg = GetColor(kColListBg);
    ctx.gfx->FillRect(ctx.bounds, colBg);
    int n = ItemsCount();
    Rect clip = ctx.clip.Intersect(ctx.bounds);
    bool isFocused = HasFlag(vwfFocused);
    if (clip.IsEmpty()) {
        return;
    }

    if (n > 0) {
        // ctx.content is our bounds minus padding, so the items area only differs
        // from it by the strip the scrollbar takes on the right
        Rect items = ctx.content;
        items.dx -= ScrollbarDx();
        // only whole rows: the strip below the last one stays background
        items.dy = UsableDy();
        int dy = GetItemHeight();
        int first = scrollY / dy;
        int last = (scrollY + items.dy - 1) / dy;
        last = std::min(last, n - 1);

        // the selection stands out more while the list has the keyboard focus,
        // like a win32 listbox's
        Color colSel = GetColor(isFocused ? kColListSelFocused : kColListSel);
        if (colSel == kColorUnset && !ColorSkipsPaint(colBg)) {
            colSel = AccentColor(colBg, isFocused ? 45 : 25);
        }

        // rows at the top and bottom of the viewport can be cut in half by it
        ctx.gfx->PushClip(clip);
        for (int i = first; i <= last; i++) {
            Rect r = {items.x, items.y + (i * dy) - scrollY, items.dx, dy};
            bool isSel = IsSelected(i);
            if (onDrawItem.IsValid()) {
                DrawItemEvent ev;
                ev.listBox = this;
                ev.gfx = ctx.gfx;
                ev.itemRect = r;
                ev.itemIndex = i;
                ev.selected = isSel;
                onDrawItem.Call(&ev);
            } else {
                if (isSel) {
                    ctx.gfx->FillRect(r, colSel);
                }
                Rect rt = r;
                rt.SubLR(DpiScaleByDpi(GetDpi(), 4), 0);
                ctx.gfx->DrawText(model->Item(i), rt, gfxTextEllipsis | gfxTextVCenter, font, GetColor(kColListText));
            }
        }
        ctx.gfx->PopClip();

        Rect thumb = ThumbRectLocal();
        if (!thumb.IsEmpty()) {
            Color colThumb = GetColor(kColListScrollbar);
            if (colThumb == kColorUnset && !ColorSkipsPaint(colBg)) {
                colThumb = AccentColor(colBg, 60);
            }
            Point orig = ctx.bounds.TL();
            thumb.Offset(orig.x, orig.y);
            // a slim thumb with a gap on both sides, like an overlay scrollbar
            thumb.SubLR(2, 2);
            ctx.gfx->FillRect(thumb, colThumb);
        }
    }

    // dashed outline while the list has the keys (annotations list, find
    // results, command palette, …)
    if (isFocused) {
        Rect ring = ctx.bounds;
        ring.SubTB(1, 1);
        ring.SubLR(1, 1);
        Color colRing = GetColor(kColListText);
        if (ColorSkipsPaint(colRing) && !ColorSkipsPaint(colBg)) {
            colRing = AccentColor(colBg, 90);
        }
        if (!ColorSkipsPaint(colRing) && !ring.IsEmpty()) {
            ctx.gfx->DrawDashedRect(ring, colRing);
        }
    }
}

void VirtListBox::OnMouseDown(VirtMouseEvent* ev) {
    Rect thumb = ThumbRectLocal();
    if (!thumb.IsEmpty() && thumb.Contains(ev->pt)) {
        draggingThumb = true;
        dragStartY = ev->ptWindow.y;
        dragStartScrollY = scrollY;
        if (root) {
            root->SetCapture(this);
        }
        ev->didHandle = true;
        return;
    }
    Rect sb = ScrollbarRectLocal();
    if (!sb.IsEmpty() && sb.Contains(ev->pt)) {
        // above / below the thumb: page towards the click
        int dir = (ev->pt.y < thumb.y) ? -1 : 1;
        ScrollBy(dir * UsableDy());
        ev->didHandle = true;
        return;
    }
    int idx = ItemFromPoint(ev->pt);
    if (idx < 0) {
        ev->didHandle = true;
        return;
    }
    ApplyClick(idx, ev->isCtrl, ev->isShift);
    ev->didHandle = true;
    return;
}

void VirtListBox::OnMouseMove(VirtMouseEvent* ev) {
    if (!draggingThumb) {
        return;
    }
    Rect sb = ScrollbarRectLocal();
    Rect thumb = ThumbRectLocal();
    int range = sb.dy - thumb.dy;
    if (range <= 0) {
        ev->didHandle = true;
        return;
    }
    int dy = ev->ptWindow.y - dragStartY;
    ScrollTo(dragStartScrollY + Scale(dy, MaxScrollY(), range));
    ev->didHandle = true;
    return;
}

void VirtListBox::OnMouseUp(VirtMouseEvent* ev) {
    draggingThumb = false;
    ev->didHandle = true;
    return;
}

void VirtListBox::OnCaptureLost() {
    draggingThumb = false;
}

void VirtListBox::OnMouseWheel(VirtMouseEvent* ev) {
    if (ev->wheelDelta == 0) {
        return;
    }
    int lines = -(ev->wheelDelta * 3) / WHEEL_DELTA;
    if (ScrollBy(lines * GetItemHeight())) {
        ev->didHandle = true;
    }
}

void VirtListBox::OnDoubleClick(VirtMouseEvent* ev) {
    int idx = ItemFromPoint(ev->pt);
    if (idx < 0) {
        return;
    }
    SelectAndNotify(idx);
    onDoubleClick.Call();
    ev->didHandle = true;
    return;
}

void VirtListBox::OnKeyDown(VirtKeyEvent* ev) {
    int n = ItemsCount();
    if (n == 0) {
        return;
    }
    if (multiSelect && ev->vkey == 'A' && ev->isCtrl && !ev->isAlt) {
        SelectAll();
        ev->didHandle = true;
        return;
    }
    if (multiSelect && ev->vkey == VK_SPACE && ev->isCtrl) {
        if (selIdx >= 0) {
            ToggleSelected(selIdx);
            Invalidate();
            onSelectionChanged.Call();
        }
        ev->didHandle = true;
        return;
    }
    int perPage = std::max(UsableDy() / GetItemHeight(), 1);
    int idx = selIdx;
    switch (ev->vkey) {
        case VK_UP:
            idx = (idx < 0) ? 0 : idx - 1;
            break;
        case VK_DOWN:
            idx = (idx < 0) ? 0 : idx + 1;
            break;
        case VK_PRIOR:
            idx = (idx < 0) ? 0 : idx - perPage;
            break;
        case VK_NEXT:
            idx = (idx < 0) ? 0 : idx + perPage;
            break;
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
            break;
        default:
            return;
    }
    idx = Clamp(idx, 0, n - 1);
    ApplyNav(idx, ev->isCtrl, ev->isShift);
    ev->didHandle = true;
    return;
}

//--- VirtSplitter

static Kind kindVirtCtrlSplitter = "virtCtrlSplitter";

static const WCHAR* kResizeOverlayClass = L"SplitterResizeOverlayWnd";
static WORD gDotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

static LRESULT CALLBACK ResizeOverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) {
        return TRUE;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        auto* brush = (HBRUSH)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (brush) {
            SetBrushOrgEx(hdc, 0, 0, nullptr);
            HdcFillRect(hdc, ToRect(ps.rcPaint), brush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterResizeOverlayClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wcex{};
    FillWndClassEx(wcex, kResizeOverlayClass, ResizeOverlayWndProc);
    RegisterClassExW(&wcex);
    registered = true;
}

VirtSplitter::VirtSplitter() {
    onMouseDown = MkMethod1<VirtSplitter, VirtMouseEvent*, &VirtSplitter::OnMouseDown>(this);
    onMouseUp = MkMethod1<VirtSplitter, VirtMouseEvent*, &VirtSplitter::OnMouseUp>(this);
    onMouseMove = MkMethod1<VirtSplitter, VirtMouseEvent*, &VirtSplitter::OnMouseMove>(this);
    onMouseEnter = MkMethod0<VirtSplitter, &VirtSplitter::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtSplitter, &VirtSplitter::OnMouseLeave>(this);
    onCaptureLost = MkMethod0<VirtSplitter, &VirtSplitter::OnCaptureLost>(this);
    onSetCursor = MkMethod1<VirtSplitter, VirtSetCursorEvent*, &VirtSplitter::OnSetCursor>(this);

    kind = kindVirtCtrlSplitter;
    colorDefaults = gColsSplitter;
    nColors = kColSplitterCount;
    // the mouse belongs to us for the whole drag, wherever it goes
    flags |= vwfCapturesMouse;
}

VirtSplitter::~VirtSplitter() {
    if (overlayHwnd) {
        DestroyWindow(overlayHwnd);
    }
    DeleteObject(brush);
    DeleteObject(bmp);
}

void VirtSplitter::HideOverlay() {
    if (overlayHwnd) {
        ShowWindow(overlayHwnd, SW_HIDE);
    }
}

// a thin dotted bar where the split would land, following the cursor
void VirtSplitter::UpdateOverlay() {
    HWND hwnd = GetHwnd();
    if (!hwnd) {
        return;
    }
    if (!bmp) {
        bmp = CreateBitmap(8, 8, 1, 1, gDotPatternBmp);
        brush = CreatePatternBrush(bmp);
    }
    if (!overlayHwnd) {
        RegisterResizeOverlayClass();
        HWND owner = GetAncestor(hwnd, GA_ROOT);
        DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        overlayHwnd = CreateWindowExW(exStyle, kResizeOverlayClass, nullptr, WS_POPUP, 0, 0, 0, 0, owner, nullptr,
                                      GetModuleHandleW(nullptr), nullptr);
        if (!overlayHwnd) {
            return;
        }
        SetWindowLongPtrW(overlayHwnd, GWLP_USERDATA, (LONG_PTR)brush);
    }

    Point pos = HwndGetCursorPos(hwnd);
    Point origin = HwndClientToScreen(hwnd, Point());
    Rect b = BoundsInWindow();
    Rect screen = {origin.x + b.x, origin.y + b.y, b.dx, b.dy};

    Rect r;
    if (type != SplitterType::Horiz) {
        r = {origin.x + pos.x - 2, screen.y, 4, screen.dy};
    } else {
        r = {screen.x, origin.y + pos.y - 2, screen.dx, 4};
    }
    SetWindowPos(overlayHwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    HwndInvalidate(overlayHwnd, true);
}

Size VirtSplitter::GetIdealSize() {
    Size sz = {bounds.dx, bounds.dy};
    if (thickness > 0) {
        // Vert is a vertical bar between two side-by-side panes, so it's the
        // width that is fixed
        if (type == SplitterType::Vert) {
            sz.dx = thickness;
        } else {
            sz.dy = thickness;
        }
    }
    return sz;
}

void VirtSplitter::Paint(VirtPaintCtx& ctx) {
    Color bg = GetColor(kColSplitterBg);
    if (ColorSkipsPaint(bg)) {
        return;
    }
    ctx.gfx->FillRect(ctx.bounds, AccentColor(bg, 30));
}

void VirtSplitter::OnMouseDown(VirtMouseEvent* ev) {
    isDragging = true;
    lastDragPos = {-1, -1};
    if (!isLive) {
        UpdateOverlay();
    }
    ev->didHandle = true;
    return;
}

void VirtSplitter::OnMouseMove(VirtMouseEvent* ev) {
    if (!isDragging) {
        return;
    }
    // SetWindowPos under a still cursor synthesizes WM_MOUSEMOVE; do not
    // relayout unless the cursor actually moved (1px flicker loop).
    HWND hwnd = GetHwnd();
    Point pos = hwnd ? HwndGetCursorPos(hwnd) : Point{-1, -1};
    if (pos == lastDragPos) {
        ev->didHandle = true;
        return;
    }
    lastDragPos = pos;
    MoveEvent mev;
    mev.w = this;
    mev.finishedDragging = false;
    onMove.Call(&mev);
    if (mev.resizeAllowed && !isLive) {
        UpdateOverlay();
    }
    ev->didHandle = true;
}

void VirtSplitter::OnMouseUp(VirtMouseEvent* ev) {
    if (!isDragging) {
        return;
    }
    isDragging = false;
    HideOverlay();
    MoveEvent mev;
    mev.w = this;
    mev.finishedDragging = true;
    onMove.Call(&mev);
    Invalidate();
    ev->didHandle = true;
}

void VirtSplitter::OnCaptureLost() {
    isDragging = false;
    HideOverlay();
}

void VirtSplitter::OnMouseEnter() {
    Invalidate();
}

void VirtSplitter::OnMouseLeave() {
    Invalidate();
}

void VirtSplitter::OnSetCursor(VirtSetCursorEvent* ev) {
    LPWSTR curId = (type == SplitterType::Vert) ? IDC_SIZEWE : IDC_SIZENS;
    if (isDragging) {
        MoveEvent mev;
        mev.w = this;
        mev.finishedDragging = false;
        mev.queryOnly = true;
        // ask the owner whether the current position is allowed, so the cursor
        // can say "no" without moving the panes (a live onMove would relayout
        // on every WM_SETCURSOR and shimmer both sides by a pixel).
        onMove.Call(&mev);
        if (!mev.resizeAllowed) {
            curId = IDC_NO;
        }
    }
    SetCursorCached(curId);
    ev->didHandle = true;
}

//--- VirtCustom

static Kind kindVirtCtrlCustom = "virtCtrlCustom";

VirtCustom::VirtCustom() {
    kind = kindVirtCtrlCustom;
}

VirtCustom::~VirtCustom() = default;

Size VirtCustom::GetIdealSize() {
    return idealSize;
}

void VirtCustom::Paint(VirtPaintCtx& ctx) {
    onPaint.Call(&ctx);
}

//--- VirtText

static Kind kindVirtCtrlText = "virtCtrlText";

// Win32 STATIC/BUTTON prefix: "&Foo" draws as "Foo" with F underlined, "&&" as
// a literal '&'. The first '&X' wins. A trailing '&' is dropped.
struct AccelPrefix {
    Str display;
    int ulOff = -1;
    int ulLen = 0;
};

static AccelPrefix ParseAccelPrefixTemp(Str s) {
    AccelPrefix res;
    if (len(s) == 0) {
        return res;
    }
    if (!str::Contains(s, StrL("&"))) {
        res.display = s;
        return res;
    }
    char* buf = AllocArrayTemp<char>(len(s) + 1);
    int out = 0;
    for (int i = 0; i < len(s); i++) {
        if (s.s[i] != '&') {
            buf[out++] = s.s[i];
            continue;
        }
        if (i + 1 >= len(s)) {
            break;
        }
        if (s.s[i + 1] == '&') {
            buf[out++] = '&';
            i++;
            continue;
        }
        if (res.ulOff < 0) {
            res.ulOff = out;
            int remain = len(s) - (i + 1);
            int n = utf8RuneLen((const u8*)(s.s + i + 1));
            if (n < 1) {
                n = 1;
            }
            res.ulLen = n > remain ? remain : n;
        }
    }
    buf[out] = 0;
    res.display = Str(buf, out);
    return res;
}

// first mnemonic char in a label: "De&fault" => 'f'; "&&" is a literal '&';
// 0 if none
char MnemonicCharInStr(Str s) {
    int n = len(s);
    for (int i = 0; i + 1 < n; i++) {
        if (s.s[i] != '&') {
            continue;
        }
        if (s.s[i + 1] == '&') {
            i++; // "&&" is an escaped '&'
            continue;
        }
        return s.s[i + 1];
    }
    return 0;
}

// cache the "&F" mnemonic of a prefix-enabled text in VirtCtrl::mnemonic.
// Called whenever the text or the prefix flag is set
static void UpdateMnemonic(VirtText* w) {
    w->mnemonic = w->prefix ? MnemonicCharInStr(w->s) : 0;
}

static Str TextToDraw(VirtText* w, AccelPrefix* prefixOut) {
    if (!w->prefix) {
        if (prefixOut) {
            *prefixOut = {};
        }
        return w->s;
    }
    AccelPrefix p = ParseAccelPrefixTemp(w->s);
    if (prefixOut) {
        *prefixOut = p;
    }
    return p.display;
}

VirtText::VirtText(Str str, PlatformFont* f) {
    kind = kindVirtCtrlText;
    colorDefaults = gColsText;
    nColors = kColTextCount;
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
    UpdateMnemonic(this);
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
    if (len(s) == 0) {
        // empty still occupies a line so a later SetText() paints into a real box
        // (the find-bar status starts empty and is filled when a search runs)
        sz = {0, PlatformFontLineHeight(font)};
        return sz;
    }
    sz = PlatformFontMeasureText(font, TextToDraw(this, nullptr));
    return sz;
}

void VirtText::Paint(VirtPaintCtx& ctx) {
    PaintText(ctx, GetColor(kColText));
}

// the drawing, in a color the caller picks: a VirtButton paints its label in
// the disabled color without having to swap a field
void VirtText::PaintText(VirtPaintCtx& ctx, Color textColor) {
    Rect r = ctx.content;
    if (r.IsEmpty()) {
        return;
    }
    u32 fmt = 0;
    if (pathEllipsis) {
        fmt |= gfxTextPathEllipsis | gfxTextVCenter;
    } else if (ellipsis) {
        fmt |= gfxTextEllipsis | gfxTextVCenter;
    }
    if (isRtl) {
        fmt |= gfxTextRtl;
    }
    switch (align) {
        case VirtTextAlign::Center:
            fmt |= gfxTextCenter | gfxTextVCenter;
            break;
        case VirtTextAlign::Right:
            fmt |= gfxTextRight;
            break;
        case VirtTextAlign::Left:
            break;
    }
    AccelPrefix pref;
    Str draw = TextToDraw(this, &pref);
    ctx.gfx->DrawText(draw, r, fmt, font, textColor);
    if (pref.ulOff >= 0 && pref.ulLen > 0 && len(draw) > 0) {
        Size full = ctx.gfx->MeasureText(draw, font);
        Size before = pref.ulOff > 0 ? ctx.gfx->MeasureText(Str(draw.s, pref.ulOff), font) : Size{};
        int chLen = pref.ulLen;
        if (pref.ulOff + chLen > len(draw)) {
            chLen = len(draw) - pref.ulOff;
        }
        Size ch = chLen > 0 ? ctx.gfx->MeasureText(Str(draw.s + pref.ulOff, chLen), font) : Size{};
        int textX = r.x;
        if (fmt & gfxTextCenter) {
            textX = r.x + ((r.dx - full.dx) / 2);
        } else if (fmt & gfxTextRight) {
            textX = r.x + r.dx - full.dx;
        }
        int ulX = textX + before.dx;
        if (isRtl) {
            Size fromAccel = ctx.gfx->MeasureText(Str(draw.s + pref.ulOff, len(draw) - pref.ulOff), font);
            ulX = textX + full.dx - fromAccel.dx;
        }
        int textY = r.y;
        if (fmt & gfxTextVCenter) {
            textY = r.y + ((r.dy - full.dy) / 2);
        }
        int ulY = textY + full.dy + underlineOffsetY - 1;
        ctx.gfx->DrawLine({ulX, ulY, ch.dx, 0}, textColor);
    }
    if (withUnderline) {
        GetIdealSize(true);
        Rect lineRect = {r.x, r.y + sz.dy + underlineOffsetY, sz.dx, 0};
        ctx.gfx->DrawLine(lineRect, textColor);
    }
}

VirtText* NewVirtText(const VirtTextArgs& args) {
    auto* w = new VirtText(args.s, args.font);
    if (args.textColor != kColorUnset) {
        w->SetColor(kColText, args.textColor);
    }
    w->align = args.align;
    w->withUnderline = args.withUnderline;
    w->isRtl = args.isRtl;
    w->ellipsis = args.ellipsis;
    w->pathEllipsis = args.pathEllipsis;
    w->prefix = args.prefix;
    UpdateMnemonic(w);
    w->underlineOffsetY = args.underlineOffsetY;
    w->padding = args.padding;
    return w;
}

//--- VirtLink

static Kind kindVirtCtrlLink = "virtCtrlLink";

VirtLink::VirtLink(Str str, PlatformFont* f) : VirtText(str, f) {
    onMouseEnter = MkMethod0<VirtLink, &VirtLink::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtLink, &VirtLink::OnMouseLeave>(this);
    cursor = CursorId::Hand;

    kind = kindVirtCtrlLink;
    // same slots as VirtText, but a link is drawn in the link color
    colorDefaults = gColsLink;
    flags &= ~vwfNoHitTest;
}

VirtLink::~VirtLink() {
    str::Free(target);
}

void VirtLink::SetTarget(Str s2) {
    str::Free(target);
    target = str::Dup(s2);
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

//--- VirtButton

static Kind kindVirtCtrlButton = "virtCtrlButton";

VirtButton::VirtButton(Str str, PlatformFont* f) : VirtText(str, f) {
    onMouseEnter = MkMethod0<VirtButton, &VirtButton::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtButton, &VirtButton::OnMouseLeave>(this);
    onKeyDown = MkMethod1<VirtButton, VirtKeyEvent*, &VirtButton::OnKeyDown>(this);
    cursor = CursorId::Hand;

    kind = kindVirtCtrlButton;
    colorDefaults = gColsBtn;
    nColors = kColBtnCount;
    flags &= ~vwfNoHitTest;
    flags |= vwfFocusable;
    align = VirtTextAlign::Center;
    prefix = true;
    UpdateMnemonic(this);
}

VirtButton::~VirtButton() = default;

bool VirtButton::IsDefault() const {
    return isDefault;
}

// a default button takes the stronger of the two button palettes. A caller that
// pointed us at a palette of its own (the toolbar's) keeps it
void VirtButton::SetIsDefault(bool v) {
    isDefault = v;
    if (colorDefaults == gColsBtn || colorDefaults == gColsBtnDefault) {
        colorDefaults = v ? gColsBtnDefault : gColsBtn;
    }
}

Size VirtButton::GetIdealSize() {
    Size s2 = VirtText::GetIdealSize();
    return {s2.dx + textPadding.left + textPadding.right, s2.dy + textPadding.top + textPadding.bottom};
}

// Disabled labels are muted, then lifted if they would vanish into `bg`.
Color VirtButton::TextColor(Color bg) const {
    Color textCol = GetColor(kColBtnText);
    if (HasFlag(vwfEnabled)) {
        return textCol;
    }
    Color disabled = GetColor(kColBtnTextDisabled);
    if (disabled != kColorUnset) {
        textCol = disabled;
    }
    return EnsureContrast(textCol, bg);
}

void VirtButton::Paint(VirtPaintCtx& ctx) {
    bool isEnabled = HasFlag(vwfEnabled);
    Color bg = GetColor((isEnabled && HasFlag(vwfHovered)) ? kColBtnBgHover : kColBtnBg);
    ctx.gfx->FillRect(ctx.bounds, bg);
    Color borderCol = GetColor(kColBtnBorder);
    if (!ColorSkipsPaint(borderCol)) {
        Rect b = ctx.bounds;
        ctx.gfx->FillRect({b.x, b.y, b.dx, 1}, borderCol);
        ctx.gfx->FillRect({b.x, b.Bottom() - 1, b.dx, 1}, borderCol);
        ctx.gfx->FillRect({b.x, b.y, 1, b.dy}, borderCol);
        ctx.gfx->FillRect({b.Right() - 1, b.y, 1, b.dy}, borderCol);
    }
    Rect r = ctx.content;
    r.SubTB(textPadding.top, textPadding.bottom);
    r.SubLR(textPadding.left, textPadding.right);
    VirtPaintCtx c2 = ctx;
    c2.content = r;
    Color textCol = TextColor(bg);
    PaintText(c2, textCol);

    if (HasFlag(vwfFocused)) {
        // focus ring, just inside the border
        Rect b = ctx.bounds;
        b.SubTB(2, 2);
        b.SubLR(2, 2);
        Color col = (textCol != kColorUnset) ? textCol : borderCol;
        if (col != kColorUnset && !b.IsEmpty()) {
            ctx.gfx->FillRect({b.x, b.y, b.dx, 1}, col);
            ctx.gfx->FillRect({b.x, b.Bottom() - 1, b.dx, 1}, col);
            ctx.gfx->FillRect({b.x, b.y, 1, b.dy}, col);
            ctx.gfx->FillRect({b.Right() - 1, b.y, 1, b.dy}, col);
        }
    }
}

// fire onClick as if the button was pressed (keyboard or WindowBase Enter)
bool VirtButton::Click() {
    if (!HasFlag(vwfEnabled) || !onClick.IsValid()) {
        return false;
    }
    VirtMouseEvent me;
    me.target = this;
    me.hit = this;
    me.pt = {bounds.dx / 2, bounds.dy / 2};
    me.ptWindow = {bounds.x + me.pt.x, bounds.y + me.pt.y};
    onClick.Call(&me);
    return true;
}

// Enter / Space press the button, like a win32 one
void VirtButton::OnKeyDown(VirtKeyEvent* ev) {
    bool isPress = (ev->vkey == VK_RETURN) || (ev->vkey == VK_SPACE);
    if (!isPress) {
        return;
    }
    if (Click()) {
        ev->didHandle = true;
    }
}

void VirtButton::OnMouseEnter() {
    Invalidate();
}

void VirtButton::OnMouseLeave() {
    Invalidate();
}

//--- VirtIconButton

static Kind kindVirtCtrlIconButton = "virtCtrlIconButton";

VirtIconButton::VirtIconButton() {
    onMouseEnter = MkMethod0<VirtIconButton, &VirtIconButton::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtIconButton, &VirtIconButton::OnMouseLeave>(this);
    onMouseMove = MkMethod1<VirtIconButton, VirtMouseEvent*, &VirtIconButton::OnMouseMove>(this);
    cursor = CursorId::Hand;

    kind = kindVirtCtrlIconButton;
    colorDefaults = gColsIconBtn;
    nColors = kColIconBtnCount;
}

int VirtIconButton::DropdownDx() const {
    return hasDropdown ? DpiScale(12) : 0;
}

Size VirtIconButton::GetIdealSize() {
    Size sz;
    if (pixmap) {
        sz = {pixmap->width, pixmap->height};
    }
    sz.dx += padding.left + padding.right;
    sz.dy += padding.top + padding.bottom;
    sz.dx += DropdownDx();
    return sz;
}

void VirtIconButton::Paint(VirtPaintCtx& ctx) {
    bool enabled = IsEnabled();
    int dropDx = DropdownDx();
    Rect action = ctx.bounds;
    Rect drop = ctx.bounds;
    if (dropDx > 0) {
        action.dx -= dropDx;
        drop.x = ctx.bounds.Right() - dropDx;
        drop.dx = dropDx;
    }
    Color bgSel = GetColor(kColIconBtnBgSelected);
    // a disabled button's last checked state is leftover (e.g. Fit Single Page
    // after switching to the home page); don't paint it as selected
    if (isSelected && enabled && bgSel != kColorUnset) {
        ctx.gfx->FillRect(action, bgSel);
    }
    Color bgHover = GetColor(kColIconBtnBgHover);
    if (enabled && HasFlag(vwfHovered) && bgHover != kColorUnset) {
        Rect hi = (dropDx > 0 && hoverOnDropdown) ? drop : action;
        ctx.gfx->FillRect(hi, bgHover);
    }
    Pixmap* px = (!enabled && pixmapDisabled) ? pixmapDisabled : pixmap;
    Rect r = ctx.content;
    if (px) {
        // pixmap size, not GetIdealSize(): that includes padding and a stretched
        // blit falls back to an opaque copy, which paints the transparent fringe black
        Size s2 = {px->width, px->height};
        int iconDx = r.dx - dropDx;
        int x = r.x + ((iconDx - s2.dx) / 2);
        int y = r.y + ((r.dy - s2.dy) / 2);
        ctx.gfx->DrawPixmap(px, {x, y, s2.dx, s2.dy});
    }
    if (dropDx > 0) {
        Color col = GetColor(enabled ? kColIconBtnChevron : kColIconBtnChevronDisabled);
        if (col == kColorUnset) {
            col = MkGray(enabled ? 0x40 : 0x90);
        }
        // Segoe UI U+25BE (▾): same small filled triangle as the Win32
        // TBSTYLE_EX_DRAWDDARROWS glyph
        float pt = 14.f * (float)DpiGet() / 96.f;
        PlatformFont* font = GetPlatformFont(StrL("Segoe UI"), pt, PlatformFontStyle::Regular);
        ctx.gfx->DrawText(StrL("\xE2\x96\xBE"), drop, gfxTextCenter | gfxTextVCenter, font, col);
    }
}

void VirtIconButton::OnMouseEnter() {
    Invalidate();
}

void VirtIconButton::OnMouseLeave() {
    hoverOnDropdown = false;
    Invalidate();
}

void VirtIconButton::OnMouseMove(VirtMouseEvent* ev) {
    if (!hasDropdown) {
        return;
    }
    bool onDrop = ev->pt.x >= bounds.dx - DropdownDx();
    if (onDrop == hoverOnDropdown) {
        return;
    }
    hoverOnDropdown = onDrop;
    Invalidate();
}

//--- VirtCloseButton

static Kind kindVirtCtrlCloseButton = "virtCtrlCloseButton";

VirtCloseButton::VirtCloseButton() {
    onMouseEnter = MkMethod0<VirtCloseButton, &VirtCloseButton::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtCloseButton, &VirtCloseButton::OnMouseLeave>(this);
    cursor = CursorId::Hand;

    kind = kindVirtCtrlCloseButton;
    colorDefaults = gColsCloseBtn;
    nColors = kColCloseCount;
}

Size VirtCloseButton::GetIdealSize() {
    return idealSize;
}

void VirtCloseButton::Paint(VirtPaintCtx& ctx) {
    bool isHover = HasFlag(vwfHovered);
    // the glyph goes in the content rect, so padding makes the hit area bigger
    // than the ✕ itself (the tab bar's close gutter)
    Rect r = ctx.content;
    Gfx* gfx = ctx.gfx;
    // the tree paints into an unmirrored buffer that is flipped as a whole for a
    // right-to-left window, so the glyph is placed as if it had been mirrored
    HWND hwnd = GetHwnd();
    int mirrorDx = HwndIsRtl(hwnd) ? HwndClientRect(hwnd).dx : 0;
    auto mirrorX = [mirrorDx](int x) { return mirrorDx ? mirrorDx - x : x; };

    // slightly translucent when it sits on content it doesn't own
    u8 a = (withCircle && !isHover) ? 215 : 255;
    Color circle = GetColor(isHover ? kColCloseCircleHover : kColCloseCircle);
    if (isHover || withCircle) {
        Rect er = r;
        if (mirrorDx) {
            er.x = mirrorX(r.x + r.dx - 1);
        }
        gfx->FillEllipse(er, circle, a);
    }

    Color xcol = GetColor(isHover ? kColCloseXHover : kColCloseX);
    int pad = r.dx / 3;
    gfx->DrawLineAA({mirrorX(r.x + pad), r.y + pad}, {mirrorX(r.x + r.dx - pad), r.y + r.dy - pad}, xcol, 2.0f, a);
    gfx->DrawLineAA({mirrorX(r.x + r.dx - pad), r.y + pad}, {mirrorX(r.x + pad), r.y + r.dy - pad}, xcol, 2.0f, a);
}

void VirtCloseButton::OnMouseEnter() {
    Invalidate();
}

void VirtCloseButton::OnMouseLeave() {
    Invalidate();
}

//--- LabelWithClose

// what the old LabelWithCloseWnd custom control used, kept so the panels look
// the same: a 16x16 ✕ with a bit of air around the text
constexpr int kLabelPad = 2;
constexpr int kCloseBtnDx = 16;
constexpr int kCloseBtnGapDx = 8;

// Scale the header ✕ and label padding for this window's DPI.
void ApplyLabelWithCloseDpi(VirtText* label, VirtCloseButton* closeBtn, int dpi) {
    if (!label || !closeBtn || dpi <= 0) {
        return;
    }
    int pad = DpiScaleByDpi(dpi, kLabelPad);
    int btnDx = DpiScaleByDpi(dpi, kCloseBtnDx);
    int gap = DpiScaleByDpi(dpi, kCloseBtnGapDx);
    label->padding = Insets{pad, pad, pad, pad};
    // the padding is part of the ideal size, so it enlarges the hit area
    // without shrinking the ✕ itself
    closeBtn->padding = Insets{0, pad, 0, gap};
    closeBtn->idealSize = {btnDx + pad + gap, btnDx};
}

LabelWithClose NewLabelWithClose(HWND hwnd, PlatformFont* font, const VirtMouseHandler& onClose) {
    int dpi = DpiGetForHwnd(hwnd);
    if (dpi <= 0) {
        dpi = DpiGet();
    }
    auto* label = NewVirtText({
        .font = font,
        .isRtl = HwndIsRtl(hwnd),
        .ellipsis = true,
    });

    auto* closeBtn = new VirtCloseButton();
    closeBtn->onClick = onClose;
    ApplyLabelWithCloseDpi(label, closeBtn, dpi);

    auto* box = new HBox();
    box->alignMain = MainAxisAlign::MainStart;
    box->alignCross = CrossAxisAlign::CrossCenter;
    // the label takes whatever is left, so the ✕ stays at the right edge; a
    // label too long for the panel is ellipsized rather than run under it
    box->AddChild(label, 1);
    box->AddChild(closeBtn);

    LabelWithClose res;
    res.box = box;
    res.label = label;
    res.closeBtn = closeBtn;
    return res;
}

//--- VirtImage

static Kind kindVirtCtrlImage = "virtCtrlImage";

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
    int x = dst.x + ((dst.dx - dx) / 2);
    int y = dst.y + ((dst.dy - dy) / 2);
    return {x, y, dx, dy};
}

VirtImage::VirtImage() {
    kind = kindVirtCtrlImage;
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
    ctx.gfx->DrawPixmap(pixmap, r);
}

//--- VirtFill

static Kind kindVirtCtrlFill = "virtCtrlFill";

VirtFill::VirtFill() {
    kind = kindVirtCtrlFill;
    colorDefaults = gColsFill;
    nColors = kColFillCount;
    flags |= vwfNoHitTest;
}

VirtFill::~VirtFill() = default;

Size VirtFill::GetIdealSize() {
    return idealSize;
}

void VirtFill::Paint(VirtPaintCtx& ctx) {
    ctx.gfx->FillRect(ctx.bounds, GetColor(kColFillBg));
}

//--- VirtLine

static Kind kindVirtCtrlLine = "virtCtrlLine";

VirtLine::VirtLine() {
    kind = kindVirtCtrlLine;
    colorDefaults = gColsLine;
    nColors = kColLineCount;
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
    ctx.gfx->FillRect(r, GetColor(kColLineFg));
}

//--- VirtSlider

static Kind kindVirtCtrlSlider = "virtCtrlSlider";

VirtSlider::VirtSlider() {
    kind = kindVirtCtrlSlider;
    colorDefaults = gColsSlider;
    nColors = kColSliderCount;
    flags |= vwfCapturesMouse;
    flags &= ~vwfFocusable;
    cursor = CursorId::Hand;
    onMouseDown = MkMethod1<VirtSlider, VirtMouseEvent*, &VirtSlider::OnMouseDown>(this);
    onMouseMove = MkMethod1<VirtSlider, VirtMouseEvent*, &VirtSlider::OnMouseMove>(this);
    onMouseUp = MkMethod1<VirtSlider, VirtMouseEvent*, &VirtSlider::OnMouseUp>(this);
    onMouseWheel = MkMethod1<VirtSlider, VirtMouseEvent*, &VirtSlider::OnMouseWheel>(this);
    onMouseEnter = MkMethod0<VirtSlider, &VirtSlider::OnMouseEnter>(this);
    onMouseLeave = MkMethod0<VirtSlider, &VirtSlider::OnMouseLeave>(this);
    onCaptureLost = MkMethod0<VirtSlider, &VirtSlider::OnCaptureLost>(this);
}

VirtSlider::~VirtSlider() = default;

int VirtSlider::ThumbRadius() const {
    HWND hwnd = GetHwnd();
    int dpi = hwnd ? DpiGetForHwnd(hwnd) : 96;
    return std::max(DpiScaleByDpi(dpi, 7), 5);
}

Rect VirtSlider::TrackRectLocal() const {
    int r = ThumbRadius();
    int thick = std::max(r / 3, 3);
    Rect c = {0, 0, bounds.dx, bounds.dy};
    int y = c.y + (c.dy - thick) / 2;
    int x = c.x + r;
    int dx = std::max(c.dx - (2 * r), 1);
    return {x, y, dx, thick};
}

int VirtSlider::ValueFromLocalX(int xLocal) {
    if (maxVal <= minVal) {
        return minVal;
    }
    Rect track = TrackRectLocal();
    int x = xLocal;
    HWND hwnd = GetHwnd();
    if (hwnd && HwndIsRtl(hwnd)) {
        x = track.x + track.dx - (xLocal - track.x);
    }
    float t = 0;
    if (track.dx > 0) {
        t = (float)(x - track.x) / (float)track.dx;
    }
    if (t < 0) {
        t = 0;
    }
    if (t > 1) {
        t = 1;
    }
    int n = maxVal - minVal;
    return minVal + (int)lroundf(t * (float)n);
}

bool VirtSlider::IsAdjusting() const {
    return adjusting;
}

void VirtSlider::SetValue(int v, bool notify) {
    if (maxVal < minVal) {
        maxVal = minVal;
    }
    if (v < minVal) {
        v = minVal;
    }
    if (v > maxVal) {
        v = maxVal;
    }
    if (v == value) {
        if (!adjusting) {
            committed = v;
        }
        return;
    }
    value = v;
    if (!adjusting) {
        committed = v;
    }
    Invalidate();
    if (notify && onValueChanged.IsValid()) {
        onValueChanged.Call();
    }
}

Size VirtSlider::GetIdealSize() {
    HWND hwnd = GetHwnd();
    int dpi = hwnd ? DpiGetForHwnd(hwnd) : 96;
    int r = ThumbRadius();
    int dx = idealDx > 0 ? idealDx : DpiScaleByDpi(dpi, 88);
    return {dx, (2 * r) + DpiScaleByDpi(dpi, 4)};
}

void VirtSlider::Paint(VirtPaintCtx& ctx) {
    Rect track = TrackRectLocal();
    track.x += ctx.bounds.x;
    track.y += ctx.bounds.y;
    int r = ThumbRadius();
    int n = maxVal - minVal;
    float t = (n <= 0) ? 0 : (float)(value - minVal) / (float)n;
    HWND hwnd = GetHwnd();
    bool rtl = hwnd && HwndIsRtl(hwnd);
    int cx;
    if (rtl) {
        cx = track.x + track.dx - (int)lroundf(t * (float)track.dx);
    } else {
        cx = track.x + (int)lroundf(t * (float)track.dx);
    }
    int cy = track.y + track.dy / 2;

    Color trackCol = GetColor(kColSliderTrack);
    Color fillCol = GetColor(kColSliderFill);
    int radius = std::max(track.dy / 2, 1);
    ctx.gfx->FillRoundedRect(track, radius, trackCol);
    Rect fill = track;
    if (rtl) {
        fill.dx = track.x + track.dx - cx;
        fill.x = cx;
    } else {
        fill.dx = cx - track.x;
    }
    if (fill.dx > 0) {
        ctx.gfx->FillRoundedRect(fill, radius, fillCol);
    }

    bool hot = HasFlag(vwfHovered) || HasFlag(vwfPressed);
    Color thumb = GetColor(hot ? kColSliderThumbHover : kColSliderThumb);
    int d = r * 2;
    ctx.gfx->FillEllipse({cx - r, cy - r, d, d}, thumb);
}

void VirtSlider::ApplyFromEvent(const VirtMouseEvent& ev, bool commit) {
    Rect b = BoundsInWindow();
    int v = ValueFromLocalX(ev.ptWindow.x - b.x);
    SetValue(v, true);
    if (commit && onValueCommitted.IsValid()) {
        onValueCommitted.Call();
    }
}

void VirtSlider::OnMouseDown(VirtMouseEvent* ev) {
    adjusting = true;
    ApplyFromEvent(*ev, false);
    ev->didHandle = true;
}

void VirtSlider::OnMouseMove(VirtMouseEvent* ev) {
    if (!adjusting) {
        return;
    }
    ApplyFromEvent(*ev, false);
    ev->didHandle = true;
}

void VirtSlider::OnMouseUp(VirtMouseEvent* ev) {
    ApplyFromEvent(*ev, true);
    committed = value;
    adjusting = false;
    ev->didHandle = true;
}

void VirtSlider::OnMouseWheel(VirtMouseEvent* ev) {
    int dir = ev->wheelDelta > 0 ? 1 : -1;
    SetValue(value + dir, true);
    if (onValueCommitted.IsValid()) {
        onValueCommitted.Call();
    }
    ev->didHandle = true;
}

void VirtSlider::OnMouseEnter() {
    Invalidate();
}

void VirtSlider::OnMouseLeave() {
    Invalidate();
}

void VirtSlider::OnCaptureLost() {
    adjusting = false;
    SetValue(committed, false);
}

//--- VirtSpacer

static Kind kindVirtCtrlSpacer = "virtCtrlSpacer";

VirtSpacer::VirtSpacer(int dx, int dy) {
    kind = kindVirtCtrlSpacer;
    idealSize = {dx, dy};
    flags |= vwfNoHitTest;
}

VirtSpacer::~VirtSpacer() = default;

Size VirtSpacer::GetIdealSize() {
    return idealSize;
}

//--- checked downcasts

// The kinds are the interned pointers defined above, so this is a pointer
// compare, not a string compare.
VirtText* AsVirtText(ILayout* l) {
    if (!l) {
        return nullptr;
    }
    Kind k = l->GetKind();
    // VirtLink and VirtButton are VirtText
    if (k == kindVirtCtrlText || k == kindVirtCtrlLink || k == kindVirtCtrlButton) {
        return (VirtText*)l;
    }
    return nullptr;
}

VirtButton* AsVirtButton(ILayout* l) {
    if (l && l->GetKind() == kindVirtCtrlButton) {
        return (VirtButton*)l;
    }
    return nullptr;
}

VirtIconButton* AsVirtIconButton(ILayout* l) {
    if (l && l->GetKind() == kindVirtCtrlIconButton) {
        return (VirtIconButton*)l;
    }
    return nullptr;
}

VirtCloseButton* AsVirtCloseButton(ILayout* l) {
    if (l && l->GetKind() == kindVirtCtrlCloseButton) {
        return (VirtCloseButton*)l;
    }
    return nullptr;
}

VirtSlider* AsVirtSlider(ILayout* l) {
    if (l && l->GetKind() == kindVirtCtrlSlider) {
        return (VirtSlider*)l;
    }
    return nullptr;
}

VirtLine* AsVirtLine(ILayout* l) {
    if (l && l->GetKind() == kindVirtCtrlLine) {
        return (VirtLine*)l;
    }
    return nullptr;
}

#if IS_DEBUG

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
#endif

//--- hosting virtual controls in a window

void RefreshVirtTops(HWND hwnd, ILayout* layout, Rect bounds, VirtRoot** rootInOut) {
    if (!layout) {
        return;
    }
    Vec<VirtCtrl*> tops;
    CollectVirtCtrls(layout, tops);
    VirtRoot* root = *rootInOut;
    if (len(tops) == 0) {
        // nothing virtual in this window: nothing to paint or dispatch
        delete root;
        *rootInOut = nullptr;
        return;
    }
    if (!root) {
        root = new VirtRoot(hwnd);
        *rootInOut = root;
    }
    root->bounds = bounds;
    root->SetTops(tops);
    // A top-level control's `bounds` is relative to the root's origin, but
    // SetBounds() computed it against whatever origin the root had *then* -
    // for a first layout that's no root at all, and for a caller that lays out
    // before refreshing it's the previous origin. One incremental resize hides
    // it (the next pass agrees again), a single-shot one like maximizing does
    // not: everything paints an origin off and disappears. Re-apply the layout
    // rects now that the origin is current; descendants are relative to their
    // parent, which this fixes for them.
    for (VirtCtrl* w : tops) {
        if (!w->lastBounds.IsEmpty()) {
            w->SetBounds(w->lastBounds);
        }
    }
    root->needsLayout = false;
}

void LayoutTreeToSize(HWND hwnd, ILayout* layout, Size size, VirtRoot** rootInOut) {
    if (!layout) {
        return;
    }
    DpiSetFromHwnd(hwnd);
    LayoutToSize(layout, size);
    RefreshVirtTops(hwnd, layout, Rect{0, 0, size.dx, size.dy}, rootInOut);
    // Virtual controls have no HWND. MoveWindow copies the host's bits, so a
    // size or child-visibility change leaves stale pixels until we invalidate.
    // CLIPCHILDREN keeps native children from being over-painted.
    if (hwnd && *rootInOut) {
        HwndInvalidate(hwnd, false);
    }
}

void PaintVirtTree(VirtRoot* root, HDC hdc, Rect clip, Color bg) {
    if (!root || len(root->tops) == 0) {
        return;
    }
    HWND hwnd = root->hwnd;
    Rect rc = HwndClientRect(hwnd);
    if (!root->gfxBuf) {
        root->gfxBuf = new GfxDoubleBuffer();
    }
    Gfx* gfx = GfxCreateWithDoubleBuffer(hwnd, hdc, root->gfxBuf);
    gfx->FillRect(rc, bg);
    root->Paint(gfx, clip);
    delete gfx;
}

bool VirtHostOnMessage(HWND hwnd, VirtRoot* root, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res, Color bg) {
    if (!root || len(root->tops) == 0) {
        return false;
    }
    if (msg == WM_ERASEBKGND || msg == WM_PRINTCLIENT) {
        // A themed / darkmode checkbox asks the parent to paint under its
        // label (DrawThemeParentBackground). Claiming handled with no fill
        // left that label on a different color than the combos (issue #6017).
        // The DC is clipped to the child for that request, so this does not
        // blank the rest of the panel. WM_PAINT still double-buffers the
        // virtual controls.
        HDC hdc = (HDC)wp;
        if (hdc) {
            HdcFillRect(hdc, HwndClientRect(hwnd), bg);
        }
        res = msg == WM_ERASEBKGND ? TRUE : 0;
        return true;
    }
    if (msg == WM_NCHITTEST) {
        // a WC_STATIC answers HTTRANSPARENT, which sends the mouse to the
        // parent window instead - the virtual controls would never see a hover
        // or a click
        res = HTCLIENT;
        return true;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintVirtTree(root, hdc, ToRect(ps.rcPaint), bg);
        EndPaint(hwnd, &ps);
        res = 0;
        return true;
    }
    return VirtTreeOnMessage(hwnd, root, msg, wp, lp, res);
}

// the tree lays out left-to-right; when the HWND has an RTL layout GDI mirrors
// the drawing for us but mouse coordinates arrive mirrored, so undo that before
// hit-testing
static LPARAM UnmirrorRtlLparam(HWND hwnd, LPARAM lp) {
    if (!HwndIsRtl(hwnd)) {
        return lp;
    }
    Point pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    UnmirrorRtl(hwnd, pt);
    return MAKELPARAM(pt.x, pt.y);
}

static bool IsVirtMouseMsg(UINT msg) {
    switch (msg) {
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_CONTEXTMENU:
            return true;
    }
    return false;
}

bool VirtTreeOnMessage(HWND hwnd, VirtRoot* root, UINT msg, WPARAM wp, LPARAM lp, LRESULT& res) {
    if (!root || len(root->tops) == 0) {
        return false;
    }
    if (msg == WM_SETCURSOR) {
        Point pt = HwndGetCursorPos(hwnd);
        UnmirrorRtl(hwnd, pt);
        root->UpdateTooltip(pt);
        Point ptLocal{0, 0};
        VirtCtrl* w = VirtAtPoint(root, pt, &ptLocal);
        // the cursor belongs to whichever ancestor claims it first
        while (w) {
            Rect b = w->BoundsInWindow();
            if (w->OnSetCursor({pt.x - b.x, pt.y - b.y})) {
                res = TRUE;
                return true;
            }
            w = w->parent;
        }
        return false;
    }
    if (IsVirtMouseMsg(msg)) {
        // WM_CONTEXTMENU and the wheel come in screen coordinates, which
        // VirtRoot::OnMessage converts itself
        bool inClientCoords = (msg != WM_CONTEXTMENU) && (msg != WM_MOUSEWHEEL);
        LPARAM lp2 = inClientCoords ? UnmirrorRtlLparam(hwnd, lp) : lp;
        return root->OnMessage(msg, wp, lp2, res);
    }
    if (msg == WM_KEYDOWN || msg == WM_CHAR) {
        return root->OnMessage(msg, wp, lp, res);
    }
    return false;
}

//--- VirtRichText

// A small markup with links, keyboard shortcuts and bold runs, parsed into a
// VirtRichText: a virtual control that wraps, paints and hit-tests itself.
// Shared by SumatraPDF's home page and notifications, and by other apps in the
// family. What it needs from the app is a CommandsContext (see VirtCtrl.h) and a
// way to open a url.

void (*gTipOpenUrl)(Str url) = nullptr;
CommandsContext* gCommandsContext = nullptr;

static Kind kindVirtRichText = "virtRichText";

VirtRichText::VirtRichText() {
    onMouseDown = MkMethod1<VirtRichText, VirtMouseEvent*, &VirtRichText::OnMouseDown>(this);
    onMouseUp = MkMethod1<VirtRichText, VirtMouseEvent*, &VirtRichText::OnMouseUp>(this);
    onSetCursor = MkMethod1<VirtRichText, VirtSetCursorEvent*, &VirtRichText::OnSetCursor>(this);
    onGetTooltip = MkMethod1<VirtRichText, VirtTooltipEvent*, &VirtRichText::OnGetTooltip>(this);

    kind = kindVirtRichText;
    colorDefaults = gColsRichText;
    nColors = kColRichCount;
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
    if (len(s) == 0) {
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
    int kbdPadX = DpiScale(7);
    // a pixel above/below so descenders and the key-cap border stay inside the
    // box; both kinds of word get it so they share a baseline when VCentered
    int padY = DpiScale(1);
    for (TipWord* w = words.next; w; w = w->next) {
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        Size sz = PlatformFontMeasureText(use, w->text);
        w->dx = sz.dx + (w->isKbd ? (2 * kbdPadX) : 0);
        w->dy = sz.dy + (2 * padY);
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

    // shorter words (plain vs key-cap) sit on the same visual center / baseline
    for (TipWord* w = words.next; w;) {
        int lineY = w->y;
        int lineH = 0;
        TipWord* lineEnd = w;
        for (TipWord* t = w; t && t->y == lineY; t = t->next) {
            lineH = std::max(lineH, t->dy);
            lineEnd = t;
        }
        TipWord* nextLine = lineEnd->next;
        for (TipWord* t = w; t != nextLine; t = t->next) {
            t->y = lineY + (lineH - t->dy) / 2;
        }
        w = nextLine;
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
    VirtCtrl::SetBounds(r);
    Rect content = r;
    content.SubTB(padding.top, padding.bottom);
    content.SubLR(padding.left, padding.right);
    LayoutText(content.dx);
}

// draws the words (link words in kColRichLink, underlined; others in kColRichText;
// isKbd words as key-caps like the keyboard help sheet)
void VirtRichText::Paint(VirtPaintCtx& ctx) {
    Gfx* gfx = ctx.gfx;
    PlatformFont* boldFont = nullptr;
    Color textCol = GetColor(kColRichText);
    Color linkCol = GetColor(kColRichLink);
    if (linkCol == kColorUnset) {
        linkCol = textCol;
    }
    Color bgCol = GetColor(kColRichBg);
    // key-cap colors: AccentColor on the background the text sits on
    if (bgCol == kColorUnset) {
        bgCol = IsLightColor(textCol) ? MkGray(0x22) : MkGray(0xf2);
    }
    Color capBg = AccentColor(bgCol, 16);
    Color capBorder = AccentColor(bgCol, 40);
    int rad = DpiScale(5);
    // words are laid out at (0, 0); shift them to where we are
    int offX = ctx.content.x;
    int offY = ctx.content.y;

    // same VCenter box for key-caps and body so they share a baseline
    u32 wordFmt = gfxTextVCenter | gfxTextNoClip | gfxTextSingleLine;
    for (TipWord* w = words.next; w; w = w->next) {
        Rect rc{offX + w->x, offY + w->y, w->dx, w->dy};
        if (w->isKbd) {
            gfx->FillRoundedRect(rc, rad, capBg, capBorder);
            gfx->DrawText(w->text, rc, wordFmt | gfxTextCenter, font, textCol);
            continue;
        }
        if (w->isBold && !boldFont) {
            boldFont = GetBoldPlatformFont(font);
        }
        PlatformFont* use = (w->isBold && boldFont) ? boldFont : font;
        gfx->DrawText(w->text, rc, wordFmt | gfxTextLeft, use, w->isLink ? linkCol : textCol);
    }
    // underline each link
    for (TipLink* link = links.next; link; link = link->next) {
        TipWord* first = link->firstWord;
        TipWord* last = link->lastWord;
        if (!first || !last) {
            continue;
        }
        int underlineY = offY + first->y + first->dy - 3;
        int x1 = offX + first->x;
        int x2 = offX + last->x + last->dx;
        gfx->DrawLine(Rect(x1, underlineY, x2 - x1, 0), linkCol);
    }
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
void VirtRichText::OnMouseDown(VirtMouseEvent* ev) {
    if (LinkAt(ev->pt)) {
        ev->didHandle = true;
    }
}

void VirtRichText::OnMouseUp(VirtMouseEvent* ev) {
    TipLink* link = LinkAt(ev->pt);
    if (!link) {
        if (onClick.IsValid()) {
            onClick.Call(ev);
            ev->didHandle = true;
        }
        return;
    }
    HWND hwnd = hwndForCmds ? hwndForCmds : GetHwnd();
    ExecuteTipLink(hwnd, link->cmd);
    ev->didHandle = true;
}

void VirtRichText::OnSetCursor(VirtSetCursorEvent* ev) {
    if (!LinkAt(ev->ptLocal) && !onClick.IsValid()) {
        return;
    }
    SetCursorCached(IDC_HAND);
    ev->didHandle = true;
}

void VirtRichText::OnGetTooltip(VirtTooltipEvent* ev) {
    TipLink* link = LinkAt(ev->ptLocal);
    if (!link) {
        return;
    }
    ev->tip = str::DupTemp(link->cmd);
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

// the equivalent of the [text](cmd) markup for a `text` that comes from outside
// the app: it becomes a link to `cmd` with nothing in it interpreted
void VirtRichText::AddPlainLink(Str text, Str cmd) {
    TipWord* prevWord = lastWord;
    TipLink* link = AppendTipLink(*this, ResolveLinkCmdTemp(cmd));
    AppendTipWordsFromText(*this, text, true, link);
    link->firstWord = FirstWordAfter(*this, prevWord);
    link->lastWord = lastWord;
    if (!link->firstWord) {
        // `text` was empty so it produced no words
        RemoveLastTipLink(*this);
    }
    layoutDx = -1;
}
