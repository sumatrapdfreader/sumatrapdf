/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#define Mui_h

// as little of mui as necessary to make ../EngineDump.cpp compile
// TODO: extract from UI toolkit so that it can be used in model independent of view

namespace mui {

void Initialize();
void Destroy();

struct CachedFont {
    WCHAR *             name;
    float               sizePt;
    Gdiplus::FontStyle  style;

    // only one of font or hdcFont can be set
    Gdiplus::Font *     font;
    HFONT               hdcFont;

    Gdiplus::FontStyle  GetStyle() const { return style; }
    float               GetSize() const { return sizePt; }
    const WCHAR *       GetName() const { return name; }

    bool                SameAs(const WCHAR *name, float sizePt, Gdiplus::FontStyle style) const {
        return this->sizePt == sizePt && this->style == style && str::Eq(this->name, name);
    }
};

CachedFont *GetCachedFontGdi(HDC hdc, const WCHAR *name, float sizePt, Gdiplus::FontStyle style);
CachedFont *GetCachedFontGdiplus(const WCHAR *name, float sizePt, Gdiplus::FontStyle style);

void InitGraphicsMode(Gdiplus::Graphics *g);
Gdiplus::Graphics *AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Gdiplus::Graphics *g);

};

class ScopedMiniMui {
public:
    ScopedMiniMui() { mui::Initialize(); }
    ~ScopedMiniMui() { mui::Destroy(); }
};

#endif
