/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The Direct2D / DirectWrite implementation of Gfx.
//
// Like the gdiplus one it draws into an HDC (through an ID2D1DCRenderTarget),
// so it can be dropped into the same places, and the HDC never leaves this
// file. The render target is bound at 96 dpi, so a DIP is a pixel and Gfx
// coordinates go through unscaled.
//
// d2d1.dll and dwrite.dll are loaded by hand rather than imported: a Windows
// that doesn't have them (or has them broken) then falls back to gdiplus
// instead of failing to start the process. Direct2DAvailable() is that check.
//
// Caveat, the same one GfxGdiplus has: our layout code measures text with gdi
// metrics and DirectWrite lays the same string out slightly differently, so a
// control sized to exactly fit its label can hand us a rect the text doesn't
// quite fit in. kTextSlack absorbs the usual case.

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

// windows.h has DrawText as a macro and d2d1.h declares a DrawText method
#ifdef DrawText
#undef DrawText
#endif
#if __has_include(<d2d1.h>) && __has_include(<dwrite.h>)
#define SUMATRA_HAS_D2D 1
#include <d2d1.h>
#include <dwrite.h>
#endif

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"

#if !SUMATRA_HAS_D2D
// mingw-w64 on some distros has no d2d1.h / dwrite.h. GfxCreate() still
// links, and Direct2DAvailable() is false so it always uses GfxGdiplus.
bool Direct2DAvailable() {
    return false;
}
GfxDirect2D::GfxDirect2D(HDC hdc) {
    this->hdc = hdc;
}
GfxDirect2D::~GfxDirect2D() = default;
void GfxDirect2D::FillRect(const Rect&, Color) {}
void GfxDirect2D::FillRects(const Rect*, int, Color, u8, int) {}
void GfxDirect2D::DrawRect(const Rect&, Color, int) {}
void GfxDirect2D::DrawDashedRect(const Rect&, Color) {}
void GfxDirect2D::FillRoundedRect(const Rect&, int, Color, Color) {}
void GfxDirect2D::FillEllipse(const Rect&, Color, u8) {}
void GfxDirect2D::DrawLine(const Rect&, Color, int) {}
void GfxDirect2D::DrawLineAA(Point, Point, Color, float, u8) {}
void GfxDirect2D::DrawFocusRect(const Rect&) {}
void GfxDirect2D::DrawText(Str, const Rect&, u32, PlatformFont*, Color) {}
void GfxDirect2D::DrawTextAt(Str, Point, u32, PlatformFont*, Color) {}
Size GfxDirect2D::MeasureText(Str, PlatformFont*) {
    return {};
}
void GfxDirect2D::DrawPixmap(Pixmap*, const Rect&) {}
void GfxDirect2D::PushClip(const Rect&) {}
void GfxDirect2D::PopClip() {}
bool GfxDirect2D::SetMirrored(bool) {
    return false;
}
ID2D1SolidColorBrush* GfxDirect2D::GetBrush(Color, u8) {
    return nullptr;
}
#else

// how much wider than its rect a string may lay out before it is ellipsized;
// covers the gdi-measure / DirectWrite-draw mismatch, not a real overflow
constexpr int kTextSlack = 4;

//--- loading d2d1.dll / dwrite.dll

typedef HRESULT(WINAPI* Sig_D2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
typedef HRESULT(WINAPI* Sig_DWriteCreateFactory)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);

static bool gTriedInit = false;
static ID2D1Factory* gD2DFactory = nullptr;
static IDWriteFactory* gDWriteFactory = nullptr;

