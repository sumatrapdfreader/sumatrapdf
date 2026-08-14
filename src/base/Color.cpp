/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

bool IsSpecialColor(Color col) {
    return col == kColorUnset || col == kColorNoChange || col == kColorTransparent;
}

// format: abgr
void UnpackColor(Color c, u8& r, u8& g, u8& b, u8& a) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
    c = c >> 8;
    a = (u8)(c & 0xff);
}

// format: bgr
void UnpackColor(Color c, u8& r, u8& g, u8& b) {
    r = (u8)(c & 0xff);
    c = c >> 8;
    g = (u8)(c & 0xff);
    c = c >> 8;
    b = (u8)(c & 0xff);
}

#if OS_WIN
Gdiplus::Color Unblend(Color c, u8 alpha) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    u8 ralpha = (u8)((float)alpha * (float)a / 255.f);
    float falpha = ((float)alpha * (float)a / 255.f);
    float tmp = 255.0f / (falpha + 0.5f);
    u8 R = (u8)floorf((float)std::max(r - (255 - ralpha), 0) * tmp);
    u8 G = (u8)floorf((float)std::max(g - (255 - ralpha), 0) * tmp);
    u8 B = (u8)floorf((float)std::max(b - (255 - ralpha), 0) * tmp);
    return {alpha, R, G, B};
}

Gdiplus::Color GdiRgbFromColor(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return {r, g, b};
}

Gdiplus::Color GdiRgbaFromColor(Color c) {
    return {c};
}
#endif

