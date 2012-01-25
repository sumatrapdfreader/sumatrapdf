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

TODO: a way to easily do text selection in generic way in EventMgr
by giving windows a way to declare they have selectable text

TODO: generic way to handle tooltips

TODO: generic way to handle cursor changes

TODO: optimize repainting by cliping to dirty regions

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

class WndFilter
{
public:
    bool skipInvisibleSubtrees;

    WndFilter() : skipInvisibleSubtrees(true) {}

    virtual ~WndFilter() {}

    virtual bool Matches(VirtWnd *w, int offX, int offY) {
        return true;
    }
};

class WndInputWantedFilter : public WndFilter
{
    int x, y;
    uint16_t wantedInputMask;

public:
    WndInputWantedFilter(int x, int y, uint16_t wantedInputMask) :
        x(x), y(y), wantedInputMask(wantedInputMask)
    {
    }
    virtual ~WndInputWantedFilter() {}
    virtual bool Matches(VirtWnd *w, int offX, int offY) {
        if ((w->wantedInputBits & wantedInputMask) != 0) {
            Rect r = Rect(offX, offY, w->pos.Width, w->pos.Height);
            return r.Contains(x, y);
        }
        return false;
    }
};

struct WndAndOffset {
    VirtWnd *wnd;
    int offX, offY;
};

static void CollectWindowsBreathFirst(VirtWnd *w, int offX, int offY, WndFilter *wndFilter, Vec<WndAndOffset> *windows)
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
static size_t CollectWindowsAt(VirtWnd *wndRoot, int x, int y, uint16_t wantedInputMask, Vec<WndAndOffset> *windows)
{
    WndInputWantedFilter filter(x, y, wantedInputMask);
    windows->Reset();
    CollectWindowsBreathFirst(wndRoot, 0, 0, &filter, windows);
    return windows->Count();
}

Brush *CreateBrush(Prop *p, const Rect& r)
{
    CrashIf(!IsColorProp(p->type));
    if (ColorSolid == p->color.type)
        return ::new SolidBrush(p->color.solid.color);

    if (ColorGradientLinear == p->color.type) {
        Color c1 = p->color.gradientLinear.startColor;
        Color c2 = p->color.gradientLinear.endColor;
        LinearGradientMode mode = p->color.gradientLinear.mode;
        return ::new LinearGradientBrush(r, c1, c2, mode);
    }
    CrashIf(true);
    return NULL;
}

static Brush *CreateBrush(Prop *p, const RectF& r)
{
    CrashIf(!IsColorProp(p->type));
    if (ColorSolid == p->color.type)
        return ::new SolidBrush(p->color.solid.color);

    if (ColorGradientLinear == p->color.type) {
        Color c1 = p->color.gradientLinear.startColor;
        Color c2 = p->color.gradientLinear.endColor;
        LinearGradientMode mode = p->color.gradientLinear.mode;
        return ::new LinearGradientBrush(r, c1, c2, mode);
    }
    CrashIf(true);
    return NULL;
}

struct BorderProps {
    Prop *  topWidth, *topColor;
    Prop *  rightWidth, *rightColor;
    Prop *  bottomWidth, *bottomColor;
    Prop *  leftWidth, *leftColor;
};

static void DrawLine(Graphics *gfx, const Point& p1, const Point& p2, float width, Brush *br)
{
    if (0 == width)
        return;
    Pen p(br, width);
    gfx->DrawLine(&p, p1, p2);
}

