/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct FrameRateWnd;
struct TxtNode;

using Gdiplus::FontStyle;
using Gdiplus::Graphics;

namespace mui {

struct CachedFont {
    const WCHAR* name;
    float sizePt;
    Gdiplus::FontStyle style;

    Gdiplus::Font* font;
    // hFont is created out of font
    HFONT hFont;

    HFONT GetHFont();
    [[nodiscard]] Gdiplus::FontStyle GetStyle() const {
        return style;
    }
    [[nodiscard]] float GetSize() const {
        return sizePt;
    }
    [[nodiscard]] const WCHAR* GetName() const {
        return name;
    }
    bool SameAs(const WCHAR* name, float sizePt, FontStyle style) const;
};

#include "TextRender.h"

void Initialize();
void Destroy();

void InitGraphicsMode(Graphics* g);
CachedFont* GetCachedFont(const WCHAR* name, float sizePt, FontStyle style);

Graphics* AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Graphics* gfx);

} // namespace mui

class ScopedMui {
  public:
    ScopedMui() {
        mui::Initialize();
    }
    ~ScopedMui() {
        mui::Destroy();
    }
};
