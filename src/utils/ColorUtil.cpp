/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "utils/WinUtil.h"

COLORREF MkRgb(u8 r, u8 g, u8 b) {
    return RGB(r, g, b);
}

COLORREF MkRgba(u8 r, u8 g, u8 b, u8 a) {
    COLORREF col = RGB(r, g, b);
    COLORREF alpha = (COLORREF)a;
    alpha = alpha << 24;
    col = col | alpha;
    return col;
}

void UnpackRgba(COLORREF c, u8& r, u8& g, u8& b, u8& a) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

void UnpackRgb(COLORREF c, u8& r, u8& g, u8& b) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
}

static COLORREF MkRgbaFloat(float r, float g, float b, float a) {
    u8 rb = (u8)(r * 255.0f);
    u8 gb = (u8)(g * 255.0f);
    u8 bb = (u8)(b * 255.0f);
    u8 aa = (u8)(a * 255.0f);
    return MkRgba(rb, gb, bb, aa);
}

static void UnpackRgbaFloat(COLORREF c, float& r, float& g, float& b, float& a) {
    r = (float)(c & 0xff);
    c = c >> 8;
    r /= 255.0f;
    g = (float)(c & 0xff);
    g /= 255.0f;
    c = c >> 8;
    b = (float)(c & 0xff);
    b /= 255.0f;
    c = c >> 8;
    a = (float)(c & 0xff);
    a /= 255.0f;
}

static void UnpackRgbFloat(COLORREF c, float& r, float& g, float& b) {
    r = (float)(c & 0xff);
    r /= 255.0f;
    c = c >> 8;
    g = (float)(c & 0xff);
    g /= 255.0f;
    c = c >> 8;
    b = (float)(c & 0xff);
    b /= 255.0f;
}

COLORREF FromPdfColorRgba(float color[4]) {
    return MkRgbaFloat(color[0], color[1], color[2], color[3]);
}

COLORREF FromPdfColorRgb(float color[3]) {
    return MkRgbaFloat(color[0], color[1], color[2], 0);
}

void ToPdfRgb(COLORREF c, float col[3]) {
    UnpackRgbFloat(c, col[0], col[1], col[2]);
}

void ToPdfRgba(COLORREF c, float col[4]) {
    UnpackRgbaFloat(c, col[0], col[1], col[2], col[3]);
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
    UnpackRgba(c, r, g, b, a);
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
    UnpackRgb(c, r, g, b);
    return Gdiplus::Color(r, g, b);
}

Gdiplus::Color GdiRgbaFromCOLORREF(COLORREF c) {
    return Gdiplus::Color(c);
}

// TODO: replace usage with GdiRgbFromCOLORREF
Gdiplus::Color FromColor(COLORREF c) {
    return Gdiplus::Color(c);
}

static COLORREF colorSetHelper(COLORREF c, u8 col, int n) {
    CrashIf(n > 3);
    DWORD mask = 0xff;
    DWORD cmask = (DWORD)col;
    for (int i = 0; i < n; i++) {
        mask = mask << 8;
        cmask = cmask << 8;
    }
    c = c & ~mask;
    c = c | cmask;
    return c;
}

COLORREF ColorSetRed(COLORREF c, u8 red) {
    return colorSetHelper(c, red, 0);
}

COLORREF ColorSetGreen(COLORREF c, u8 green) {
    return colorSetHelper(c, green, 1);
}

COLORREF ColorSetBlue(COLORREF c, u8 blue) {
    return colorSetHelper(c, blue, 2);
}

COLORREF ColorSetAlpha(COLORREF c, u8 alpha) {
    return colorSetHelper(c, alpha, 3);
}

// TODO: remove use of SerializeColorRgb() and replace with SerializeColor
void SerializeColorRgb(COLORREF c, str::Str& out) {
    u8 r, g, b;
    UnpackRgb(c, r, g, b);
    char* s = str::Format("#%02x%02x%02x", r, g, b);
    out.Append(s);
    free(s);
}

void SerializeColor(COLORREF c, str::Str& out) {
    u8 r, g, b, a;
    UnpackRgba(c, r, g, b, a);
    char* s = nullptr;
    if (a > 0) {
        s = str::Format("#%02x%02x%02x%02x", a, r, g, b);
    } else {
        s = str::Format("#%02x%02x%02x", r, g, b);
    }
    out.Append(s);
    free(s);
}

/* Parse 'txt' as hex color and return the result in 'destColor' */
bool ParseColor(COLORREF* destColor, const WCHAR* txt) {
    CrashIf(!destColor);
    if (str::StartsWith(txt, L"0x")) {
        txt += 2;
    } else if (str::StartsWith(txt, L"#")) {
        txt += 1;
    }

    unsigned int r, g, b;
    bool ok = str::Parse(txt, L"%2x%2x%2x%$", &r, &g, &b);
    *destColor = RGB(r, g, b);
    return ok;
}

/* Parse 'txt' as hex color and return the result in 'destColor' */
bool ParseColor(COLORREF* destColor, std::string_view sv) {
    CrashIf(!destColor);
    const char* txt = sv.data();
    size_t n = sv.size();
    if (str::StartsWith(txt, "0x")) {
        txt += 2;
        n -= 2;
    } else if (str::StartsWith(txt, "#")) {
        txt += 1;
        n -= 1;
    }

    unsigned int r, g, b, a;
    bool ok = str::Parse(txt, n, "%2x%2x%2x%2x", &a, &r, &g, &b);
    if (ok) {
        *destColor = MkRgba((u8)r, (u8)g, (u8)b, (u8)a);
        return true;
    }
    ok = str::Parse(txt, n, "%2x%2x%2x", &r, &g, &b);
    *destColor = MkRgb((u8)r, (u8)g, (u8)b);
    return ok;
}

/* Parse 'txt' as hex color and return the result in 'destColor' */
bool ParseColor(COLORREF* destColor, const char* txt) {
    std::string_view sv(txt);
    return ParseColor(destColor, sv);
}

COLORREF AdjustLightness(COLORREF c, float factor) {
    u8 R, G, B;
    UnpackRgb(c, R, G, B);
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
    if (0.0f == lightness)
        return RGB(BYTE(units + 0.5f), BYTE(units + 0.5f), BYTE(units + 0.5f));
    return AdjustLightness(c, 1.0f + units / lightness);
}

// cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
float GetLightness(COLORREF c) {
    u8 R, G, B;
    UnpackRgb(c, R, G, B);
    BYTE M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    return (M + m) / 2.0f;
}

#if OS_WIN
BYTE GetRValueSafe(COLORREF rgb) {
    rgb = rgb & 0xff;
    return (u8)rgb;
}

BYTE GetGValueSafe(COLORREF rgb) {
    rgb = (rgb >> 8) & 0xff;
    return (u8)rgb;
}

BYTE GetBValueSafe(COLORREF rgb) {
    rgb = (rgb >> 16) & 0xff;
    return (u8)rgb;
}
#endif
