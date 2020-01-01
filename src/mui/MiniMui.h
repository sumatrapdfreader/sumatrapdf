/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile
// TODO: extract from UI toolkit so that it can be used in model independent of view

namespace mui {

void Initialize();
void Destroy();

struct CachedFont {
    WCHAR* name;
    float sizePt;
    Gdiplus::FontStyle style;

    Gdiplus::Font* font;
    // hFont is created out of font
    HFONT hFont;

    HFONT GetHFont();
    Gdiplus::FontStyle GetStyle() const {
        return style;
    }
    float GetSize() const {
        return sizePt;
    }
    const WCHAR* GetName() const {
        return name;
    }

    bool SameAs(const WCHAR* name, float sizePt, Gdiplus::FontStyle style) const {
        return this->sizePt == sizePt && this->style == style && str::Eq(this->name, name);
    }
};

CachedFont* GetCachedFont(const WCHAR* name, float sizePt, Gdiplus::FontStyle style);

void InitGraphicsMode(Gdiplus::Graphics* g);
Gdiplus::Graphics* AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Gdiplus::Graphics* g);
}; // namespace mui

class ScopedMiniMui {
  public:
    ScopedMiniMui() {
        mui::Initialize();
    }
    ~ScopedMiniMui() {
        mui::Destroy();
    }
};
