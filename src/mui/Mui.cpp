/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/Log.h"
#include "Mui.h"

/*
MUI is a simple UI library for win32.
MUI stands for nothing, it's just ui and gui are overused.

MUI is intended to allow building UIs that have modern
capabilities not supported by the standard win32 HWND
architecture:
- overlapping, alpha-blended windows
- animations
- a saner layout

It's inspired by WPF, WDL (http://www.cockos.com/wdl/),
DirectUI (https://github.com/kjk/directui).

MUI is minimal - it only supports stuff needed for Sumatra.
I got burned trying to build the whole toolkit at once with DirectUI.
Less code there is, the easier it is to change or extend.

The basic architectures is that of a tree of "virtual" (not backed
by HWND) windows. Each window can have children (making it a container).
Children windows are positioned relative to its parent window and can
be positioned outside of parent's bounds.

There must be a parent window backed by HWND which handles windows
messages and paints child windows on WM_PAINT.

Event handling is loosly coupled.
*/

namespace mui {

// if true, shows the bounding box of each control with red outline
static bool gDebugPaint = false;

void Initialize() {
    InitializeBase();
    css::Initialize();
}

void Destroy() {
    FreeControlCreators();
    FreeLayoutCreators();
    css::Destroy();
    DestroyBase();
}

// the caller needs to manually invalidate all windows
// for this to take place
void SetDebugPaint(bool debug) {
    gDebugPaint = debug;
}

bool IsDebugPaint() {
    return gDebugPaint;
}

#define RECTFromRect(r) \
    { r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom() }

HwndWrapper* GetRootHwndWnd(const Control* c) {
    while (c->parent) {
        c = c->parent;
    }
    if (!c->hwndParent)
        return nullptr;
    return (HwndWrapper*)c;
}

// traverse tree upwards to find HWND that is ultimately backing
// this window
HWND GetHwndParent(const Control* c) {
    HwndWrapper* wHwnd = GetRootHwndWnd(c);
    if (wHwnd)
        return wHwnd->hwndParent;
    return nullptr;
}

void CollectWindowsBreathFirst(Control* c, int offX, int offY, WndFilter* wndFilter, Vec<CtrlAndOffset>* ctrls) {
    if (wndFilter->skipInvisibleSubtrees && !c->IsVisible())
        return;

    offX += c->pos.X;
    offY += c->pos.Y;
    if (wndFilter->Matches(c, offX, offY)) {
        CtrlAndOffset coff = {c, offX, offY};
        ctrls->Append(coff);
    }

    size_t children = c->GetChildCount();
    for (size_t i = 0; i < children; i++) {
        CollectWindowsBreathFirst(c->GetChild(i), offX, offY, wndFilter, ctrls);
    }
}

// Find all windows containing a given point (x, y) and interested in at least
// one of the input evens in wantedInputMask. We have to traverse all windows
// because children are not guaranteed to be bounded by their parent.
// It's very likely to return more than one window because our window hierarchy
// is a tree. Because we traverse the tree breadth-first, parent windows will be
// in windows array before child windows. In most cases caller can use the last
// window in returned array (but can use a custom logic as well).
// Returns number of matched windows as a convenience.
size_t CollectWindowsAt(Control* wndRoot, int x, int y, uint16_t wantedInputMask, Vec<CtrlAndOffset>* controls) {
    WndInputWantedFilter filter(x, y, wantedInputMask);
    controls->Reset();
    CollectWindowsBreathFirst(wndRoot, 0, 0, &filter, controls);
    return controls->size();
}

static void DrawLine(Graphics* gfx, const Point& p1, const Point& p2, float width, Brush* br) {
    if (0 == width)
        return;
    Pen p(br, width);
    gfx->DrawLine(&p, p1, p2);
}

void DrawBorder(Graphics* gfx, const Rect r, CachedStyle* s) {
    Point p1, p2;
    float width;

    // top
    p1.X = r.X;
    p1.Y = r.Y;
    p2.X = r.X + r.Width;
    p2.Y = p1.Y;
    width = s->borderWidth.top;
    Brush* br = BrushFromColorData(s->borderColors.top, r);
    DrawLine(gfx, p1, p2, width, br);

    // right
    p1 = p2;
    p2.X = p1.X;
    p2.Y = p1.Y + r.Height;
    width = s->borderWidth.right;
    br = BrushFromColorData(s->borderColors.right, r);
    DrawLine(gfx, p1, p2, width, br);

    // bottom
    p1 = p2;
    p2.X = r.X;
    p2.Y = p1.Y;
    width = s->borderWidth.bottom;
    br = BrushFromColorData(s->borderColors.bottom, r);
    DrawLine(gfx, p1, p2, width, br);

    // left
    p1 = p2;
    p2.X = p1.X;
    p2.Y = r.Y;
    width = s->borderWidth.left;
    br = BrushFromColorData(s->borderColors.left, r);
    DrawLine(gfx, p1, p2, width, br);
}

static void InvalidateAtOff(HWND hwnd, const Rect* r, int offX, int offY) {
    RECT rc = RECTFromRect((*r));
    rc.left += offX;
    rc.right += offX;
    rc.top += offY;
    rc.bottom += offY;
    InvalidateRect(hwnd, &rc, FALSE);
}

// r1 and r2 are relative to w. If both are nullptr, we invalidate the whole w
void RequestRepaint(Control* c, const Rect* r1, const Rect* r2) {
    // we might be called when the control hasn't yet been
    // placed in the window hierarchy
    if (!c->parent && !c->hwndParent)
        return;

    Rect wRect(0, 0, c->pos.Width, c->pos.Height);

    int offX = 0, offY = 0;
    c->MapMyToRootPos(offX, offY);
    while (c->parent) {
        c = c->parent;
    }
    HWND hwnd = c->hwndParent;
    CrashIf(!hwnd);
    HwndWrapper* wnd = GetRootHwndWnd(c);
    if (wnd)
        wnd->MarkForRepaint();

    // if we have r1 or r2, invalidate those, else invalidate w
    bool didInvalidate = false;
    if (r1) {
        InvalidateAtOff(hwnd, r1, offX, offY);
        didInvalidate = true;
    }

    if (r2) {
        InvalidateAtOff(hwnd, r2, offX, offY);
        didInvalidate = true;
    }

    if (didInvalidate)
        return;

    InvalidateAtOff(hwnd, &wRect, offX, offY);
}

void RequestLayout(Control* c) {
    HwndWrapper* wnd = GetRootHwndWnd(c);
    if (wnd)
        wnd->RequestLayout();
}
} // namespace mui
