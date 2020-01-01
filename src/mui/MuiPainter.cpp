/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/Log.h"

#include "Mui.h"

namespace mui {

static bool BitmapNotBigEnough(Bitmap* bmp, int dx, int dy) {
    if (nullptr == bmp)
        return true;
    if (bmp->GetWidth() < (UINT)dx)
        return true;
    if (bmp->GetHeight() < (UINT)dy)
        return true;
    return false;
}

Painter::Painter(HwndWrapper* wnd) : wnd(wnd), cacheBmp(nullptr) {
}

Painter::~Painter() {
    ::delete cacheBmp;
}

// we paint the background in Painter() because I don't
// want to add an artificial Control window just to cover
// the whole HWND and paint the background.
void Painter::PaintBackground(Graphics* g, Rect r) {
    // TODO: don't quite get why I need to expand the rectangle, but
    // sometimes there's a seemingly 1 pixel artifact on the left and
    // at the top if I don't do this
    r.Inflate(1, 1);
    ColorData* bgColor = wnd->cachedStyle->bgColor;
    Brush* br = BrushFromColorData(bgColor, r);
    g->FillRectangle(br, r);
}

// TODO: figure out how INT16_MIN was defined
// This is in <intsafe.h> and utils\msvc\stdint.h under some conditions
#define MY_INT16_MIN (-32767 - 1)
#define MY_INT16_MAX 32767

// Paint windows in z-order by first collecting the windows
// and then painting consecutive layers with the same z-order,
// starting with the lowest z-order.
// We don't sort because we want to preserve the order of
// containment of windows with the same z-order and non-stable
// sort could change it.
static void PaintWindowsInZOrder(Graphics* g, Control* c) {
    Vec<CtrlAndOffset> toPaint;
    WndFilter wndFilter;
    Pen debugPen(Color(255, 0, 0), 1);

    CollectWindowsBreathFirst(c, 0, 0, &wndFilter, &toPaint);
    size_t paintedCount = 0;
    int16_t lastPaintedZOrder = MY_INT16_MIN;
    for (;;) {
        // find which z-order should we paint now
        int16_t minUnpaintedZOrder = MY_INT16_MAX;
        for (CtrlAndOffset& coff : toPaint) {
            int16_t zOrder = coff.c->zOrder;
            if ((zOrder > lastPaintedZOrder) && (zOrder < minUnpaintedZOrder))
                minUnpaintedZOrder = zOrder;
        }
        for (CtrlAndOffset& coff : toPaint) {
            if (minUnpaintedZOrder == coff.c->zOrder) {
                coff.c->Paint(g, coff.offX, coff.offY);
                if (IsDebugPaint()) {
                    Rect bbox(coff.offX, coff.offY, coff.c->pos.Width, coff.c->pos.Height);
                    g->DrawRectangle(&debugPen, bbox);
                }
                ++paintedCount;
            }
        }
        if (paintedCount == toPaint.size())
            return;
        CrashIf(paintedCount > toPaint.size());
        lastPaintedZOrder = minUnpaintedZOrder;
    }
}

// Should be called from WM_PAINT. Recursively paints a given window and
// all its children. Control must be the top-level window associated
// with HWND.
// Note: maybe should be split into BeginPaint()/Paint()/EndPaint()
// calls so that the caller can do more drawing after Paint()
void Painter::Paint(HWND hwnd, bool isDirty) {
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
    // I thought I could just do PaintBackground(&gDC, r.ToGdipRect())
    // but that generates flickr which leads me to believe that either
    // Graphics::FillRectangle() ignores clip region or clip region is not set
    // properly. Current solution detects a resize, paints a background and the
    // last version of page, which somewhat eliminates the problem but also
    // sometimes causes flickr
    // See http://www.catch22.net/tuts/flicker for info on win repainting
    if (cacheBmp && !sizeDuringLastPaint.Equals(Size(r.dx, r.dy))) {
        PaintBackground(&gDC, r.ToGdipRect());
        gDC.DrawImage(cacheBmp, 0, 0);
        sizeDuringLastPaint = Size(r.dx, r.dy);
    }

    if (BitmapNotBigEnough(cacheBmp, r.dx, r.dy)) {
        ::delete cacheBmp;
        cacheBmp = ::new Bitmap(r.dx, r.dy, &gDC);
        isDirty = true;
    }

    // TODO: log clipBounds for debugging
    // Rect clipBounds;
    // clip.GetBounds(&cliBounds)

    // draw to a bitmap cache unless we were asked to skip
    // this step and just blit cached bitmap because the caller
    // knows it didn't change
    if (isDirty) {
        Graphics g((Image*)cacheBmp);
        InitGraphicsMode(&g);
        g.SetClip(&clip, CombineModeReplace);

        PaintBackground(&g, r.ToGdipRect());
        PaintWindowsInZOrder(&g, wnd);
    }

    // TODO: try to manually draw only the part that falls within
    // clipBounds or is it done automatically by DrawImage() ?
    gDC.DrawImage(cacheBmp, 0, 0);
    EndPaint(hwnd, &ps);
}
} // namespace mui
