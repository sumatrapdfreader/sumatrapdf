/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/PlatformFont.h"

// root node of the intrusive list of interned fonts; only its `next` is used
static PlatformFont gPlatformFonts;
// fonts are asked for from background threads (ebook formatting), so the list
// needs a lock. It is not re-entrant, so nothing called while holding it may
// ask for a font
static Mutex gPlatformFontsMutex;

static int CalculateAverageCharWidth(PlatformFont* font) {
    Size size = PlatformFontMeasureText(font, StrL("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
    int average = (size.dx + 25) / 52;
    if (average > 0) {
        return average;
    }
    return std::max((int)(font->sizePt * 0.55f), 1);
}

bool PlatformFont::SameAs(Str otherName, float otherSizePt, PlatformFontStyle otherStyle) const {
    if (sizePt != otherSizePt) {
        return false;
    }
    if (style != otherStyle) {
        return false;
    }
    return str::Eq(name, otherName);
}

static PlatformFont* GetPlatformFontInternal(Str name, float sizePt, PlatformFontStyle style, uintptr_t nativeId) {
    gPlatformFontsMutex.Lock();
    defer {
        gPlatformFontsMutex.Unlock();
    };

    for (PlatformFont* font = gPlatformFonts.next; font; font = font->next) {
        if (nativeId ? font->nativeId == nativeId : font->nativeId == 0 && font->SameAs(name, sizePt, style)) {
            return font;
        }
    }

    Arena* arena = GetPermArena();
    PlatformFont* font = New<PlatformFont>(arena);
    font->name = str::Dup(arena, name);
    font->sizePt = sizePt;
    font->style = style;
    font->nativeId = nativeId;
    if (!PlatformFontCreateNative(font)) {
        // no font could be created: hand out the last one that worked, like
        // the gdiplus font cache used to
        return gPlatformFonts.next;
    }
    font->averageCharWidth = CalculateAverageCharWidth(font);
    ListInsertFront(&gPlatformFonts.next, font);
    return font;
}

PlatformFont* GetPlatformFont(Str name, float sizePt, PlatformFontStyle style) {
    return GetPlatformFontInternal(name, sizePt, style, 0);
}

#if OS_WIN
PlatformFont* GetPlatformFontForNative(Str name, float sizePt, PlatformFontStyle style, uintptr_t nativeId) {
    return GetPlatformFontInternal(name, sizePt, style, nativeId);
}
#endif
