/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "utils/WinUtil.h"

COLORREF MkRgb(byte r, byte g, byte b) {
    return RGB(r, g, b);
}

COLORREF MkRgb(float r, float g, float b) {
    byte rb = (byte)(r * 255.0);
    byte gb = (byte)(g * 255.0);
    byte bb = (byte)(b * 255.0);
    return MkRgb(rb, gb, bb);
}

COLORREF MkRgba(byte r, byte g, byte b, byte a) {
    COLORREF col = RGB(r, g, b);
    COLORREF alpha = (COLORREF)a;
    alpha = alpha << 24;
    col = col | alpha;
    return col;
}

void UnpackRgba(COLORREF c, u8& r, u8& g, u8& b, u8& a) {
    r = (u8)(c & 0xff);
    c = c << 8;
    g = (u8)(c & 0xff);
    c = c << 8;
    b = (u8)(c & 0xff);
    c = c << 8;
    a = (u8)(c & 0xff);
}

void UnpackRgb(COLORREF c, u8& r, u8& g, u8& b) {
    u8 a;
    UnpackRgba(c, r, g, b, a);
}

void SerializeColor(COLORREF c, str::Str<char>& out) {
    u8 r = GetRValueSafe(c);
    u8 g = GetGValueSafe(c);
    u8 b = GetBValueSafe(c);
    char* s = str::Format("#%02x%02x%02x", r, g, b);
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
bool ParseColor(COLORREF* destColor, const char* txt) {
    CrashIf(!destColor);
    if (str::StartsWith(txt, "0x")) {
        txt += 2;
    } else if (str::StartsWith(txt, "#")) {
        txt += 1;
    }

    unsigned int r, g, b;
    bool ok = str::Parse(txt, "%2x%2x%2x%$", &r, &g, &b);
    *destColor = RGB(r, g, b);
    return ok;
}

COLORREF AdjustLightness(COLORREF c, float factor) {
    BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
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
    BYTE R = GetRValueSafe(c), G = GetGValueSafe(c), B = GetBValueSafe(c);
    BYTE M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    return (M + m) / 2.0f;
}
