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

TODO: probably should take border widths into account when measuring
buttons.

TODO: a way to easily do text selection in generic way in EventMgr
by giving windows a way to declare they have selectable text

TODO: generic way to handle tooltips

TODO: generic way to handle cursor changes

TODO: optimize repainting by cliping to dirty regions

TODO: add a notion of z-order so that we can paint/respond to
events in a more flexible order than the one dictated
by parent-child relantionship (?)

TODO: when registering for events and such we use VirtWnd * to identify
a window which leaves small (but non-zero) chance that the window
has been destoryed or a pointer was re-used for another window.
We could fix that by automatically assigning unique id for each
VirtWnd and identify windows by that

TODO: some claim GDI+ text drawing is slower than GDI, so we could try
to use GDI instead

TODO: add size to content option to VirtWndHwnd (a bool flag, if set instead
of using window's size as available area, use infinite and size the
window to the result of the layout process). Alternatively (or in
addition) could have a way to only do "size to content" on first layout
and then do regular layout

TODO: ability to specify minimum window size and enforce it during layout and
resizing (WM_GETMINMAXINFO)

TODO: optimize size of VirtWnd. One idea is that instead of embedding rarely used
properties (like e.g. VirtWnd::hwndParent), we could maintain separate mapping
from VirtWnd * to such properties e.g. in an array. Another idea is to move
rarely used fields into a separate struct linked from VirtWnd. If none of rarely
used fields was set, we wouldn't have to allocate that struct.
*/

#include "mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

namespace mui {

// we use uint16_t for those
STATIC_ASSERT(VirtWnd::WndWantedInputBitLast < 16, max16bitsForWantedIntputBits);
STATIC_ASSERT(VirtWnd::WndStateBitLast < 16, max16bitsForWndStateuBits);

// Graphics object that can be used at any time to measure text
static Graphics *   gGraphicsForFontMeasure = NULL;
static Bitmap *     gBitmapForFontMeasureGraphics = NULL;
static BYTE *       gBitmapDataForFontMeasureGraphics = NULL;

static void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

void Initialize()
{
    css::Initialize();
    CrashIf(gBitmapForFontMeasureGraphics || gGraphicsForFontMeasure);
    // using a small bitmap under assumption that Graphics used only
    // for measuring text doesn't need the actual bitmap
    const int bmpDx = 32;
    const int bmpDy = 4;
    const int stride = bmpDx * 4;
    gBitmapDataForFontMeasureGraphics = (BYTE*)malloc(bmpDx * bmpDy * 4);
    gBitmapForFontMeasureGraphics = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, gBitmapDataForFontMeasureGraphics);
    CrashIf(!gBitmapForFontMeasureGraphics);
    gGraphicsForFontMeasure = ::new Graphics((Image*)gBitmapForFontMeasureGraphics);
    CrashIf(!gGraphicsForFontMeasure);
    InitGraphicsMode(gGraphicsForFontMeasure);
}

void Destroy()
{
    ::delete gGraphicsForFontMeasure;
    ::delete gBitmapForFontMeasureGraphics;
    free(gBitmapDataForFontMeasureGraphics);
    css::Destroy();
}

Graphics *GetGraphicsForMeasureText()
{
    return gGraphicsForFontMeasure;
}

// When doing layout we need to measure strings even
// at times when we don't have a convenient access
// to Graphics object hence this function
Rect MeasureTextWithFont(Font *f, const TCHAR *s)
{
    RectF r = MeasureText(gGraphicsForFontMeasure, f, s);
    Rect res((int)r.X, (int)r.Y, (int)r.Width, (int)r.Height);
    return res;
}

#define RECTFromRect(r) { r.GetLeft(), r.GetTop(), r.GetRight(), r.GetBottom() }

static VirtWndHwnd *GetRootHwndWnd(VirtWnd *w)
{
    while (w->parent) {
        w = w->parent;
    }
    if (!w->hwndParent)
        return NULL;
    return (VirtWndHwnd*)w;
}

void RequestRepaint(VirtWnd *w)
{
    if (!w->IsVisible())
        return;

    if (w->pos.IsEmptyArea())
        return;

    Rect r(w->pos);
    while (w->parent) {
        w = w->parent;
        r.X += w->pos.X;
        r.Y += w->pos.Y;
    }
    CrashIf(!w->hwndParent);
    RECT rc = RECTFromRect(r);
    InvalidateRect(w->hwndParent, &rc, TRUE);
}

