/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

VerticalLayout& VerticalLayout::Add(Control *c)
{
    VerticalLayoutData d;
    d.control = c;
    controls.Append(d);
    return *this;
}

void VerticalLayout::Measure(const Size availableSize)
{

}

void VerticalLayout::Arrange(const Rect finalRect)
{

}

}
