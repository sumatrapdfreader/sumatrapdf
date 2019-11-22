/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// a "unset" state for COLORREF value. technically all colors are valid
// this one is hopefully not used in practice
// can't use constexpr because they'll end up in multiple .lib and conflict
#define ColorUnset ((COLORREF)(0xfeffffff))

COLORREF MkRgb(u8 r, u8 g, u8 b);
COLORREF MkRgb(float r, float g, float b); // in 0..1 range
COLORREF MkRgba(u8 r, u8 g, u8 b, u8 a);
void UnpackRgb(COLORREF, u8& r, u8& g, u8& b);
void UnpackRgba(COLORREF, u8& r, u8& g, u8& b, u8& a);

bool ParseColor(COLORREF* destColor, const WCHAR* txt);
bool ParseColor(COLORREF* destColor, const char* txt);
void SerializeColor(COLORREF, str::Str<char>&);

COLORREF AdjustLightness(COLORREF c, float factor);
COLORREF AdjustLightness2(COLORREF c, float units);
float GetLightness(COLORREF c);

#if OS_WIN
/* In debug mode, VS 2010 instrumentations complains about GetRValue() etc.
This adds equivalent functions that don't have this problem and ugly
substitutions to make sure we don't use Get*Value() in the future */
BYTE GetRValueSafe(COLORREF rgb);
BYTE GetGValueSafe(COLORREF rgb);
BYTE GetBValueSafe(COLORREF rgb);

#undef GetRValue
#define GetRValue UseGetRValueSafeInstead
#undef GetGValue
#define GetGValue UseGetGValueSafeInstead
#undef GetBValue
#define GetBValue UseGetBValueSafeInstead
#endif
