/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void InitializeBase();
void DestroyBase();

void EnterMuiCriticalSection();
void LeaveMuiCriticalSection();

class ScopedMuiCritSec {
  public:
    ScopedMuiCritSec() {
        EnterMuiCriticalSection();
    }

    ~ScopedMuiCritSec() {
        LeaveMuiCriticalSection();
    }
};

struct CachedFont {
    const WCHAR* name;
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
    bool SameAs(const WCHAR* name, float sizePt, FontStyle style) const;
};

void InitGraphicsMode(Graphics* g);
CachedFont* GetCachedFont(const WCHAR* name, float sizePt, FontStyle style);

Graphics* AllocGraphicsForMeasureText();
void FreeGraphicsForMeasureText(Graphics* gfx);

int CeilI(float n);
