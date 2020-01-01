/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/VecSegmented.h"
#include "utils/HtmlParserLookup.h"
#include "Mui.h"

using namespace Gdiplus;

/*
A css-like way to style controls/windows.

We define a bunch of css-like properties.

We have a Style, which is a logical group of properties.

Each control can have one or more styles that define how
a control looks like. A window has only one set of properties
but e.g. a button has two (one for normal look and one for
mouse hover look).

We define a bunch of default style so that if e.g. button
doesn't have a style explicitly set, it'll get all the necessary
properties from our default set.

Prop objects are never freed. To conserve memory, they are
internalized i.e. there are never 2 Prop objects with exactly
the same data.
*/

namespace mui {
namespace css {

#define MKARGB(a, r, g, b) (((ARGB)(b)) | ((ARGB)(g) << 8) | ((ARGB)(r) << 16) | ((ARGB)(a) << 24))
#define MKRGB(r, g, b) (((ARGB)(b)) | ((ARGB)(g) << 8) | ((ARGB)(r) << 16) | ((ARGB)(0xff) << 24))

struct FontCacheEntry {
    Prop* fontName;
    Prop* fontSize;
    Prop* fontWeight;
    Font* font;

    // Prop objects are interned, so if the pointer is
    // the same, the value is the same too
    bool operator==(FontCacheEntry& other) const {
        return ((fontName == other.fontName) && (fontSize == other.fontSize) && (fontWeight == other.fontWeight));
    }
};

// gStyleDefault is a fallback style. It contains a default
// value for each possible property. If a property is not
// found in a given style, we use the value from gStyleDefault
// An app might modify gStyleDefault but it should be done
// as a first thing to avoid stale props from caching (cache
// will be correctly refreshed if Control::SetStyle() is called)
static Style* gStyleDefault = nullptr;
// default button styles for convenience. The user must explicitly
// use them in inheritance chain of their custom button styles
// They can be obtained via GetStyleButtonDefault() and GetStyleButtonDefaultMouseOver()
static Style* gStyleButtonDefault = nullptr;
static Style* gStyleButtonMouseOver = nullptr;

struct StyleCacheEntry {
    Style* style;
    size_t styleId;
    CachedStyle cachedStyle;
};

// Those must be VecSegmented so that code can retain pointers to
// their elements (we can't move the memory)
static VecSegmented<Prop>* gAllProps = nullptr;
static VecSegmented<StyleCacheEntry>* gStyleCache = nullptr;

void Initialize() {
    CrashIf(gAllProps);

    gAllProps = new VecSegmented<Prop>();

    // gStyleDefault must have values for all properties
    gStyleDefault = new Style();
    gStyleDefault->SetName("default");
    gStyleDefault->Set(Prop::AllocFontName(L"Times New Roman"));
    gStyleDefault->Set(Prop::AllocFontSize(14));
    gStyleDefault->Set(Prop::AllocFontWeight(FontStyleBold));
    gStyleDefault->Set(Prop::AllocColorSolid(PropColor, "black"));
// gStyleDefault->Set(Prop::AllocColorSolid(PropBgColor, 0xff, 0xff, 0xff));
#if 0
    ARGB c1 = MKRGB(0x00, 0x00, 0x00);
    ARGB c2 = MKRGB(0xff, 0xff, 0xff);
#else
    ARGB c1 = MKRGB(0xf5, 0xf6, 0xf6);
    ARGB c2 = MKRGB(0xe4, 0xe4, 0xe3);
#endif
    gStyleDefault->Set(Prop::AllocColorLinearGradient(PropBgColor, LinearGradientModeVertical, c1, c2));
    gStyleDefault->SetBorderWidth(1);
    gStyleDefault->SetBorderColor(MKRGB(0x99, 0x99, 0x99));
    gStyleDefault->Set(Prop::AllocColorSolid(PropBorderBottomColor, "#888"));
    gStyleDefault->Set(Prop::AllocPadding(0, 0, 0, 0));
    gStyleDefault->Set(Prop::AllocTextAlign(Align_Left));
    gStyleDefault->Set(Prop::AllocAlign(PropVertAlign, ElAlign::Center));
    gStyleDefault->Set(Prop::AllocAlign(PropHorizAlign, ElAlign::Center));
    gStyleDefault->Set(Prop::AllocColorSolid(PropFill, "white"));
    gStyleDefault->Set(Prop::AllocColorSolid(PropStroke, "black"));
    gStyleDefault->Set(Prop::AllocWidth(PropStrokeWidth, 0.5f));

    gStyleButtonDefault = new Style(gStyleDefault);
    gStyleButtonDefault->SetName("buttonDefault");
    gStyleButtonDefault->Set(Prop::AllocPadding(4, 8, 4, 8));
    gStyleButtonDefault->Set(Prop::AllocFontName(L"Lucida Grande"));
    gStyleButtonDefault->Set(Prop::AllocFontSize(8));
    gStyleButtonDefault->Set(Prop::AllocFontWeight(FontStyleBold));

    gStyleButtonMouseOver = new Style(gStyleButtonDefault);
    gStyleButtonMouseOver->SetName("buttonDefaultMouseOver");
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderTopColor, "#777"));
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderRightColor, "#777"));
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderBottomColor, "#666"));
    // gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBgColor, 180, 0, 0, 255));
    // gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));

    gStyleCache = new VecSegmented<StyleCacheEntry>();
    CacheStyle(gStyleDefault, nullptr);
    CacheStyle(gStyleButtonDefault, nullptr);
    CacheStyle(gStyleButtonMouseOver, nullptr);
}

