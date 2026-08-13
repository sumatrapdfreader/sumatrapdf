/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include Base.h instead of including directly

// a "unset" state for Color value. technically all colors are valid
// this one is hopefully not used in practice
constexpr Color kColorUnset = ((Color)(0xfeffffff));
// kColorNoChange indicates that we shouldn't change the color
constexpr Color kColorNoChange((Color)(0xfdffffff));

// PdfColor is aarrggbb, where 0xff alpha is opaque and 0x0 alpha is transparent
// Color is ggrrbb (Win32 COLORREF layout) and typically has no alpha
using PdfColor = uint64_t;

struct ParsedColor {
    bool wasParsed = false;
    bool parsedOk = false;
    Color col = 0;
    PdfColor pdfCol = 0;
};

Color MkGray(u8 x);
Color MkColor(u8 r, u8 g, u8 b, u8 a = 0);
void UnpackColor(Color, u8& r, u8& g, u8& b);
void UnpackColor(Color, u8& r, u8& g, u8& b, u8& a);

bool IsSpecialColor(Color col);

void ParseColor(ParsedColor& parsed, Str txt);
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
