/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

// A buffered Gfx must finish its backend drawing before copying pixels to the
// target. Derived destructors run first, so this base destructor is late enough
// for Direct2D's EndDraw() and GDI+'s Graphics teardown to have completed.
Gfx::~Gfx() {
    if (doubleBufferTarget && doubleBufferSource && !doubleBufferSize.IsEmpty()) {
        // BitBlt onto LAYOUT_RTL mirrors glyphs; the buffer is LTR (issue #6113).
        DWORD layout = GetLayout(doubleBufferTarget);
        bool mirrored = layout != GDI_ERROR && (layout & LAYOUT_RTL);
        if (mirrored) {
            SetLayout(doubleBufferTarget, 0);
        }
        BitBlt(doubleBufferTarget, 0, 0, doubleBufferSize.dx, doubleBufferSize.dy, doubleBufferSource, 0, 0, SRCCOPY);
        if (mirrored) {
            SetLayout(doubleBufferTarget, layout);
        }
    }
}

GfxHdc::GfxHdc(HDC hdc) {
    this->hdc = hdc;
}

void GfxHdc::FillRect(const Rect& r, Color col) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    HdcFillRect(hdc, r, col);
}

void GfxHdc::DrawRect(const Rect& r, Color col, int thickness) {
    if (ColorSkipsPaint(col) || r.IsEmpty() || thickness < 1) {
        return;
    }
    int t = std::min({thickness, r.dx, r.dy});
    HdcFillRect(hdc, {r.x, r.y, r.dx, t}, col);
    HdcFillRect(hdc, {r.x, r.Bottom() - t, r.dx, t}, col);
    int sideDy = r.dy - (2 * t);
    if (sideDy > 0) {
        HdcFillRect(hdc, {r.x, r.y + t, t, sideDy}, col);
        HdcFillRect(hdc, {r.Right() - t, r.y + t, t, sideDy}, col);
    }
}

void GfxHdc::DrawDashedRect(const Rect& r, Color col) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    AutoDeletePen pen(CreatePen(PS_DASH, 1, col));
    ScopedSelectObject restorePen(hdc, pen);
    ScopedSelectObject restoreBrush(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, r.x, r.y, r.Right() + 1, r.Bottom() + 1);
}

// how wide the thing the DC draws into is, for mirroring
static int HdcSurfaceDx(HDC hdc) {
    HWND hwnd = WindowFromDC(hdc);
    if (hwnd) {
        return HwndClientRect(hwnd).dx;
    }
    auto bmp = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);
    BITMAP bi{};
    if (bmp && GetObjectW(bmp, sizeof(bi), &bi) != 0) {
        return bi.bmWidth;
    }
    return GetDeviceCaps(hdc, HORZRES);
}

// The shapes that have to be anti-aliased (rounded rects, circles, diagonals)
// are drawn with gdiplus even here — gdi has nothing that does it. gdiplus
// draws straight through a mirrored (LAYOUT_RTL) DC instead of picking up its
// orientation, so the mirroring is applied by hand to keep those shapes in the
// same place as everything gdi draws.
struct GdiplusOnHdc {
    Gdiplus::Graphics g;

    explicit GdiplusOnHdc(HDC hdc) : g(hdc) {
        // deliberately not CompositingQualityHighQuality: that blends alpha in
        // linear space, which makes every translucent overlay far more opaque
        // than the alpha says (the selection's 0x5f over red came out at 0xa4
        // instead of 0x5f). The alphas in the app are all picked against the
        // default sRGB blending, which is also what GfxGdiplus uses, so the
        // same overlay looks the same in both backends
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPageUnit(Gdiplus::UnitPixel);
        DWORD layout = GetLayout(hdc);
        if (layout != GDI_ERROR && (layout & LAYOUT_RTL)) {
            g.ScaleTransform(-1, 1);
            g.TranslateTransform((float)HdcSurfaceDx(hdc), 0, Gdiplus::MatrixOrderAppend);
        }
    }
};

void GfxHdc::FillRects(const Rect* rects, int count, Color col, u8 alpha, int outlineWidth) {
    if (ColorSkipsPaint(col) || count <= 0) {
        return;
    }
    GdiplusOnHdc gh(hdc);
    Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
    for (int i = 0; i < count; i++) {
        if (!rects[i].IsEmpty()) {
            path.AddRectangle(ToGdipRect(rects[i]));
        }
    }
    u8 r, g, b;
    UnpackColor(col, r, g, b);
    Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, r, g, b));
    gh.g.FillPath(&brush, &path);
    if (outlineWidth > 0) {
        path.Outline(nullptr, 0.2f);
        Gdiplus::Pen pen(Gdiplus::Color(alpha, 0, 0, 0), (float)outlineWidth);
        gh.g.DrawPath(&pen, &path);
    }
}

