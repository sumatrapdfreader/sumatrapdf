/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/mac/GuiMacBridge.h"
#include "gui/PlatformFont.h"

bool PlatformFontCreateNative(PlatformFont* font) {
    Str name = len(font->name) > 0 ? font->name : StrL("system-ui");
    int style = (int)font->style;
    font->nativeFont = MacGuiFontCreate(name.s, len(name), font->sizePt, style & (int)PlatformFontStyle::Bold,
                                        style & (int)PlatformFontStyle::Italic);
    return font->nativeFont != nullptr;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (!font || len(s) == 0) {
        return {};
    }
    int dx = 0;
    int dy = 0;
    MacGuiFontMeasure(font->nativeFont, s.s, len(s), maxDx, &dx, &dy);
    return {dx, dy};
}

int PlatformFontLineHeight(PlatformFont* font) {
    return PlatformFontMeasureText(font, StrL("Ag")).dy;
}

static PlatformFontStyle MakeStyle(bool bold, bool italic) {
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    return style;
}

PlatformFont* GetDefaultGuiFont(bool bold, bool italic) {
    return GetPlatformFont(StrL("system-ui"), MacGuiDefaultFontSize(), MakeStyle(bold, italic));
}

PlatformFont* GetDefaultGuiFontOfSize(int size) {
    float sizePt = (float)size * 72.0f / 96.0f;
    return GetPlatformFont(StrL("system-ui"), sizePt, PlatformFontStyle::Regular);
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (len(fontName) == 0 || str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = StrL("system-ui");
    }
    float sizePt = (float)size * 72.0f / 96.0f;
    return GetPlatformFont(fontName, sizePt, MakeStyle(bold, italic));
}

PlatformFont* GetScaledPlatformFont(PlatformFont* font, int percent) {
    if (!font || percent <= 0) {
        return nullptr;
    }
    return GetPlatformFont(font->name, font->sizePt * (float)percent / 100.0f, font->style);
}

PlatformFont* GetBoldPlatformFont(PlatformFont* font) {
    if (!font) {
        return nullptr;
    }
    if (font->boldVariant) {
        return font->boldVariant;
    }
    if ((int)font->style & (int)PlatformFontStyle::Bold) {
        font->boldVariant = font;
        return font;
    }
    font->boldVariant = GetPlatformFont(font->name, font->sizePt, font->style | PlatformFontStyle::Bold);
    return font->boldVariant ? font->boldVariant : font;
}
