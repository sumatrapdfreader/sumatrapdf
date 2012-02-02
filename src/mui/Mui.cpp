/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "mui.h"
#include "BaseUtil.h"
#include "Vec.h"
#include "GeomUtil.h"
#include <Windowsx.h>
#include "MuiCss.h"

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

Event handling tries to be loosly coupled. The traditional way of
providing e.g. a virtual OnClick() on a button class forces creating
lots of subclasses and forcing logic into a button class. We provide
a way to subscribe any class implementing IClickHandler interface
to register for click evens from any window that generates thems.

TODO: generic way to handle tooltips

TODO: generic, flexible layout where I can just add windows. See
http://www.codeproject.com/Articles/194173/QuickDialogs-a-library-for-creating-dialogs-quickl
for how it could look like, syntax-wise. Layout itself could implement
something similar to html box model (http://marlongrech.wordpress.com/2012/01/23/thinking-in-boxes/) or maybe like Rebol/View (http://www.rebol.com/docs/view-system.html)

TODO: add size to content option to HwndWrapper (a bool flag, if set instead
of using window's size as available area, use infinite and size the
window to the result of the layout process). Alternatively (or in
addition) could have a way to only do "size to content" on first layout
and then do regular layout

TODO: a way to easily do text selection in generic way in EventMgr
by giving windows a way to declare they have selectable text

TODO: some claim GDI+ text drawing is slower than GDI, so we could try
to use GDI instead

TODO: since we already paint to a cached bitmap, we could do rendering
on a background thread and then just blit the bitmap to a window in
WM_PAINT, assuming rendering on a non-ui thread is ok with gdi+.
Technicall in WM_PAINT we could just start a thread to do the painting
and when it's finished we would bilt the bitmap on ui thread. If there
were WM_PAINT messages sent in-between, we would note that and start
painting again when the in-progress painting has finished.

TODO: optimize size of Control. One idea is that instead of embedding rarely used
properties (like e.g. Control::hwndParent), we could maintain separate mapping
from Control * to such properties e.g. in an array. Another idea is to move
rarely used fields into a separate struct linked from Control. If none of rarely
used fields was set, we wouldn't have to allocate that struct.

TODO: optimize repainting by cliping to dirty regions (?)
*/

#include "Mui.h"

//using namespace Gdiplus;
//#include "GdiPlusUtil.h"

namespace mui {

static GraphicsForMeasureText * uiGraphicsForMeasureText = NULL;
static Graphics *               uiGfxForMeasure = NULL;

void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

void Initialize()
{
    InitializeBase();
    css::Initialize();
    uiGraphicsForMeasureText = AllocGraphicsForMeasureText();
    uiGfxForMeasure = uiGraphicsForMeasureText->Get();
}

void Destroy()
{
    delete uiGraphicsForMeasureText;
    css::Destroy();
    DestroyBase();
}

Graphics *UIThreadGraphicsForMeasureText()
{
    return uiGfxForMeasure;
}

GraphicsForMeasureText::GraphicsForMeasureText() : gfx(NULL), bmp(NULL)
{
}

GraphicsForMeasureText::~GraphicsForMeasureText()
{
    ::delete gfx;
    ::delete bmp;
}

bool GraphicsForMeasureText::Create()
{
    // using a small bitmap under assumption that Graphics used only
    // for measuring text doesn't need the actual bitmap
    bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, data);
    if (!bmp)
        return false;
    gfx = ::new Graphics((Image*)bmp);
    if (!gfx)
        return false;
    InitGraphicsMode(gfx);
    return true;
}

Graphics *GraphicsForMeasureText::Get()
{
    return gfx;
}

GraphicsForMeasureText *AllocGraphicsForMeasureText()
{
    GraphicsForMeasureText *res = new GraphicsForMeasureText();
    if (!res->Create()) {
        delete res;
        return NULL;
    }
    return res;
}

#define RECTFromRect(r) { r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom() }

HwndWrapper *GetRootHwndWnd(const Control *w)
{
    while (w->parent) {
        w = w->parent;
    }
    if (!w->hwndParent)
        return NULL;
    return (HwndWrapper*)w;
}

// traverse tree upwards to find HWND that is ultimately backing
// this window
HWND GetHwndParent(const Control *w)
{
    HwndWrapper *wHwnd = GetRootHwndWnd(w);
    if (wHwnd)
        return wHwnd->hwndParent;
    return NULL;
}

void CollectWindowsBreathFirst(Control *w, int offX, int offY, WndFilter *wndFilter, Vec<WndAndOffset> *windows)
{
    if (wndFilter->skipInvisibleSubtrees && !w->IsVisible())
        return;

    offX += w->pos.X;
    offY += w->pos.Y;
    if (wndFilter->Matches(w, offX, offY)) {
        WndAndOffset wnd = { w, offX, offY };
        windows->Append(wnd);
    }

    size_t children = w->GetChildCount();
    for (size_t i = 0; i < children; i++) {
        CollectWindowsBreathFirst(w->GetChild(i), offX, offY, wndFilter, windows);
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
size_t CollectWindowsAt(Control *wndRoot, int x, int y, uint16 wantedInputMask, Vec<WndAndOffset> *windows)
{
    WndInputWantedFilter filter(x, y, wantedInputMask);
    windows->Reset();
    CollectWindowsBreathFirst(wndRoot, 0, 0, &filter, windows);
    return windows->Count();
}

static void DrawLine(Graphics *gfx, const Point& p1, const Point& p2, float width, Brush *br)
{
    if (0 == width)
        return;
    Pen p(br, width);
    gfx->DrawLine(&p, p1, p2);
}

void DrawBorder(Graphics *gfx, const Rect r, const BorderProps& bp)
{
    Point   p1, p2;
    float   width;

    // top
    p1.X = r.X; p1.Y = r.Y;
    p2.X = r.X + r.Width; p2.Y = p1.Y;
    width = bp.topWidth->width.width;
    WrappedBrush wb1 = BrushFromProp(bp.topColor, r);
    DrawLine(gfx, p1, p2, width, wb1.brush);

    // right
    p1 = p2;
    p2.X = p1.X; p2.Y = p1.Y + r.Height;
    width = bp.rightWidth->width.width;
    WrappedBrush wb2 = BrushFromProp(bp.rightColor, r);
    DrawLine(gfx, p1, p2, width, wb2.brush);

    // bottom
    p1 = p2;
    p2.X = r.X; p2.Y = p1.Y;
    width = bp.bottomWidth->width.width;
    WrappedBrush wb3 = BrushFromProp(bp.bottomColor, r);
    DrawLine(gfx, p1, p2, width, wb3.brush);

    // left
    p1 = p2;
    p2.X = p1.X; p2.Y = r.Y;
    width = bp.leftWidth->width.width;
    WrappedBrush wb4 = BrushFromProp(bp.leftColor, r);
    DrawLine(gfx, p1, p2, width, wb4.brush);
}

static void InvalidateAtOff(HWND hwnd, const Rect *r, int offX, int offY)
{
    RECT rc = RECTFromRect((*r));
    rc.left += offX; rc.top += offY;
    InvalidateRect(hwnd, &rc, FALSE);
}

void RequestRepaint(Control *w, const Rect *r1, const Rect *r2)
{
    // calculate the offset of window w within its root window
    int offX = 0, offY = 0;
    while (w->parent) {
        w = w->parent;
        offX += w->pos.X;
        offY += w->pos.Y;
    }
    HWND hwnd = w->hwndParent;
    CrashIf(!hwnd);

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

    InvalidateAtOff(hwnd, &w->pos, offX, offY);
}

void RequestLayout(Control *w)
{
    if (!w->IsVisible())
        return;

    HwndWrapper *wnd = GetRootHwndWnd(w);
    if (wnd)
        wnd->RequestLayout();
}

}
