/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// note: include BaseUtil.h instead of including directly

// a "unset" state for COLORREF value. technically all colors are valid
// this one is hopefully not used in practice
// can't use constexpr because they'll end up in multiple .lib and conflict
#define ColorUnset ((COLORREF)(0xfeffffff))
// ColorNoChange indicates that we shouldn't change the color
#define ColorNoChange ((COLORREF)(0xfdffffff))

// PdfColor is aarrggbb, where 0xff alpha is opaque and 0x0 alpha is transparent
// this is different than COLORREF, which ggrrbb and no alpha
using PdfColor = uint64_t;

struct ParsedColor {
    bool wasParsed = false;
    bool parsedOk = false;
    COLORREF col = 0;
    PdfColor pdfCol = 0;
};

COLORREF MkGray(u8 x);
COLORREF MkColor(u8 r, u8 g, u8 b, u8 a = 0);
void UnpackColor(COLORREF, u8& r, u8& g, u8& b);
void UnpackColor(COLORREF, u8& r, u8& g, u8& b, u8& a);

void ParseColor(ParsedColor& parsed, const char* txt);
bool ParseColor(COLORREF* destColor, const char* s);
COLORREF ParseColor(const char* s, COLORREF defCol = 0);
TempStr SerializeColorTemp(COLORREF);

PdfColor MkPdfColor(u8 r, u8 g, u8 b, u8 a = 0xff); // 0xff is opaque
void UnpackPdfColor(PdfColor, u8& r, u8& g, u8& b, u8& a);
void SerializePdfColor(PdfColor c, str::Str& out);

COLORREF AdjustLightness(COLORREF c, float factor);
COLORREF AdjustLightness2(COLORREF c, float units);
float GetLightness(COLORREF c);
bool IsLightColor(COLORREF c);

// TODO: use AdjustLightness instead to compensate for the alpha?
Gdiplus::Color Unblend(COLORREF c, u8 alpha);
Gdiplus::Color GdiRgbFromCOLORREF(COLORREF c);
Gdiplus::Color GdiRgbaFromCOLORREF(COLORREF c);

constexpr COLORREF RgbToCOLORREF(COLORREF rgb) {
    return ((rgb & 0x0000FF) << 16) | (rgb & 0x00FF00) | ((rgb & 0xFF0000) >> 16);
}

/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */
u8 GetRed(COLORREF rgb);
u8 GetGreen(COLORREF rgb);
u8 GetBlue(COLORREF rgb);
u8 GetAlpha(COLORREF rgb);