void RequestLayout(VirtWnd *w)
{
    if (!w->IsVisible())
        return;

    VirtWndHwnd *wnd = GetRootHwndWnd(w);
    if (wnd)
        wnd->RequestLayout();
}

VirtWnd::VirtWnd(VirtWnd *parent)
{
    wantedInputBits = 0;
    stateBits = 0;
    hwndParent = NULL;
    layout = NULL;
    pos = Rect();
    SetParent(parent);
}

VirtWnd::~VirtWnd()
{
    delete layout;
    DeleteVecMembers(children);
}

// traverse tree upwards to find HWND that is ultimately backing
// this window
HWND VirtWnd::GetHwndParent() const
{
    const VirtWnd *curr = this;
    while (curr) {
        if (curr->hwndParent)
            return curr->hwndParent;
        curr = curr->parent;
    }
    return NULL;
}

void VirtWnd::AddChild(VirtWnd *wnd, int pos)
{
    CrashAlwaysIf(NULL == wnd);
    if ((pos < 0) || (pos >= (int)children.Count()))
        children.Append(wnd);
    else
        children.InsertAt(pos, wnd);
    wnd->SetParent(this);
}

void VirtWnd::Measure(Size availableSize)
{
    if (layout) {
        layout->Measure(availableSize, this);
    } else {
        desiredSize = Size();
    }
}

void VirtWnd::Arrange(Rect finalRect)
{
    if (layout) {
        layout->Arrange(finalRect, this);
    } else {
        pos = finalRect;
    }
}

// Requests the window to draw itself on a Graphics canvas.
// offX and offY is a position of this window within
// Graphics canvas (pos is relative to that offset)
void VirtWnd::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;
}

VirtWndButton::VirtWndButton(const TCHAR *s)
{
    text = NULL;
    cssRegular = NULL;
    cssMouseOver = NULL;
    wantedInputBits = (uint16_t)-1; // wants everything
    SetText(s);
}

void VirtWndButton::GetPropSetsForState(PropSet **set1, PropSet **set2) const
{
    if (bit::IsSet(stateBits, MouseOverBit)) {
        *set1 = cssMouseOver;
        *set2 = gPropSetButtonMouseOver;
    } else {
        *set1 = cssRegular;
        *set2 = gPropSetButtonRegular;
    }
}

Prop *VirtWndButton::GetPropForState(PropType type) const
{
    PropSet *s1, *s2;
    GetPropSetsForState(&s1, &s2);
    return FindProp(s1, s2, type);
}

void VirtWndButton::GetPropsForState(PropToFind *propsToFind, size_t propsToFindCount) const
{
    PropSet *s1, *s2;
    GetPropSetsForState(&s1, &s2);
    FindProps(s1, s2, propsToFind, propsToFindCount);
}

Font *VirtWndButton::GetFontForState() const
{
    PropSet *s1, *s2;
    GetPropSetsForState(&s1, &s2);
    return CachedFontFromProps(s1, s2);
}

void VirtWndButton::RecalculateSize()
{
    Prop *prop = GetPropForState(PropPadding);
    PaddingData padding = prop->padding;

    if (!text) {
        desiredSize.Width = padding.left + padding.right;
        desiredSize.Height = padding.top  + padding.bottom;
        return;
    }

    Rect bbox = MeasureTextWithFont(GetFontForState(), text);
    bbox.GetSize(&desiredSize);
    desiredSize.Width  += (padding.left + padding.right);
    desiredSize.Height += (padding.top  + padding.bottom);
}

void VirtWndButton::SetText(const TCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize();
    RequestLayout(this);
    RequestRepaint(this);
}

void VirtWndButton::Measure(Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
}

struct BorderProps {
    Prop *  topWidth;
    Prop *  rightWidth;
    Prop *  bottomWidth;
    Prop *  leftWidth;
};

static void DrawLine(Graphics *gfx, const Point& p1, const Point& p2, float width)
{
    if (0 == width)
        return;
    Color c(0, 0, 0);
    Pen p(c, width);
    gfx->DrawLine(&p, p1, p2);
}

