/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/PlatformFont.h"

using Gdiplus::Font;
using Gdiplus::Ok;
using Gdiplus::Status;

PlatformFont* GetPlatformFontForNative(Str name, float sizePt, PlatformFontStyle style, uintptr_t nativeId);

// the Graphics used for font metrics doesn't draw anything, so its bitmap can
// be tiny
constexpr int kMeasureBmpDx = 32;
constexpr int kMeasureBmpDy = 4;

constexpr u16 kFontFlagItalic = 0x01;
constexpr u16 kFontFlagBold = 0x02;

struct CreatedFontInfo {
    CreatedFontInfo* next = nullptr;
    Str name;
    HFONT font = nullptr;
    u16 size = 0;
    u16 flags = 0;
};

static CreatedFontInfo* gFonts = nullptr;

static CreatedFontInfo* FindCreatedFont(Str name, int size, u16 flags) {
    for (CreatedFontInfo* font = gFonts; font; font = font->next) {
        if (font->size == (u16)size && font->flags == flags && str::Eq(font->name, name)) {
            return font;
        }
    }
    return nullptr;
}

static HFONT RememberCreatedFont(HFONT font, Str name, int size, u16 flags) {
    auto* created = new CreatedFontInfo();
    created->name = str::Dup(name);
    created->font = font;
    created->size = (u16)size;
    created->flags = flags;
    ListInsertFront(&gFonts, created);
    return font;
}

void DeleteCreatedFonts() {
    CreatedFontInfo* font = gFonts;
    while (font) {
        auto* next = font->next;
        str::Free(font->name);
        DeleteFont(font->font);
        delete font;
        font = next;
    }
    gFonts = nullptr;
}

