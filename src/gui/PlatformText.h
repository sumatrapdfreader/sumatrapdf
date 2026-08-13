/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Measuring and drawing text with the platform's text engine, for code that
// lays out its own text (the ebook formatter). Everything here works in utf-8;
// converting to whatever the platform wants is the implementation's job.

// PlatformFont lives in gui/PlatformFont.h; include it before this header

enum class PlatformTextMeasureMethod {
    Gdiplus,      // uses MeasureTextAccurate, which is slower than MeasureTextQuick
    GdiplusQuick, // uses MeasureTextQuick
    Gdi,
    Hdc,
    Stub,
};

struct PlatformTextRender {
    virtual void SetFont(PlatformFont* font) = 0;
    virtual float GetCurrFontLineSpacing() = 0;
    virtual RectF Measure(Str s) = 0;

    virtual void SetTextColor(Color col) = 0;

    // this is only for the benefit of the gdi renderer. In GDI+, Draw() uses
    // transparent background color (i.e. whatever is under).
    // GDI doesn't support such transparency so the best we can do is simulate
    // that if the background is solid color. It won't work in other cases
    virtual void SetTextBgColor(Color col) = 0;

    // GDI+ calls cannot be done if we called Graphics::GetHDC(). However, getting/releasing
    // hdc is very expensive and kills performance if we do it for every Draw(). So we add
    // explicit Lock()/Unlock() calls (only important for the gdi renderer) so that a caller
    // can batch Draw() calls to minimize GetHDC()/ReleaseHDC() calls
    virtual void Lock() = 0;
    virtual void Unlock() = 0;

    virtual void Draw(Str s, RectF bb, bool isRtl) = 0;

    virtual ~PlatformTextRender() = default;

    // these only need Measure(), so they are the same for every implementation
    float GetSpaceDx();
    // how much of s fits in dx, in utf-8 bytes and never cutting a sequence in
    // half. The caller usually already measured the whole string (that's how it
    // knows it needs to wrap); pass that width as sWidth to skip re-measuring
    int StringLenForWidth(Str s, float dx, float sWidth = -1);
};

// measures (and draws) with resources of its own, created on demand, so there
// is nothing to initialize first
PlatformTextRender* CreatePlatformTextRender(PlatformTextMeasureMethod method);

#if OS_WIN
// draws into a Graphics owned by the caller
PlatformTextRender* CreateGdiplusTextRender(Gdiplus::Graphics* gfx);
#endif

// frees what measuring text allocated along the way. Only worth calling at
// shutdown; measuring after it just allocates again
void PlatformFontDestroy();
