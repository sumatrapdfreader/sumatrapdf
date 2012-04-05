/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"
#include "VecSegmented.h"
#include "Scoped.h"

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

#define MKARGB(a, r, g, b) (((ARGB) (b)) | ((ARGB) (g) << 8) | ((ARGB) (r) << 16) | ((ARGB) (a) << 24))
#define MKRGB(r, g, b) (((ARGB) (b)) | ((ARGB) (g) << 8) | ((ARGB) (r) << 16) | ((ARGB)(0xff) << 24))

struct FontCacheEntry {
    Prop *fontName;
    Prop *fontSize;
    Prop *fontWeight;
    Font *font;

    // Prop objects are interned, so if the pointer is
    // the same, the value is the same too
    bool operator==(FontCacheEntry& other) const {
        return ((fontName == other.fontName) &&
                (fontSize == other.fontSize) &&
                (fontWeight == other.fontWeight));
    }
};

// gStyleDefault is a fallback style. It contains a default
// value for each possible property. If a property is not
// found in a given style, we use the value from gStyleDefault
// An app might modify gStyleDefault but it should be done
// as a first thing to avoid stale props from caching (cache
// will be correctly refreshed if Control::SetStyle() is called)
Style *gStyleDefault = NULL;
// gStyleButtonDefault and gStyleButtonMouseOver are default
// button styles for convenience. The user must explicitly
// use them in inheritance chain of their custom button styles
Style *gStyleButtonDefault = NULL;
Style *gStyleButtonMouseOver = NULL;

struct StyleCacheEntry {
    Style *     style1;
    size_t      style1Id;
    Style *     style2;
    size_t      style2Id;
    CachedStyle cachedStyle;
};

// Those must be VecSegmented so that code can retain pointers to
// their elements (we can't move the memory)
static VecSegmented<Prop> *            gAllProps = NULL;
static VecSegmented<StyleCacheEntry> * gStyleCache = NULL;

void Initialize()
{
    CrashIf(gAllProps);

    gAllProps = new VecSegmented<Prop>();

    // gStyleDefault must have values for all properties
    gStyleDefault = new Style();
    gStyleDefault->Set(Prop::AllocFontName(L"Times New Roman"));
    gStyleDefault->Set(Prop::AllocFontSize(14));
    gStyleDefault->Set(Prop::AllocFontWeight(FontStyleBold));
    gStyleDefault->Set(Prop::AllocColorSolid(PropColor, "black"));
    //gStyleDefault->Set(Prop::AllocColorSolid(PropBgColor, 0xff, 0xff, 0xff));
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
    gStyleDefault->Set(Prop::AllocAlign(PropVertAlign, ElAlignCenter));
    gStyleDefault->Set(Prop::AllocAlign(PropHorizAlign, ElAlignCenter));
    gStyleDefault->Set(Prop::AllocColorSolid(PropFill, "white"));
    gStyleDefault->Set(Prop::AllocColorSolid(PropStroke, "black"));
    gStyleDefault->Set(Prop::AllocWidth(PropStrokeWidth, 0.5f));

    gStyleButtonDefault = new Style(gStyleDefault);
    gStyleButtonDefault->Set(Prop::AllocPadding(4, 8, 4, 8));
    gStyleButtonDefault->Set(Prop::AllocFontName(L"Lucida Grande"));
    gStyleButtonDefault->Set(Prop::AllocFontSize(8));
    gStyleButtonDefault->Set(Prop::AllocFontWeight(FontStyleBold));

    gStyleButtonMouseOver = new Style(gStyleButtonDefault);
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderTopColor, "#777"));
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderRightColor, "#777"));
    gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBorderBottomColor, "#666"));
    //gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBgColor, 180, 0, 0, 255));
    //gStyleButtonMouseOver->Set(Prop::AllocColorSolid(PropBgColor, "transparent"));

    gStyleCache = new VecSegmented<StyleCacheEntry>();
}

void Destroy()
{
    for (Prop *p = gAllProps->IterStart(); p; p = gAllProps->IterNext()) {
        p->Free();
    }

    delete gAllProps;

    delete gStyleButtonDefault;
    delete gStyleButtonMouseOver;

    delete gStyleCache;
    delete gStyleDefault;
}

bool IsWidthProp(PropType type)
{
    return (PropBorderTopWidth == type) ||
           (PropBorderRightWidth == type) ||
           (PropBorderBottomWidth == type) ||
           (PropBorderLeftWidth == type) ||
           (PropStrokeWidth == type);
}