#if 0
static Gdiplus::Color Unblend(PageAnnotation::Color c, u8 alpha) {
    alpha = (u8)(alpha * c.a / 255.f);
    u8 R = (u8)floorf(std::max(c.r - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    u8 G = (u8)floorf(std::max(c.g - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    u8 B = (u8)floorf(std::max(c.b - (255 - alpha), 0) * 255.0f / alpha + 0.5f);
    return Gdiplus::Color(alpha, R, G, B);
}
#endif

// TODO: use AdjustLightness instead to compensate for the alpha?
// TODO: not sure if that's the exact translation of the original (above)
TempStr SerializeColorTemp(Color c) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    if (a > 0) {
        return fmt("#%02x%02x%02x%02x", a, r, g, b);
    }
    return fmt("#%02x%02x%02x", r, g, b);
}

void ParseColor(ParsedColor& parsed, Str txt) {
    if (parsed.wasParsed) {
        return;
    }
    parsed.wasParsed = true;
    parsed.parsedOk = false;
    if (!txt) {
        return;
    }
    TempStr s = str::DupTemp(txt);
    str::TrimWSInPlace(s, str::TrimOpt::Both);
    if (str::EqI(s, StrL("checkered")) || str::EqI(s, StrL("unset"))) {
        parsed.col = kColorUnset;
        parsed.parsedOk = true;
        return;
    }
    if (!str::TrimPrefix(s, StrL("0x"))) {
        str::TrimPrefix(s, StrL("#"));
    }
    int n = len(s);
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned int a = 0;
    bool ok = n == 8 && !str::IsNull(str::Parse(s, "%2x%2x%2x%2x%$", &a, &r, &g, &b));
    if (ok) {
        parsed.col = MkRgba((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b, (u8)a);
        parsed.parsedOk = true;
        return;
    }

    ok = n == 6 && !str::IsNull(str::Parse(s, "%2x%2x%2x%$", &r, &g, &b));
    if (!ok) {
        return;
    }
    parsed.col = MkRgb((u8)r, (u8)g, (u8)b);
    parsed.pdfCol = MkPdfColor((u8)r, (u8)g, (u8)b);
    parsed.parsedOk = true;
}

/* Parse 's' as hex color and return the result in 'destColor' */
void ParseColor(ParsedColor& parsed) {
    ParseColor(parsed, parsed.s);
}

// the cached parse belongs to the old text, so it has to go with it
void SetColorText(ParsedColor& parsed, Str txt) {
    str::ReplaceWithCopy(&parsed.s, txt);
    parsed.wasParsed = false;
    parsed.parsedOk = false;
}

void FreeColorText(ParsedColor& parsed) {
    str::Free(parsed.s);
    parsed.s = {};
    parsed.wasParsed = false;
    parsed.parsedOk = false;
}

bool ParseColor(Color* destColor, Str s) {
    ReportIf(!destColor);
    ParsedColor p;
    ParseColor(p, s);
    *destColor = p.col;
    return p.parsedOk;
}

void SerializePdfColor(PdfColor c, str::Builder& out) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    out.Append(fmt("#%02x%02x%02x", r, g, b));
}

Color ParseColor(Str s, Color defCol) {
    Color c;
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

Color AdjustLightness(Color c, float factor) {
    u8 R, G, B;
    UnpackColor(c, R, G, B);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Hue_and_chroma
    u8 M = std::max(std::max(R, G), B), m = std::min(std::min(R, G), B);
    if (M == m) {
        // for grayscale values, lightness is proportional to the color value
        u8 X = (u8)limitValue((int)floorf(((float)M * factor) + 0.5f), 0, 255);
        return MkRgb(X, X, X);
    }
    u8 C = M - m;
    int hueDiff;
    if (M == R) {
        hueDiff = G - B;
    } else if (M == G) {
        hueDiff = B - R;
    } else {
        hueDiff = R - G;
    }
    u8 Ha = (u8)abs(hueDiff);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Lightness
    float L2 = (float)(M + m);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#Saturation
    float S = (float)C / (L2 > 255.0f ? 510.0f - L2 : L2);

    L2 = limitValue(L2 * factor, 0.0f, 510.0f);
    // cf. http://en.wikipedia.org/wiki/HSV_color_space#From_HSL
    float C1 = (L2 > 255.0f ? 510.0f - L2 : L2) * S;
    float X1 = C1 * (float)Ha / (float)C;
    float m1 = (L2 - C1) / 2;
    auto chromaOrX = [](bool isMax, bool isMin, float c1, float x1) -> float {
        if (isMax) {
            return c1;
        }
        if (!isMin) {
            return x1;
        }
        return 0.f;
    };
    R = (u8)floorf(chromaOrX(M == R, m == R, C1, X1) + m1 + 0.5f);
    G = (u8)floorf(chromaOrX(M == G, m == G, C1, X1) + m1 + 0.5f);
    B = (u8)floorf(chromaOrX(M == B, m == B, C1, X1) + m1 + 0.5f);
    return MkRgb(R, G, B);
}

// Adjusts lightness by 1/255 units.
Color AdjustLightness2(Color c, float units) {
    float lightness = GetLightness(c);
    units = limitValue(units, -lightness, 255.0f - lightness);
    if (0.0f == lightness) {
        u8 x = (u8)lroundf(units);
        return MkRgb(x, x, x);
    }
    return AdjustLightness(c, 1.0f + (units / lightness));
}

// http://en.wikipedia.org/wiki/HSV_color_space#Lightness
float GetLightness(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    u8 m1 = std::max(std::max(r, g), b);
    u8 m2 = std::min(std::min(r, g), b);
    return (float)(m1 + m2) / 2.0f;
}

// return true for light color, false for dark
// https://stackoverflow.com/questions/52879235/determine-color-lightness-via-rgb
bool IsLightColor(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    float y = (0.2126f * float(r)) + (0.7152f * float(g)) + (0.0722f * float(b));
    return y > 127.5f; // mid 256
}

bool IsNearBlack(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return r < 10 && g < 10 && b < 10;
}

// Darken a light color, lighten a dark one, so the result stands out from `col`
// whatever the theme. `dark` defaults to `light` when 0.
// shift a color away from itself by `light` units when it's light, `dark` when
// it's dark (dark defaults to `light`), for hover / selected / accent states
Color AccentColor(Color col, int light, int dark) {
    if (dark == 0) {
        dark = light;
    }
    if (IsLightColor(col)) {
        return AdjustLightness2(col, (float)-light);
    }
    return AdjustLightness2(col, (float)dark);
}

DWORD PremultiplyPixel(Color c, u8 alpha) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    r = (u8)((r * alpha) / 255);
    g = (u8)((g * alpha) / 255);
    b = (u8)((b * alpha) / 255);
    return (alpha << 24) | (r << 16) | (g << 8) | b;
}

/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */
u8 GetRed(Color rgb) {
    rgb = rgb & 0xff;
    return (u8)rgb;
}

u8 GetGreen(Color rgb) {
    rgb = (rgb >> 8) & 0xff;
    return (u8)rgb;
}

u8 GetBlue(Color rgb) {
    rgb = (rgb >> 16) & 0xff;
    return (u8)rgb;
}

u8 GetAlpha(Color rgb) {
    rgb = (rgb >> 24) & 0xff;
    return (u8)rgb;
}