void Destroy() {
    for (Prop& p : *gAllProps) {
        p.Free();
    }
    delete gAllProps;

    for (StyleCacheEntry& e : *gStyleCache) {
        delete e.style;
    }
    delete gStyleCache;
}

bool IsWidthProp(PropType type) {
    return (PropBorderTopWidth == type) || (PropBorderRightWidth == type) || (PropBorderBottomWidth == type) ||
           (PropBorderLeftWidth == type) || (PropStrokeWidth == type);
}

bool IsColorProp(PropType type) {
    return (PropColor == type) || (PropBgColor == type) || (PropBorderTopColor == type) ||
           (PropBorderRightColor == type) || (PropBorderBottomColor == type) || (PropBorderLeftColor == type) ||
           (PropFill == type) || (PropStroke == type);
}

bool IsAlignProp(PropType type) {
    return ((PropVertAlign == type) || (PropHorizAlign == type));
}

// TODO: use FindCssColor from gen_fast_string_lookup.py
//       once there are significantly more colors
static struct {
    const char* name;
    ARGB value;
} gCssKnownColors[] = {
    {"black", (ARGB)Color::Black},
    {"blue", (ARGB)Color::Blue},
    {"gray", (ARGB)Color::Gray},
    {"green", (ARGB)Color::Green},
    {"red", (ARGB)Color::Red},
    {"white", (ARGB)Color::White},
    {"transparent", MKARGB(0, 0, 0, 0)},
    {"sepia", MKRGB(0xfb, 0xf0, 0xd9)},
    {"light blue", MKRGB(0x64, 0xc7, 0xef)},
    {"light gray", MKRGB(0xf0, 0xf0, 0xf0)},
};

static bool GetKnownCssColor(const char* name, ARGB& colOut) {
    for (size_t i = 0; i < dimof(gCssKnownColors); i++) {
        if (str::EqI(name, gCssKnownColors[i].name)) {
            colOut = gCssKnownColors[i].value;
            return true;
        }
    }
    return false;
}