bool IsColorProp(PropType type)
{
    return (PropColor == type) ||
           (PropBgColor == type) ||
           (PropBorderTopColor == type) ||
           (PropBorderRightColor == type) ||
           (PropBorderBottomColor == type) ||
           (PropBorderLeftColor == type) ||
           (PropFill == type) ||
           (PropStroke == type);
}

bool IsAlignProp(PropType type)
{
    return ((PropVertAlign == type) || (PropHorizAlign == type));
}

static const char *gCssKnownColorsStrings = "black\0blue\0gray\0green\0red\0transparent\0white\0yellow\0";
static ARGB gCssKnownColorsValues[] = { MKRGB(0, 0, 0), MKRGB(0,0,255), MKRGB(128,128,128), MKRGB(0,128,0), MKRGB(255,0,0), MKARGB(0,0,0,0), MKRGB(255,255,255), MKRGB(255,255,0) };

static bool GetKnownCssColor(const char *name, ARGB& colOut)
{
    int pos = str::FindStrPosI(gCssKnownColorsStrings, name, str::Len(name));
    if (-1 == pos)
        return false;
    colOut = gCssKnownColorsValues[pos];
    return true;
}

// Parses css-like color formats:
// rrggbb, #rrggbb, #aarrggbb, #rgb
// rgb(r,g,b), rgba(r,g,b,a) rgb(r%, g%, b%), rgba(r%, g%, b%, a%)
// cf. https://developer.mozilla.org/en/CSS/color_value
static ARGB ParseCssColor(const char *color)
{
    // parse #RRGGBB and #RGB and rgb(R,G,B)
    int a, r, g, b;

    // #rgb is shorthand for #rrggbb
    if (str::Parse(color, "#%1x%1x%1x%$", &r, &g, &b)) {
        r |= (r << 4);
        g |= (g << 4);
        b |= (b << 4);
        return MKRGB(r, g, b);
    }

    // rrggbb, #rrggbb and rgb(n,n,n)
    if (str::Parse(color, "#%2x%2x%2x%$", &r, &g, &b) ||
        str::Parse(color, "%2x%2x%2x%$", &r, &g, &b) ||
        str::Parse(color, "rgb(%d,%d,%d)", &r, &g, &b)) {
        return MKRGB(r, g, b);
    }

    // parse rgba(R,G,B,A) and #aarrggbb
    if (str::Parse(color, "#%2x%2x%2x%2x%$", &a, &r, &g, &b) ||
        str::Parse(color, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a)) {
        return MKARGB(a, r, g, b);
    }

    // parse rgb(R%,G%,B%) and rgba(R%,G%,B%,A%)
    float fa = 1.0f, fr, fg, fb;
    if (str::Parse(color, "rgb(%f%%,%f%%,%f%%)", &fr, &fg, &fb) ||
        str::Parse(color, "rgba(%f%%,%f%%,%f%%,%f%%)", &fr, &fg, &fb, &fa)) {
        return MKARGB((int)(fa * 2.55f), (int)(fr * 2.55f), (int)(fg * 2.55f), (int)(fb * 2.55f));
    }

    // parse known color names
    ARGB colVal = MKARGB(0,0,0,0); // transparent if not known
    GetKnownCssColor(color, colVal);
    return colVal;
}

bool ColorData::operator==(const ColorData& other) const
{
    if (type != other.type)
        return false;

    if (ColorSolid == type)
        return solid.color == other.solid.color;

    if (ColorGradientLinear == type)
    {
        return (gradientLinear.mode       == other.gradientLinear.mode) &&
               (gradientLinear.startColor == other.gradientLinear.startColor) &&
               (gradientLinear.endColor   == other.gradientLinear.endColor);
    }
    CrashIf(true);
    return false;
}

bool ElAlignData::operator==(const ElAlignData& other) const
{
    return ((elementPoint == other.elementPoint) && (containerPoint == other.containerPoint));
}

void ElAlignData::Set(ElAlign align)
{
    if (ElAlignCenter == align) {
        elementPoint   = .5f;
        containerPoint = .5f;
    } else if ((ElAlignTop == align) || (ElAlignLeft == align)) {
        elementPoint   = 0.f;
        containerPoint = 0.f;
    } else if ((ElAlignBottom == align) || (ElAlignRight == align)) {
        elementPoint   = 1.f;
        containerPoint = 1.f;
    } else {
        CrashIf(true);
    }
}

// calculates the offset of an element within container
int ElAlignData::CalcOffset(int elSize, int containerSize)
{
    int ep = (int)((float)elSize        * elementPoint  );
    int cp = (int)((float)containerSize * containerPoint);
    return cp - ep;
}