static void DrawBorder(Graphics *gfx, const Rect r, const BorderProps& bp)
{
    Point p1, p2;
    float width;

    // top
    p1.X = r.X; p1.Y = r.Y;
    p2.X = r.X + r.Width; p2.Y = p1.Y;
    width = bp.topWidth->width.width;
    DrawLine(gfx, p1, p2, width);

    // right
    p1 = p2;
    p2.X = p1.X; p2.Y = p1.Y + r.Height;
    width = bp.rightWidth->width.width;
    DrawLine(gfx, p1, p2, width);

    // bottom
    p1 = p2;
    p2.X = r.X; p2.Y = p1.Y;
    width = bp.bottomWidth->width.width;
    DrawLine(gfx, p1, p2, width);

    // left
    p1 = p2;
    p2.X = p1.X; p2.Y = r.Y;
    width = bp.leftWidth->width.width;
    DrawLine(gfx, p1, p2, width);
}

void VirtWndButton::Paint(Graphics *gfx, int offX, int offY)
{
    struct PropToFind props[] = {
        { PropColor, NULL },
        { PropBgColor, NULL },
        { PropPadding, NULL },
        { PropBorderTopWidth, NULL },
        { PropBorderRightWidth, NULL },
        { PropBorderBottomWidth, NULL },
        { PropBorderLeftWidth, NULL },
    };

    if (!IsVisible())
        return;

    GetPropsForState(props, dimof(props));
    Prop *propCol   = props[0].prop;
    Prop *propBgCol = props[1].prop;
    Prop *propPadding = props[2].prop;

    // TODO: if this is a gradient brush, should set the rectangle
    Brush *brBgColor = propBgCol->color.brush;
    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    gfx->FillRectangle(brBgColor, bbox);

    BorderProps bp = {
        props[3].prop,
        props[4].prop,
        props[5].prop,
        props[6].prop,
    };

    PaddingData padding = propPadding->padding;
    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, bp);
    if (!text)
        return;

    int x = offX + padding.left;
    int y = offY + padding.bottom;
    Brush *brColor = propCol->color.brush;
    gfx->DrawString(text, str::Len(text), GetFontForState(), PointF((REAL)x, (REAL)y), NULL, brColor);
}

static bool BitmapSizeEquals(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return false;
    return ((dx == bmp->GetWidth()) && (dy == bmp->GetHeight()));
}

// we paint the background in VirtWndPainter() because I don't
// want to add an artificial VirtWnd window just to cover
// the whole HWND and paint the background.
// It can be over-ridden for easy customization.
// TODO: I wish there was less involved way of over-ridding
// single functions. Chrome has an implementation of callbacks
// for C++ which we might investigate.
void VirtWndPainter::PaintBackground(Graphics *g, Rect r)
{
    LinearGradientBrush bgBrush(RectF(0, 0, (REAL)r.Width, (REAL)r.Height), Color(0xd0,0xd0,0xd0), Color(0xff,0xff,0xff), LinearGradientModeVertical);
    r.Inflate(1, 1);
    g->FillRectangle(&bgBrush, r);
}

void VirtWndPainter::PaintRecursively(Graphics *g, VirtWnd *wnd, int offX, int offY)
{
    if (!wnd->IsVisible())
        return;
    offX += wnd->pos.GetLeft();
    offY += wnd->pos.GetTop();
    wnd->Paint(g, offX, offY);

    for (size_t i = 0; i < wnd->GetChildCount(); i++) {
        VirtWnd *w = wnd->GetChild(i);
        PaintRecursively(g, w, offX, offY);
    }
}

// Should be called from WM_PAINT. Recursively paints a given window and
// all its children. VirtWnd must be the top-level window associated
// with HWND.
// Note: maybe should be split into BeginPaint()/Paint()/EndPaint()
// calls so that the caller can do more drawing after Paint()
void VirtWndPainter::OnPaint(HWND hwnd)
{
    CrashAlwaysIf(hwnd != wnd->hwndParent);

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    // TODO: be intelligent about only repainting changed
    // parts for perf. Note: if cacheBmp changes, we need
    // to repaint everything
    Graphics gDC(dc);
    ClientRect rc2(hwnd);
    if (!BitmapSizeEquals(cacheBmp, rc2.dx, rc2.dy)) {
        // note: could only re-allocate when the size increases
        ::delete cacheBmp;
        cacheBmp = ::new Bitmap(rc2.dx, rc2.dy, &gDC);
    }

    Graphics g((Image*)cacheBmp);
    InitGraphicsMode(&g);

    Rect r(rc2.x, rc2.y, rc2.dx, rc2.dy);
    PaintBackground(&g, r);

    PaintRecursively(&g, wnd, 0, 0);

    gDC.DrawImage(cacheBmp, 0, 0);
    EndPaint(hwnd, &ps);
}