// Parses css-like color formats:
// rrggbb, #rrggbb, #aarrggbb, #rgb, 0xrgb, 0xrrggbb
// rgb(r,g,b), rgba(r,g,b,a) rgb(r%, g%, b%), rgba(r%, g%, b%, a%)
// cf. https://developer.mozilla.org/en/CSS/color_value
ARGB ParseCssColor(const char* color) {
    // parse #RRGGBB and #RGB and rgb(R,G,B)
    int a, r, g, b;

    // a bit too relaxed, but by skipping 0x and #
    // we'll easily parse all variations of hex-encoded values
    if (color[0] == '0' && color[1] == 'x')
        color += 2;

    if (*color == '#')
        ++color;

    // parse: #rgb, 0xrgb, rgb (which is a shortcut for #rrggbb)
    if (str::Parse(color, "%1x%1x%1x%$", &r, &g, &b)) {
        r |= (r << 4);
        g |= (g << 4);
        b |= (b << 4);
        return MKRGB(r, g, b);
    }

    // parse rrggbb, #rrggbb, 0xrrggbb and rgb(n,n,n)
    if (str::Parse(color, "%2x%2x%2x%$", &r, &g, &b) || str::Parse(color, "rgb(%d,%d,%d)", &r, &g, &b)) {
        return MKRGB(r, g, b);
    }

    // parse aarrggbb, #aarrggbb, 0xaarrggbb and rgba(R,G,B,A)
    if (str::Parse(color, "%2x%2x%2x%2x%$", &a, &r, &g, &b) || str::Parse(color, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a)) {
        return MKARGB(a, r, g, b);
    }

    // parse rgb(R%,G%,B%) and rgba(R%,G%,B%,A%)
    float fa = 1.0f, fr, fg, fb;
    if (str::Parse(color, "rgb(%f%%,%f%%,%f%%)", &fr, &fg, &fb) ||
        str::Parse(color, "rgba(%f%%,%f%%,%f%%,%f%%)", &fr, &fg, &fb, &fa)) {
        return MKARGB((int)(fa * 2.55f), (int)(fr * 2.55f), (int)(fg * 2.55f), (int)(fb * 2.55f));
    }

    // parse known color names
    ARGB colVal = MKARGB(0, 0, 0, 0); // transparent if not known
    GetKnownCssColor(color, colVal);
    return colVal;
}

bool ColorData::operator==(const ColorData& other) const {
    if (type != other.type)
        return false;

    if (ColorSolid == type)
        return solid.color == other.solid.color;

    if (ColorGradientLinear == type) {
        return (gradientLinear.mode == other.gradientLinear.mode) &&
               (gradientLinear.startColor == other.gradientLinear.startColor) &&
               (gradientLinear.endColor == other.gradientLinear.endColor);
    }
    CrashIf(true);
    return false;
}

bool ElAlignData::operator==(const ElAlignData& other) const {
    return ((elementPoint == other.elementPoint) && (containerPoint == other.containerPoint));
}

// Note: the order must match enum ElAlign
struct ElAlignData g_ElAlignVals[5] = {
    {.5f, .5f}, // ElAlign::Center
    {0.f, 0.f}, // ElAlign::Top
    {1.f, 1.f}, // ElAlign::Bottom
    {0.f, 0.f}, // ElAlign::Left
    {1.f, 1.f}, // ElAlign::Right
};

// calculates the offset of an element within container
int ElAlignData::CalcOffset(int elSize, int containerSize) const {
    int ep = (int)((float)elSize * elementPoint);
    int cp = (int)((float)containerSize * containerPoint);
    return cp - ep;
}

void Prop::Free() {
    if (PropFontName == type)
        free(fontName);
    else if (PropStyleName == type)
        free(styleName);
    else if (IsColorProp(type) && (ColorSolid == color.type))
        ::delete color.solid.cachedBrush;
    else if (IsColorProp(type) && (ColorGradientLinear == color.type)) {
        ::delete color.gradientLinear.cachedBrush;
        ::delete color.gradientLinear.rect;
    }
}

bool Prop::Eq(const Prop* other) const {
    if (type != other->type)
        return false;

    switch (type) {
        case PropStyleName:
            return str::Eq(styleName, other->styleName);
        case PropFontName:
            return str::Eq(fontName, other->fontName);
        case PropFontSize:
            return fontSize == other->fontSize;
        case PropFontWeight:
            return fontWeight == other->fontWeight;
        case PropPadding:
            return padding == other->padding;
        case PropTextAlign:
            return textAlign == other->textAlign;
    }

    if (IsColorProp(type))
        return color == other->color;

    if (IsWidthProp(type))
        return width == other->width;

    if (IsAlignProp(type))
        return elAlign == other->elAlign;

    CrashIf(true);
    return false;
}

