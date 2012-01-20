/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "mui.h"
#include "BaseUtil.h"
#include "Vec.h"
#include "GeomUtil.h"

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
*/

#include "mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"

namespace mui {

// Graphics object that can be used at any time to measure text
static Graphics *   gGraphicsForFontMeasure = NULL;
static Bitmap *     gBitmapForFontMeasureGraphics = NULL;
static BYTE *       gBitmapDataForFontMeasureGraphics = NULL;

static Font *       gDefaultFont = NULL;

static void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);

    gDefaultFont = ::new Font(FontFamily::GenericSansSerif(), 12, FontStyleBold);
    //gDefaultFont = ::new Font(L"Times New Roman", 18, FontStyleBold);
}

void Initialize()
{
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
    ::delete gDefaultFont;
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

VirtWnd::VirtWnd(VirtWnd *parent)
{
    SetParent(parent);
    isVisible = true;
    layout = NULL;
    pos = Rect();
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
    if (!isVisible)
        return;
}

void VirtWndButton::RecalculateSize()
{
    desiredSize = Size(120,28);
    if (!text)
        return;
    Rect bbox = MeasureTextWithFont(gDefaultFont, text);
    bbox.GetSize(&desiredSize);
    desiredSize.Width  += (padding.left + padding.right);
    desiredSize.Height += (padding.top  + padding.bottom);
}

void VirtWndButton::SetText(const TCHAR *s)
{
    str::ReplacePtr(&text, s);
    RecalculateSize();
    // TODO: this should trigger repaint and relayout
}

void VirtWndButton::Measure(Size availableSize)
{
    // desiredSize is calculated when we change the
    // text, font or other attributes that influence
    // the size so it doesn't have to be calculated
    // here
}

void VirtWndButton::Paint(Graphics *gfx, int offX, int offY)
{
    if (!isVisible)
        return;

    SolidBrush br(Color(255,0,0));
    SolidBrush bgBr(Color(180, 255, 255, 255)); // semi-transparent white
    RectF bbox((REAL)offX, (REAL)offY, (REAL)pos.Width, (REAL)pos.Height);
    gfx->FillRectangle(&bgBr, bbox);

    if (!text)
        return;

    int x = offX + padding.left;
    int y = offY + padding.bottom;
    gfx->DrawString(text, str::Len(text), gDefaultFont, PointF((REAL)x, (REAL)y), NULL, &br);
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
    if (!wnd->isVisible)
        return;
    offX += wnd->pos.GetLeft();
    offY += wnd->pos.GetTop();
    wnd->Paint(g, offX, offY);

    for (size_t i = 0; i < wnd->GetChildCount(); i++) {
        VirtWnd *w = wnd->children.At(i);
        PaintRecursively(g, w, offX, offY);
    }
}

// Should be called from WM_PAINT. Recursively paints a given window and
// all its children. VirtWnd must be the top-level window associated
// with HWND.
// Note: maybe should be split into BeginPaint()/Paint()/EndPaint()
// calls so that the caller can do more drawing after Paint()
void VirtWndPainter::OnPaint(HWND hwnd, VirtWnd *hwndWnd)
{
    CrashAlwaysIf(hwnd != hwndWnd->hwndParent);

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

    PaintRecursively(&g, hwndWnd, 0, 0);

    gDC.DrawImage(cacheBmp, 0, 0);
    EndPaint(hwnd, &ps);
}

}