// TODO: build an iterator for (VirtWnd *, Rect) to make such logic reusable
// in more places and eliminate recursion (?)
static void FindWindowsAtRecur(Vec<VirtWnd*> *windows, VirtWnd *w,  int offX, int offY, int x, int y, uint32_t wantedInputMask)
{
    if (!w->IsVisible())
        return;

    offX += w->pos.X;
    offY += w->pos.Y;
    if ((w->wantedInputBits & wantedInputMask) != 0) {
        Rect r = Rect(offX, offY, w->pos.Width, w->pos.Height);

        if (r.Contains(x, y))
            windows->Append(w);
    }

    size_t children = w->GetChildCount();
    for (size_t i = 0; i < children; i++) {
        FindWindowsAtRecur(windows, w->GetChild(i), offX, offY, x, y, wantedInputMask);
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
static size_t FindWindowsAt(Vec<VirtWnd*> *windows, VirtWnd *wndRoot, int x, int y, uint32_t wantedInputMask=(uint32_t)-1)
{
    windows->Reset();
    FindWindowsAtRecur(windows, wndRoot, 0, 0, x, y, wantedInputMask);
    return windows->Count();
}

void EventMgr::RegisterForClickEvent(VirtWnd *wndSource, IClickHandler *clickHandler)
{
    ClickHandler ch = { wndSource, clickHandler };
    clickHandlers.Append(ch);
}

IClickHandler *EventMgr::GetClickHandlerFor(VirtWnd *wndSource)
{
    for (size_t i = 0; i < clickHandlers.Count(); i++) {
        ClickHandler ch = clickHandlers.At(i);
        if (ch.wndSource == wndSource)
            return ch.clickHandler;
    }
    return NULL;
}

LRESULT EventMgr::OnMouseMove(WPARAM keys, int x, int y, bool& handledOut)
{
    Vec<VirtWnd*> windows;
    uint32_t wantedInputMask = bit::FromBit<uint32_t>(VirtWnd::WantsMouseOverBit);
    size_t count = FindWindowsAt(&windows, wndRoot, x, y, wantedInputMask);
    if (0 == count) {
        if (currOver) {
            currOver->NotifyMouseLeave();
            currOver = NULL;
        }
        return 0;
    }

    VirtWnd *w = windows.Last();
    if (w != currOver) {
        if (currOver)
            currOver->NotifyMouseLeave();
        currOver = w;
        currOver->NotifyMouseEnter();
    }
    return 0;
}

// TODO: quite possibly the real logic for generating "click" events is
// more complicated
LRESULT EventMgr::OnLButtonUp(WPARAM keys, int x, int y, bool& handledOut)
{
    Vec<VirtWnd*> windows;
    uint32_t wantedInputMask = bit::FromBit<uint32_t>(VirtWnd::WantsMouseClickBit);
    size_t count = FindWindowsAt(&windows, wndRoot, x, y, wantedInputMask);
    if (0 == count)
        return 0;
    VirtWnd *w = windows.Last();
    IClickHandler *clickHandler = GetClickHandlerFor(w);
    if (clickHandler)
        clickHandler->Clicked(w);
    return 0;
}

// TODO: not sure if handledOut serves any purpose (what exactly should it mean?)
LRESULT EventMgr::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut)
{
    wndRoot->LayoutIfRequested();

    handledOut = false;
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    if (WM_MOUSEMOVE == msg) {
        return OnMouseMove(wParam, x, y, handledOut);
    }

    if (WM_LBUTTONUP == msg) {
        return OnLButtonUp(wParam, x, y, handledOut);
    }
    return 0;
}

void RegisterForClickEvent(VirtWnd *wndSource, IClickHandler *clickEvent)
{
    CrashIf(!wndSource->WantsMouseClick());
    VirtWndHwnd *wHwnd = GetRootHwndWnd(wndSource);
    CrashIf(!wHwnd);
    wHwnd->evtMgr->RegisterForClickEvent(wndSource, clickEvent);
}

}