PlatformFont* HdcCreateSimpleFont(HDC hdc, Str fontName, int fontSizePt) {
    int realSize = MulDiv(fontSizePt, GetDeviceCaps(hdc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
    u16 flags = 0;
    auto* font = FindCreatedFont(fontName, realSize, flags);
    if (font) {
        return GetPlatformFont(font->font);
    }

    LOGFONTW lf{};
    lf.lfHeight = -realSize;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH;
    wstr::BufSet(WStr(lf.lfFaceName, dimof(lf.lfFaceName)), ToWStrTemp(fontName));
    lf.lfWeight = FW_DONTCARE;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    HFONT res = CreateFontIndirectW(&lf);
    return GetPlatformFont(RememberCreatedFont(res, fontName, realSize, flags));
}

PlatformFont* GetDefaultGuiFontOfSize(int size) {
    auto* font = FindCreatedFont(Str(), size, 0);
    if (font) {
        return GetPlatformFont(font->font);
    }

    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    ncm.lfMessageFont.lfHeight = -size;
    HFONT res = CreateFontIndirectW(&ncm.lfMessageFont);
    return GetPlatformFont(RememberCreatedFont(res, Str(), size, 0));
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = Str();
    }
    u16 flags = (bold ? kFontFlagBold : 0) | (italic ? kFontFlagItalic : 0);
    auto* font = FindCreatedFont(fontName, size, flags);
    if (font) {
        return GetPlatformFont(font->font);
    }

    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    if (len(fontName) > 0) {
        wstr::BufSet(WStr(ncm.lfMessageFont.lfFaceName, dimof(ncm.lfMessageFont.lfFaceName)), ToWStrTemp(fontName));
    }
    ncm.lfMessageFont.lfHeight = -size;
    if (bold) {
        ncm.lfMessageFont.lfWeight = FW_BOLD;
    }
    if (italic) {
        ncm.lfMessageFont.lfItalic = TRUE;
    }
    HFONT res = CreateFontIndirectW(&ncm.lfMessageFont);
    return GetPlatformFont(RememberCreatedFont(res, fontName, size, flags));
}

PlatformFont* GetDefaultGuiFont(bool bold, bool italic) {
    u16 flags = (bold ? kFontFlagBold : 0) | (italic ? kFontFlagItalic : 0);
    NONCLIENTMETRICS ncm{};
    if (!GetNonClientMetricsForDpi(DpiGet(), &ncm)) {
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    }
    int size = (int)std::abs(ncm.lfMessageFont.lfHeight);
    auto* font = FindCreatedFont(Str(), size, flags);
    if (font) {
        return GetPlatformFont(font->font);
    }
    if (bold) {
        ncm.lfMessageFont.lfWeight = FW_BOLD;
    }
    if (italic) {
        ncm.lfMessageFont.lfItalic = TRUE;
    }
    HFONT res = CreateFontIndirectW(&ncm.lfMessageFont);
    return GetPlatformFont(RememberCreatedFont(res, Str(), size, flags));
}

PlatformFont* GetScaledPlatformFont(PlatformFont* font, int percent) {
    if (!font || percent <= 0) {
        return nullptr;
    }
    HFONT hfont = font->GetHFont();
    LOGFONTW lf{};
    if (!hfont || GetObjectW(hfont, sizeof(lf), &lf) == 0) {
        return font;
    }
    lf.lfHeight = MulDiv(lf.lfHeight, percent, 100);
    Str name = ToUtf8Temp(WStr(lf.lfFaceName));
    u16 flags = (lf.lfWeight >= FW_BOLD ? kFontFlagBold : 0) | (lf.lfItalic ? kFontFlagItalic : 0);
    auto* cached = FindCreatedFont(name, std::abs(lf.lfHeight), flags);
    if (cached) {
        return GetPlatformFont(cached->font);
    }
    HFONT scaled = CreateFontIndirectW(&lf);
    if (!scaled) {
        return font;
    }
    return GetPlatformFont(RememberCreatedFont(scaled, name, std::abs(lf.lfHeight), flags));
}

static Gdiplus::FontStyle ToGdiPlusFontStyle(PlatformFontStyle style) {
    return (Gdiplus::FontStyle)(int)style;
}

// Gdiplus::Font(HDC, HFONT) matches the HFONT by enumerating every installed
// font, which dominates tab-bar startup when the catalog is large. Build from
// the realized family name instead (once, cached on PlatformFont).
static Font* GdiplusFontFromHfont(HFONT hf, float sizePt, PlatformFontStyle style) {
    if (!hf || sizePt <= 0) {
        return nullptr;
    }
    LOGFONTW lf{};
    if (GetObjectW(hf, sizeof(lf), &lf) == 0) {
        return nullptr;
    }
    WCHAR family[LF_FACESIZE]{};
    wstr::BufSet(WStr(family, dimof(family)), WStr(lf.lfFaceName));
    HDC dc = CreateCompatibleDC(nullptr);
    if (dc) {
        HGDIOBJ prev = SelectObject(dc, hf);
        UINT cb = GetOutlineTextMetricsW(dc, 0, nullptr);
        if (cb >= sizeof(OUTLINETEXTMETRICW)) {
            auto* otm = (OUTLINETEXTMETRICW*)AllocArrayTemp<u8>((int)cb);
            otm->otmSize = cb;
            if (GetOutlineTextMetricsW(dc, cb, otm) && otm->otmpFamilyName) {
                const WCHAR* name = (const WCHAR*)((const u8*)otm + (uintptr_t)otm->otmpFamilyName);
                if (name[0]) {
                    wstr::BufSet(WStr(family, dimof(family)), WStr(name));
                }
            }
        }
        SelectObject(dc, prev);
        DeleteDC(dc);
    }
    Gdiplus::FontStyle gpStyle = ToGdiPlusFontStyle(style);
    Font* font = new Font(family, sizePt, gpStyle);
    if (font->GetLastStatus() != Ok) {
        delete font;
        return nullptr;
    }
    return font;
}

// gdiplus is what lays out and draws our ebook text, so the font is created
// there and the HFONT (needed by the gdi text renderers) is derived from it
bool PlatformFontCreateNative(PlatformFont* f) {
    if (f->nativeId) {
        f->hfont = (HFONT)f->nativeId;
        f->gdiFont = GdiplusFontFromHfont(f->hfont, f->sizePt, f->style);
        return true;
    }
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

void PlatformFontDestroyNative(PlatformFont* font) {
    delete font->gdiFont;
    font->gdiFont = nullptr;
    // adopted UI HFONTs (nativeId) are owned by the creator, not us
    if (font->hfont && font->nativeId == 0) {
        DeleteFont(font->hfont);
        font->hfont = nullptr;
    }
}

void PlatformFontShutdownNative() {}

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
    return GetPlatformFontForNative(ToUtf8Temp(WStr(lf.lfFaceName)), sizePt, style, (uintptr_t)hfont);
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
