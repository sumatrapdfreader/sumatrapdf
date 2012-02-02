/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

// a critical section for everything that needs protecting in mui
// we use only one for simplicity as long as contention is not a problem
static CRITICAL_SECTION gMuiCs;

void EnterMuiCriticalSection()
{
    EnterCriticalSection(&gMuiCs);
}

void LeaveMuiCriticalSection()
{
    LeaveCriticalSection(&gMuiCs);
}

// Global, thread-safe font cache. Font objects live forever.
struct FontCacheEntry {
    WCHAR *     name;
    float       size;
    FontStyle   style;

    Font *      font;

    bool SameAs(const WCHAR *name, float size, FontStyle style);
};

static Vec<FontCacheEntry> *gFonts = NULL;


void InitializeBase()
{
    InitializeCriticalSection(&gMuiCs);
    gFonts = new Vec<FontCacheEntry>();
}

void DestroyBase()
{
    DeleteCriticalSection(&gMuiCs);
    for (size_t i = 0; i < gFonts->Count(); i++) {
        FontCacheEntry e = gFonts->At(i);
        free(e.name);
        ::delete e.font;
    }
    delete gFonts;
}

static Font *CreateFontByStyle(const WCHAR *name, REAL size, FontStyle style)
{
    return ::new Font(name, size, style);
}

bool FontCacheEntry::SameAs(const WCHAR *otherName, float otherSize, FontStyle otherStyle)
{
    if (size != otherSize)
        return false;
    if (style != otherStyle)
        return false;
    return str::Eq(name, otherName);
}

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style)
{
    ScopedMuiCritSec muiCs;

    for (size_t i = 0; i < gFonts->Count(); i++) {
        FontCacheEntry f = gFonts->At(i);
        if (f.SameAs(name, size, style)) {
            return f.font;
        }
    }
    FontCacheEntry f = { str::Dup(name), size, style, NULL };
    // TODO: handle a failure to create a font. Use fontCache[0] if exists
    // or try to fallback to a known font like Times New Roman
    f.font = CreateFontByStyle(name, size, style);
    gFonts->Append(f);
    return f.font;
}


}
