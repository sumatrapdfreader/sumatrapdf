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

#include "mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

namespace mui {

// we use uint16 for those
STATIC_ASSERT(Control::WantedInputBitLast < 16, max16bitsForWantedIntputBits);
STATIC_ASSERT(Control::StateBitLast < 16, max16bitsForStateBits);

static GraphicsForMeasureText * uiGraphicsForMeasureText = NULL;
static Graphics *               uiGfxForMeasure = NULL;

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
    uiGraphicsForMeasureText = AllocGraphicsForMeasureText();
    uiGfxForMeasure = uiGraphicsForMeasureText->Get();
}

void Destroy()
{
    css::Destroy();
    delete uiGraphicsForMeasureText;
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

static HwndWrapper *GetRootHwndWnd(const Control *w)
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

class WndFilter
{
public:
    bool skipInvisibleSubtrees;

    WndFilter() : skipInvisibleSubtrees(true) {}

    virtual ~WndFilter() {}

    virtual bool Matches(Control *w, int offX, int offY) {
        return true;
    }
};

class WndInputWantedFilter : public WndFilter
{
    int x, y;
    uint16 wantedInputMask;

public:
    WndInputWantedFilter(int x, int y, uint16 wantedInputMask) :
        x(x), y(y), wantedInputMask(wantedInputMask)
    {
    }
    virtual ~WndInputWantedFilter() {}
    virtual bool Matches(Control *w, int offX, int offY) {
        if ((w->wantedInputBits & wantedInputMask) != 0) {
            Rect r = Rect(offX, offY, w->pos.Width, w->pos.Height);
            return r.Contains(x, y);
        }
        return false;
    }
};

struct WndAndOffset {
    Control *wnd;
    int offX, offY;
};

static void CollectWindowsBreathFirst(Control *w, int offX, int offY, WndFilter *wndFilter, Vec<WndAndOffset> *windows)
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
static size_t CollectWindowsAt(Control *wndRoot, int x, int y, uint16 wantedInputMask, Vec<WndAndOffset> *windows)
{
    WndInputWantedFilter filter(x, y, wantedInputMask);
    windows->Reset();
    CollectWindowsBreathFirst(wndRoot, 0, 0, &filter, windows);
    return windows->Count();
}

Brush *BrushFromProp(Prop *p, const Rect& r)
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

static Brush *BrushFromProp(Prop *p, const RectF& r)
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
    br = BrushFromProp(bp.topColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // right
    p1 = p2;
    p2.X = p1.X; p2.Y = p1.Y + r.Height;
    width = bp.rightWidth->width.width;
    br = BrushFromProp(bp.rightColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // bottom
    p1 = p2;
    p2.X = r.X; p2.Y = p1.Y;
    width = bp.bottomWidth->width.width;
    br = BrushFromProp(bp.bottomColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;

    // left
    p1 = p2;
    p2.X = p1.X; p2.Y = r.Y;
    width = bp.leftWidth->width.width;
    br = BrushFromProp(bp.leftColor, r);
    DrawLine(gfx, p1, p2, width, br);
    ::delete br;
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

Control::Control(Control *newParent)
{
    wantedInputBits = 0;
    stateBits = 0;
    zOrder = 0;
    parent = NULL;
    hwndParent = NULL;
    layout = NULL;
    hCursor = NULL;
    cachedProps = NULL;
    SetCurrentStyle(NULL, gStyleDefault);
    pos = Rect();
    if (newParent)
        SetParent(newParent);
}

Control::~Control()
{
    delete layout;
    DeleteVecMembers(children);
    if (!parent)
        return;
    HwndWrapper *root = GetRootHwndWnd(parent);
    CrashIf(!root);
    UnRegisterEventHandlers(root->evtMgr);
}

void Control::SetParent(Control *newParent)
{
    HwndWrapper *prevRoot = NULL;
    if (parent)
        prevRoot = GetRootHwndWnd(parent);
    HwndWrapper *newRoot = GetRootHwndWnd(newParent);
    CrashIf(!newRoot);

    parent = newParent;

    if (prevRoot)
        UnRegisterEventHandlers(prevRoot->evtMgr);

    RegisterEventHandlers(newRoot->evtMgr);
}

Control *Control::GetChild(size_t idx) const
{
    return children.At(idx);
}

size_t Control::GetChildCount() const
{
    return children.Count();
}

bool Control::WantsMouseClick() const
{
    return bit::IsSet(wantedInputBits, WantsMouseClickBit);
}

bool Control::WantsMouseMove() const
{
    return bit::IsSet(wantedInputBits, WantsMouseMoveBit);
}

bool Control::IsMouseOver() const
{
    return bit::IsSet(stateBits, MouseOverBit);
}

bool Control::IsVisible() const
{
    return !bit::IsSet(stateBits, IsHiddenBit);
}

void Control::SetIsMouseOver(bool isOver)
{
    if (isOver)
        bit::Set(stateBits, MouseOverBit);
    else
        bit::Clear(stateBits, MouseOverBit);
}

void Control::AddChild(Control *wnd, int pos)
{
    CrashAlwaysIf(NULL == wnd);
    if ((pos < 0) || (pos >= (int)children.Count()))
        children.Append(wnd);
    else
        children.InsertAt(pos, wnd);
    wnd->SetParent(this);
}

void Control::Measure(const Size availableSize)
{
    if (layout) {
        layout->Measure(availableSize, this);
    } else {
        desiredSize = Size();
    }
}

void Control::MeasureChildren(Size availableSize) const
{
    for (size_t i = 0; i < GetChildCount(); i++) {
        GetChild(i)->Measure(availableSize);
    }
}

void Control::Arrange(const Rect finalRect)
{
    if (layout) {
        layout->Arrange(finalRect, this);
    } else {
        SetPosition(finalRect);
    }
}

void Control::Show()
{
    if (IsVisible())
        return; // perf: avoid unnecessary repaints
    bit::Clear(stateBits, IsHiddenBit);
    RequestRepaint(this);
}

void Control::Hide()
{
    if (!IsVisible())
        return;
    RequestRepaint(this); // request repaint before hiding, to trigger repaint
    bit::Set(stateBits, IsHiddenBit);
}

void Control::SetPosition(const Rect& p)
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
void Control::MapRootToMyPos(int& x, int& y) const
{
    int offX = pos.X;
    int offY = pos.Y;
    const Control *w = this;
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
void Control::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;
}

void Control::SetCurrentStyle(Style *style1, Style *style2)
{
    cachedProps = CachePropsForStyle(style1, style2);
}

Prop *Control::GetCachedProp(PropType propType) const
{
    return cachedProps[propType];
}

Prop **Control::GetCachedProps() const
{
    return cachedProps;
}

Button::Button(const TCHAR *s)
{
    text = NULL;
    wantedInputBits = (uint16)-1; // wants everything
    styleDefault = NULL;
    styleMouseOver = NULL;
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    SetText(s);
}

Button::~Button()
{
    free(text);
}

static void AddBorders(int& dx, int& dy, Prop **props)
{
    Prop *p1 = props[PropBorderLeftWidth];
    Prop *p2 =  props[PropBorderRightWidth];
    // note: width is a float, not sure how I should round them
    dx += (int)(p1->GetWidth() + p2->GetWidth());
    p1 = props[PropBorderTopWidth];
    p2 =  props[PropBorderBottomWidth];
    dy += (int)(p1->GetWidth() + p2->GetWidth());
}

Size Button::GetBorderAndPaddingSize() const
{
    Prop **props = GetCachedProps();
    PaddingData pad = props[PropPadding]->padding;
    int dx = pad.left + pad.right;
    int dy = pad.top  + pad.bottom;
    AddBorders(dx, dy, props);
    return Size(dx, dy);
}

void Button::NotifyMouseEnter()
{
    SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    RecalculateSize(true);
}

void Button::NotifyMouseLeave()
{
    SetCurrentStyle(styleDefault, gStyleButtonDefault);
    RecalculateSize(true);
}

// Update desired size of the button.
// If the size changes, trigger layout (which will in
// turn request repaints of affected areas)
void Button::RecalculateSize(bool repaintIfSizeDidntChange)
{
    Size prevSize = desiredSize;

    desiredSize = GetBorderAndPaddingSize();
    textDx = 0;
    if (text) {
        Graphics *gfx = UIThreadGraphicsForMeasureText();
        Font *font = CachedFontFromCachedProps(GetCachedProps());
        RectF bbox = MeasureText(gfx, font, text);
        textDx = (size_t)bbox.Width; // TODO: round up?
        desiredSize.Width  += textDx;
        desiredSize.Height += (INT)bbox.Height; // TODO: round up?
    }

    if (!prevSize.Equals(desiredSize))
        RequestLayout(this);
    else if (repaintIfSizeDidntChange)
        RequestRepaint(this);
}

void Button::SetText(const TCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize(true);
}

void Button::Measure(const Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
}

void Button::SetStyles(Style *def, Style *mouseOver)
{
    styleDefault = def;
    styleMouseOver = mouseOver;

    if (IsMouseOver())
        SetCurrentStyle(styleMouseOver, gStyleButtonMouseOver);
    else
        SetCurrentStyle(styleDefault, gStyleButtonDefault);

    RecalculateSize(true);
}

// given the size of a container, the size of an element inside
// a container and alignment, calculates the position of
// element within the container.
static int AlignedOffset(int containerDx, int elDx, AlignAttr align)
{
    if (Align_Left == align)
        return 0;
    if (Align_Right == align)
        return containerDx - elDx;
    // Align_Center or Align_Justify
    return (containerDx - elDx) / 2;
}

void Button::Paint(Graphics *gfx, int offX, int offY)
{
    if (!IsVisible())
        return;

    Prop **props = GetCachedProps();
    Prop *col   = props[PropColor];
    Prop *bgCol = props[PropBgColor];
    Prop *padding = props[PropPadding];
    Prop *topWidth = props[PropBorderTopWidth];
    Prop *leftWidth = props[PropBorderLeftWidth];
    Prop *textAlign = props[PropTextAlign];

    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    Brush *brBgColor = BrushFromProp(bgCol, bbox);
    gfx->FillRectangle(brBgColor, bbox);
    ::delete brBgColor;

    BorderProps bp = {
        props[PropBorderTopWidth], props[PropBorderTopColor],
        props[PropBorderRightWidth], props[PropBorderRightColor],
        props[PropBorderBottomWidth], props[PropBorderBottomColor],
        props[PropBorderLeftWidth], props[PropBorderLeftColor],
    };

    Rect r(offX, offY, pos.Width, pos.Height);
    DrawBorder(gfx, r, bp);
    if (str::IsEmpty(text))
        return;

    PaddingData pad = padding->padding;
    int alignedOffX = AlignedOffset(pos.Width - pad.left - pad.right, textDx, textAlign->align.align);
    int x = offX + alignedOffX + pad.left + (int)leftWidth->width.width;
    int y = offY + pad.top + (int)topWidth->width.width;
    Brush *brColor = BrushFromProp(col, bbox); // restrict bbox to just the text?
    Font *font = CachedFontFromCachedProps(props);
    gfx->DrawString(text, str::Len(text), font, PointF((REAL)x, (REAL)y), NULL, brColor);
    ::delete brColor;
}

static bool BitmapSizeEquals(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return false;
    return ((dx == bmp->GetWidth()) && (dy == bmp->GetHeight()));
}

static bool BitmapNotBigEnough(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return true;
    if (bmp->GetWidth() < (UINT)dx)
        return true;
    if (bmp->GetHeight() < (UINT)dy)
        return true;
    return false;
}
// Set minimum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMinSize(Size s)
{
    evtMgr->SetMinSize(s);
}

// Set maximum size for the HWND represented by this HwndWrapper.
// It is enforced in EventManager.
// Default size is (0,0) which is unlimited.
// For top-level windows it's the size of the whole window, including
// non-client area like borders, title area etc.
void HwndWrapper::SetMaxSize(Size s)
{
    evtMgr->SetMaxSize(s);
}

void HwndWrapper::SetHwnd(HWND hwnd)
{
    CrashIf(NULL != hwndParent);
    hwndParent = hwnd;
    evtMgr = new EventMgr(this);
    painter = new Painter(this);
}

// called when either the window size changed (as a result
// of WM_SIZE) or when window tree changes
void HwndWrapper::TopLevelLayout()
{
    CrashIf(!hwndParent);
    ClientRect rc(hwndParent);
    Size availableSize(rc.dx, rc.dy);
    Measure(availableSize);
    Rect r(0, 0, desiredSize.Width, desiredSize.Height);
    Arrange(r);
    layoutRequested = false;
}

HwndWrapper::HwndWrapper(HWND hwnd) 
    : painter(NULL), evtMgr(NULL), layoutRequested(false)
{
    if (hwnd)
        SetHwnd(hwnd);
}

HwndWrapper::~HwndWrapper()
{
    delete evtMgr;
    delete painter;
}

// mark for re-layout at the earliest convenience
void HwndWrapper::RequestLayout()
{
    layoutRequested = true;
    // trigger message queue so that the layout request is processed
    PostMessage(hwndParent, WM_NULL, 0, 0);
}

void HwndWrapper::LayoutIfRequested()
{
    if (layoutRequested)
        TopLevelLayout();
}

void HwndWrapper::OnPaint(HWND hwnd)
{
    CrashIf(hwnd != hwndParent);
    painter->Paint(hwnd);
}

Painter::Painter(HwndWrapper *wnd)
    : wnd(wnd), cacheBmp(NULL)
{
}

// we paint the background in Painter() because I don't
// want to add an artificial Control window just to cover
// the whole HWND and paint the background.
void Painter::PaintBackground(Graphics *g, Rect r)
{
    // TODO: don't quite get why I need to expand the rectangle, but
    // sometimes there's a seemingly 1 pixel artifact on the left and
    // at the top if I don't do this
    r.Inflate(1,1);
    Prop *bgProp = wnd->GetCachedProp(PropBgColor);
    Brush *br = BrushFromProp(bgProp, r);
    g->FillRectangle(br, r);
    ::delete br;
}

// Paint windows in z-order by first collecting the windows
// and then paint consecutive layers with the same z-order,
// starting with the lowest z-order.
// We don't sort because we want to preserve the order of
// containment within z-order and non-stable sort could
// mess that up. Also, this should be faster in common
// case where most windows are in the same z-order.
void PaintWindowsInZOrder(Graphics *g, Control *wnd)
{
    Vec<WndAndOffset> windowsToPaint;
    WndFilter wndFilter;
    CollectWindowsBreathFirst(wnd, 0, 0, &wndFilter, &windowsToPaint);
    size_t paintedCount = 0;
    int16 lastPaintedZOrder = INT16_MIN;
    size_t winCount = windowsToPaint.Count();
    for (;;) {
        // find which z-order should we paint now
        int16 minUnpaintedZOrder = INT16_MAX;
        for (size_t i = 0; i < winCount; i++) {
            WndAndOffset woff = windowsToPaint.At(i);
            int16 zOrder = woff.wnd->zOrder;
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
// all its children. Control must be the top-level window associated
// with HWND.
// Note: maybe should be split into BeginPaint()/Paint()/EndPaint()
// calls so that the caller can do more drawing after Paint()
void Painter::Paint(HWND hwnd)
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

    // TODO: fix showing black parts when resizing a window.
    // my theory is that we see black background on right/bottom
    // of the window when we resize the window because the os paints
    // it black and we take too long to perform the whole paint so the
    // black part persists long enough for human eye to notice.
    // To fix that we could try to paint the black part immediately
    // to gDC using the same color as the background. This is problematic
    // for two reasons:
    // - I don't know which part exactly needs to be repainted
    // - it can be tricky if background is a gradient
    // I thought I could just do PaintBackground(&gDC, Rect(0, 0, r.dx, r.dy))
    // but that generates flickr which leads me to believe that either
    // Graphics::FillRectangle() ignores clip region or clip region is not set
    // properly. Current solution detects a resize, paints a background and the
    // last version of page, which somewhat eliminates the problem but also
    // sometimes causes flickr
    // See http://www.catch22.net/tuts/flicker for info on win repainting
    if (cacheBmp && !sizeDuringLastPaint.Equals(Size(r.dx, r.dy))) {
        PaintBackground(&gDC, Rect(0, 0, r.dx, r.dy));
        gDC.DrawImage(cacheBmp, 0, 0);
        sizeDuringLastPaint = Size(r.dx, r.dy);
    }

    if (BitmapNotBigEnough(cacheBmp, r.dx, r.dy)) {
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

EventMgr::EventMgr(HwndWrapper *wndRoot)
    : wndRoot(wndRoot), currOver(NULL)
{
    CrashIf(!wndRoot);
    //CrashIf(wndRoot->hwnd);
}

EventMgr::~EventMgr()
{
}

// Set minimum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMinSize(Size s)
{
    minSize = s;
}

// Set maximum size that will be enforced by handling WM_GETMINMAXINFO
// Default is (0,0), which is unlimited
void EventMgr::SetMaxSize(Size s)
{
    maxSize = s;
}

void EventMgr::UnRegisterClickHandlers(IClickHandler *clickHandler)
{
    size_t i = 0;
    while (i < clickHandlers.Count()) {
        ClickHandler h = clickHandlers.At(i);
        if (h.clickHandler == clickHandler)
            clickHandlers.RemoveAt(i);
        else
            i++;
    }
}

void EventMgr::RegisterClickHandler(Control *wndSource, IClickHandler *clickHandler)
{
    ClickHandler ch = { wndSource, clickHandler };
    clickHandlers.Append(ch);
}

IClickHandler *EventMgr::GetClickHandlerFor(Control *wndSource)
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
LRESULT EventMgr::OnMouseMove(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<WndAndOffset> windows;
    Control *w;

    uint16 wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseOverBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count) {
        if (currOver) {
            currOver->SetIsMouseOver(false);
            currOver->NotifyMouseLeave();
            currOver = NULL;
        }
    } else {
        // TODO: should this take z-order into account ?
        w = windows.Last().wnd;
        if (w != currOver) {
            if (currOver) {
                currOver->SetIsMouseOver(false);
                currOver->NotifyMouseLeave();
            }
            currOver = w;
            currOver->SetIsMouseOver(true);
            currOver->NotifyMouseEnter();
        }
    }

    wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseMoveBit);
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
// (x, y) is in the coordinates of the root window
LRESULT EventMgr::OnLButtonUp(WPARAM keys, int x, int y, bool& wasHandled)
{
    Vec<WndAndOffset> windows;
    uint16 wantedInputMask = bit::FromBit<uint16>(Control::WantsMouseClickBit);
    size_t count = CollectWindowsAt(wndRoot, x, y, wantedInputMask, &windows);
    if (0 == count)
        return 0;
    // TODO: should this take z-order into account?
    Control *w = windows.Last().wnd;
    w->MapRootToMyPos(x, y);
    IClickHandler *clickHandler = GetClickHandlerFor(w);
    if (clickHandler)
        clickHandler->Clicked(w, x, y);
    return 0;
}

static void SetIfNotZero(LONG& l, int i, bool& didSet)
{
    if (i != 0) {
        l = i;
        didSet = true;
    }
}

LRESULT EventMgr::OnGetMinMaxInfo(MINMAXINFO *info, bool& wasHandled)
{
    SetIfNotZero(info->ptMinTrackSize.x, minSize.Width,  wasHandled);
    SetIfNotZero(info->ptMinTrackSize.y, minSize.Height, wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.x, maxSize.Width,  wasHandled);
    SetIfNotZero(info->ptMaxTrackSize.y, maxSize.Height, wasHandled);
    return 0;
}

LRESULT EventMgr::OnSetCursor(int x, int y, bool& wasHandled)
{
    if (currOver && currOver->hCursor) {
        SetCursor(currOver->hCursor);
        wasHandled = true;
    }
    return TRUE;
}

LRESULT EventMgr::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam, bool& wasHandled)
{
    if (WM_SIZE == msg)
        wndRoot->RequestLayout();

    wndRoot->LayoutIfRequested();

    wasHandled = false;
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    if (WM_SETCURSOR == msg) {
        POINT pt;
        if (GetCursorPos(&pt) && ScreenToClient(wndRoot->hwndParent, &pt))
            return OnSetCursor(pt.x, pt.y, wasHandled);
        return 0;
    }

    if (WM_MOUSEMOVE == msg)
        return OnMouseMove(wParam, x, y, wasHandled);

    if (WM_LBUTTONUP == msg)
        return OnLButtonUp(wParam, x, y, wasHandled);

    if (WM_GETMINMAXINFO == msg)
        return OnGetMinMaxInfo((MINMAXINFO*)lParam, wasHandled);

    return 0;
}

}
