/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The gdiplus implementation of Gfx. Everything is anti-aliased and text is
// laid out by gdiplus, so it looks the same as the parts of the app that
// already drew with gdiplus (tabs, close buttons, the selection toolbar).
//
// It still draws into an HDC, because that is how gdiplus reaches a window, but
// the HDC never leaves this file: no caller does gdi drawing on it.
//
// Caveat: our layout code measures text with gdi metrics
// (PlatformFontMeasureText -> DrawText), and gdiplus lays the same string out a
// little wider. A control sized to exactly fit its label therefore hands us a
// rect the text doesn't quite fit in. kGdiplusTextSlack absorbs the usual case;
// the longest labels can still end up with an "…" that the gdi implementation
// wouldn't add. Measuring through Gfx::MeasureText() would remove the mismatch.

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;
using Gdiplus::StringFormat;

GfxGdiplus::GfxGdiplus(HDC hdc) {
    this->hdc = hdc;
    gfx = new Graphics(hdc);
    ownsGfx = true;
    InitGraphicsMode(gfx);
    textColor = GetTextColor(hdc);
}

GfxGdiplus::GfxGdiplus(Graphics* g) {
    gfx = g;
    ownsGfx = false;
}

GfxGdiplus::~GfxGdiplus() {
    if (ownsGfx) {
        delete gfx;
    }
}

// how much wider than its rect a string may lay out before it is ellipsized;
// covers the gdi-measure / gdiplus-draw mismatch, not a real overflow
constexpr int kGdiplusTextSlack = 4;

static Gdiplus::Color ToGdipColor(Color col, u8 alpha = 255) {
    return {alpha, GetRValue(col), GetGValue(col), GetBValue(col)};
}

void GfxGdiplus::FillRect(const Rect& r, Color col) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    SolidBrush br(ToGdipColor(col));
    // a solid fill has nothing to anti-alias and half-covered edge pixels would
    // let the old content show through
    Gdiplus::SmoothingMode prev = gfx->GetSmoothingMode();
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx->FillRectangle(&br, ToGdipRect(r));
    gfx->SetSmoothingMode(prev);
}

void GfxGdiplus::FillRects(const Rect* rects, int count, Color col, u8 alpha, int outlineWidth) {
    if (ColorSkipsPaint(col) || count <= 0) {
        return;
    }
    GraphicsPath path(Gdiplus::FillModeWinding);
    for (int i = 0; i < count; i++) {
        if (!rects[i].IsEmpty()) {
            path.AddRectangle(ToGdipRect(rects[i]));
        }
    }
    SolidBrush brush(ToGdipColor(col, alpha));
    gfx->FillPath(&brush, &path);
    if (outlineWidth > 0) {
        path.Outline(nullptr, 0.2f);
        Pen pen(ToGdipColor(kColBlack, alpha), (float)outlineWidth);
        gfx->DrawPath(&pen, &path);
    }
}

void GfxGdiplus::DrawRect(const Rect& r, Color col, int thickness) {
    if (ColorSkipsPaint(col) || r.IsEmpty() || thickness < 1) {
        return;
    }
    Pen pen(ToGdipColor(col), (float)thickness);
    // gdiplus centers the stroke on the path, so inset by half of it to keep
    // the whole outline inside the rect
    float half = (float)thickness / 2.0f;
    Gdiplus::RectF rf((float)r.x + half, (float)r.y + half, (float)r.dx - (float)thickness,
                      (float)r.dy - (float)thickness);
    Gdiplus::SmoothingMode prev = gfx->GetSmoothingMode();
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx->DrawRectangle(&pen, rf);
    gfx->SetSmoothingMode(prev);
}

void GfxGdiplus::DrawDashedRect(const Rect& r, Color col) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    Pen pen(ToGdipColor(col));
    pen.SetDashStyle(Gdiplus::DashStyleDash);
    Gdiplus::SmoothingMode prev = gfx->GetSmoothingMode();
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx->DrawRectangle(&pen, r.x, r.y, r.dx, r.dy);
    gfx->SetSmoothingMode(prev);
}

// `d` is the diameter of the corner circles (see the same helper in Gfx_win.cpp)
static void AddRoundedRectPath(GraphicsPath& path, const Rect& rc, int d) {
    path.AddArc(rc.x, rc.y, d, d, 180, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y, d, d, 270, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y + rc.dy - d - 1, d, d, 0, 90);
    path.AddArc(rc.x, rc.y + rc.dy - d - 1, d, d, 90, 90);
    path.CloseFigure();
}

