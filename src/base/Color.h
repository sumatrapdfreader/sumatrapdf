/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

// Win32 COLORREF layout (0x00bbggrr); typically no alpha
#if OS_WIN
using Color = COLORREF;
#else
using Color = uint32_t;
#endif

// a "unset" state for Color value. technically all colors are valid
// this one is hopefully not used in practice
constexpr Color kColorUnset = ((Color)(0xfeffffff));
// kColorNoChange indicates that we shouldn't change the color
constexpr Color kColorNoChange((Color)(0xfdffffff));
// explicit "don't paint" / no fill / no border (not inherit/default)
constexpr Color kColorTransparent = ((Color)(0xfcffffff));

constexpr bool ColorSkipsPaint(Color c) {
    return c == kColorUnset || c == kColorTransparent;
}

// PdfColor is aarrggbb, where 0xff alpha is opaque and 0x0 alpha is transparent
// Color is ggrrbb (Win32 COLORREF layout) and typically has no alpha
using PdfColor = uint64_t;

// A color setting: the text the user wrote (e.g. "#ff0000", "checkered") and
// the parse of it, filled in on first use. Settings hold one of these rather
// than a string and a separate cache, so the two can't drift apart.
struct ParsedColor {
    Str s;
    bool wasParsed = false;
    bool parsedOk = false;
    Color col = 0;
    PdfColor pdfCol = 0;
};

constexpr Color MkRgb(u8 r, u8 g, u8 b) {
    return (Color)r | ((Color)g << 8) | ((Color)b << 16);
}
constexpr Color MkRgba(u8 r, u8 g, u8 b, u8 a) {
    return MkRgb(r, g, b) | ((Color)a << 24);
}
constexpr Color MkGray(u8 x) {
    return MkRgb(x, x, x);
}
constexpr Color kColWhite = MkRgb(0xff, 0xff, 0xff);
constexpr Color kColBlack = MkRgb(0, 0, 0);
constexpr Color kColRed = MkRgb(0xff, 0, 0);
constexpr Color kColGreen = MkRgb(0, 0xff, 0);
constexpr Color kColBlue = MkRgb(0, 0, 0xff);
constexpr Color kColYellow = MkRgb(0xff, 0xff, 0);
constexpr Color kColGray = MkGray(0xdd);
void UnpackColor(Color, u8& r, u8& g, u8& b);
void UnpackColor(Color, u8& r, u8& g, u8& b, u8& a);

bool IsSpecialColor(Color col);

void ParseColor(ParsedColor& parsed, Str txt);
// parse ParsedColor::s, if it hasn't been parsed yet
void ParseColor(ParsedColor& parsed);
// replace the text and drop the cached parse
void SetColorText(ParsedColor& parsed, Str txt);
void FreeColorText(ParsedColor& parsed);
bool ParseColor(Color* destColor, Str s);
Color ParseColor(Str s, Color defCol = 0);
TempStr SerializeColorTemp(Color);

PdfColor MkPdfColor(u8 r, u8 g, u8 b, u8 a = 0xff); // 0xff is opaque
void UnpackPdfColor(PdfColor, u8& r, u8& g, u8& b, u8& a);
void SerializePdfColor(PdfColor c, str::Builder& out);

Color AdjustLightness(Color c, float factor);
Color AdjustLightness2(Color c, float units);
float GetLightness(Color c);
bool IsLightColor(Color c);
Color AccentColor(Color col, int light, int dark = 0);
bool IsNearBlack(Color c);
DWORD PremultiplyPixel(Color c, u8 alpha);

// GDI+ only exists on Windows; portable code works with Color
#if OS_WIN
Gdiplus::Color Unblend(Color c, u8 alpha);
Gdiplus::Color GdiRgbFromColor(Color c);
Gdiplus::Color GdiRgbaFromColor(Color c);
#endif

constexpr Color RgbToColor(Color rgb) {
    return ((rgb & 0x0000FF) << 16) | (rgb & 0x00FF00) | ((rgb & 0xFF0000) >> 16);
}

u8 GetRed(Color rgb);
u8 GetGreen(Color rgb);
u8 GetBlue(Color rgb);
u8 GetAlpha(Color rgb);