void GfxHdc::FillQuads(const Point* pts, int nQuads, Color col, u8 alpha, int outlineWidth) {
    if (ColorSkipsPaint(col) || nQuads <= 0 || !pts) {
        return;
    }
    GdiplusOnHdc gh(hdc);
    Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
    for (int i = 0; i < nQuads; i++) {
        const Point* p = pts + i * 4;
        Gdiplus::Point gp[4] = {{p[0].x, p[0].y}, {p[1].x, p[1].y}, {p[2].x, p[2].y}, {p[3].x, p[3].y}};
        path.AddPolygon(gp, 4);
    }
    u8 r, g, b;
    UnpackColor(col, r, g, b);
    Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, r, g, b));
    gh.g.FillPath(&brush, &path);
    if (outlineWidth > 0) {
        path.Outline(nullptr, 0.2f);
        Gdiplus::Pen pen(Gdiplus::Color(alpha, 0, 0, 0), (float)outlineWidth);
        gh.g.DrawPath(&pen, &path);
    }
}

// `d` is the diameter of the corner circles, so it is the radius doubled; the
// callers pass a value tuned to look right rather than a true radius
static void AddRoundedRectPath(Gdiplus::GraphicsPath& path, const Rect& rc, int d) {
    path.AddArc(rc.x, rc.y, d, d, 180, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y, d, d, 270, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y + rc.dy - d - 1, d, d, 0, 90);
    path.AddArc(rc.x, rc.y + rc.dy - d - 1, d, d, 90, 90);
    path.CloseFigure();
}

// gdiplus rather than gdi's RoundRect() because the corners have to be
// anti-aliased to look like anything
void GfxHdc::FillRoundedRect(const Rect& r, int radius, Color fill, Color border) {
    if (r.IsEmpty()) {
        return;
    }
    if (radius <= 0) {
        FillRect(r, fill);
        DrawRect(r, border);
        return;
    }
    GdiplusOnHdc gp(hdc);
    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(path, r, radius);
    if (!ColorSkipsPaint(fill)) {
        Gdiplus::SolidBrush br(GdiRgbFromColor(fill));
        gp.g.FillPath(&br, &path);
    }
    if (!ColorSkipsPaint(border)) {
        Gdiplus::Pen pen(GdiRgbFromColor(border), 1);
        gp.g.DrawPath(&pen, &path);
    }
}

void GfxHdc::FillEllipse(const Rect& r, Color col, u8 alpha) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    GdiplusOnHdc gp(hdc);
    Gdiplus::SolidBrush br(Gdiplus::Color(alpha, GetRValue(col), GetGValue(col), GetBValue(col)));
    gp.g.FillEllipse(&br, r.x, r.y, r.dx - 1, r.dy - 1);
}

// kColorUnset draws in the surface's current text color, which is how the
// underline under a VirtText picks up the color the text was drawn in
void GfxHdc::DrawLine(const Rect& r, Color col, int thickness) {
    if (col == kColorUnset) {
        col = GetTextColor(hdc);
    }
    Rect r2 = r;
    if (r2.dy == 0) {
        r2.dy = thickness;
    } else if (r2.dx == 0) {
        r2.dx = thickness;
    }
    HdcFillRect(hdc, r2, col);
}

void GfxHdc::DrawLineAA(Point p1, Point p2, Color col, float thickness, u8 alpha) {
    if (col == kColorUnset) {
        col = GetTextColor(hdc);
    }
    GdiplusOnHdc gp(hdc);
    Gdiplus::Pen pen(Gdiplus::Color(alpha, GetRValue(col), GetGValue(col), GetBValue(col)), thickness);
    gp.g.DrawLine(&pen, p1.x, p1.y, p2.x, p2.y);
}

void GfxHdc::DrawFocusRect(const Rect& r) {
    RECT rr = ToRECT(r);
    ::DrawFocusRect(hdc, &rr);
}