static void InitDirect2D() {
    if (gTriedInit) {
        return;
    }
    gTriedInit = true;

    HMODULE d2d = LoadLibraryW(L"d2d1.dll");
    HMODULE dw = LoadLibraryW(L"dwrite.dll");
    if (!d2d || !dw) {
        logf("InitDirect2D: d2d1.dll=%p dwrite.dll=%p\n", (const void*)d2d, (const void*)dw);
        return;
    }
    auto createD2D = (Sig_D2D1CreateFactory)GetProcAddress(d2d, "D2D1CreateFactory");
    auto createDW = (Sig_DWriteCreateFactory)GetProcAddress(dw, "DWriteCreateFactory");
    if (!createD2D || !createDW) {
        logf("InitDirect2D: no factory entry points\n");
        return;
    }
    HRESULT hr = createD2D(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, (void**)&gD2DFactory);
    if (FAILED(hr)) {
        logf("InitDirect2D: D2D1CreateFactory failed 0x%x\n", (int)hr);
        return;
    }
    hr = createDW(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&gDWriteFactory);
    if (FAILED(hr)) {
        logf("InitDirect2D: DWriteCreateFactory failed 0x%x\n", (int)hr);
        gD2DFactory->Release();
        gD2DFactory = nullptr;
        return;
    }
}

bool Direct2DAvailable() {
    InitDirect2D();
    return gD2DFactory != nullptr && gDWriteFactory != nullptr;
}

//--- text formats
//
// A PlatformFont is interned and lives for the whole run, so the text format
// made from it can be cached against it and never freed. The per-draw settings
// (alignment, wrapping, trimming) are set on each use.

struct CachedTextFormat {
    PlatformFont* font;
    IDWriteTextFormat* format;
    IDWriteInlineObject* ellipsis;
};

static Vec<CachedTextFormat>* gTextFormats = nullptr;

static IDWriteTextFormat* GetTextFormat(PlatformFont* font, IDWriteInlineObject** ellipsisOut) {
    if (!font || !gDWriteFactory) {
        return nullptr;
    }
    if (!gTextFormats) {
        gTextFormats = new Vec<CachedTextFormat>();
    }
    for (CachedTextFormat& c : *gTextFormats) {
        if (c.font == font) {
            *ellipsisOut = c.ellipsis;
            return c.format;
        }
    }

    // derive the family / weight / size from the font's HFONT: that is what the
    // layout measured with, so the two stay as close as they can
    LOGFONTW lf{};
    HFONT hf = font->GetHFont();
    if (!hf || GetObjectW(hf, sizeof(lf), &lf) == 0) {
        return nullptr;
    }
    // lfHeight < 0 is the em size, which is what DirectWrite wants; a positive
    // one is the cell height, close enough to fall back to
    float emSize = (float)(lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight);
    if (emSize <= 0) {
        emSize = font->GetSize() * 96.f / 72.f;
    }
    auto weight = (DWRITE_FONT_WEIGHT)(lf.lfWeight > 0 ? lf.lfWeight : FW_NORMAL);
    DWRITE_FONT_STYLE style = lf.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;

    // lfFaceName is a GDI face name: it can be a registry alias ("MS Shell Dlg"
    // -> Microsoft Sans Serif) or a face DirectWrite folds into a family plus
    // weight ("Arial Black" -> Arial, black). CreateTextFormat() wants a
    // DirectWrite family name and silently falls back to a default font for
    // anything else, which drew the about page and home page tips in the wrong
    // font. Realize the font through GDI (resolves the aliases), then map the
    // LOGFONT through the GDI interop to the real family / weight / style
    HDC dc = CreateCompatibleDC(nullptr);
    if (dc) {
        HGDIOBJ prevFont = SelectObject(dc, hf);
        // GetTextFace() reports the alias itself back, so ask for the outline
        // metrics instead: otmpFamilyName is the physical font GDI realized
        // ("Microsoft Sans Serif" for "MS Shell Dlg")
        UINT cb = GetOutlineTextMetricsW(dc, 0, nullptr);
        if (cb >= sizeof(OUTLINETEXTMETRICW)) {
            auto* otm = (OUTLINETEXTMETRICW*)AllocArrayTemp<u8>((int)cb);
            otm->otmSize = cb;
            if (GetOutlineTextMetricsW(dc, cb, otm) && otm->otmpFamilyName) {
                // the string members are offsets from the start of the struct
                const WCHAR* family = (const WCHAR*)((const u8*)otm + (uintptr_t)otm->otmpFamilyName);
                if (family[0]) {
                    memset(lf.lfFaceName, 0, sizeof(lf.lfFaceName));
                    for (int i = 0; i < LF_FACESIZE - 1 && family[i]; i++) {
                        lf.lfFaceName[i] = family[i];
                    }
                }
            }
        }
        SelectObject(dc, prevFont);
        DeleteDC(dc);
    }
    WCHAR familyName[LF_FACESIZE]{};
    IDWriteGdiInterop* interop = nullptr;
    gDWriteFactory->GetGdiInterop(&interop);
    if (interop) {
        IDWriteFont* dwFont = nullptr;
        interop->CreateFontFromLOGFONT(&lf, &dwFont);
        if (dwFont) {
            IDWriteFontFamily* family = nullptr;
            dwFont->GetFontFamily(&family);
            if (family) {
                IDWriteLocalizedStrings* names = nullptr;
                family->GetFamilyNames(&names);
                if (names) {
                    UINT32 idx = 0;
                    BOOL exists = FALSE;
                    names->FindLocaleName(L"en-us", &idx, &exists);
                    if (!exists) {
                        idx = 0;
                    }
                    names->GetString(idx, familyName, dimof(familyName));
                    names->Release();
                }
                family->Release();
            }
            weight = dwFont->GetWeight();
            style = dwFont->GetStyle();
            stretch = dwFont->GetStretch();
            dwFont->Release();
        }
        interop->Release();
    }
    const WCHAR* familyToUse = familyName[0] ? familyName : lf.lfFaceName;

    IDWriteTextFormat* format = nullptr;
    HRESULT hr = gDWriteFactory->CreateTextFormat(familyToUse, nullptr, weight, style, stretch, emSize, L"", &format);
    if (FAILED(hr) || !format) {
        logf("GetTextFormat: CreateTextFormat('%s') failed 0x%x\n", ToUtf8Temp(WStr(lf.lfFaceName)), (int)hr);
        return nullptr;
    }
    IDWriteInlineObject* ellipsis = nullptr;
    gDWriteFactory->CreateEllipsisTrimmingSign(format, &ellipsis);
    gTextFormats->Append({font, format, ellipsis});
    *ellipsisOut = ellipsis;
    return format;
}