void Prop::Free()
{
    if (PropFontName == type)
        free(fontName);

    if (IsColorProp(type) && (ColorSolid == color.type))
        ::delete color.solid.cachedBrush;
    if (IsColorProp(type) && (ColorGradientLinear == color.type)) {
        ::delete color.gradientLinear.cachedBrush;
        ::delete color.gradientLinear.rect;
    }
}

bool Prop::Eq(const Prop *other) const
{
    if (type != other->type)
        return false;

    switch (type) {
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

static Prop *FindExistingProp(Prop *prop)
{
    for (Prop *p = gAllProps->IterStart(); p; p = gAllProps->IterNext()) {
        if (p->Eq(prop))
            return p;
    }
    return NULL;
}

static Prop *UniqifyProp(Prop& p)
{
    Prop *existing = FindExistingProp(&p);
    if (existing) {
        p.Free();
        return existing;
    }
    return gAllProps->Append(p);
}

Prop *Prop::AllocFontName(const WCHAR *name)
{
    Prop p(PropFontName);
    p.fontName = str::Dup(name);
    return UniqifyProp(p);
}

Prop *Prop::AllocFontSize(float size)
{
    Prop p(PropFontSize);
    p.fontSize = size;
    return UniqifyProp(p);
}

Prop *Prop::AllocFontWeight(FontStyle style)
{
    Prop p(PropFontWeight);
    p.fontWeight = style;
    return UniqifyProp(p);
}

Prop *Prop::AllocWidth(PropType type, float width)
{
    CrashIf(!IsWidthProp(type));
    Prop p(type);
    p.width = width;
    return UniqifyProp(p);
}

Prop *Prop::AllocTextAlign(AlignAttr align)
{
    Prop p(PropTextAlign);
    p.textAlign = align;
    return UniqifyProp(p);
}

Prop *Prop::AllocAlign(PropType type, float elPoint, float containerPoint)
{
    CrashIf(!IsAlignProp(type));
    Prop p(type);
    p.elAlign = GetElAlign(elPoint, containerPoint);
    return UniqifyProp(p);
}

Prop *Prop::AllocAlign(PropType type, ElAlign align)
{
    CrashIf(!IsAlignProp(type));
    Prop p(type);
    p.elAlign.Set(align);
    return UniqifyProp(p);
}

Prop *Prop::AllocPadding(int top, int right, int bottom, int left)
{
    Padding pd = { top, right, bottom, left };
    Prop p(PropPadding);
    p.padding = pd;
    return UniqifyProp(p);
}

Prop *Prop::AllocColorSolid(PropType type, ARGB color)
{
    CrashIf(!IsColorProp(type));
    Prop p(type);
    p.color.type = ColorSolid;
    p.color.solid.color = color;
    p.color.solid.cachedBrush = ::new SolidBrush(color);
    return UniqifyProp(p);
}

Prop *Prop::AllocColorSolid(PropType type, int a, int r, int g, int b)
{
    return AllocColorSolid(type, MKARGB(a, r, g, b));
}

Prop *Prop::AllocColorSolid(PropType type, int r, int g, int b)
{
    return AllocColorSolid(type, MKARGB(0xff, r, g, b));
}

Prop *Prop::AllocColorLinearGradient(PropType type, LinearGradientMode mode, ARGB startColor, ARGB endColor)
{
    Prop p(type);
    p.color.type = ColorGradientLinear;
    p.color.gradientLinear.mode = mode;
    p.color.gradientLinear.startColor = startColor;
    p.color.gradientLinear.endColor = endColor;

    p.color.gradientLinear.rect = ::new RectF();
    p.color.gradientLinear.cachedBrush = NULL;
    return UniqifyProp(p);
}

Prop *Prop::AllocColorLinearGradient(PropType type, LinearGradientMode mode, const char *startColor, const char *endColor)
{
    ARGB c1 = ParseCssColor(startColor);
    ARGB c2 = ParseCssColor(endColor);
    return AllocColorLinearGradient(type, mode, c1, c2);
}

Prop *Prop::AllocColorSolid(PropType type, const char *color)
{
    ARGB col = ParseCssColor(color);
    return AllocColorSolid(type, col);
}

Style* Style::GetInheritsFrom() const
{
    return inheritsFrom;
}

// Identity is a way to track changes to Style
size_t Style::GetIdentity() const
{
    int identity = gen;
    Style *curr = inheritsFrom;
    while (curr) {
        identity += curr->gen;
        curr = curr->inheritsFrom;
    }
    return identity;
}

// Add a property to a set, if a given PropType doesn't exist,
// replace if a given PropType already exists in the set.
void Style::Set(Prop *prop)
{
    CrashIf(!prop);
    for (Prop **p = props.IterStart(); p; p = props.IterNext()) {
        if ((*p)->type == prop->type) {
            if (!prop->Eq(*p))
                ++gen;
            *p = prop;
            return;
        }
    }
    props.Append(prop);
    ++gen;
}

void Style::SetBorderWidth(float width)
{
    Set(Prop::AllocWidth(PropBorderTopWidth, width));
    Set(Prop::AllocWidth(PropBorderRightWidth, width));
    Set(Prop::AllocWidth(PropBorderBottomWidth, width));
    Set(Prop::AllocWidth(PropBorderLeftWidth, width));
}

void Style::SetBorderColor(ARGB color)
{
    Set(Prop::AllocColorSolid(PropBorderTopColor, color));
    Set(Prop::AllocColorSolid(PropBorderRightColor, color));
    Set(Prop::AllocColorSolid(PropBorderBottomColor, color));
    Set(Prop::AllocColorSolid(PropBorderLeftColor, color));
}

static bool FoundAllProps(Prop **props)
{
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
static bool GetAllProps(Style *style, Prop **props)
{
    while (style) {
        for (Prop **p = style->props.IterStart(); p; p = style->props.IterNext()) {
            int propIdx = (int)(*p)->type;
            CrashIf(propIdx >= (int)PropsCount);
            bool didSet = false;
            if (!props[propIdx]) {
                props[propIdx] = *p;
                didSet = true;
            }
            if (didSet && FoundAllProps(props))
                return true;
        }
        style = style->GetInheritsFrom();
    }
    return false;
}

static size_t GetStyleId(Style *style) {
    if (!style)
        return 0;
    return style->GetIdentity();
}

CachedStyle *CacheStyle(Style *style)
{
    ScopedMuiCritSec muiCs;

    Style *style1 = style;
    Style *style2 = gStyleDefault;

    StyleCacheEntry *e;
    bool updateEntry = false;
    for (e = gStyleCache->IterStart(); e; e = gStyleCache->IterNext()) {
        if ((e->style1 == style1) && (e->style2 == style2)) {
            if ((e->style1Id == GetStyleId(style1)) &&
                (e->style2Id == GetStyleId(style2))) {
                return &e->cachedStyle;
            }
            updateEntry = true;
            break;
        }
    }

    Prop* props[PropsCount] = { 0 };
    if (!GetAllProps(style1, props))
        GetAllProps(style2, props);
    CrashIf(!FoundAllProps(props));

    CachedStyle s;
    s.fontName             = props[PropFontName]->fontName;
    s.fontSize             = props[PropFontSize]->fontSize;
    s.fontWeight           = props[PropFontWeight]->fontWeight;
    s.padding              = props[PropPadding]->padding;
    s.color                = &(props[PropColor]->color);
    s.bgColor              = &(props[PropBgColor]->color);
    s.borderWidth.top      = props[PropBorderTopWidth]->width;
    s.borderWidth.right    = props[PropBorderRightWidth]->width;
    s.borderWidth.bottom   = props[PropBorderBottomWidth]->width;
    s.borderWidth.left     = props[PropBorderLeftWidth]->width;
    s.borderColors.top     = &(props[PropBorderTopColor]->color);
    s.borderColors.right   = &(props[PropBorderRightColor]->color);
    s.borderColors.bottom  = &(props[PropBorderBottomColor]->color);
    s.borderColors.left    = &(props[PropBorderLeftColor]->color);
    s.textAlign            = props[PropTextAlign]->textAlign;
    s.vertAlign            = props[PropVertAlign]->elAlign;
    s.horizAlign           = props[PropHorizAlign]->elAlign;
    s.fill                 = &(props[PropFill]->color);
    s.stroke               = &(props[PropStroke]->color);
    s.strokeWidth          = props[PropStrokeWidth]->width;

    if (updateEntry) {
        e->cachedStyle = s;
        return &e->cachedStyle;
    }

    StyleCacheEntry newEntry = { style1, GetStyleId(style1), style2, GetStyleId(style2), s };
    e = gStyleCache->Append(newEntry);
    return &e->cachedStyle;
}

Brush *BrushFromColorData(ColorData *color, const RectF& r)
{
    if (ColorSolid == color->type)
        return color->solid.cachedBrush;

    if (ColorGradientLinear == color->type) {
        ColorDataGradientLinear *d = &color->gradientLinear;
        LinearGradientBrush *br = d->cachedBrush;
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

Brush *BrushFromColorData(ColorData *color, const Rect& r)
{
    return BrushFromColorData(color, RectF((float)r.X, (float)r.Y, (float)r.Width, (float)r.Height));
}

} // namespace css
} // namespace mui
