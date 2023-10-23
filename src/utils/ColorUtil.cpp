/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

COLORREF MkColor(u8 r, u8 g, u8 b, u8 a) {
    COLORREF r2 = r;
    COLORREF g2 = (COLORREF)g << 8;
    COLORREF b2 = (COLORREF)b << 16;
    COLORREF a2 = (COLORREF)a << 24;
    return r2 | g2 | b2 | a2;
}

COLORREF MkGray(u8 x) {
    return MkColor(x, x, x);
}

// format: abgr
void UnpackColor(COLORREF c, u8& r, u8& g, u8& b, u8& a) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

// format: bgr
void UnpackColor(COLORREF c, u8& r, u8& g, u8& b) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
}

#if 0
static Gdiplus::Color Unblend(PageAnnotation::Color c, BYTE alpha) {
    alpha = (BYTE)(alpha * c.a / 255.f);
    BYTE R = (BYTE)floorf(std::max(c.r - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    BYTE G = (BYTE)floorf(std::max(c.g - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    BYTE B = (BYTE)floorf(std::max(c.b - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    return Gdiplus::Color(alpha, R, G, B);
}
#endif

// TODO: not sure if that's the exact translation of the original (above)
Gdiplus::Color Unblend(COLORREF c, u8 alpha) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    u8 ralpha = (BYTE)(alpha * a / 255.f);
    float falpha = ((float)alpha * (float)a / 255.f);
    float tmp = 255.0f / (falpha + 0.5f);
    BYTE R = (BYTE)floorf(std::max(r - (255 - ralpha), 0) * tmp);
    BYTE G = (BYTE)floorf(std::max(g - (255 - ralpha), 0) * tmp);
    BYTE B = (BYTE)floorf(std::max(b - (255 - ralpha), 0) * tmp);
    return Gdiplus::Color(alpha, R, G, B);
}

Gdiplus::Color GdiRgbFromCOLORREF(COLORREF c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return Gdiplus::Color(r, g, b);
}

Gdiplus::Color GdiRgbaFromCOLORREF(COLORREF c) {
    return Gdiplus::Color(c);
}

TempStr SerializeColorTemp(COLORREF c) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    char* s = nullptr;
    if (a > 0) {
        s = str::FormatTemp("#%02x%02x%02x%02x", a, r, g, b);
    } else {
        s = str::FormatTemp("#%02x%02x%02x", r, g, b);
    }
    return s;
}

void ParseColor(ParsedColor& parsed, const char* txt) {
    if (parsed.wasParsed) {
        return;
    }
    parsed.wasParsed = true;
    parsed.parsedOk = false;
    if (!txt) {
        return;
    }
    char* s = str::DupTemp(txt);
    str::TrimWSInPlace(s, str::TrimOpt::Both);
    if (str::StartsWith(s, "0x")) {
        s += 2;
    } else if (str::StartsWith(s, "#")) {
        s += 1;
    }
    size_t n = str::Len(s);
    unsigned int r, g, b, a;
    bool ok = str::Parse(s, n, "%2x%2x%2x%2x", &a, &r, &g, &b);
    if (ok) {
        parsed.col = MkColor((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.parsedOk = true;
        return;
    }

    ok = str::Parse(s, n, "%2x%2x%2x", &r, &g, &b);
    if (!ok) {
        return;
    }
    parsed.col = MkColor((u8)r, (u8)g, (u8)b);
    parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b);
    parsed.parsedOk = true;
}

/* Parse 's' as hex color and return the result in 'destColor' */
bool ParseColor(COLORREF* destColor, const char* s) {
    CrashIf(!destColor);
    ParsedColor p;
    ParseColor(p, s);
    *destColor = p.col;
    return p.parsedOk;
}

void SerializePdfColor(PdfColor c, str::Str& out) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    out.AppendFmt("#%02x%02x%02x", r, g, b);
}

COLORREF ParseColor(const char* s, COLORREF defCol) {
    COLORREF c;
    if (ParseColor(&c, s)) {
        return c;
    }
    return defCol;
}

// return argb
PdfColor MkPdfColor(u8 r, u8 g, u8 b, u8 a) {
    PdfColor b2 = (PdfColor)b;
    PdfColor g2 = (PdfColor)g << 8;
    PdfColor r2 = (PdfColor)r << 16;
    PdfColor a2 = (PdfColor)a << 24;
    return a2 | r2 | g2 | b2;
}

// argb
void UnpackPdfColor(PdfColor c, u8& r, u8& g, u8& b, u8& a) {
    b = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    r = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

COLORREF AdjustLightness(COLORREF c, float factor) {
    u8 R, G, B;
    UnpackColor(c, R, G, B);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Hue_and_chroma
    BYTE M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    if (M == m) {
        // for grayscale values, lightness is proportional to the color value
        BYTE X = (BYTE)limitValue((int)floorf(M * factor + 0.5f), 0, 255);
        return RGB(X, X, X);
    }
    BYTE C = M - m;
    BYTE Ha = (BYTE)abs(M == R ? G - B : M == G ? B - R : R - G);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
    float L2 = (float)(M + m);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Saturation
    float S = C / (L2 > 255.0f ? 510.0f - L2 : L2);

    L2 = limitValue(L2 * factor, 0.0f, 510.0f);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#From_HSL
    float C1 = (L2 > 255.0f ? 510.0f - L2 : L2) * S;
    float X1 = C1 * Ha / C;
    float m1 = (L2 - C1) / 2;
    R = (BYTE)floorf((M == R ? C1 : m != R ? X1 : 0) + m1 + 0.5f);
    G = (BYTE)floorf((M == G ? C1 : m != G ? X1 : 0) + m1 + 0.5f);
    B = (BYTE)floorf((M == B ? C1 : m != B ? X1 : 0) + m1 + 0.5f);
    return RGB(R, G, B);
}

// Adjusts lightness by 1/255 units.
COLORREF AdjustLightness2(COLORREF c, float units) {
    float lightness = GetLightness(c);
    units = limitValue(units, -lightness, 255.0f - lightness);
    if (0.0f == lightness) {
        return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
    }
    return AdjustLightness(c, 1.0f + units / lightness);
}

// http://en.wikipedia.org/wiki/HSV_color_space#Lightness
float GetLightness(COLORREF c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    u8 m1 = std::max(std::max(r, g), b);
    u8 m2 = std::min(std::min(r, g), b);
    return (float)(m1 + m2) / 2.0f;
}

// return true for light color, false for dark
// https://stackoverflow.com/questions/52879235/determine-color-lightness-via-rgb
bool IsLightColor(COLORREF c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    float y = 0.2126f * float(r) + 0.7152f * float(g) + 0.0722f * float(b);
    return y > 127.5f; // mid 256
}

u8 GetRed(COLORREF rgb) {
    rgb = rgb & 0xff;
    return (u8)rgb;
}

u8 GetGreen(COLORREF rgb) {
    rgb = (rgb >> 8) & 0xff;
    return (u8)rgb;
}

u8 GetBlue(COLORREF rgb) {
    rgb = (rgb >> 16) & 0xff;
    return (u8)rgb;
}

u8 GetAlpha(COLORREF rgb) {
    rgb = (rgb >> 24) & 0xff;
    return (u8)rgb;
}
