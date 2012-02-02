/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

#if 0
static bool BitmapSizeEquals(Bitmap *bmp, int dx, int dy)
{
    if (NULL == bmp)
        return false;
    return ((dx == bmp->GetWidth()) && (dy == bmp->GetHeight()));
}
#endif

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
    WrappedBrush br = BrushFromProp(bgProp, r);
    g->FillRectangle(br.brush, r);
}

// Paint windows in z-order by first collecting the windows
// and then paint consecutive layers with the same z-order,
// starting with the lowest z-order.
// We don't sort because we want to preserve the order of
// containment within z-order and non-stable sort could
// mess that up. Also, this should be faster in common
// case where most windows are in the same z-order.
static void PaintWindowsInZOrder(Graphics *g, Control *wnd)
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

}