static Prop* FindExistingProp(Prop* prop) {
    for (Prop& p : *gAllProps) {
        if (p.Eq(prop))
            return &p;
    }
    return nullptr;
}

static Prop* UniqifyProp(Prop& p) {
    Prop* existing = FindExistingProp(&p);
    if (existing) {
        p.Free();
        return existing;
    }
    return gAllProps->push_back(p);
}

Prop* Prop::AllocStyleName(const char* styleName) {
    Prop p(PropStyleName);
    p.styleName = str::Dup(styleName);
    return UniqifyProp(p);
}

Prop* Prop::AllocFontName(const WCHAR* name) {
    Prop p(PropFontName);
    p.fontName = str::Dup(name);
    return UniqifyProp(p);
}

Prop* Prop::AllocFontSize(float size) {
    Prop p(PropFontSize);
    p.fontSize = size;
    return UniqifyProp(p);
}

Prop* Prop::AllocFontWeight(FontStyle style) {
    Prop p(PropFontWeight);
    p.fontWeight = style;
    return UniqifyProp(p);
}

Prop* Prop::AllocWidth(PropType type, float width) {
    CrashIf(!IsWidthProp(type));
    Prop p(type);
    p.width = width;
    return UniqifyProp(p);
}

Prop* Prop::AllocTextAlign(AlignAttr align) {
    Prop p(PropTextAlign);
    p.textAlign = align;
    return UniqifyProp(p);
}

Prop* Prop::AllocAlign(PropType type, float elPoint, float containerPoint) {
    CrashIf(!IsAlignProp(type));
    Prop p(type);
    p.elAlign = GetElAlign(elPoint, containerPoint);
    return UniqifyProp(p);
}

Prop* Prop::AllocAlign(PropType type, ElAlign align) {
    CrashIf(!IsAlignProp(type));
    Prop p(type);
    p.elAlign = GetElAlign(align);
    return UniqifyProp(p);
}

Prop* Prop::AllocPadding(int top, int right, int bottom, int left) {
    Padding pd = {top, right, bottom, left};
    Prop p(PropPadding);
    p.padding = pd;
    return UniqifyProp(p);
}

Prop* Prop::AllocColorSolid(PropType type, ARGB color) {
    CrashIf(!IsColorProp(type));
    Prop p(type);
    p.color.type = ColorSolid;
    p.color.solid.color = color;
    p.color.solid.cachedBrush = nullptr;
    Prop* res = UniqifyProp(p);
    CrashIf(res->color.type != ColorSolid);
    CrashIf(res->color.solid.color != color);
    if (!res->color.solid.cachedBrush)
        res->color.solid.cachedBrush = ::new SolidBrush(color);
    return res;
}

Prop* Prop::AllocColorSolid(PropType type, int a, int r, int g, int b) {
    return AllocColorSolid(type, MKARGB(a, r, g, b));
}

Prop* Prop::AllocColorSolid(PropType type, int r, int g, int b) {
    return AllocColorSolid(type, MKARGB(0xff, r, g, b));
}

Prop* Prop::AllocColorLinearGradient(PropType type, LinearGradientMode mode, ARGB startColor, ARGB endColor) {
    Prop p(type);
    p.color.type = ColorGradientLinear;
    p.color.gradientLinear.mode = mode;
    p.color.gradientLinear.startColor = startColor;
    p.color.gradientLinear.endColor = endColor;

    p.color.gradientLinear.rect = ::new RectF();
    p.color.gradientLinear.cachedBrush = nullptr;
    return UniqifyProp(p);
}

Prop* Prop::AllocColorLinearGradient(PropType type, LinearGradientMode mode, const char* startColor,
                                     const char* endColor) {
    ARGB c1 = ParseCssColor(startColor);
    ARGB c2 = ParseCssColor(endColor);
    return AllocColorLinearGradient(type, mode, c1, c2);
}

Prop* Prop::AllocColorSolid(PropType type, const char* color) {
    ARGB col = ParseCssColor(color);
    return AllocColorSolid(type, col);
}

Style* Style::GetInheritsFrom() const {
    return inheritsFrom;
}

