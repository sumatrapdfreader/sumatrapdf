/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "wingui/PlatformFont.h"

// root node of the intrusive list of interned fonts; only its `next` is used
static PlatformFont gPlatformFonts;
// fonts are asked for from background threads (ebook formatting), so the list
// needs a lock. It is not re-entrant, so nothing called while holding it may
// ask for a font
static Mutex gPlatformFontsMutex;

bool PlatformFont::SameAs(Str otherName, float otherSizePt, PlatformFontStyle otherStyle) const {
    if (sizePt != otherSizePt) {
        return false;
    }
    if (style != otherStyle) {
        return false;
    }
    return str::Eq(name, otherName);
}

PlatformFont* GetPlatformFont(Str name, float sizePt, PlatformFontStyle style) {
    gPlatformFontsMutex.Lock();
    defer {
        gPlatformFontsMutex.Unlock();
    };

    for (PlatformFont* font = gPlatformFonts.next; font; font = font->next) {
        if (font->SameAs(name, sizePt, style)) {
            return font;
        }
    }

    Arena* arena = GetPermArena();
    PlatformFont* font = New<PlatformFont>(arena);
    font->name = str::Dup(arena, name);
    font->sizePt = sizePt;
    font->style = style;
    if (!PlatformFontCreateNative(font)) {
        // no font could be created: hand out the last one that worked, like
        // the gdiplus font cache used to
        return gPlatformFonts.next;
    }
    ListInsertFront(&gPlatformFonts.next, font);
    return font;
}