//--- GfxDirect2D

static D2D1_COLOR_F ToD2DColor(Color col, u8 alpha = 255) {
    return D2D1::ColorF((float)GetRValue(col) / 255.0f, (float)GetGValue(col) / 255.0f, (float)GetBValue(col) / 255.0f,
                        (float)alpha / 255.0f);
}

static D2D1_RECT_F ToD2DRect(const Rect& r) {
    return D2D1::RectF((float)r.x, (float)r.y, (float)r.Right(), (float)r.Bottom());
}

// the size of what the dc draws into, so the render target can be bound with
// its origin at the dc's origin and Gfx coordinates go through unchanged
static Size HdcSurfaceSize(HDC hdc) {
    HWND hwnd = WindowFromDC(hdc);
    if (hwnd) {
        return HwndClientRect(hwnd).Size();
    }
    auto bmp = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);
    BITMAP bi{};
    if (bmp && GetObjectW(bmp, sizeof(bi), &bi) != 0) {
        return {bi.bmWidth, bi.bmHeight};
    }
    return {GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES)};
}

GfxDirect2D::GfxDirect2D(HDC hdc) {
    this->hdc = hdc;
    textColor = GetTextColor(hdc);
    if (!Direct2DAvailable()) {
        return;
    }
    Size sz = HdcSurfaceSize(hdc);
    if (sz.dx <= 0 || sz.dy <= 0) {
        return;
    }
    // 96 dpi so a DIP is a pixel; the caller's coordinates are pixels
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f,
        96.0f, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT);
    HRESULT hr = gD2DFactory->CreateDCRenderTarget(&props, &target);
    if (FAILED(hr) || !target) {
        logf("GfxDirect2D: CreateDCRenderTarget failed 0x%x\n", (int)hr);
        target = nullptr;
        return;
    }
    RECT rc = {0, 0, sz.dx, sz.dy};
    hr = target->BindDC(hdc, &rc);
    if (FAILED(hr)) {
        logf("GfxDirect2D: BindDC failed 0x%x\n", (int)hr);
        target->Release();
        target = nullptr;
        return;
    }
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    target->BeginDraw();
    drawing = true;
}