// Identity is a way to track changes to Style
size_t Style::GetIdentity() const {
    size_t identity = gen;
    Style* curr = inheritsFrom;
    while (curr) {
        identity += curr->gen;
        curr = curr->inheritsFrom;
    }
    identity += gStyleDefault->gen;
    return identity;
}

// Add a property to a set, if a given PropType doesn't exist,
// replace if a given PropType already exists in the set.
void Style::Set(Prop* prop) {
    CrashIf(!prop);
    Prop*& p = props.FindEl([&](Prop* p2) { return p2->type == prop->type; });
    if (p) {
        if (!prop->Eq(p))
            ++gen;
        p = prop;
        return;
    }
    props.Append(prop);
    ++gen;
}

void Style::SetName(const char* styleName) {
    Set(Prop::AllocStyleName(styleName));
}

void Style::SetPadding(int width) {
    Set(Prop::AllocPadding(width, width, width, width));
}

void Style::SetPadding(int topBottom, int leftRight) {
    Set(Prop::AllocPadding(topBottom, leftRight, topBottom, leftRight));
}

void Style::SetPadding(int top, int right, int bottom, int left) {
    Set(Prop::AllocPadding(top, right, bottom, left));
}

void Style::SetBorderWidth(float width) {
    Set(Prop::AllocWidth(PropBorderTopWidth, width));
    Set(Prop::AllocWidth(PropBorderRightWidth, width));
    Set(Prop::AllocWidth(PropBorderBottomWidth, width));
    Set(Prop::AllocWidth(PropBorderLeftWidth, width));
}

void Style::SetBorderColor(ARGB color) {
    Set(Prop::AllocColorSolid(PropBorderTopColor, color));
    Set(Prop::AllocColorSolid(PropBorderRightColor, color));
    Set(Prop::AllocColorSolid(PropBorderBottomColor, color));
    Set(Prop::AllocColorSolid(PropBorderLeftColor, color));
}

static bool FoundAllProps(Prop** props) {
    for (size_t i = 0; i < (size_t)PropsCount; i++) {
        if (!props[i])
            return false;
    }
    return true;
}

// props points to the Prop* array whos size must be PropsCount.
// This function is designed to be called multiple times with
// different styles. It only sets a given property in props
// array if it's not already set (it should be all NULLs the first
// time).
// As an optimization it returns true if we got all props
static bool GetAllProps(Style* style, Prop** props) {
    bool inherited = false;
    while (style) {
        for (Prop* p : style->props) {
            // PropStyleName is not inheritable
            if ((PropStyleName == p->type) && inherited)
                continue;
            int propIdx = (int)p->type;
            CrashIf(propIdx >= (int)PropsCount);
            bool didSet = false;
            if (!props[propIdx]) {
                props[propIdx] = p;
                didSet = true;
            }
            if (didSet && FoundAllProps(props))
                return true;
        }
        style = style->GetInheritsFrom();
        inherited = true;
    }
    return false;
}

static size_t GetStyleId(Style* style) {
    if (!style)
        return 0;
    return style->GetIdentity();
}

