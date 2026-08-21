/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"

// TODO: not quite sure why measuring a space directly gives a (much) bigger
// size than the difference below, which looks like better spacing to me
float PlatformTextRender::GetSpaceDx() {
    float l1 = Measure(StrL("wa")).dx;
    float l2 = Measure(StrL("w a")).dx;
    return l2 - l1;
}

int PlatformTextRender::StringLenForWidth(Str s, float dx, float sWidth) {
    float fullWidth = sWidth >= 0 ? sWidth : Measure(s).dx;
    if (fullWidth <= dx) {
        return len(s);
    }
    // make the best guess of the length that fits
    int sLen = len(s);
    int n = (int)((dx / fullWidth) * (float)sLen);
    ReportIf(n > sLen);
    // the guess can land inside a utf-8 sequence; back up to its start
    n = Utf8CodepointStartByte(s, n);
    if (n == 0) {
        // nothing fits in the remaining space; caller flushes the line and
        // re-lays the run at full width. Don't Measure an empty string.
        return 0;
    }
    RectF r = Measure(Str(s.s, n));
    // find the length of s that fits within dx iff the next codepoint makes it
    // exceed dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    while (n > 0) {
        int prevN = n;
        if (dir == 1) {
            Utf8CodepointNext(s, n);
        } else {
            Utf8CodepointPrev(s, n);
        }
        if (n == prevN) {
            break;
        }
        r = Measure(Str(s.s, n));
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.dx > dx) {
                return prevN;
            }
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.dx < dx) {
                return n;
            }
        }
    }
    // even a single codepoint is longer than available space
    return 0;
}

// guesses sizes from the font size instead of asking the platform, and draws
// nothing. Used by tests and as the fallback where there is no platform text
// engine
struct StubTextRender : PlatformTextRender {
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

    // a rough guess, so measure by codepoint rather than by utf-8 byte
    RectF Measure(Str s) override {
        return {0, 0, (float)Utf8CodepointCount(s) * AverageCharDx(), GetCurrFontLineSpacing()};
    }

    void SetTextColor(Color) override {}
    void SetTextBgColor(Color) override {}
    void Lock() override {}
    void Unlock() override {}
    void Draw(Str, RectF, bool) override {}
};

#if OS_WIN
// implemented in PlatformText_win.cpp
PlatformTextRender* CreateNativeTextRender(PlatformTextMeasureMethod method);
#endif

PlatformTextRender* CreatePlatformTextRender(PlatformTextMeasureMethod method) {
#if OS_WIN
    if (method != PlatformTextMeasureMethod::Stub) {
        return CreateNativeTextRender(method);
    }
#else
    (void)method;
#endif
    return new StubTextRender();
}

#if !OS_WIN
void PlatformFontDestroy() {
    // nothing is allocated lazily by the stub renderer
}
#endif