GfxDirect2D::~GfxDirect2D() {
    if (!target) {
        return;
    }
    // an unbalanced PushClip would make EndDraw fail
    while (len(clipDepth) > 0) {
        PopClip();
    }
    if (drawing) {
        HRESULT hr = target->EndDraw();
        if (FAILED(hr)) {
            logf("GfxDirect2D: EndDraw failed 0x%x\n", (int)hr);
        }
    }
    target->Release();
    target = nullptr;
}

// every draw needs a brush and they are cheap to make, but not free; one brush
// re-colored per call is what the d2d samples do
ID2D1SolidColorBrush* GfxDirect2D::GetBrush(Color col, u8 alpha) {
    if (!target) {
        return nullptr;
    }
    if (!brush) {
        HRESULT hr = target->CreateSolidColorBrush(ToD2DColor(col, alpha), &brush);
        if (FAILED(hr)) {
            return nullptr;
        }
        return brush;
    }
    brush->SetColor(ToD2DColor(col, alpha));
    brush->SetOpacity(1.0f);
    return brush;
}

void GfxDirect2D::FillRect(const Rect& r, Color col) {
    if (!target || ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    ID2D1SolidColorBrush* br = GetBrush(col);
    if (!br) {
        return;
    }
    // a solid fill has nothing to anti-alias and half-covered edge pixels
    // would let the old content show through
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->FillRectangle(ToD2DRect(r), br);
}

void GfxDirect2D::FillRects(const Rect* rects, int count, Color col, u8 alpha, int outlineWidth) {
    if (!target || ColorSkipsPaint(col) || count <= 0) {
        return;
    }
    ID2D1PathGeometry* path = nullptr;
    HRESULT hr = gD2DFactory->CreatePathGeometry(&path);
    if (FAILED(hr)) {
        return;
    }
    ID2D1GeometrySink* sink = nullptr;
    hr = path->Open(&sink);
    if (SUCCEEDED(hr)) {
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        for (int i = 0; i < count; i++) {
            const Rect& r = rects[i];
            if (r.IsEmpty()) {
                continue;
            }
            sink->BeginFigure(D2D1::Point2F((float)r.x, (float)r.y), D2D1_FIGURE_BEGIN_FILLED);
            D2D1_POINT_2F points[] = {
                D2D1::Point2F((float)r.Right(), (float)r.y),
                D2D1::Point2F((float)r.Right(), (float)r.Bottom()),
                D2D1::Point2F((float)r.x, (float)r.Bottom()),
            };
            sink->AddLines(points, dimof(points));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
        hr = sink->Close();
        sink->Release();
    }
    if (SUCCEEDED(hr)) {
        target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        ID2D1SolidColorBrush* br = GetBrush(col, alpha);
        if (br) {
            target->FillGeometry(path, br);
        }
        if (outlineWidth > 0) {
            br = GetBrush(kColBlack, alpha);
            if (br) {
                target->DrawGeometry(path, br, (float)outlineWidth);
            }
        }
    }
    path->Release();
}

void GfxDirect2D::DrawRect(const Rect& r, Color col, int thickness) {
    if (!target || ColorSkipsPaint(col) || r.IsEmpty() || thickness < 1) {
        return;
    }
    ID2D1SolidColorBrush* br = GetBrush(col);
    if (!br) {
        return;
    }
    // d2d centers the stroke on the path, so inset by half of it to keep the
    // whole outline inside the rect
    float half = (float)thickness / 2.0f;
    D2D1_RECT_F rf =
        D2D1::RectF((float)r.x + half, (float)r.y + half, (float)r.Right() - half, (float)r.Bottom() - half);
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->DrawRectangle(rf, br, (float)thickness);
}

void GfxDirect2D::DrawDashedRect(const Rect& r, Color col) {
    if (!target || ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    ID2D1SolidColorBrush* br = GetBrush(col);
    if (!br) {
        return;
    }
    D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties();
    props.dashStyle = D2D1_DASH_STYLE_DASH;
    ID2D1StrokeStyle* stroke = nullptr;
    gD2DFactory->CreateStrokeStyle(props, nullptr, 0, &stroke);
    D2D1_RECT_F rf = D2D1::RectF((float)r.x, (float)r.y, (float)r.Right(), (float)r.Bottom());
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->DrawRectangle(rf, br, 1.0f, stroke);
    if (stroke) {
        stroke->Release();
    }
}

void GfxDirect2D::FillRoundedRect(const Rect& r, int radius, Color fill, Color border) {
    if (!target || r.IsEmpty()) {
        return;
    }
    if (radius <= 0) {
        FillRect(r, fill);
        DrawRect(r, border);
        return;
    }
    // the callers pass the corner circles' diameter (see the gdi/gdiplus
    // implementations), d2d wants the radii
    float rad = (float)radius / 2.0f;
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    if (!ColorSkipsPaint(fill)) {
        ID2D1SolidColorBrush* br = GetBrush(fill);
        if (br) {
            target->FillRoundedRectangle(D2D1::RoundedRect(ToD2DRect(r), rad, rad), br);
        }
    }
    if (!ColorSkipsPaint(border)) {
        ID2D1SolidColorBrush* br = GetBrush(border);
        if (br) {
            D2D1_RECT_F rf =
                D2D1::RectF((float)r.x + 0.5f, (float)r.y + 0.5f, (float)r.Right() - 0.5f, (float)r.Bottom() - 0.5f);
            target->DrawRoundedRectangle(D2D1::RoundedRect(rf, rad, rad), br, 1.0f);
        }
    }
}

void GfxDirect2D::FillEllipse(const Rect& r, Color col, u8 alpha) {
    if (!target || ColorSkipsPaint(col) || r.IsEmpty()) {
        return;
    }
    ID2D1SolidColorBrush* br = GetBrush(col, alpha);
    if (!br) {
        return;
    }
    float rx = (float)(r.dx - 1) / 2.0f;
    float ry = (float)(r.dy - 1) / 2.0f;
    D2D1_ELLIPSE e = D2D1::Ellipse(D2D1::Point2F((float)r.x + rx, (float)r.y + ry), rx, ry);
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->FillEllipse(e, br);
}

void GfxDirect2D::DrawLine(const Rect& r, Color col, int thickness) {
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

void GfxDirect2D::DrawLineAA(Point p1, Point p2, Color col, float thickness, u8 alpha) {
    if (!target) {
        return;
    }
    if (col == kColorUnset) {
        col = textColor;
    }
    ID2D1SolidColorBrush* br = GetBrush(col, alpha);
    if (!br) {
        return;
    }
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->DrawLine(D2D1::Point2F((float)p1.x, (float)p1.y), D2D1::Point2F((float)p2.x, (float)p2.y), br, thickness);
}

void GfxDirect2D::DrawFocusRect(const Rect& r) {
    if (!target || r.IsEmpty()) {
        return;
    }
    ID2D1SolidColorBrush* br = GetBrush(textColor);
    if (!br) {
        return;
    }
    if (!dottedStroke) {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties();
        props.dashStyle = D2D1_DASH_STYLE_DOT;
        gD2DFactory->CreateStrokeStyle(props, nullptr, 0, &dottedStroke);
    }
    D2D1_RECT_F rf =
        D2D1::RectF((float)r.x + 0.5f, (float)r.y + 0.5f, (float)r.Right() - 0.5f, (float)r.Bottom() - 0.5f);
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    target->DrawRectangle(rf, br, 1.0f, dottedStroke);
}

// the per-draw half of the text format: alignment, wrapping and trimming
static void SetTextFormatFlags(IDWriteTextFormat* format, IDWriteInlineObject* ellipsis, u32 flags) {
    bool wrap = (flags & gfxTextWrap) != 0;
    format->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);

    if (flags & gfxTextCenter) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    } else if (flags & gfxTextRight) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    } else {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    bool vcenter = !wrap && (flags & gfxTextVCenter) != 0;
    format->SetParagraphAlignment(vcenter ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    format->SetReadingDirection((flags & gfxTextRtl) ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
                                                     : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);

    DWRITE_TRIMMING trimming{};
    bool ellipsize = (flags & (gfxTextEllipsis | gfxTextPathEllipsis)) != 0;
    if (ellipsize && ellipsis) {
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
        format->SetTrimming(&trimming, ellipsis);
    } else {
        trimming.granularity = DWRITE_TRIMMING_GRANULARITY_NONE;
        format->SetTrimming(&trimming, nullptr);
    }
}

void GfxDirect2D::DrawText(Str s, const Rect& r, u32 flags, PlatformFont* font, Color col) {
    if (!target || r.IsEmpty() || len(s) == 0) {
        return;
    }
    IDWriteInlineObject* ellipsis = nullptr;
    IDWriteTextFormat* format = GetTextFormat(font, &ellipsis);
    if (!format) {
        return;
    }
    u32 fl = flags;
    bool wrap = (fl & gfxTextWrap) != 0;
    if ((fl & (gfxTextEllipsis | gfxTextPathEllipsis)) && !wrap) {
        // the rect was measured with gdi metrics, which run a little narrower
        // than DirectWrite lays the same string out; without the slack a label
        // in a rect measured to fit it exactly gets an "…" anyway
        Size sz = MeasureText(s, font);
        if (sz.dx <= r.dx + kTextSlack) {
            fl &= ~(u32)(gfxTextEllipsis | gfxTextPathEllipsis);
        }
    }
    SetTextFormatFlags(format, ellipsis, fl);

    ID2D1SolidColorBrush* br = GetBrush(col == kColorUnset ? textColor : col);
    if (!br) {
        return;
    }
    TempWStr ws = ToWStrTemp(s);
    // text that can't be ellipsized isn't clipped either, same as the gdi side
    bool ellipsize = (fl & (gfxTextEllipsis | gfxTextPathEllipsis)) != 0;
    bool noClip = (!ellipsize && !wrap) || (fl & gfxTextNoClip);
    auto opts = noClip ? D2D1_DRAW_TEXT_OPTIONS_NONE : D2D1_DRAW_TEXT_OPTIONS_CLIP;
    target->DrawText(ws.s, (UINT32)ws.len, format, ToD2DRect(r), br, opts);
}

void GfxDirect2D::DrawTextAt(Str s, Point pos, u32 flags, PlatformFont* font, Color col) {
    if (len(s) == 0) {
        return;
    }
    // the point is the top-left of the text; give it all the room it wants
    Rect r = {pos.x, pos.y, 1 << 16, 1 << 16};
    u32 fl = (flags & gfxTextRtl) | gfxTextSingleLine | gfxTextNoClip;
    DrawText(s, r, fl, font, col);
}

Size GfxDirect2D::MeasureText(Str s, PlatformFont* font) {
    if (len(s) == 0 || !gDWriteFactory) {
        return {};
    }
    IDWriteInlineObject* ellipsis = nullptr;
    IDWriteTextFormat* format = GetTextFormat(font, &ellipsis);
    if (!format) {
        return {};
    }
    SetTextFormatFlags(format, ellipsis, gfxTextSingleLine);
    TempWStr ws = ToWStrTemp(s);
    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = gDWriteFactory->CreateTextLayout(ws.s, (UINT32)ws.len, format, 1e6f, 1e6f, &layout);
    if (FAILED(hr) || !layout) {
        return {};
    }
    DWRITE_TEXT_METRICS tm{};
    layout->GetMetrics(&tm);
    layout->Release();
    return {(int)ceilf(tm.widthIncludingTrailingWhitespace), (int)ceilf(tm.height)};
}

// d2d wants 32bpp premultiplied BGRA; a Pixmap can be several other things.
// Returns the rows to hand it (into `scratch` when a conversion was needed).
static const u8* PixmapAsPremulBgra(Pixmap* px, Vec<u8>& scratch, int* strideOut) {
    if (px->format == PixmapFormat::BGRA8 && px->premultiplied) {
        *strideOut = px->stride;
        return px->data;
    }
    Pixmap* src = px;
    Pixmap* owned = nullptr;
    if (px->format == PixmapFormat::Native) {
        // only the platform bitmap knows how to read those pixels
        owned = PixmapCopyAs32bppDIB(px);
        if (!owned) {
            return nullptr;
        }
        src = owned;
    }
    int w = src->width, h = src->height;
    int stride = w * 4;
    scratch.Reset();
    u8* dst = VecReserve(scratch, stride * h);
    int srcBpp = PixmapBytesPerPixel(src->format);
    bool isRgba = src->format == PixmapFormat::RGBA8;
    bool hasAlpha = src->format != PixmapFormat::BGR8;
    for (int y = 0; y < h; y++) {
        const u8* sp = src->data + ((size_t)y * src->stride);
        u8* dp = dst + ((size_t)y * stride);
        for (int x = 0; x < w; x++, sp += srcBpp, dp += 4) {
            u8 b = isRgba ? sp[2] : sp[0];
            u8 g = sp[1];
            u8 rr = isRgba ? sp[0] : sp[2];
            u8 a = hasAlpha ? sp[3] : 255;
            if (hasAlpha && !src->premultiplied && a != 255) {
                b = (u8)((b * a) / 255);
                g = (u8)((g * a) / 255);
                rr = (u8)((rr * a) / 255);
            }
            dp[0] = b;
            dp[1] = g;
            dp[2] = rr;
            dp[3] = a;
        }
    }
    if (owned) {
        FreePixmap(owned);
    }
    *strideOut = stride;
    return dst;
}

void GfxDirect2D::DrawPixmap(Pixmap* px, const Rect& r) {
    if (!target || !px || r.IsEmpty()) {
        return;
    }
    Vec<u8> scratch;
    int stride = 0;
    const u8* rows = PixmapAsPremulBgra(px, scratch, &stride);
    if (!rows) {
        return;
    }
    D2D1_BITMAP_PROPERTIES bp =
        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
    ID2D1Bitmap* bmp = nullptr;
    HRESULT hr =
        target->CreateBitmap(D2D1::SizeU((UINT32)px->width, (UINT32)px->height), rows, (UINT32)stride, bp, &bmp);
    if (FAILED(hr) || !bmp) {
        return;
    }
    target->DrawBitmap(bmp, ToD2DRect(r), 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    bmp->Release();
}

void GfxDirect2D::PushClip(const Rect& r) {
    if (!target) {
        return;
    }
    // d2d intersects with what is already pushed, and pops in order
    target->PushAxisAlignedClip(ToD2DRect(r), D2D1_ANTIALIAS_MODE_ALIASED);
    clipDepth.Append(1);
}

void GfxDirect2D::PopClip() {
    if (!target) {
        return;
    }
    if (len(clipDepth) == 0) {
        ReportIf(true);
        return;
    }
    clipDepth.Pop();
    target->PopAxisAlignedClip();
}

// d2d doesn't pick up the window's orientation from the device context, so a
// d2d surface is never mirrored and there is nothing to turn off
bool GfxDirect2D::SetMirrored(bool) {
    return false;
}
#endif