static uint ToDrawTextFormat(u32 flags) {
    uint fmt = DT_NOPREFIX;
    bool wrap = (flags & gfxTextWrap) != 0;
    bool ellipsis = (flags & (gfxTextEllipsis | gfxTextPathEllipsis)) != 0;
    if (wrap) {
        fmt |= DT_WORDBREAK;
    }
    if (flags & gfxTextPathEllipsis) {
        fmt |= DT_PATH_ELLIPSIS;
    } else if (flags & gfxTextEllipsis) {
        fmt |= wrap ? DT_WORD_ELLIPSIS : DT_END_ELLIPSIS;
    }
    if (!wrap && (ellipsis || (flags & (gfxTextSingleLine | gfxTextVCenter)))) {
        fmt |= DT_SINGLELINE;
    }
    if (flags & gfxTextVCenter) {
        fmt |= DT_VCENTER;
    }
    // text that can't be ellipsized isn't clipped either: that's what the
    // callers of the original DrawText() got
    if ((!ellipsis && !wrap) || (flags & gfxTextNoClip)) {
        fmt |= DT_NOCLIP;
    }
    if (flags & gfxTextRtl) {
        fmt |= DT_RTLREADING;
    }
    if (flags & gfxTextCenter) {
        fmt |= DT_CENTER;
    } else if (flags & gfxTextRight) {
        fmt |= DT_RIGHT;
    }
    return fmt;
}

// sets the color / background mode DrawText needs and puts back what it found,
// so a caller that paints many items into one DC doesn't have to
struct ScopedTextState {
    HDC hdc;
    Color prevCol = kColorUnset;
    int prevBkMode = 0;
    bool setCol = false;

    ScopedTextState(HDC hdc, Color col) {
        this->hdc = hdc;
        setCol = (col != kColorUnset);
        if (setCol) {
            prevCol = SetTextColor(hdc, col);
        }
        prevBkMode = SetBkMode(hdc, TRANSPARENT);
    }
    ~ScopedTextState() {
        if (setCol) {
            SetTextColor(hdc, prevCol);
        }
        if (prevBkMode != 0) {
            SetBkMode(hdc, prevBkMode);
        }
    }
};

void GfxHdc::DrawText(Str s, const Rect& r, u32 flags, PlatformFont* font, Color col) {
    if (r.IsEmpty() || len(s) == 0) {
        return;
    }
    ScopedTextState st(hdc, col);
    HdcDrawText(hdc, s, r, ToDrawTextFormat(flags), font ? font->GetHFont() : nullptr);
}

void GfxHdc::DrawTextAt(Str s, Point pos, u32 flags, PlatformFont* font, Color col) {
    if (len(s) == 0) {
        return;
    }
    ScopedTextState st(hdc, col);
    HdcDrawText(hdc, s, pos, ToDrawTextFormat(flags), font ? font->GetHFont() : nullptr);
}

Size GfxHdc::MeasureText(Str s, PlatformFont* font) {
    if (len(s) == 0) {
        return {};
    }
    HFONT hf = font ? font->GetHFont() : nullptr;
    if (!hf) {
        // no font given: measure with whatever the surface has selected
        return HdcGetTextExtentPoint32(hdc, s);
    }
    ScopedSelectFont prev(hdc, hf);
    return HdcGetTextExtentPoint32(hdc, s);
}

void GfxHdc::DrawPixmap(Pixmap* px, const Rect& r) {
    if (!px) {
        return;
    }
    BlitPixmapAlpha(px, hdc, r);
}

void GfxHdc::PushClip(const Rect& r) {
    int saved = SaveDC(hdc);
    VecAppend(savedDCs, saved);
    IntersectClipRect(hdc, r.x, r.y, r.Right(), r.Bottom());
}

void GfxHdc::PopClip() {
    int n = len(savedDCs);
    if (n == 0) {
        ReportIf(true);
        return;
    }
    int saved = VecPop(savedDCs);
    RestoreDC(hdc, saved);
}

bool GfxHdc::SetMirrored(bool mirror) {
    DWORD prev = ::SetLayout(hdc, mirror ? LAYOUT_RTL : 0);
    if (prev == GDI_ERROR) {
        return false;
    }
    return (prev & LAYOUT_RTL) != 0;
}

bool gUseDirect2D = true;

