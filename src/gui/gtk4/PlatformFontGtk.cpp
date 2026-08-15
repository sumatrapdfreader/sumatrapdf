/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/PlatformFont.h"

#include <pango/pangocairo.h>

static PangoFontDescription* NativeFont(PlatformFont* font) {
    return font ? (PangoFontDescription*)font->nativeFont : nullptr;
}

bool PlatformFontCreateNative(PlatformFont* font) {
    auto* desc = pango_font_description_new();
    if (!desc) {
        return false;
    }
    Str name = len(font->name) > 0 ? font->name : StrL("Sans");
    pango_font_description_set_family(desc, CStrTemp(name));
    pango_font_description_set_size(desc, (int)(font->sizePt * PANGO_SCALE));
    int style = (int)font->style;
    if (style & (int)PlatformFontStyle::Bold) {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    }
    if (style & (int)PlatformFontStyle::Italic) {
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    }
    font->nativeFont = desc;
    return true;
}

static PangoLayout* NewLayout(PlatformFont* font, Str s) {
    PangoFontMap* map = pango_cairo_font_map_get_default();
    PangoContext* context = pango_font_map_create_context(map);
    PangoLayout* layout = pango_layout_new(context);
    g_object_unref(context);
    if (font) {
        pango_layout_set_font_description(layout, NativeFont(font));
    }
    pango_layout_set_text(layout, s.s, len(s));
    return layout;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (len(s) == 0) {
        return {};
    }
    PangoLayout* layout = NewLayout(font, s);
    if (maxDx >= 0) {
        pango_layout_set_width(layout, maxDx * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    }
    int dx = 0;
    int dy = 0;
    pango_layout_get_pixel_size(layout, &dx, &dy);
    g_object_unref(layout);
    return {dx, dy};
}

int PlatformFontLineHeight(PlatformFont* font) {
    return PlatformFontMeasureText(font, StrL("Ag")).dy;
}

PlatformFont* GetDefaultGuiFont(bool bold, bool italic) {
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    return GetPlatformFont(StrL("Sans"), 10.0f, style);
}

PlatformFont* GetDefaultGuiFontOfSize(int size) {
    float sizePt = (float)size * 72.0f / 96.0f;
    return GetPlatformFont(StrL("Sans"), sizePt, PlatformFontStyle::Regular);
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (len(fontName) == 0 || str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = StrL("Sans");
    }
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    float sizePt = (float)size * 72.0f / 96.0f;
    return GetPlatformFont(fontName, sizePt, style);
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
