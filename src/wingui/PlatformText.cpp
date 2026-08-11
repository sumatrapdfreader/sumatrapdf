/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "wingui/PlatformFont.h"
#include "wingui/PlatformText.h"

// guesses sizes from the font size instead of asking the platform. Used by
// tests and as the fallback where there is no platform text engine
struct StubTextMeasurer : PlatformTextMeasurer {
    PlatformFont* currFont = nullptr;

    float CurrFontSize() const {
        if (currFont && currFont->GetSize() > 0) {
            return currFont->GetSize();
        }
        return 12.5f;
    }

    float AverageCharDx() const { return CurrFontSize() * 0.55f; }

    void SetFont(PlatformFont* font) override { currFont = font; }

    float GetCurrFontLineSpacing() override { return CurrFontSize() * 1.25f; }

    float GetSpaceDx() override { return AverageCharDx(); }

    // a rough guess, so measure by codepoint rather than by utf-8 byte
    RectF Measure(Str s) override {
        return RectF(0, 0, (float)Utf8CodepointCount(s) * AverageCharDx(), GetCurrFontLineSpacing());
    }

    int StringLenForWidth(Str s, float dx, float sWidth) override {
        int n = len(s);
        if (n == 0 || dx <= 0) {
            return 0;
        }
        if (sWidth < 0) {
            sWidth = Measure(s).dx;
        }
        if (sWidth <= dx) {
            return n;
        }
        int res = (int)floorf((float)n * dx / sWidth);
        res = limitValue(res, 0, n);
        // don't cut a utf-8 sequence in half
        if (res < n) {
            res = Utf8CodepointStartByte(s, res);
        }
        return res;
    }
};

#if OS_WIN
// implemented in PlatformText_win.cpp
PlatformTextMeasurer* CreateNativeTextMeasurer(PlatformTextMeasureMethod method);
#endif

PlatformTextMeasurer* CreatePlatformTextMeasurer(PlatformTextMeasureMethod method) {
#if OS_WIN
    if (method != PlatformTextMeasureMethod::Stub) {
        return CreateNativeTextMeasurer(method);
    }
#else
    (void)method;
#endif
    return new StubTextMeasurer();
}

#if !OS_WIN
void PlatformFontDestroy() {
    // nothing is allocated lazily by the stub measurer
}
#endif
