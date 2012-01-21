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
        free(fontName.name);
    }
}

Prop *Prop::AllocFontName(const TCHAR *name)
{
    // TODO: a saner way to do this, it's going to be tedious
    // to repeat all this code for each property type
    for (size_t i = 0; i < gAllProps->Count(); i++) {
        Prop *p = gAllProps->At(i);
        if (PropFontName != p->type)
            continue;
        if (!str::Eq(p->fontName.name, name))
            continue;
        return p;
    }
    Prop *np = new Prop(PropFontName);
    np->fontName.name = str::Dup(name);
    gAllProps->Append(np);
    return np;
}

Prop *Prop::AllocFontSize(float size)
{
    return NULL;
}

Prop *Prop::AllocFontWeight(FontStyle style)
{
    return NULL;
}

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