// The two implementations that draw the same way (anti-aliased shapes, their
// own text layout), so that flipping gUseDirect2D swaps the backend under
// whatever is painting and the two can be compared. Direct2D only when the OS
// actually has it. The caller owns the result.
Gfx* GfxCreate(HDC hdc) {
    if (gUseDirect2D && Direct2DAvailable()) {
        auto* d2d = new GfxDirect2D(hdc);
        // BindDC fails on a 24-bit DDB memory DC; don't return a no-op painter
        if (d2d->target) {
            return d2d;
        }
        delete d2d;
    }
    return new GfxGdiplus(hdc);
}

void GfxDestroyDoubleBuffer(GfxDoubleBuffer* b) {
    if (!b) {
        return;
    }
    if (b->hdc && b->prevBitmap) {
        SelectObject(b->hdc, b->prevBitmap);
    }
    DeleteObject(b->bitmap);
    DeleteDC(b->hdc);
    b->hdc = nullptr;
    b->bitmap = nullptr;
    b->prevBitmap = nullptr;
    b->dx = 0;
    b->dy = 0;
}

void GfxDestroyDoubleBuffer(HwndBase* w) {
    if (!w) {
        return;
    }
    GfxDoubleBuffer b{};
    b.hdc = w->gfxDoubleBufferHdc;
    b.bitmap = w->gfxDoubleBufferBitmap;
    b.prevBitmap = w->gfxDoubleBufferPrevBitmap;
    b.dx = w->gfxDoubleBufferDx;
    b.dy = w->gfxDoubleBufferDy;
    GfxDestroyDoubleBuffer(&b);
    w->gfxDoubleBufferHdc = b.hdc;
    w->gfxDoubleBufferBitmap = b.bitmap;
    w->gfxDoubleBufferPrevBitmap = b.prevBitmap;
    w->gfxDoubleBufferDx = b.dx;
    w->gfxDoubleBufferDy = b.dy;
}

// Keep one bitmap. A repaint at the same client size reuses it; resizing
// replaces it, and a failed allocation falls back to drawing on hdc.
Gfx* GfxCreateWithDoubleBuffer(HWND hwnd, HDC hdc, GfxDoubleBuffer* b) {
    if (!hwnd || !hdc || !b) {
        return GfxCreate(hdc);
    }

    Size size = HwndClientRect(hwnd).Size();
    if (size.dx != b->dx || size.dy != b->dy) {
        GfxDestroyDoubleBuffer(b);
        b->dx = size.dx;
        b->dy = size.dy;
        if (!size.IsEmpty()) {
            b->hdc = CreateCompatibleDC(hdc);
            // CreateCompatibleDC copies LAYOUT_RTL; keep the DIB LTR (issue #6113)
            SetLayout(b->hdc, 0);
            // 32-bit DIB: Direct2D BindDC rejects a 24-bit DDB from CreateCompatibleBitmap
            b->bitmap = CreateMemoryBitmap(size);
            if (b->hdc && b->bitmap) {
                b->prevBitmap = SelectObject(b->hdc, b->bitmap);
            }
            if (!b->hdc || !b->bitmap || !b->prevBitmap) {
                GfxDestroyDoubleBuffer(b);
                b->dx = size.dx;
                b->dy = size.dy;
            }
        }
    }

    if (!b->hdc) {
        return GfxCreate(hdc);
    }
    SetLayout(b->hdc, 0);
    SetBkMode(b->hdc, TRANSPARENT);
    Gfx* gfx = GfxCreate(b->hdc);
    gfx->doubleBufferTarget = hdc;
    gfx->doubleBufferSource = b->hdc;
    gfx->doubleBufferSize = size;
    return gfx;
}

Gfx* GfxCreateWithDoubleBuffer(HwndBase* w, HDC hdc) {
    if (!w || !w->hwnd) {
        return GfxCreate(hdc);
    }
    GfxDoubleBuffer b{};
    b.hdc = w->gfxDoubleBufferHdc;
    b.bitmap = w->gfxDoubleBufferBitmap;
    b.prevBitmap = w->gfxDoubleBufferPrevBitmap;
    b.dx = w->gfxDoubleBufferDx;
    b.dy = w->gfxDoubleBufferDy;
    Gfx* gfx = GfxCreateWithDoubleBuffer(w->hwnd, hdc, &b);
    w->gfxDoubleBufferHdc = b.hdc;
    w->gfxDoubleBufferBitmap = b.bitmap;
    w->gfxDoubleBufferPrevBitmap = b.prevBitmap;
    w->gfxDoubleBufferDx = b.dx;
    w->gfxDoubleBufferDy = b.dy;
    return gfx;
}
