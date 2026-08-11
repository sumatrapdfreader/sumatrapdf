/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

#include "wingui/PlatformFont.h"


using Gdiplus::Font;
using Gdiplus::Ok;
using Gdiplus::Status;

// the Graphics used for font metrics doesn't draw anything, so its bitmap can
// be tiny
constexpr int kMeasureBmpDx = 32;
constexpr int kMeasureBmpDy = 4;

static Gdiplus::FontStyle ToGdiPlusFontStyle(PlatformFontStyle style) {
    return (Gdiplus::FontStyle)(int)style;
}

// gdiplus is what lays out and draws our ebook text, so the font is created
// there and the HFONT (needed by the gdi text renderers) is derived from it
bool PlatformFontCreateNative(PlatformFont* f) {
    Gdiplus::FontStyle style = ToGdiPlusFontStyle(f->style);
    // gdiplus wants a NUL-terminated WCHAR*
    TempWStr nameW = ToWStrTemp(f->name);
    Font* font = new Font(nameW.s, f->sizePt, style);
    if (font->GetLastStatus() != Ok) {
        delete font;
        font = new Font(L"Times New Roman", f->sizePt, style);
        if (font->GetLastStatus() != Ok) {
            delete font;
            return false;
        }
    }
    f->gdiFont = font;
    return true;
}

HFONT PlatformFont::GetHFont() {
    if (hfont) {
        return hfont;
    }
    if (!gdiFont) {
        return nullptr;
    }
    // TODO: Graphics is probably only used for metrics, so this might not be
    // 100% correct (e.g. 2 monitors with different DPIs?) but the previous code
    // wasn't much better
    // a bitmap-backed Graphics, like the one text is measured with. This runs
    // once per font and can run on any thread, so it isn't worth caching
    u8 data[kMeasureBmpDx * kMeasureBmpDy * 4]{};
    Gdiplus::Bitmap bmp(kMeasureBmpDx, kMeasureBmpDy, kMeasureBmpDx * 4, PixelFormat32bppARGB, data);
    Gdiplus::Graphics gfx((Gdiplus::Image*)&bmp);
    InitGraphicsMode(&gfx);
    LOGFONTW lf;
    Status status = gdiFont->GetLogFontW(&gfx, &lf);
    ReportIf(status != Ok);
    hfont = CreateFontIndirectW(&lf);
    ReportIf(!hfont);
    return hfont;
}

// the app's UI fonts are HFONTs created from system metrics rather than from a
// (name, size, style) description. They live until the process exits, so they
// can be interned the same way
PlatformFont* GetPlatformFont(HFONT hfont) {
    if (!hfont) {
        return nullptr;
    }
    LOGFONTW lf{};
    if (GetObjectW(hfont, sizeof(lf), &lf) == 0) {
        return nullptr;
    }
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (lf.lfWeight >= FW_BOLD) {
        style = style | PlatformFontStyle::Bold;
    }
    if (lf.lfItalic) {
        style = style | PlatformFontStyle::Italic;
    }
    if (lf.lfUnderline) {
        style = style | PlatformFontStyle::Underline;
    }
    if (lf.lfStrikeOut) {
        style = style | PlatformFontStyle::Strikeout;
    }
    // lfHeight is in pixels (negative = character height); gdiplus sizes are in
    // points at 96 dpi, which is what the rest of the font cache is keyed on
    int dyPx = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
    float sizePt = (float)dyPx * 72.f / 96.f;
    PlatformFont* font = GetPlatformFont(ToUtf8Temp(WStr(lf.lfFaceName)), sizePt, style);
    if (font && !font->hfont) {
        // prefer the caller's HFONT: it is the one the rest of the UI draws
        // with, so text measured here matches text drawn by the win32 controls
        font->hfont = hfont;
    }
    return font;
}

// derived from the font's own HFONT rather than from (name, size, Bold): an
// adopted UI font's point size is reconstructed from its LOGFONT, so going
// through gdiplus again could land on slightly different metrics
PlatformFont* GetBoldPlatformFont(PlatformFont* f) {
    if (!f) {
        return nullptr;
    }
    if (f->boldVariant) {
        return f->boldVariant;
    }
    if ((int)f->style & (int)PlatformFontStyle::Bold) {
        f->boldVariant = f;
        return f;
    }
    f->boldVariant = f; // fall back to the font itself if anything below fails
    HFONT hf = f->GetHFont();
    if (!hf) {
        return f;
    }
    LOGFONTW lf{};
    if (GetObjectW(hf, sizeof(lf), &lf) == 0) {
        return f;
    }
    lf.lfWeight = FW_BOLD;
    HFONT bold = CreateFontIndirectW(&lf);
    if (!bold) {
        return f;
    }
    PlatformFont* res = GetPlatformFont(bold);
    if (res) {
        f->boldVariant = res;
    }
    return f->boldVariant;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (len(s) == 0) {
        return {};
    }
    HFONT hf = font ? font->GetHFont() : nullptr;
    AutoReleaseDC dc(nullptr);
    uint fmt = DT_LEFT | DT_NOPREFIX;
    if (maxDx < 0) {
        maxDx = 4096;
        fmt |= DT_NOCLIP;
    } else {
        fmt |= DT_WORDBREAK;
    }
    return HdcMeasureText(dc, s, maxDx, fmt, hf);
}

int PlatformFontLineHeight(PlatformFont* font) {
    HFONT hf = font ? font->GetHFont() : nullptr;
    AutoReleaseDC dc(nullptr);
    ScopedSelectFont prev(dc, hf);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    return (int)(tm.tmHeight + tm.tmExternalLeading);
}
