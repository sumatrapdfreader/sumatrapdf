/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// ng: the POSIX half of gui/PlatformFont.cpp. Linux uses Pango and macOS uses
// CoreText; wasm retains estimated metrics until browser font measurement is
// exposed to native code.

#include "base/Base.h"

#include "gui/PlatformFont.h"

#if OS_LINUX
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

#elif OS_DARWIN
#include "base/MacTypesHide.h"
#include <CoreText/CoreText.h>
#include "base/MacTypesShow.h"

static CTFontRef NativeFont(PlatformFont* font) {
    return font ? (CTFontRef)font->nativeFont : nullptr;
}

static CFStringRef NewCfString(Str s) {
    return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.s, len(s), kCFStringEncodingUTF8, false);
}

bool PlatformFontCreateNative(PlatformFont* font) {
    CTFontRef base = nullptr;
    if (len(font->name) > 0) {
        CFStringRef name = NewCfString(font->name);
        if (name) {
            base = CTFontCreateWithName(name, font->sizePt, nullptr);
            CFRelease(name);
        }
    } else {
        base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, font->sizePt, nullptr);
    }
    if (!base) {
        return false;
    }
    CTFontSymbolicTraits traits = 0;
    int style = (int)font->style;
    if (style & (int)PlatformFontStyle::Bold) {
        traits |= kCTFontBoldTrait;
    }
    if (style & (int)PlatformFontStyle::Italic) {
        traits |= kCTFontItalicTrait;
    }
    if (traits) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(base, font->sizePt, nullptr, traits,
                                                              kCTFontBoldTrait | kCTFontItalicTrait);
        if (styled) {
            CFRelease(base);
            base = styled;
        }
    }
    font->nativeFont = (void*)base;
    return true;
}

static CFAttributedStringRef NewAttributedString(PlatformFont* font, Str s) {
    CFStringRef text = NewCfString(s);
    if (!text) {
        return nullptr;
    }
    const void* keys[] = {kCTFontAttributeName};
    const void* values[] = {NativeFont(font)};
    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef result = CFAttributedStringCreate(kCFAllocatorDefault, text, attrs);
    CFRelease(attrs);
    CFRelease(text);
    return result;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (!font || len(s) == 0) {
        return {};
    }
    CFAttributedStringRef text = NewAttributedString(font, s);
    if (!text) {
        return {};
    }
    CGSize size{};
    if (maxDx < 0) {
        CTLineRef line = CTLineCreateWithAttributedString(text);
        CGFloat ascent = 0;
        CGFloat descent = 0;
        CGFloat leading = 0;
        size.width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
        size.height = ascent + descent + leading;
        CFRelease(line);
    } else {
        CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(text);
        CFIndex n = CFAttributedStringGetLength(text);
        size = CTFramesetterSuggestFrameSizeWithConstraints(framesetter, CFRangeMake(0, n), nullptr,
                                                            CGSizeMake(maxDx, CGFLOAT_MAX), nullptr);
        CFRelease(framesetter);
    }
    CFRelease(text);
    return {(int)ceil(size.width), (int)ceil(size.height)};
}

int PlatformFontLineHeight(PlatformFont* font) {
    CTFontRef native = NativeFont(font);
    if (!native) {
        return 0;
    }
    return (int)ceil(CTFontGetAscent(native) + CTFontGetDescent(native) + CTFontGetLeading(native));
}

static PlatformFontStyle FontStyle(bool bold, bool italic) {
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
    return GetPlatformFont(Str(), 12.0f, FontStyle(bold, italic));
}

PlatformFont* GetDefaultGuiFontOfSize(int size) {
    return GetPlatformFont(Str(), (float)size * 72.0f / 96.0f, PlatformFontStyle::Regular);
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = {};
    }
    return GetPlatformFont(fontName, (float)size * 72.0f / 96.0f, FontStyle(bold, italic));
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

#else
// same constants StubTextRender uses, so layout and drawing agree
constexpr float kAvgCharWidthRatio = 0.55f;
constexpr float kLineSpacingRatio = 1.25f;
constexpr float kDefaultGuiFontSizePt = 12.0f;

static float FontSizeOf(PlatformFont* font) {
    if (font && font->sizePt > 0) {
        return font->sizePt;
    }
    return kDefaultGuiFontSizePt;
}

bool PlatformFontCreateNative(PlatformFont* f) {
    f->nativeFont = nullptr;
    return true;
}

Size PlatformFontMeasureText(PlatformFont* font, Str s, int maxDx) {
    if (len(s) == 0) {
        return {};
    }
    float charDx = FontSizeOf(font) * kAvgCharWidthRatio;
    int lineDy = PlatformFontLineHeight(font);
    int dx = (int)(Utf8CodepointCount(s) * charDx + 0.5f);
    if (maxDx < 0 || dx <= maxDx) {
        return Size(dx, lineDy);
    }
    int nLines = (dx + maxDx - 1) / maxDx;
    return Size(maxDx, lineDy * nLines);
}

int PlatformFontLineHeight(PlatformFont* font) {
    return (int)(FontSizeOf(font) * kLineSpacingRatio + 0.5f);
}

PlatformFont* GetDefaultGuiFont(bool bold, bool italic) {
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    return GetPlatformFont(Str(), kDefaultGuiFontSizePt, style);
}

PlatformFont* GetDefaultGuiFontOfSize(int size) {
    return GetPlatformFont(Str(), (float)size, PlatformFontStyle::Regular);
}

PlatformFont* GetUserGuiFont(Str fontName, int size) {
    return GetUserGuiFontEx(fontName, size, false, false);
}

PlatformFont* GetUserGuiFontEx(Str fontName, int size, bool bold, bool italic) {
    if (str::EqI(fontName, StrL("automatic")) || str::EqI(fontName, StrL("auto"))) {
        fontName = Str();
    }
    PlatformFontStyle style = PlatformFontStyle::Regular;
    if (bold) {
        style = style | PlatformFontStyle::Bold;
    }
    if (italic) {
        style = style | PlatformFontStyle::Italic;
    }
    return GetPlatformFont(fontName, (float)size, style);
}

PlatformFont* GetScaledPlatformFont(PlatformFont* font, int percent) {
    if (!font || percent <= 0) {
        return nullptr;
    }
    float size = font->sizePt * (float)percent / 100.f;
    return GetPlatformFont(font->name, size, font->style);
}

PlatformFont* GetBoldPlatformFont(PlatformFont* f) {
    if (!f) {
        return nullptr;
    }
    if (f->boldVariant) {
        return f->boldVariant;
    }
    if ((int)f->style & (int)PlatformFontStyle::Bold) {
        f->boldVariant = f;
        return f;
    }
    f->boldVariant = GetPlatformFont(f->name, f->sizePt, f->style | PlatformFontStyle::Bold);
    if (!f->boldVariant) {
        f->boldVariant = f;
    }
    return f->boldVariant;
}
#endif