// Because all styles implicitly inherit the properties they didn't explicitly
// set from gStyleDefault, we need to track changes (via GetStyleId()) of both the given
// style an gStyleDefault.
// If a given style doesn't exist, we add it to the cache.
// If it exists but it was modified or gStyleDefault was modified, we update the cache.
// If it exists and didn't change, we return cached entry.
CachedStyle* CacheStyle(Style* style, bool* changedOut) {
    bool changedTmp;
    if (!changedOut)
        changedOut = &changedTmp;
    *changedOut = false;

    ScopedMuiCritSec muiCs;
    StyleCacheEntry* e = nullptr;

    for (StyleCacheEntry& e2 : *gStyleCache) {
        if (e2.style == style) {
            if (e2.styleId == GetStyleId(style)) {
                return &e2.cachedStyle;
            }
            e = &e2;
            break;
        }
    }

    *changedOut = true;
    Prop* props[PropsCount] = {0};
    if (!GetAllProps(style, props))
        GetAllProps(gStyleDefault, props);
    for (size_t i = 0; i < dimof(props); i++) {
        CrashIf(!props[i]);
    }

    CachedStyle s;
    s.styleName = props[PropStyleName]->styleName;
    s.fontName = props[PropFontName]->fontName;
    s.fontSize = props[PropFontSize]->fontSize;
    s.fontWeight = props[PropFontWeight]->fontWeight;
    s.padding = props[PropPadding]->padding;
    s.color = &(props[PropColor]->color);
    s.bgColor = &(props[PropBgColor]->color);
    s.borderWidth.top = props[PropBorderTopWidth]->width;
    s.borderWidth.right = props[PropBorderRightWidth]->width;
    s.borderWidth.bottom = props[PropBorderBottomWidth]->width;
    s.borderWidth.left = props[PropBorderLeftWidth]->width;
    s.borderColors.top = &(props[PropBorderTopColor]->color);
    s.borderColors.right = &(props[PropBorderRightColor]->color);
    s.borderColors.bottom = &(props[PropBorderBottomColor]->color);
    s.borderColors.left = &(props[PropBorderLeftColor]->color);
    s.textAlign = props[PropTextAlign]->textAlign;
    s.vertAlign = props[PropVertAlign]->elAlign;
    s.horizAlign = props[PropHorizAlign]->elAlign;
    s.fill = &(props[PropFill]->color);
    s.stroke = &(props[PropStroke]->color);
    s.strokeWidth = props[PropStrokeWidth]->width;

    if (e) {
        e->cachedStyle = s;
        e->styleId = GetStyleId(style);
        return &e->cachedStyle;
    }

    StyleCacheEntry newEntry = {style, GetStyleId(style), s};
    e = gStyleCache->push_back(newEntry);
    return &e->cachedStyle;
}

CachedStyle* CachedStyleByName(const char* name) {
    if (!name)
        return nullptr;
    for (StyleCacheEntry& e : *gStyleCache) {
        if (str::Eq(e.cachedStyle.styleName, name))
            return &e.cachedStyle;
    }
    return nullptr;
}

Style* StyleByName(const char* name) {
    if (!name)
        return nullptr;
    for (StyleCacheEntry& e : *gStyleCache) {
        if (str::Eq(e.cachedStyle.styleName, name))
            return e.style;
    }
    return nullptr;
}

Brush* BrushFromColorData(ColorData* color, const RectF& r) {
    if (ColorSolid == color->type)
        return color->solid.cachedBrush;

    if (ColorGradientLinear == color->type) {
        ColorDataGradientLinear* d = &color->gradientLinear;
        LinearGradientBrush* br = d->cachedBrush;
        if (!br || !r.Equals(*d->rect)) {
            ::delete br;
            br = ::new LinearGradientBrush(r, d->startColor, d->endColor, d->mode);
            *d->rect = r;
            d->cachedBrush = br;
        }
        return br;
    }

    CrashIf(true);
    return ::new SolidBrush(0);
}

Brush* BrushFromColorData(ColorData* color, const Rect& r) {
    return BrushFromColorData(color, RectF((float)r.X, (float)r.Y, (float)r.Width, (float)r.Height));
}

static void AddBorders(int& dx, int& dy, CachedStyle* s) {
    const BorderWidth& bw = s->borderWidth;
    // note: width is a float, not sure how I should round them
    dx += (int)(bw.left + bw.right);
    dy += (int)(bw.top + bw.bottom);
}

Size GetBorderAndPaddingSize(CachedStyle* s) {
    Padding pad = s->padding;
    int dx = pad.left + pad.right;
    int dy = pad.top + pad.bottom;
    AddBorders(dx, dy, s);
    return Size(dx, dy);
}

Style* GetStyleDefault() {
    return gStyleDefault;
}

Style* GetStyleButtonDefault() {
#ifdef DEBUG
    return StyleByName("buttonDefault");
#else
    return gStyleButtonDefault;
#endif
}

Style* GetStyleButtonDefaultMouseOver() {
#ifdef DEBUG
    return StyleByName("buttonDefaultMouseOver");
#else
    return gStyleButtonMouseOver;
#endif
}

} // namespace css
} // namespace mui
