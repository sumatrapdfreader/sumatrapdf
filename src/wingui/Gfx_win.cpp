/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"

Gfx GfxFromHdc(HDC hdc) {
    Gfx gfx;
    gfx.hdc = hdc;
    return gfx;
}

void GfxFillRect(Gfx* gfx, const Rect& r, COLORREF col) {
    if (col == kColorUnset || r.IsEmpty()) {
        return;
    }
    HdcFillRect(gfx->hdc, r, col);
}

// kColorUnset draws in the surface's current text color, which is how the
// underline under a VirtWndText picks up the color the text was drawn in
void GfxDrawLine(Gfx* gfx, const Rect& r, COLORREF col, int thickness) {
    if (col == kColorUnset) {
        col = GetTextColor(gfx->hdc);
    }
    Rect r2 = r;
    if (r2.dy == 0) {
        r2.dy = thickness;
    } else if (r2.dx == 0) {
        r2.dx = thickness;
    }
    HdcFillRect(gfx->hdc, r2, col);
}

static uint ToDrawTextFormat(u32 flags) {
    uint fmt = DT_NOPREFIX;
    if (flags & gfxTextEllipsis) {
        fmt |= DT_END_ELLIPSIS | DT_SINGLELINE | DT_VCENTER;
    } else {
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

void GfxDrawText(Gfx* gfx, Str s, const Rect& r, u32 flags, PlatformFont* font, COLORREF col) {
    if (r.IsEmpty() || len(s) == 0) {
        return;
    }
    HDC hdc = gfx->hdc;
    COLORREF prevCol = kColorUnset;
    if (col != kColorUnset) {
        prevCol = SetTextColor(hdc, col);
    }
    int prevBkMode = SetBkMode(hdc, TRANSPARENT);
    HdcDrawText(hdc, s, r, ToDrawTextFormat(flags), font ? font->GetHFont() : nullptr);
    if (col != kColorUnset) {
        SetTextColor(hdc, prevCol);
    }
    if (prevBkMode != 0) {
        SetBkMode(hdc, prevBkMode);
    }
}

void GfxDrawPixmap(Gfx* gfx, Pixmap* px, const Rect& r) {
    if (!px) {
        return;
    }
    BlitPixmapAlpha(px, gfx->hdc, r);
}
