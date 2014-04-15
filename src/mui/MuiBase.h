/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef MuiBase_h
#error "dont include twice!"
#endif
#define MuiBase_h

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
    const WCHAR *       name;
    float               size;
    Gdiplus::FontStyle  style;

    // only one of font or hdcFont can be set
    Gdiplus::Font *     font;
    HDC                 hdcFont;

    Gdiplus::FontStyle  GetStyle() const { return style; }
    float               GetSize() const { return size; }
    const WCHAR *       GetName() const { return name; }
    bool                SameAs(const WCHAR *name, float size, FontStyle style) const;
};

void        InitGraphicsMode(Graphics *g);
CachedFont *GetCachedFontGdi(const WCHAR *name, float size, FontStyle style);
CachedFont *GetCachedFontGdiplus(const WCHAR *name, float size, FontStyle style);

Graphics *  AllocGraphicsForMeasureText();
void        FreeGraphicsForMeasureText(Graphics *gfx);

int         CeilI(float n);

