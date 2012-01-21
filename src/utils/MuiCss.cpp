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

TODO: not sure how to implement "cascading" part of CSS. One
option is to use containment hierarchy so that implictly a
button inside a window inherits unset properties from its
parent, but that seems weird (since window could then get
its properties from window's default PropSet, while we would
want to use a separate default PropSets for buttons).

Another option is to have explicit cascading hierarchy in
PropSet itself (i.e. a pointer to parent PropSet from which
it would inherit its properties if not set, or from default
set if NULL).

TODO: a way to change default PropSets for easy global
customization of app's look.

TODO: Prop allocations should come from block allocator, so
that we don't pay OS's malloc() per-object overhead.

*/

namespace mui {
namespace css {

Vec<Prop*> *gAllProps;

void Initialize()
{
    gAllProps = new Vec<Prop*>();
    // TODO: create default PropSets
}

void Destroy()
{
    DeleteVecMembers(*gAllProps);
    delete gAllProps;
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

} // namespace css
} // namespace mui