void GfxGdiplus::FillRoundedRect(const Rect& r, int radius, Color fill, Color border) {
    if (r.IsEmpty()) {
        return;
    }
    if (radius <= 0) {
        FillRect(r, fill);
        DrawRect(r, border);
        return;
    }
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    GraphicsPath path;
    AddRoundedRectPath(path, r, radius);
    if (!ColorSkipsPaint(fill)) {
        SolidBrush br(ToGdipColor(fill));
        gfx->FillPath(&br, &path);
    }
    if (!ColorSkipsPaint(border)) {
        Pen pen(ToGdipColor(border), 1);
        gfx->DrawPath(&pen, &path);
    }
}

void GfxGdiplus::FillEllipse(const Rect& r, Color col, u8 alpha) {
    if (ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    SolidBrush br(ToGdipColor(col, alpha));
    gfx->FillEllipse(&br, r.x, r.y, r.dx - 1, r.dy - 1);
}

void GfxGdiplus::DrawLine(const Rect& r, Color col, int thickness) {
    if (col == kColorUnset) {
        col = textColor;
    }
    Rect r2 = r;
    if (r2.dy == 0) {
        r2.dy = thickness;
    } else if (r2.dx == 0) {
        r2.dx = thickness;
    }
    // a horizontal / vertical line is a thin rect: drawn as a fill it lands on
    // whole pixels instead of being smeared over two rows
    FillRect(r2, col);
}

void GfxGdiplus::DrawLineAA(Point p1, Point p2, Color col, float thickness, u8 alpha) {
    if (col == kColorUnset) {
        col = textColor;
    }
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Pen pen(ToGdipColor(col, alpha), thickness);
    gfx->DrawLine(&pen, p1.x, p1.y, p2.x, p2.y);
}

void GfxGdiplus::DrawFocusRect(const Rect& r) {
    if (r.IsEmpty()) {
        return;
    }
    Pen pen(ToGdipColor(textColor));
    pen.SetDashStyle(Gdiplus::DashStyleDot);
    Gdiplus::SmoothingMode prev = gfx->GetSmoothingMode();
    gfx->SetSmoothingMode(Gdiplus::SmoothingModeNone);
    gfx->DrawRectangle(&pen, r.x, r.y, r.dx - 1, r.dy - 1);
    gfx->SetSmoothingMode(prev);
}

// Built on GenericTypographic, not GenericDefault: the default format pads the
// layout rect by about a sixth of an em on each side, so a rect measured to fit
// the text (our layout code measures with gdi metrics) came up short and every
// label was clipped or ellipsized.
static void InitStringFormat(StringFormat& sf, u32 flags) {
    int ff = 0;
    bool wrap = (flags & gfxTextWrap) != 0;
    // gdiplus wraps to the layout rect by default; gdi's DrawText only does it
    // when asked, and that is the behavior the flags describe
    if (!wrap) {
        ff |= Gdiplus::StringFormatFlagsNoWrap;
    }
    // text that can't be ellipsized isn't clipped either, same as the gdi side
    bool ellipsis = (flags & (gfxTextEllipsis | gfxTextPathEllipsis)) != 0;
    if ((!ellipsis && !wrap) || (flags & gfxTextNoClip)) {
        ff |= Gdiplus::StringFormatFlagsNoClip;
    }
    if (flags & gfxTextRtl) {
        ff |= Gdiplus::StringFormatFlagsDirectionRightToLeft;
    }
    sf.SetFormatFlags(ff);

    if (flags & gfxTextPathEllipsis) {
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisPath);
    } else if (flags & gfxTextEllipsis) {
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    } else {
        sf.SetTrimming(Gdiplus::StringTrimmingNone);
    }

    if (flags & gfxTextCenter) {
        sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    } else if (flags & gfxTextRight) {
        sf.SetAlignment(Gdiplus::StringAlignmentFar);
    } else {
        sf.SetAlignment(Gdiplus::StringAlignmentNear);
    }
    bool vcenter = !wrap && (flags & gfxTextVCenter) != 0;
    sf.SetLineAlignment(vcenter ? Gdiplus::StringAlignmentCenter : Gdiplus::StringAlignmentNear);
}

// A PlatformFont describes itself with gdiplus unless it was adopted from one
// of the app's HFONTs, in which case the gdiplus font has to be derived from
// that HFONT. Sets *owned when the caller has to free the result.
Gdiplus::Font* GfxGdiplus::GetGdiplusFont(PlatformFont* font, bool* owned) {
    *owned = false;
    Gdiplus::Font* f = font ? font->GetGdiplusFont() : nullptr;
    if (f) {
        return f;
    }
    // no font (or one adopted from an HFONT): fall back to what the dc has
    // selected, which is what the gdi implementation draws with
    HFONT hf = font ? font->GetHFont() : nullptr;
    if (!hf && hdc) {
        hf = (HFONT)GetCurrentObject(hdc, OBJ_FONT);
    }
    if (!hf || !hdc) {
        return nullptr;
    }
    f = new Gdiplus::Font(hdc, hf);
    if (f->GetLastStatus() != Gdiplus::Ok) {
        delete f;
        return nullptr;
    }
    *owned = true;
    return f;
}

