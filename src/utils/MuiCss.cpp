/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MuiCss.h"

using namespace Gdiplus;

/*
A css-like way to style controls/windows.

We define a bunch of css-like properties.

We have a PropSet, which is a logical group of properties.

Each control can have one or more PropSets that define how
a control looks like. A window has only one set of properties
but a button has several, one for each visual state of
the button (normal, on hover, pressed, default).

We define a bunch of default PropSet so that if e.g. button
doesn't have PropSet explicitly set, it'll get all the necessary
properties from our default set and have a consistent look.

Prop objects are never freed. To conserve memory, they are
internalized i.e. there are never 2 Prop objects with exactly
the same data.

TODO: Prop allocations should come from block allocator, so
that we don't pay OS's malloc() per-object overhead.

*/

namespace mui {
namespace css {

struct FontCacheEntry {
    Prop *fontName;
    Prop *fontSize;
    Prop *fontWeight;
    Font *font;

    // Prop objects are interned, so if the pointer is
    // the same, the value is the same too
    bool Eq(FontCacheEntry *other) {
        return ((fontName == other->fontName) &&
                (fontSize == other->fontSize) &&
                (fontWeight == other->fontWeight));
    }
};


Vec<Prop*> *gAllProps = NULL;

PropSet *gDefaults = NULL;

PropSet *gPropSetButtonRegular = NULL;
PropSet *gPropSetButtonMouseOver = NULL;

static Vec<FontCacheEntry> *gCachedFonts = NULL;

void Initialize()
{
    CrashIf(gAllProps);
    gAllProps = new Vec<Prop*>();
    gCachedFonts = new Vec<FontCacheEntry>();

    // gDefaults is the very basic set shared by everyone
    gDefaults = new PropSet();
    gDefaults->Set(Prop::AllocFontName(L"Times New Roman"));
    gDefaults->Set(Prop::AllocFontSize(14));
    gDefaults->Set(Prop::AllocFontWeight(FontStyleBold));

    gPropSetButtonRegular = new PropSet();
    gPropSetButtonRegular->inheritsFrom = gDefaults;

    gPropSetButtonMouseOver = new PropSet();
    gPropSetButtonMouseOver->inheritsFrom = gDefaults;
}

static void DeleteCachedFonts()
{
    for (size_t i = 0; i < gCachedFonts->Count(); i++) {
        FontCacheEntry c = gCachedFonts->At(i);
        ::delete c.font;
    }
    delete gCachedFonts;
}

void Destroy()
{
    DeleteVecMembers(*gAllProps);
    delete gAllProps;

    delete gPropSetButtonRegular;
    delete gPropSetButtonMouseOver;

    DeleteCachedFonts();
}

Prop::~Prop()
{
    if (PropFontName == type) {
        free((void*)fontName.name);
    }
}

bool Prop::Eq(Prop *other)
{
    if (type != other->type)
        return false;
    switch (type) {
    case PropFontName:
        return str::Eq(fontName.name, other->fontName.name);
    case PropFontSize:
        return fontSize.size == other->fontSize.size;
    case PropFontWeight:
        return fontWeight.style == other->fontWeight.style;
    default:
        CrashIf(true);
        break;
    }
    return false;
}

// TODO: could speed up this at the expense of insert speed by
// sorting gAllProps by prop->type, so that we only need to
// search a subset of gAllProps. We could either explicitly
// maintain an index of PropType -> index in gAllProps of
// first property of this type or do binary search.
static Prop *FindExistingProp(Prop *prop)
{
    for (size_t i = 0; i < gAllProps->Count(); i++) {
        Prop *p = gAllProps->At(i);
        if (p->Eq(prop))
            return p;
    }
    return NULL;
}

// can't use ALLOC_BODY() because it has to create a copy of name
Prop *Prop::AllocFontName(const TCHAR *name)
{
    Prop p(PropFontName);
    p.fontName.name = name;
    Prop *newProp = FindExistingProp(&p);
    // perf trick: we didn't str::Dup() fontName.name so must NULL
    // it out so that ~Prop() doesn't try to free it
    p.fontName.name = NULL;
    if (newProp)
        return newProp;
    newProp = new Prop(PropFontName);
    newProp->fontName.name = str::Dup(name);
    gAllProps->Append(newProp);
    return newProp;
}

#define ALLOC_BODY(propType, structName, argName) \
    Prop p(propType); \
    p.structName.argName = argName; \
    Prop *newProp = FindExistingProp(&p); \
    if (newProp) \
        return newProp; \
    newProp = new Prop(propType); \
    newProp->structName.argName = argName; \
    gAllProps->Append(newProp); \
    return newProp

Prop *Prop::AllocFontSize(float size)
{
    ALLOC_BODY(PropFontSize, fontSize, size);
}

Prop *Prop::AllocFontWeight(FontStyle style)
{
    ALLOC_BODY(PropFontWeight, fontWeight, style);
}

#undef ALLOC_BODY

// Add a property to a set, if a given PropType doesn't exist,
// replace if a given PropType already exists in the set.
void PropSet::Set(Prop *prop)
{
    for (size_t i = 0; i < props.Count(); i++) {
        Prop *p = props.At(i);
        if (p->type == prop->type) {
            props.At(i) = prop;
            return;
        }
    }
    props.Append(prop);
}

static void FindFontProps(PropSet *props, Prop **fontNameOut, Prop **fontSizeOut, Prop **fontWeightOut)
{
    Prop *p;

    PropSet *curr = props;
    while (curr) {
        for (size_t i = 0; i < curr->props.Count(); i++) {
            p = curr->props.At(i);
            if ((PropFontName == p->type) && (NULL == *fontNameOut))
                *fontNameOut = p;
            if ((PropFontSize == p->type) && (NULL == *fontSizeOut))
                *fontSizeOut = p;
            if ((PropFontWeight == p->type) && (NULL == *fontWeightOut))
                *fontWeightOut = p;
            if (*fontNameOut && *fontSizeOut && *fontWeightOut)
                return; // found them all
        }
        curr = curr->inheritsFrom;
    }
}

// convenience function: given 2 set of properties, find font-related
// properties and construct a font object.
// Providing 2 sets of props is an optimization: conceptually it's equivalent
// to setting propsFirst->inheritsFrom = propsSecond, but this way allows
// us to have global properties for known cases and not create dummy PropSets just
// to link them (e.g. if VirtWndButton::cssRegular is NULL, we'll use gPropSetButtonRegular)
// Caller should not delete the font - it's cached for performance and deleted at exit
// in DeleteCachedFonts()
Font *CachedFontFromProps(PropSet *propsFirst, PropSet *propsSecond)
{
    Prop *fontName = NULL, *fontSize = NULL, *fontWeight = NULL;
    FindFontProps(propsFirst, &fontName, &fontSize, &fontWeight);
    FindFontProps(propsSecond, &fontName, &fontSize, &fontWeight);
    CrashIf(!fontName || !fontSize || !fontWeight);
    FontCacheEntry c = { fontName, fontSize, fontWeight, NULL };
    for (size_t i = 0; i < gCachedFonts->Count(); i++) {
        FontCacheEntry c2 = gCachedFonts->At(i);
        if (c2.Eq(&c)) {
            CrashIf(NULL == c2.font);
            return c2.font;
        }
    }
    c.font = ::new Font(fontName->fontName.name, fontSize->fontSize.size, fontWeight->fontWeight.style);
    gCachedFonts->Append(c);
    return c.font;
}

} // namespace css
} // namespace mui