static void DrawBorder(Graphics *gfx, const Rect r, const BorderProps& bp)
{
    Point   p1, p2;
    float   width;
    Brush * br;

    // top
    p1.X = r.X; p1.Y = r.Y;
    p2.X = r.X + r.Width; p2.Y = p1.Y;
    width = bp.topWidth->width.width;
    br = CreateBrush(bp.topColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // right
    p1 = p2;
    p2.X = p1.X; p2.Y = p1.Y + r.Height;
    width = bp.rightWidth->width.width;
    br = CreateBrush(bp.rightColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // bottom
    p1 = p2;
    p2.X = r.X; p2.Y = p1.Y;
    width = bp.bottomWidth->width.width;
    br = CreateBrush(bp.bottomColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // left
    p1 = p2;
    p2.X = p1.X; p2.Y = r.Y;
    width = bp.leftWidth->width.width;
    br = CreateBrush(bp.leftColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;
}

static void InvalidateAtOff(HWND hwnd, const Rect *r, int offX, int offY)
{
    RECT rc = RECTFromRect((*r));
    rc.left += offX; rc.top += offY;
    InvalidateRect(hwnd, &rc, TRUE);
}

void RequestRepaint(VirtWnd *w, const Rect *r1, const Rect *r2)
{
    int offX = w->pos.X, offY = w->pos.Y;
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
    zOrder = 0;
    hwndParent = NULL;
    layout = NULL;
    styleDefault = NULL;
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

void VirtWnd::Measure(const Size availableSize)
{
    if (layout) {
        layout->Measure(availableSize, this);
    } else {
        desiredSize = Size();
    }
}

void VirtWnd::MeasureChildren(Size availableSize) const
{
    for (size_t i = 0; i < GetChildCount(); i++) {
        GetChild(i)->Measure(availableSize);
    }
}

void VirtWnd::Arrange(const Rect finalRect)
{
    if (layout) {
        layout->Arrange(finalRect, this);
    } else {
        SetPosition(finalRect);
    }
}

void VirtWnd::Show()
{
    if (IsVisible())
        return; // perf: avoid unnecessary repaints
    bit::Clear(stateBits, IsHiddenBit);
    RequestRepaint(this);
}

void VirtWnd::Hide()
{
    if (!IsVisible())
        return;
    RequestRepaint(this); // request repaint before hiding, to trigger repaint
    bit::Set(stateBits, IsHiddenBit);
}

void VirtWnd::SetPosition(const Rect& p)
{
    if (p.Equals(pos))
        return;  // perf optimization
    // when changing position we need to invalidate both
    // before and after position
    // TODO: not sure why I need this, but without it there
    // are drawing artifacts
    Rect p1(p); p1.Inflate(1,1);
    Rect p2(pos); p2.Inflate(1,1);
    RequestRepaint(this, &p1, &p2);
    pos = p;
}

// convert position (x,y) int coordiantes of root window
// to position in this window's coordinates
void VirtWnd::MapRootToMyPos(int& x, int& y) const
{
    int offX = pos.X;
    int offY = pos.Y;
    const VirtWnd *w = this;
    while (w->parent) {
        w = w->parent;
        offX += w->pos.X;
        offY += w->pos.Y;
    }
    x -= offX;
    y -= offY;
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
    styleMouseOver = NULL;
    wantedInputBits = (uint16_t)-1; // wants everything
    SetText(s);
}

void VirtWndButton::GetStyleForState(Style **first, Style **second) const
{
    if (bit::IsSet(stateBits, MouseOverBit)) {
        *first = styleMouseOver;
        *second = gStyleButtonMouseOver;
    } else {
        *first = styleDefault;
        *second = gStyleButtonDefault;
    }
}

Prop *VirtWndButton::GetPropForState(PropType type) const
{
    Style *s1, *s2;
    GetStyleForState(&s1, &s2);
    return GetProp(s1, s2, type);
}

void VirtWndButton::GetPropsForState(PropToGet *props, size_t propsCount) const
{
    Style *s1, *s2;
    GetStyleForState(&s1, &s2);
    GetProps(s1, s2, props, propsCount);
}

Font *VirtWndButton::GetFontForState() const
{
    Style *s1, *s2;
    GetStyleForState(&s1, &s2);
    return CachedFontFromProps(s1, s2);
}

Size VirtWndButton::GetBorderAndPaddingSize() const
{
    PropToGet props[] = {
        { PropPadding, NULL },
        { PropBorderLeftWidth, NULL },
        { PropBorderRightWidth, NULL },
        { PropBorderTopWidth, NULL },
        { PropBorderBottomWidth, NULL },
    };

    GetPropsForState(props, dimof(props));
    PaddingData pad = props[0].prop->padding;
    int dx = pad.left + pad.right;
    int dy = pad.top  + pad.bottom;

    // note: width is a float, not sure how I should round them
    dx += (int)(props[1].prop->GetWidth() + props[2].prop->GetWidth());
    dy += (int)(props[3].prop->GetWidth() + props[4].prop->GetWidth());
    return Size(dx, dy);
}

void VirtWndButton::NotifyMouseEnter()
{
    VirtWnd::NotifyMouseEnter();
    RecalculateSize(true);
}

void VirtWndButton::NotifyMouseLeave()
{
    VirtWnd::NotifyMouseLeave();
    RecalculateSize(true);
}

// Update desired size of the button.
// If the size changes, trigger layout (which will in
// turn request repaints of affected areas)
void VirtWndButton::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    desiredSize = GetBorderAndPaddingSize();
    textDx = 0;
    if (text) {
        Rect bbox = MeasureTextWithFont(GetFontForState(), text);
        textDx = bbox.Width;
        desiredSize.Width  += textDx;
        desiredSize.Height += bbox.Height;
    }

    if (!prevSize.Equals(desiredSize))
        RequestLayout(this);
    else if (repaintIfSizeDidntChange)
        RequestRepaint(this);
}

void VirtWndButton::SetText(const TCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize(true);
}

void VirtWndButton::Measure(const Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
}

void VirtWndButton::SetStyles(Style *def, Style *mouseOver)
{
    styleDefault = def;
    styleMouseOver = mouseOver;
    RecalculateSize(true);
}

int AlignedOffset(int containerDx, int elDx, AlignAttr align)
{
    if (Align_Left == align)
        return 0;
    if (Align_Right == align)
        return containerDx - elDx;
    // Align_Center or Align_Justify
    return (containerDx - elDx) / 2;
}

void VirtWndButton::Paint(Graphics *gfx, int offX, int offY)
{
    struct PropToGet props[] = {
        { PropColor, NULL },
        { PropBgColor, NULL },
        { PropPadding, NULL },
        { PropBorderTopWidth, NULL },
        { PropBorderTopColor, NULL },
        { PropBorderRightWidth, NULL },
        { PropBorderRightColor, NULL },
        { PropBorderBottomWidth, NULL },
        { PropBorderBottomColor, NULL },
        { PropBorderLeftWidth, NULL },
        { PropBorderLeftColor, NULL },
        { PropTextAlign, NULL },
    };

    if (!IsVisible())
        return;

    GetPropsForState(props, dimof(props));
    Prop *col   = props[0].prop;
    Prop *bgCol = props[1].prop;
    Prop *padding = props[2].prop;
    Prop *topWidth = props[3].prop;
    Prop *leftWidth = props[9].prop;
    Prop *textAlign = props[10].prop;

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = CreateBrush(bgCol, bbox);
    gfx->FillRectangle(brBgColor, bbox);
    ::delete brBgColor;

    BorderProps bp = {
        props[3].prop, props[4].prop,
        props[5].prop, props[6].prop,
        props[7].prop, props[8].prop,
        props[9].prop, props[10].prop,
    };

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, bp);
    if (str::IsEmpty(text))
        return;

    PaddingData pad = padding->padding;
    int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, textDx, textAlign->align.align);
    int x = offX + alignedOffX + pad.left + (int)leftWidth->width.width;
    int y = offY + pad.top + (int)topWidth->width.width;
    Brush *brColor = CreateBrush(col, bbox); // restrict bbox to just the text?
    gfx->DrawString(text, str::Len(text), GetFontForState(), PointF((REAL)x, (REAL)y), NULL, brColor);
    ::delete brColor;
}

static bool BitmapSizeEquals(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return false;
    return ((dx == bmp->GetWidth()) && (dy == bmp->GetHeight()));
}

void VirtWndHwnd::SetHwnd(HWND hwnd)
{
    CrashIf(NULL != hwndParent);
    hwndParent = hwnd;
    evtMgr = new EventMgr(this);
    painter = new VirtWndPainter(this);
}

// called when either the window size changed (as a result
// of WM_SIZE) or when window tree changes
void VirtWndHwnd::TopLevelLayout()
{
    CrashIf(!hwndParent);
    ClientRect rc(hwndParent);
    Size availableSize(rc.dx, rc.dy);
    Measure(availableSize);
    Rect r(0, 0, desiredSize.Width, desiredSize.Height);
    Arrange(r);
    layoutRequested = false;
}

// we paint the background in VirtWndPainter() because I don't
// want to add an artificial VirtWnd window just to cover
// the whole HWND and paint the background.
void VirtWndPainter::PaintBackground(Graphics *g, Rect r)
{
    Prop *bgProp = GetProp(wnd->styleDefault, gStyleDefault, PropBgColor);
    Brush *br = CreateBrush(bgProp, r);
    g->FillRectangle(br, r);
    ::delete br;
}

#define MIN_INT16  -32768
#define MAX_INT16   32767

// Paint windows in z-order by first collecting the windows
// and then paint consequitive layers with the same z-order,
// starting with the lowest z-order.
// We don't sort because we want to preserve the order of
// containment within z-order and non-stable sort could
// mess that up. Also, this should be faster in common
// case where most windows are in the same z-order.
void PaintWindowsInZOrder(Graphics *g, VirtWnd *wnd)
{
    Vec<WndAndOffset> windowsToPaint;
    WndFilter wndFilter;
    CollectWindowsBreathFirst(wnd, 0, 0, &wndFilter, &windowsToPaint);
    size_t paintedCount = 0;
    int16_t lastPaintedZOrder = MIN_INT16;
    size_t winCount = windowsToPaint.Count();
    for (;;) {
        // find which z-order should we paint now
        int16_t minUnpaintedZOrder = MAX_INT16;
        for (size_t i = 0; i < winCount; i++) {
            WndAndOffset woff = windowsToPaint.At(i);
            int16_t zOrder = woff.wnd->zOrder;
            if ((zOrder > lastPaintedZOrder) && (zOrder < minUnpaintedZOrder))
                minUnpaintedZOrder = zOrder;
        }
        for (size_t i = 0; i < winCount; i++) {
            WndAndOffset woff = windowsToPaint.At(i);
            if (minUnpaintedZOrder == woff.wnd->zOrder) {
                woff.wnd->Paint(g, woff.offX, woff.offY);
                ++paintedCount;
            }
        }
        if (paintedCount == winCount)
            return;
        CrashIf(paintedCount > winCount);
        lastPaintedZOrder = minUnpaintedZOrder;
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

    Region clip;

    // TODO: be intelligent about only repainting changed
    // parts for perf. Note: if cacheBmp changes, we need
    // to repaint everything
    Graphics gDC(dc);
    gDC.GetClip(&clip);

    ClientRect r(hwnd);
    if (!BitmapSizeEquals(cacheBmp, r.dx, r.dy)) {
        // note: could only re-allocate when the size increases
        ::delete cacheBmp;
        cacheBmp = ::new Bitmap(r.dx, r.dy, &gDC);
    }

    //TODO: log clipBounds for debugging
    //Rect clipBounds;
    //clip.GetBounds(&cliBounds)

    Graphics g((Image*)cacheBmp);
    InitGraphicsMode(&g);
    g.SetClip(&clip, CombineModeReplace);

    PaintBackground(&g, Rect(0, 0, r.dx, r.dy));
    PaintWindowsInZOrder(&g, wnd);

    // TODO: try to manually draw only the part that falls within
    // clipBounds or is it done automatically by DrawImage() ?
    gDC.DrawImage(cacheBmp, 0, 0);
    EndPaint(hwnd, &ps);
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

// TODO: optimize by getting both mouse over and mouse move windows in one call
// x, y is a position in the root window
LRESULT EventMgr::OnMouseMove(WPARAM keys, int x, int y, bool& handledOut)
{
    Vec<WndAndOffset> windows;
    VirtWnd *w;

    uint32_t wantedInputMask = bit::FromBit<uint32_t>(VirtWnd::WantsMouseOverBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count) {
        if (currOver) {
            currOver->NotifyMouseLeave();
            currOver = NULL;
        }
    } else {
        // TODO: should this take z-order into account ?
        w = windows.Last().wnd;
        if (w != currOver) {
            if (currOver)
                currOver->NotifyMouseLeave();
            currOver = w;
            currOver->NotifyMouseEnter();
        }
    }

    wantedInputMask = bit::FromBit<uint32_t>(VirtWnd::WantsMouseMoveBit);
    count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    w = windows.Last().wnd;
    w->MapRootToMyPos(x, y);
    w->NotifyMouseMove(x, y);
    return 0;
}

// TODO: quite possibly the real logic for generating "click" events is
// more complicated
LRESULT EventMgr::OnLButtonUp(WPARAM keys, int x, int y, bool& handledOut)
{
    Vec<WndAndOffset> windows;
    uint16_t wantedInputMask = bit::FromBit<uint16_t>(VirtWnd::WantsMouseClickBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    // TODO: should this take zOrder into account?
    VirtWnd *w = windows.Last().wnd;
    IClickHandler *clickHandler = GetClickHandlerFor(w);
    if (clickHandler)
        clickHandler->Clicked(w);
    return 0;
}

// TODO: not sure if handledOut serves any purpose (what exactly should it mean?)
LRESULT EventMgr::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& handledOut)
{
    if (WM_SIZE == msg)
        wndRoot->RequestLayout();

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

    if (WM_NULL == msg) {
        return 0;
    }

    return 0;
}

void RegisterForClickEvent(VirtWnd *wndSource, IClickHandler *clickHandler)
{
    CrashIf(!wndSource->WantsMouseClick());
    VirtWndHwnd *wHwnd = GetRootHwndWnd(wndSource);
    CrashIf(!wHwnd);
    wHwnd->evtMgr->RegisterForClickEvent(wndSource, clickHandler);
}

}

