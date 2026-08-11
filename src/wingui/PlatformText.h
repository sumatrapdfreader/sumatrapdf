/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Measuring text with the platform's text engine, for code that lays out its
// own text (the ebook formatter). Everything here works in utf-8; converting to
// whatever the platform wants is the implementation's job.

// PlatformFont lives in wingui/PlatformFont.h; include it before this header

enum class PlatformTextMeasureMethod {
    Gdiplus,
    GdiplusQuick,
    Gdi,
    Hdc,
    Stub,
};

struct PlatformTextMeasurer {
    virtual void SetFont(PlatformFont* font) = 0;
    virtual float GetCurrFontLineSpacing() = 0;
    virtual float GetSpaceDx() = 0;
    // s is utf-8, and the length StringLenForWidth returns is in bytes; it
    // never cuts a utf-8 sequence in half
    virtual RectF Measure(Str s) = 0;
    virtual int StringLenForWidth(Str s, float dx, float sWidth = -1) = 0;
    virtual ~PlatformTextMeasurer() = default;
};

// the platform resources this needs are created on demand, so there is nothing
// to initialize first
PlatformTextMeasurer* CreatePlatformTextMeasurer(PlatformTextMeasureMethod method);

// frees what measuring text allocated along the way. Only worth calling at
// shutdown; measuring after it just allocates again
void PlatformFontDestroy();