void GfxGdiplus::DrawText(Str s, const Rect& r, u32 flags, PlatformFont* font, Color col) {
    if (r.IsEmpty() || len(s) == 0) {
        return;
    }
    bool ownsFont = false;
    Gdiplus::Font* f = GetGdiplusFont(font, &ownsFont);
    if (!f) {
        return;
    }
    StringFormat sf(StringFormat::GenericTypographic());
    InitStringFormat(sf, flags);
    TempWStr ws = ToWStrTemp(s);

    bool trims = (flags & (gfxTextEllipsis | gfxTextPathEllipsis)) && !(flags & gfxTextWrap);
    if (trims) {
        // the layout that produced `r` measured with gdi metrics, which run a
        // hair narrower than gdiplus lays the same string out. Without the
        // slack a label in a rect measured to fit it exactly gets an "…" anyway
        RectF bbox = MeasureTextAccurate(gfx, f, ws);
        if ((int)bbox.dx <= r.dx + kGdiplusTextSlack) {
            sf.SetTrimming(Gdiplus::StringTrimmingNone);
            sf.SetFormatFlags(sf.GetFormatFlags() | Gdiplus::StringFormatFlagsNoClip);
        }
    }

    SolidBrush br(ToGdipColor(col == kColorUnset ? textColor : col));
    gfx->DrawString(ws.s, ws.len, f, ToGdipRectF(r), &sf, &br);
    if (ownsFont) {
        delete f;
    }
}

void GfxGdiplus::DrawTextAt(Str s, Point pos, u32 flags, PlatformFont* font, Color col) {
    if (len(s) == 0) {
        return;
    }
    bool ownsFont = false;
    Gdiplus::Font* f = GetGdiplusFont(font, &ownsFont);
    if (!f) {
        return;
    }
    // the typographic format has no side bearing, so the glyphs start exactly
    // at the point (GenericDefault insets them by about a sixth of an em)
    StringFormat sf(StringFormat::GenericTypographic());
    int ff = Gdiplus::StringFormatFlagsNoWrap | Gdiplus::StringFormatFlagsNoClip;
    if (flags & gfxTextRtl) {
        ff |= Gdiplus::StringFormatFlagsDirectionRightToLeft;
    }
    sf.SetFormatFlags(ff);
    SolidBrush br(ToGdipColor(col == kColorUnset ? textColor : col));
    TempWStr ws = ToWStrTemp(s);
    gfx->DrawString(ws.s, ws.len, f, Gdiplus::PointF((float)pos.x, (float)pos.y), &sf, &br);
    if (ownsFont) {
        delete f;
    }
}

Size GfxGdiplus::MeasureText(Str s, PlatformFont* font) {
    if (len(s) == 0) {
        return {};
    }
    bool ownsFont = false;
    Gdiplus::Font* f = GetGdiplusFont(font, &ownsFont);
    if (!f) {
        return {};
    }
    RectF bbox = MeasureTextAccurate(gfx, f, ToWStrTemp(s));
    if (ownsFont) {
        delete f;
    }
    return {(int)lroundf(bbox.dx), (int)lroundf(bbox.dy)};
}

void GfxGdiplus::DrawPixmap(Pixmap* px, const Rect& r) {
    if (!px || r.IsEmpty()) {
        return;
    }
    Gdiplus::Bitmap* bmp = WrapPixmapGdiplus(px);
    if (!bmp) {
        return;
    }
    gfx->DrawImage(bmp, ToGdipRect(r), 0, 0, px->width, px->height, Gdiplus::UnitPixel);
    delete bmp;
}

void GfxGdiplus::PushClip(const Rect& r) {
    Gdiplus::GraphicsState st = gfx->Save();
    savedStates.Append((u32)st);
    gfx->SetClip(ToGdipRect(r), Gdiplus::CombineModeIntersect);
}

void GfxGdiplus::PopClip() {
    int n = len(savedStates);
    if (n == 0) {
        ReportIf(true);
        return;
    }
    u32 st = savedStates.Pop();
    gfx->Restore((Gdiplus::GraphicsState)st);
}

// gdiplus doesn't pick up the window's orientation from the device context, so
// a gdiplus surface is never mirrored and there is nothing to turn off
bool GfxGdiplus::SetMirrored(bool) {
    return false;
}
